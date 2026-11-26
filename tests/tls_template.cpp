#include "revenge/loader.hpp"
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#elif defined(__linux__)
#include <pthread.h>
#endif
using namespace revenge;
void check(bool v){if(!v)throw std::runtime_error("TLS model assertion");}
void put(LoadPlan& p,size_t at,uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)p.mapped.at(at+i)=uint8_t(v>>(8*i));}
LoadPlan fixture(){LoadPlan p{};p.image.image_size=0x4000;p.image.base=p.mapped_base=0x180000000ull;p.header_bytes=512;p.mapped.resize(0x4000);p.image.sections={{"text",0x1000,0x1000,0,0,0x60000000},{"data",0x2000,0x1000,0,0,0xc0000000},{"rdata",0x3000,0x1000,0,0,0x40000000}};put(p,0x3c,0x80,4);put(p,0x94,240,2);put(p,0x98,0x20b,2);put(p,0x98+108,16,4);put(p,0x98+184,0x3000,4);put(p,0x98+188,40,4);put(p,0x3000,p.mapped_base+0x3050,8);put(p,0x3008,p.mapped_base+0x3054,8);put(p,0x3010,p.mapped_base+0x2000,8);put(p,0x3020,12,4);put(p,0x3024,0x500000,4);put(p,0x3050,0x12345678,4);return p;}
int main(){try{auto p=fixture();auto t=tls_template(p);check(t.present&&t.raw_rva==0x3050&&t.index_rva==0x2000&&t.alignment==16&&t.zero_fill==12&&t.initial==std::vector<uint8_t>{0x78,0x56,0x34,0x12});std::atomic<unsigned> passed{0};std::vector<std::jthread> workers;for(unsigned i=0;i<8;++i)workers.emplace_back([&,i]{for(unsigned j=0;j<512;++j){auto a=tls_instance(t);if(a.size()!=16||a[0]!=0x78||a[15]!=0)return;a[0]=uint8_t(i);a[15]=uint8_t(j);auto b=tls_instance(t);if(b[0]!=0x78||b[15]!=0)return;++passed;}});workers.clear();check(passed==4096&&p.mapped[0x2000]==0&&p.mapped[0x3050]==0x78);
 // Host dynamic TLS lifecycle, not guest static TLS installation.
#if defined(_WIN32) || defined(__linux__)
 struct HostKey{
#ifdef _WIN32
 DWORD key=TlsAlloc();HostKey(){check(key!=TLS_OUT_OF_INDEXES);}~HostKey(){TlsFree(key);}bool set(void* v){return TlsSetValue(key,v)!=0;}void* get(){return TlsGetValue(key);}
#else
 pthread_key_t key{};HostKey(){check(pthread_key_create(&key,nullptr)==0);}~HostKey(){pthread_key_delete(key);}bool set(void* v){return pthread_setspecific(key,v)==0;}void* get(){return pthread_getspecific(key);}
#endif
 } key;
 std::atomic<unsigned> host_passed{0};check(key.get()==nullptr);auto parent=tls_instance(t);check(key.set(parent.data()));
 for(unsigned i=0;i<8;++i)workers.emplace_back([&,i]{if(key.get()!=nullptr)return;auto instance=tls_instance(t);if(!key.set(instance.data()))return;for(unsigned j=0;j<512;++j){auto* data=static_cast<uint8_t*>(key.get());if(data!=instance.data()||data[1]!=0x56||data[15]!=uint8_t(j?j-1:0))return;data[0]=uint8_t(i);data[15]=uint8_t(j);++host_passed;}if(!key.set(nullptr)||key.get()!=nullptr)--host_passed;});
 workers.clear();check(host_passed==4096&&key.get()==parent.data()&&parent[0]==0x78&&parent[15]==0);check(key.set(nullptr));std::cout<<"Host dynamic TLS: 4096 slot checks, 8 worker lifecycles, parent isolation and cleanup passed. No guest static TLS.\n";
#endif
 unsigned rejected=0;auto reject=[&](size_t at,uint64_t v,unsigned n){auto q=p;put(q,at,v,n);try{(void)tls_template(q);}catch(const std::runtime_error&){++rejected;return;}throw std::runtime_error("accepted malformed TLS model");};reject(0x3008,p.mapped_base+0x3040,8);reject(0x3010,p.mapped_base+0x3050,8);reject(0x3010,p.mapped_base+0x2001,8);reject(0x3020,0xffffffff,4);reject(0x3024,0xf00000,4);reject(0x98+188,39,4);reject(0x3000,p.mapped_base-1,8);auto q=p;put(q,0x98+184,0,4);put(q,0x98+188,0,4);check(!tls_template(q).present);std::cout<<"Offline TLS: "<<passed<<" isolated instance pairs, 8 workers, zero fill, alignment metadata, unchanged source, "<<rejected<<" malformed inputs rejected. No guest static TLS installed.\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
