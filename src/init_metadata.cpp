#include "revenge/loader.hpp"
#include <sstream>
#include <stdexcept>
#include <algorithm>
#include <utility>
namespace revenge {
namespace {
uint64_t read(std::span<const uint8_t> b,uint64_t p,unsigned n){if(p>b.size()||n>b.size()-p)throw std::runtime_error("init metadata: truncated range");uint64_t v=0;for(unsigned i=0;i<n;++i)v|=uint64_t(b[p+i])<<(8*i);return v;}
}
std::string init_metadata_report(const LoadPlan& p){
 auto b=std::span<const uint8_t>(p.mapped);auto nt=read(b,0x3c,4),opt=nt+24,os=read(b,nt+20,2),dirs=read(b,opt+108,4);auto dir=[&](unsigned i){if(i>=dirs||112ull+8ull*(i+1)>os)return std::pair<uint64_t,uint64_t>{0,0};auto a=read(b,opt+112+8ull*i,4),n=read(b,opt+116+8ull*i,4);if(a>b.size()||n>b.size()-a)throw std::runtime_error("init metadata: directory range");return std::pair{a,n};};
 auto spans=page_layout(p);auto flags=[&](uint64_t r,uint64_t n){if(r>b.size()||n>b.size()-r)return uint8_t(0);uint8_t f=15;for(const auto& s:spans)if(r<uint64_t(s.rva)+s.bytes&&s.rva<r+n)f&=s.flags;return f;};
 auto va=[&](uint64_t a,uint64_t n){if(a<p.mapped_base||a-p.mapped_base>b.size()||n>b.size()-(a-p.mapped_base))throw std::runtime_error("init metadata: VA out of range");return a-p.mapped_base;};
 std::ostringstream o;o<<"Offline initialisation metadata (no execution).\n";auto [tls,ts]=dir(9);unsigned callback_nonexec=0;bool writable_index=false;bool template_readable=false;uint64_t template_bytes=0;
 if(tls){if(ts<40)throw std::runtime_error("init metadata: TLS truncated");auto start=read(b,tls,8),end=read(b,tls+8,8);if(start&&end>=start){template_bytes=end-start;template_readable=template_bytes==0||(flags(va(start,template_bytes),template_bytes)&9)==9;}auto index=read(b,tls+16,8),cb=read(b,tls+24,8);if(index)writable_index=(flags(va(index,4),4)&2)!=0;if(cb){auto r=va(cb,8);bool terminated=false;for(unsigned i=0;i<65536;++i){auto a=read(b,r+8ull*i,8);if(!a){terminated=true;break;}if(!(flags(va(a,1),1)&4))++callback_nonexec;}if(!terminated)throw std::runtime_error("init metadata: TLS callbacks not terminated");}}
 o<<"TLS template bytes: "<<p.tls_bytes<<"; zero fill: "<<p.tls_zero_fill<<"; writable index: "<<writable_index<<"; callback targets outside executable pages: "<<callback_nonexec<<".\n";
 o<<"TLS raw template extent: "<<template_bytes<<"; readable occupied template: "<<template_readable<<".\n";
 auto [un,us]=dir(3);uint64_t v1=0,v2=0,other=0,handlers=0,chained=0,nonexec=0,unordered=0,invalid=0,previous=0,checked=0,invalid_codes=0,skipped_versions=0,operations=0,max_stack=0;
 if(un){if(us%12)throw std::runtime_error("init metadata: exception table size");for(uint64_t j=0;j<us;j+=12){auto a=read(b,un+j,4),e=read(b,un+j+4,4),u=read(b,un+j+8,4);if(j&&a<previous)++unordered;previous=e;if(e<=a||!(flags(a,e-a)&4))++nonexec;
  // A bounded header/slot/tail check, not a semantic x64 unwind interpreter.
  if(u%4||u>b.size()||b.size()-u<4){++invalid;continue;}auto tag=read(b,u,1),version=tag&7,f=tag>>3,count=read(b,u+2,1);if(version==1)++v1;else if(version==2)++v2;else ++other;
  auto tail=u+4+2*((count+1)&~UINT64_C(1));auto need=(f&4)?12ull:(f&3)?4ull:0ull;if(f&4)++chained;if(f&3)++handlers;if(version<1||version>2||f>7||((f&4)&&(f&3))||tail>b.size()||need>b.size()-tail){++invalid;continue;}
  if(version==1){
   ++checked;bool bad=false;uint64_t stack=0,prev=read(b,u+1,1);unsigned frame=static_cast<unsigned>(read(b,u+3,1));if(!(frame&15)&&(frame>>4))bad=true;
   for(uint64_t k=0;k<count&&!bad;){auto at=u+4+2*k,off=read(b,at,1),tag2=read(b,at+1,1),op=tag2&15,info=tag2>>4;uint64_t slots=1,amount=0;
    if(off>prev){bad=true;break;}prev=off;
    switch(op){case 0: amount=8;break;case 1: if(info>1){bad=true;break;}slots=info?3:2;break;case 2:amount=8+8*info;break;case 3:if(info||!(frame&15))bad=true;break;case 4:case 8:slots=2;break;case 5:case 9:slots=3;break;case 10:if(info>1)bad=true;else amount=40+8*info;break;default:bad=true;break;}
    if(bad||slots>count-k){bad=true;break;}if(op==1){amount=info?read(b,at+2,4):8*read(b,at+2,2);if(!amount||amount%8)bad=true;}stack+=amount;++operations;k+=slots;
   }if(bad)++invalid_codes;else max_stack=std::max(max_stack,stack);
  }else ++skipped_versions;
  if((f&3)&&!(flags(read(b,tail,4),1)&4))++invalid;
  if(f&4){auto ca=read(b,tail,4),ce=read(b,tail+4,4),cu=read(b,tail+8,4);if(ce<=ca||!(flags(ca,ce-ca)&4)||cu%4||cu>b.size()||b.size()-cu<4)++invalid;}
 }}
 o<<"Unwind headers: v1="<<v1<<"; v2="<<v2<<"; other="<<other<<"; handlers="<<handlers<<"; chained="<<chained<<".\nRuntime function ranges outside executable pages: "<<nonexec<<"; unordered/overlap: "<<unordered<<"; suspicious header/tail records: "<<invalid<<".\n";
 o<<"Unwind v1 slot diagnostics: records checked="<<checked<<"; operations="<<operations<<"; suspicious slot records="<<invalid_codes<<"; other versions skipped="<<skipped_versions<<"; maximum local stack adjustment="<<max_stack<<".\n";
 o<<"Relocations applied to data model: "<<p.relocations<<"; IAT unbound: "<<p.slots.size()<<". No TLS callbacks, unwind registration or entry-point invocation. Not a compatibility verdict.\n";return o.str();
}
}
