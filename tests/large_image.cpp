// Independently generated data-only PE fixture. No external binaries or execution.
#include "revenge/loader.hpp"
#include <algorithm>
#include <cstring>
#if defined(_WIN32)
#define NOMINMAX
#include <windows.h>
#elif defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif
#include <iostream>
#include <stdexcept>
using namespace revenge;
namespace {
constexpr uint32_t pages=1200, data_rva=0x2000, data_bytes=pages*4096, rel_rva=data_rva+data_bytes, rel_bytes=pages*1032, extent=192*1024*1024;
constexpr uint64_t base=0x140000000ull;
void check(bool v,const char* s){if(!v)throw std::runtime_error(s);}
void put(std::vector<uint8_t>& b,size_t p,uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)b.at(p+i)=uint8_t(v>>(8*i));}
uint64_t get(const std::vector<uint8_t>& b,size_t p){uint64_t v=0;for(unsigned i=0;i<8;++i)v|=uint64_t(b.at(p+i))<<(8*i);return v;}
std::vector<uint8_t> fixture(){
 constexpr size_t o=0x98,s=o+240,raw_data=1024,raw_rel=raw_data+data_bytes;
 std::vector<uint8_t> b(raw_rel+((rel_bytes+511)/512)*512);
 put(b,0,0x5a4d,2);put(b,0x3c,0x80,4);put(b,0x80,0x4550,4);put(b,0x84,0x8664,2);put(b,0x86,3,2);put(b,0x94,240,2);put(b,0x96,0x22,2);
 put(b,o,0x20b,2);put(b,o+16,0x1000,4);put(b,o+24,base,8);put(b,o+32,4096,4);put(b,o+36,512,4);put(b,o+56,extent,4);put(b,o+60,512,4);put(b,o+68,3,2);put(b,o+108,16,4);put(b,o+152,rel_rva,4);put(b,o+156,rel_bytes,4);
 auto sec=[&](unsigned i,const char* name,uint32_t rva,uint32_t size,uint32_t raw,uint32_t rawsize,uint32_t flags){auto p=s+i*40;for(size_t j=0;name[j];++j)b.at(p+j)=uint8_t(name[j]);put(b,p+8,size,4);put(b,p+12,rva,4);put(b,p+16,rawsize,4);put(b,p+20,raw,4);put(b,p+36,flags,4);};
 sec(0,".text",0x1000,512,512,512,0x60000020);sec(1,".data",data_rva,data_bytes,raw_data,data_bytes,0xc0000040);sec(2,".reloc",rel_rva,rel_bytes,raw_rel,((rel_bytes+511)/512)*512,0x42000040);
 for(uint32_t i=0;i<pages;++i){auto r=raw_rel+i*1032;put(b,r,data_rva+i*4096,4);put(b,r+4,1032,4);for(uint32_t j=0;j<512;++j){put(b,r+8+j*2,0xa000+j*8,2);put(b,raw_data+i*4096+j*8,base+data_rva+i*4096+j*8,8);}}
 return b;
}
}
int main(){try{const auto b=fixture();for(auto target:{base+0x40000000ull,base-0x40000000ull}){auto p=prepare_image(b,target);check(p.mapped.size()==extent&&p.relocations==pages*512,"large image extent/count");for(uint32_t i=0;i<pages*512;++i){auto r=data_rva+i*8;check(get(p.mapped,r)==target+r,"relocation mismatch");}auto spans=page_layout(p);check(spans.back().flags==0&&spans.back().rva<extent,"large gap classification");check(std::all_of(p.mapped.begin()+spans.back().rva,p.mapped.end(),[](uint8_t x){return x==0;}),"large gap zero fill");}
 // Map only independently generated data, deliberately never executable.
#if defined(_WIN32) || defined(__linux__)
 {struct Mapping{void* ptr{};Mapping(){
#ifdef _WIN32
 ptr=VirtualAlloc(nullptr,extent,MEM_RESERVE|MEM_COMMIT,PAGE_READWRITE);
#else
 ptr=mmap(nullptr,extent,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);if(ptr==MAP_FAILED)ptr=nullptr;
#endif
 check(ptr!=nullptr,"native allocation failed");}~Mapping(){
#ifdef _WIN32
 VirtualFree(ptr,0,MEM_RELEASE);
#else
 munmap(ptr,extent);
#endif
 }} m;
 auto address=reinterpret_cast<uintptr_t>(m.ptr);auto plan=prepare_image(b,address);auto* dest=static_cast<uint8_t*>(m.ptr);std::memcpy(dest,plan.mapped.data(),extent);
 for(uint32_t i=0;i<pages*512;++i){uint64_t value;std::memcpy(&value,dest+data_rva+i*8,8);check(value==address+data_rva+i*8,"native mapping relocation mismatch");}
 for(auto span:page_layout(plan)){
#ifdef _WIN32
 DWORD protection=(span.flags&2)?PAGE_READWRITE:(span.flags&1)?PAGE_READONLY:PAGE_NOACCESS,old{};check(VirtualProtect(dest+span.rva,span.bytes,protection,&old)!=0,"native protection failed");MEMORY_BASIC_INFORMATION info{};check(VirtualQuery(dest+span.rva,&info,sizeof(info))==sizeof(info)&&info.Protect==protection&&info.RegionSize>=span.bytes,"native protection query failed");
#else
 int protection=((span.flags&1)?PROT_READ:0)|((span.flags&2)?PROT_WRITE:0);check(mprotect(dest+span.rva,span.bytes,protection)==0,"native protection failed");
#endif
 }
 uint64_t first;std::memcpy(&first,dest+data_rva,8);check(first==address+data_rva,"protected data read failed");std::cout<<"Native data-only mapping: 192 MiB, 614400 relocated pointers, non-executable protections applied.\n";
 }
#endif
 auto invalid=b;put(invalid,1024+data_bytes+10,0xa000,2);bool rejected=false;try{(void)prepare_image(invalid,base+0x10000);}catch(const std::runtime_error&){rejected=true;}check(rejected,"duplicate relocation accepted");
 std::cout<<"Large image model: 192 MiB, 614400 DIR64 relocations per rebase, upward/downward rebasing, zero fill, gap permissions and duplicate rejection passed. Offline model verified; separate data-only OS mapping where supported. No target execution.\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
