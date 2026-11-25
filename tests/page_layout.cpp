#include "revenge/loader.hpp"
#include <iostream>
#include <stdexcept>
using namespace revenge;
void require(bool b){if(!b)throw std::runtime_error("page layout fixture mismatch");}
void put(std::vector<uint8_t>& b,size_t p,uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)b.at(p+i)=uint8_t(v>>(8*i));}
int main(){try{
 std::vector<uint8_t> b(2048);const size_t o=0x98,s=o+240;put(b,0,0x5a4d,2);put(b,0x3c,0x80,4);put(b,0x80,0x4550,4);put(b,0x84,0x8664,2);put(b,0x86,3,2);put(b,0x94,240,2);put(b,o,0x20b,2);put(b,o+16,0x1000,4);put(b,o+24,0x140000000,8);put(b,o+56,0x5000,4);put(b,o+60,512,4);put(b,o+108,16,4);
 auto sec=[&](unsigned i,uint32_t rva,uint32_t virt,uint32_t raw,uint32_t flags){auto p=s+i*40;put(b,p,'a'+i,1);put(b,p+8,virt,4);put(b,p+12,rva,4);put(b,p+16,512,4);put(b,p+20,raw,4);put(b,p+36,flags,4);};
 sec(0,0x1000,512,512,0x60000000);sec(1,0x1800,512,1024,0xc0000000);sec(2,0x3000,0x1800,1536,0x40000000);
 auto p=prepare_image(b);auto spans=page_layout(p);require(spans==std::vector<PageSpan>{{0,4096,9},{4096,4096,15},{8192,4096,0},{12288,8192,9}});
 p.slots={{"fixture.dll","one",0x1800},{"fixture.dll","two",0x3000}};auto report=layout_report(p);require(report.find("Writable+executable bytes: 4096; unmapped gap bytes: 4096")!=std::string::npos&&report.find("executable page: 1; writable page: 1")!=std::string::npos&&report.find("IAT slots outside writable spans: 1 / 2")!=std::string::npos);
 require(p.mapped[0x47ff]==0);p.image.sections[0].rva=0x5000;bool rejected=false;try{page_layout(p);}catch(...){rejected=true;}require(rejected);
 std::cout<<"Offline page layout: header, shared W+X page, gap, zero-fill extent, entry and IAT diagnostics passed.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
