#include "revenge/target_plan.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
using namespace revenge;
void put(LoadPlan& p,size_t at,uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)p.mapped.at(at+i)=static_cast<uint8_t>(v>>(8*i));}
void name(LoadPlan& p,size_t at,const char* s){for(;*s;++s)put(p,at++,static_cast<unsigned char>(*s),1);put(p,at,0,1);}
void check(bool v){if(!v)throw std::runtime_error("target plan fixture mismatch");}
int main(){try{LoadPlan p{};p.image.image_size=0x4000;p.image.machine=0x8664;p.image.subsystem=3;p.image.entry=0x1000;p.image.imports={{"kernel32.dll",{"Sleep"},{false}},{"d3d12_x.dll",{"Example"},{false}}};p.mapped.resize(0x4000);put(p,0x3c,0x80,4);put(p,0x94,240,2);constexpr size_t opt=0x98;put(p,opt+108,16,4);put(p,opt+112+13*8,0x2000,4);put(p,opt+116+13*8,64,4);put(p,opt+112+10*8,0x3000,4);put(p,opt+116+10*8,148,4);put(p,0x3000,148,4);put(p,0x3090,0x100,4);
 put(p,0x2000,1,4);put(p,0x2004,0x2100,4);put(p,0x200c,0x2200,4);put(p,0x2010,0x2300,4);name(p,0x2100,"example.dll");put(p,0x2300,0x2400,8);put(p,0x2308,0x8000000000000005ull,8);name(p,0x2402,"Example");auto report=target_plan_report(p,"fixture.exe");check(report.find("Delay example.dll!Example")!=std::string::npos&&report.find("Delay example.dll!#5")!=std::string::npos&&report.find("delay symbols: 2")!=std::string::npos&&report.find("guard flags: 0x100")!=std::string::npos&&report.find("console ABI unavailable: direct=1 delay=0")!=std::string::npos);
 auto reject=[&](size_t pos,uint64_t v,unsigned n){auto q=p;put(q,pos,v,n);try{target_plan_report(q,"fixture.exe");}catch(const std::runtime_error&){return;}throw std::runtime_error("malformed target accepted");};reject(0x2000,0,4);reject(0x2308,0x8000000000010005ull,8);reject(0x2010,0x3fff,4);reject(0x3000,149,4);reject(opt+116+13*8,32,4);std::cout<<"Target plan: direct categories, named and ordinal delay imports, load configuration and five malformed inputs passed.\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
