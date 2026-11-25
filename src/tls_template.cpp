#include "revenge/loader.hpp"
#include <sstream>
#include <stdexcept>
namespace revenge { namespace {
constexpr uint64_t limit=1024*1024;
[[noreturn]] void bad(const char* s){throw std::runtime_error(std::string("TLS template: ")+s);}
uint64_t read(std::span<const uint8_t> b,uint64_t p,unsigned n){if(p>b.size()||n>b.size()-p)bad("truncated field");uint64_t v=0;for(unsigned i=0;i<n;++i)v|=uint64_t(b[p+i])<<(i*8);return v;}
}
TlsTemplate tls_template(const LoadPlan& p){
 const auto b=std::span<const uint8_t>(p.mapped);const auto pages=page_layout(p);
 auto pe=read(b,0x3c,4),opt=pe+24,sz=read(b,pe+20,2);
 if(sz<112||read(b,opt,2)!=0x20b)bad("optional header");
 auto dirs=read(b,opt+108,4);if(dirs<=9)return {};
 if(sz<192)bad("directory outside optional header");
 auto tls=read(b,opt+184,4),n=read(b,opt+188,4);if(!tls&&!n)return {};if(!tls||n<40||tls>b.size()||n>b.size()-tls)bad("invalid directory");
 auto permitted=[&](uint64_t r,uint64_t length,uint8_t required){if(r>b.size()||length>b.size()-r)return false;for(const auto& page:pages)if(r<uint64_t(page.rva)+page.bytes&&page.rva<r+length&&(page.flags&required)!=required)return false;return true;};
 auto rva=[&](uint64_t va,uint64_t length){if(va<p.mapped_base)bad("VA below base");auto r=va-p.mapped_base;if(r>b.size()||length>b.size()-r)bad("VA outside image");return r;};
 if(!permitted(tls,40,9))bad("directory not readable");
 auto first=read(b,tls,8),last=read(b,tls+8,8),index=read(b,tls+16,8),zero=read(b,tls+32,4),characteristics=read(b,tls+36,4);
 if(last<first||(!first&&last))bad("reversed or missing template address");
 auto bytes=last-first;
 if(bytes>limit||zero>limit-bytes)bad("instance exceeds 1 MiB model cap");
 auto raw=first?rva(first,bytes):0;auto idx=rva(index,4);if(idx%4||!permitted(idx,4,10))bad("index not aligned/writable");
 if(bytes&&!permitted(raw,bytes,9))bad("template not readable");
 unsigned align=static_cast<unsigned>((characteristics>>20)&15);if(align==15)bad("reserved alignment");
 TlsTemplate out;out.present=true;out.raw_rva=static_cast<uint32_t>(raw);out.index_rva=static_cast<uint32_t>(idx);out.alignment=align?1u<<(align-1):1;out.zero_fill=static_cast<uint32_t>(zero);
 out.initial.assign(b.begin()+raw,b.begin()+raw+bytes);return out;
}
std::vector<uint8_t> tls_instance(const TlsTemplate& t){if(!t.present||t.initial.size()>limit||t.zero_fill>limit-t.initial.size())bad("invalid instance");auto out=t.initial;out.resize(out.size()+t.zero_fill,0);return out;}
std::string tls_template_report(const LoadPlan& p){auto t=tls_template(p);std::ostringstream o;o<<"Offline TLS template model (no OS TLS installation).\nTLS template present: "<<t.present<<"; raw bytes: "<<t.initial.size()<<"; zero fill: "<<t.zero_fill<<"; declared alignment model: "<<t.alignment<<".\n";if(t.present)o<<"Template RVA: 0x"<<std::hex<<t.raw_rva<<"; index RVA: 0x"<<t.index_rva<<std::dec<<"; independent instance bytes: "<<tls_instance(t).size()<<".\n";o<<"No TLS index written, callback invoked, OS TLS slot installed or target code executed.\n";return o.str();}
}
