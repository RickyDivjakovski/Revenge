#include "revenge/loader.hpp"
#include "revenge/launch.hpp"
#include <algorithm>
#include <array>
#include <fstream>
#include <limits>
#include <sstream>
#include <stdexcept>
#include <unordered_set>
namespace revenge { namespace {
constexpr uint64_t max_image=512ull*1024*1024;
void bounds(uint64_t p,uint64_t n,uint64_t size){if(p>size||n>size-p)throw std::runtime_error("loader: invalid image range");}
uint64_t get(std::span<const uint8_t> b,uint64_t p,unsigned n){bounds(p,n,b.size());uint64_t v=0;for(unsigned i=0;i<n;++i)v|=uint64_t(b[p+i])<<(8*i);return v;}
void put(std::span<uint8_t> b,uint64_t p,uint64_t v,unsigned n){bounds(p,n,b.size());for(unsigned i=0;i<n;++i)b[p+i]=uint8_t(v>>(8*i));}
uint64_t va_rva(uint64_t va,uint64_t base,uint64_t size,uint64_t count){if(va<base)throw std::runtime_error("loader: VA below image base");auto r=va-base;bounds(r,count,size);return r;}
}
LoadPlan prepare_image(std::span<const uint8_t> bytes,uint64_t new_base){
 LoadPlan p; p.image=inspect(bytes);auto& x=p.image;
 if(x.image_size>max_image)throw std::runtime_error("loader: image exceeds 512 MiB limit");
 if(!new_base)new_base=x.base;
 if(new_base%65536 || new_base>UINT64_MAX-x.image_size)throw std::runtime_error("loader: invalid target base");
 p.mapped_base=new_base; p.mapped.resize(x.image_size);
 auto pe=get(bytes,0x3c,4),opt=pe+24,os=get(bytes,pe+20,2),hdr=get(bytes,opt+60,4),dirs=get(bytes,opt+108,4);
 auto dir=[&](unsigned i){if(i>=dirs||112ull+8ull*(i+1)>os)return std::pair<uint64_t,uint64_t>{0,0};auto a=get(bytes,opt+112+8ull*i,4),n=get(bytes,opt+116+8ull*i,4);if(a)bounds(a,n,x.image_size);return std::pair{a,n};};
 std::copy_n(bytes.begin(),hdr,p.mapped.begin());
 std::vector<std::pair<uint64_t,uint64_t>> used{{0,hdr}};
 for(auto& s:x.sections){uint64_t a=s.rva,n=std::max(s.raw_size,s.virtual_size);if(!n)continue;bounds(a,n,x.image_size);for(auto [lo,hi]:used)if(a<hi&&lo<a+n)throw std::runtime_error("loader: overlapping sections");used.emplace_back(a,a+n);if(s.raw_size)std::copy_n(bytes.begin()+s.raw,s.raw_size,p.mapped.begin()+a);}
 auto m=std::span<uint8_t>(p.mapped);
 // Resolve the exact IAT slot RVAs but do not bind them to unknown host functions.
 auto [imp,is]=dir(1);if(imp){for(size_t i=0;i<x.imports.size();++i){uint64_t desc=imp+20ull*i;bounds(desc,20,m.size());auto ft=get(m,desc+16,4);auto& d=x.imports[i];bounds(ft,8ull*(d.symbols.size()+1),m.size());for(size_t j=0;j<d.symbols.size();++j)p.slots.push_back({d.dll,d.symbols[j],uint32_t(ft+8ull*j)});}}
 auto [rel,rs]=dir(5);if(!rel&&new_base!=x.base)throw std::runtime_error("loader: cannot rebase image without relocations");
 std::vector<uint64_t> targets;std::unordered_set<uint64_t> seen;
 if(rel){uint64_t pos=rel,end=rel+rs;while(pos<end){bounds(pos,8,end);auto page=get(m,pos,4),size=get(m,pos+4,4);if(size<8||size%4||size>end-pos||page%4096)throw std::runtime_error("loader: invalid relocation block");for(uint64_t at=pos+8;at<pos+size;at+=2){auto e=get(m,at,2),type=e>>12;if(type==0)continue;if(type!=10)throw std::runtime_error("loader: unsupported AMD64 relocation type "+std::to_string(type));auto r=page+(e&0xfff);bounds(r,8,m.size());if(!seen.insert(r).second)throw std::runtime_error("loader: duplicate relocation");targets.push_back(r);}pos+=size;}}
 for(auto r:targets)put(m,r,get(m,r,8)+(new_base-x.base),8);
 p.relocations=targets.size();
 auto [tls,ts]=dir(9);if(tls){if(ts<40)throw std::runtime_error("loader: truncated TLS directory");auto start=get(m,tls,8),end=get(m,tls+8,8),index=get(m,tls+16,8),cb=get(m,tls+24,8);if(end<start)throw std::runtime_error("loader: reversed TLS range");p.tls_bytes=end-start;p.tls_zero_fill=get(m,tls+32,4);if(start||end)va_rva(start,new_base,m.size(),p.tls_bytes);if(index)va_rva(index,new_base,m.size(),4);if(cb){auto r=va_rva(cb,new_base,m.size(),8);bool done=false;for(unsigned i=0;i<65536;++i){auto a=get(m,r+8ull*i,8);if(!a){done=true;break;}va_rva(a,new_base,m.size(),1);++p.tls_callbacks;}if(!done)throw std::runtime_error("loader: excessive TLS callbacks");}}
 auto [un,us]=dir(3);if(un){if(us%12)throw std::runtime_error("loader: malformed x64 exception table");p.unwind_entries=us/12;for(uint64_t i=0;i<us;i+=12){auto b=get(m,un+i,4),e=get(m,un+i+4,4),u=get(m,un+i+8,4);if(b>=e||e>m.size()||u>=m.size())throw std::runtime_error("loader: invalid runtime function range");}}
 p.delay_imports=dir(13).first!=0;p.load_config=dir(10).first!=0;
 return p;
}
LoadPlan prepare_file(const std::filesystem::path& path,uint64_t base){std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("cannot open executable");auto n=f.tellg();if(n<0||n>std::streamoff(max_image))throw std::runtime_error("file too large (512 MiB limit)");std::vector<uint8_t>b(static_cast<size_t>(n));f.seekg(0);if(!f.read(reinterpret_cast<char*>(b.data()),n))throw std::runtime_error("read failed");return prepare_image(b,base);}
std::string preparation_report(const LoadPlan& p,const std::filesystem::path& exe){std::ostringstream o;o<<"Revenge Native - offline loader preparation\n"<<"Image bytes: "<<p.mapped.size()<<"\nPreferred base: 0x"<<std::hex<<p.image.base<<"\nPrepared base: 0x"<<p.mapped_base<<"\nEntry VA: 0x"<<p.mapped_base+p.image.entry<<std::dec<<"\nDIR64 relocations: "<<p.relocations<<"\nIAT slots (unbound): "<<p.slots.size()<<"\nTLS template bytes: "<<p.tls_bytes<<"\nTLS zero fill bytes: "<<p.tls_zero_fill<<"\nTLS callbacks (not invoked): "<<p.tls_callbacks<<"\nUnwind entries (not registered): "<<p.unwind_entries<<"\nDelay imports present: "<<p.delay_imports<<"\nLoad config present: "<<p.load_config<<"\nXbox markers: "<<has_console_markers(p.image,exe)<<"\n\nDirect DLL inventory (presence is not ABI compatibility):\n";for(auto& d:p.image.imports){o<<"  "<<d.dll<<" - "<<d.symbols.size()<<" imports\n";}o<<"\nSTOP: no executable memory allocated, no imports bound, no TLS or entry point invoked. Console ABI and graphics translation are not implemented.\n";return o.str();}
}
