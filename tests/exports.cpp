#include "revenge/exports.hpp"
#include <iostream>
#include <stdexcept>
using namespace revenge;
void check(bool ok){if(!ok)throw std::runtime_error("export fixture mismatch");}
void put(std::vector<uint8_t>& b,size_t p,uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)b.at(p+i)=uint8_t(v>>(8*i));}
void str(std::vector<uint8_t>& b,size_t p,const std::string& s){for(char c:s)b.at(p++)=c;b.at(p)=0;}
int main(){try{
 std::vector<uint8_t> b(2048);size_t o=0x98,s=o+240;put(b,0,0x5a4d,2);put(b,0x3c,0x80,4);put(b,0x80,0x4550,4);put(b,0x84,0x8664,2);put(b,0x86,2,2);put(b,0x94,240,2);put(b,o,0x20b,2);put(b,o+16,0x1000,4);put(b,o+24,0x140000000,8);put(b,o+56,0x3000,4);put(b,o+60,512,4);put(b,o+108,16,4);put(b,o+112,0x2000,4);put(b,o+116,0x200,4);
 auto sec=[&](size_t p,uint32_t rva,uint32_t raw,uint32_t flags){put(b,p+8,512,4);put(b,p+12,rva,4);put(b,p+16,512,4);put(b,p+20,raw,4);put(b,p+36,flags,4);};sec(s,0x1000,512,0x60000000);sec(s+40,0x2000,1024,0x40000000);
 auto w=[&](size_t p,uint64_t v,unsigned n=4){put(b,1024+p,v,n);};w(12,0x2080);w(16,7);w(20,4);w(24,3);w(28,0x2040);w(32,0x2050);w(36,0x2060);w(0x40,0x1000);w(0x44,0x2090);w(0x48,0);w(0x4c,0x1010);w(0x50,0x20a0);w(0x54,0x20b0);w(0x58,0x20c0);w(0x60,0,2);w(0x62,1,2);w(0x64,0,2);str(b,0x480,"fixture.dll");str(b,0x490,"other.target");str(b,0x4a0,"Alpha");str(b,0x4b0,"Forward");str(b,0x4c0,"#10");
 auto t=inspect_exports(b);check(t.module=="fixture.dll"&&t.symbols.size()==4&&t.symbols[0].names.size()==2&&t.symbols[1].forwarder=="other.target"&&!t.symbols[2].rva&&t.symbols[3].ordinal==10);
 Image im{};im.imports={{"FIXTURE.DLL",{"Alpha","#10","#10","Forward","#9","alpha"},{false,true,false,false,true,false}},{"absent.dll",{"foo"},{false}}};auto report=dependency_report(im,{{"fixture.dll",t}});check(report.find("Direct exports present: 3; forwarded: 1; missing symbols: 2; module not supplied: 1; bound: 0")!=std::string::npos);
 unsigned rejected=0;auto bad=[&](size_t p,uint64_t v,unsigned n){auto x=b;put(x,p,v,n);try{inspect_exports(x);}catch(...){++rejected;}};bad(0x460,4,2);bad(0x460,2,2);bad(0x440,0x3000,4);bad(0x450,0x2200,4);bad(0x454,0x20a0,4);bad(0x444,0x21ff,4);bad(0x414,65537,4);check(rejected==7);
 bool dup=false;try{dependency_report(im,{{"fixture.dll",t},{"FIXTURE.DLL",t}});}catch(...){dup=true;}check(dup);
 std::cout<<"Offline exports: aliases, named/ordinal imports, holes, forwarders, case rules, 7 malformed tables and duplicate modules passed.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
