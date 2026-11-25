// Fixed, independently authored PE fixture. No user paths or arbitrary machine code.
#include "revenge/loader.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
#if defined(_WIN32)
#ifndef NOMINMAX
#define NOMINMAX
#endif
#include <windows.h>
#elif defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#endif
namespace {
using namespace revenge;
constexpr uint64_t preferred=0x140000000ull;
constexpr size_t extent=0x5000;
constexpr uint32_t import_entry=0x1040, import_slot=0x2040;
unsigned fixture_answer() noexcept {return 0x5256u;}
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void put(std::vector<uint8_t>& b,size_t p,uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)b.at(p+i)=static_cast<uint8_t>(v>>(8*i));}
std::vector<uint8_t> fixture(){
 std::vector<uint8_t> b(0xa00);constexpr size_t o=0x98,s=o+240;
 put(b,0,0x5a4d,2);put(b,0x3c,0x80,4);put(b,0x80,0x4550,4);put(b,0x84,0x8664,2);put(b,0x86,3,2);put(b,0x94,240,2);put(b,0x96,0x22,2);
 put(b,o,0x20b,2);put(b,o+16,0x1000,4);put(b,o+24,preferred,8);put(b,o+32,4096,4);put(b,o+36,512,4);put(b,o+56,extent,4);put(b,o+60,512,4);put(b,o+68,3,2);put(b,o+108,16,4);
 put(b,o+112+1*8,0x2100,4);put(b,o+116+1*8,40,4);
 put(b,o+112+3*8,0x4080,4);put(b,o+116+3*8,12,4);
 put(b,o+112+5*8,0x4000,4);put(b,o+116+5*8,12,4);
 auto sec=[&](unsigned i,const char* name,uint32_t rva,uint32_t size,uint32_t raw,uint32_t flags){auto p=s+i*40;for(size_t j=0;name[j];++j)b.at(p+j)=static_cast<uint8_t>(name[j]);put(b,p+8,size,4);put(b,p+12,rva,4);put(b,p+16,512,4);put(b,p+20,raw,4);put(b,p+36,flags,4);};
 sec(0,".text",0x1000,512,0x200,0x60000020);sec(1,".data",0x2000,4096,0x400,0xc0000040);sec(2,".reloc",0x4000,512,0x600,0x42000040);
 // Leaf function, both Win64 and System V x64: ENDBR64; mov rax, &counter;
 // mov ecx,[rax]; add ecx,1; mov [rax],ecx; mov eax,ecx; ret.
 constexpr std::array<uint8_t,26> code={0xf3,0x0f,0x1e,0xfa,0x48,0xb8,0,0,0,0,0,0,0,0,0x8b,0x08,0x83,0xc1,0x01,0x89,0x08,0x89,0xc8,0xc3,0xcc,0xcc};
 std::copy(code.begin(),code.end(),b.begin()+0x200);put(b,0x206,preferred+0x2000,8);put(b,0x400,41,4);
 // Second fixture entry calls precisely one host function with the no-argument x64 ABI.
 // 40-byte stack reservation preserves 16-byte alignment and Win64 shadow space.
 // RIP-relative IAT address: 0x2040 - 0x104e = 0xff2.
 constexpr std::array<uint8_t,19> import_code={0xf3,0x0f,0x1e,0xfa,0x48,0x83,0xec,0x28,0xff,0x15,0xf2,0x0f,0,0,0x48,0x83,0xc4,0x28,0xc3};
 std::copy(import_code.begin(),import_code.end(),b.begin()+0x240);
 // File-backed import descriptor, lookup and IAT, plus NUL-terminated names.
 put(b,0x500,0x2140,4);put(b,0x50c,0x2160,4);put(b,0x510,import_slot,4);
 put(b,0x540,0x2180,8);put(b,0x440,0x2180,8);
 constexpr char dll[]="rv_fixture_host.dll", symbol[]="rv_fixture_answer";
 std::copy_n(dll,sizeof(dll),b.begin()+0x560);std::copy_n(symbol,sizeof(symbol),b.begin()+0x582);
 put(b,0x600,0x1000,4);put(b,0x604,12,4);put(b,0x608,0xa006,2);
 // Fixed nonleaf wrapper's unwind record: 40-byte stack reservation.
 put(b,0x680,import_entry,4);put(b,0x684,import_entry+import_code.size(),4);put(b,0x688,0x4090,4);
 put(b,0x690,1,1);put(b,0x691,8,1);put(b,0x692,1,1);put(b,0x694,8,1);put(b,0x695,0x42,1);return b;
}
#if (defined(_WIN32) && (defined(_M_X64)||defined(__x86_64__))) || (defined(__linux__) && defined(__x86_64__))
struct Mapping {
 uint8_t* p=nullptr;void* reservation=nullptr;size_t reserved=extent;
 Mapping(){
#ifdef _WIN32
 SYSTEM_INFO si{};GetSystemInfo(&si);check(si.dwPageSize==4096&&si.dwAllocationGranularity==65536,"unsupported page geometry");
 reservation=VirtualAlloc(nullptr,extent,MEM_COMMIT|MEM_RESERVE,PAGE_READWRITE);check(reservation!=nullptr,"VirtualAlloc failed");p=static_cast<uint8_t*>(reservation);
#else
 check(sysconf(_SC_PAGESIZE)==4096,"unsupported page geometry");reserved=extent+65536;reservation=mmap(nullptr,reserved,PROT_READ|PROT_WRITE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0);check(reservation!=MAP_FAILED,"mmap failed");p=reinterpret_cast<uint8_t*>((reinterpret_cast<uintptr_t>(reservation)+65535)&~uintptr_t(65535));
#endif
 }
 Mapping(const Mapping&)=delete;Mapping& operator=(const Mapping&)=delete;
 ~Mapping(){
#ifdef _WIN32
 if(reservation)VirtualFree(reservation,0,MEM_RELEASE);
#else
 if(reservation&&reservation!=MAP_FAILED)munmap(reservation,reserved);
#endif
 }
 void protect(uint32_t rva,uint32_t size,uint8_t flags){check((flags&6)!=6,"W+X rejected");
#ifdef _WIN32
 DWORD next=(flags&4)?PAGE_EXECUTE_READ:(flags&2)?PAGE_READWRITE:(flags&1)?PAGE_READONLY:PAGE_NOACCESS,old{};
 check(VirtualProtect(p+rva,size,next,&old)!=0,"VirtualProtect failed");MEMORY_BASIC_INFORMATION info{};check(VirtualQuery(p+rva,&info,sizeof(info))==sizeof(info)&&info.Protect==next&&info.RegionSize>=size,"protection verification failed");
#else
 int next=((flags&1)?PROT_READ:0)|((flags&2)?PROT_WRITE:0)|((flags&4)?PROT_EXEC:0);check(mprotect(p+rva,size,next)==0,"mprotect failed");
#endif
 }
 void prepare(){auto bytes=fixture();auto plan=prepare_image(bytes,reinterpret_cast<uintptr_t>(p));
 check(plan.relocations==1&&plan.slots.size()==1&&!plan.image.tls&&plan.image.unwind&&plan.unwind_entries==1&&!plan.delay_imports&&!plan.load_config,"unexpected fixed fixture metadata");
 check(init_metadata_report(plan).find("maximum local stack adjustment=40")!=std::string::npos,"fixed unwind metadata mismatch");
 check(page_layout(plan)==std::vector<PageSpan>{{0,4096,9},{4096,4096,13},{8192,4096,11},{12288,4096,0},{16384,4096,9}},"fixture page layout mismatch");
 check(std::all_of(plan.mapped.begin()+0x2200,plan.mapped.begin()+0x3000,[](uint8_t v){return v==0;}),"zero-fill mismatch");
 const auto& slot=plan.slots.front();check(slot.dll=="rv_fixture_host.dll"&&slot.symbol=="rv_fixture_answer"&&slot.rva==import_slot,"fixed host import declaration mismatch");
 // No dynamic lookup, external module, arbitrary input or generic import binder.
 put(plan.mapped,import_slot,reinterpret_cast<uintptr_t>(&fixture_answer),8);
 std::memcpy(p,plan.mapped.data(),extent);for(auto span:page_layout(plan))protect(span.rva,span.bytes,span.flags);
#ifdef _WIN32
 check(FlushInstructionCache(GetCurrentProcess(),p+0x1000,4096)!=0,"instruction cache flush failed");
#else
 __builtin___clear_cache(reinterpret_cast<char*>(p+0x1000),reinterpret_cast<char*>(p+0x2000));
#endif
 }
 unsigned invoke(){using Entry=unsigned(*)();return reinterpret_cast<Entry>(p+0x1000)();}
 unsigned invoke_import(){using Entry=unsigned(*)();return reinterpret_cast<Entry>(p+import_entry)();}
#ifdef _WIN32
 void verify_unwind(){
  auto* table=reinterpret_cast<PRUNTIME_FUNCTION>(p+0x4080);auto base=static_cast<DWORD64>(reinterpret_cast<uintptr_t>(p));
  check(RtlAddFunctionTable(table,1,base)!=0,"fixed unwind registration failed");
  struct Registration {PRUNTIME_FUNCTION table;~Registration(){if(table)RtlDeleteFunctionTable(table);}} registration{table};
  DWORD64 found_base=0;auto* found=RtlLookupFunctionEntry(base+import_entry+8,&found_base,nullptr);
  check(found==table&&found_base==base,"fixed unwind lookup mismatch");
  alignas(16) std::array<DWORD64,6> stack{};const auto caller=static_cast<DWORD64>(reinterpret_cast<uintptr_t>(&fixture_answer));stack[5]=caller;
  CONTEXT context{};context.ContextFlags=CONTEXT_CONTROL|CONTEXT_INTEGER;context.Rip=base+import_entry+8;context.Rsp=static_cast<DWORD64>(reinterpret_cast<uintptr_t>(stack.data()));
  const auto initial_rsp=context.Rsp;PVOID handler_data=nullptr;DWORD64 establisher=0;
  auto handler=RtlVirtualUnwind(UNW_FLAG_NHANDLER,base,context.Rip,found,&context,&handler_data,&establisher,nullptr);
  check(!handler&&context.Rip==caller&&context.Rsp==initial_rsp+48,"fixed virtual unwind mismatch");
  check(invoke_import()==0x5256u,"registered wrapper execution mismatch");
  check(RtlDeleteFunctionTable(table)!=0,"fixed unwind deregistration failed");registration.table=nullptr;
 }
#endif
};
#endif
}
int main(){try{
#if (defined(_WIN32) && (defined(_M_X64)||defined(__x86_64__))) || (defined(__linux__) && defined(__x86_64__))
 Mapping a,b;a.prepare();b.prepare();check(a.p!=b.p,"mapping isolation");check(a.invoke()==42&&a.invoke()==43&&b.invoke()==42&&a.invoke()==44&&b.invoke()==43,"relocated entry results");check(a.invoke_import()==0x5256u&&b.invoke_import()==0x5256u,"fixed host import call results");
 #ifdef _WIN32
 a.verify_unwind();b.verify_unwind();std::cout<<"Fixed native unwind: two registered tables, lookup, virtual stack recovery, wrapper calls and deregistration passed.\n";
#else
 std::cout<<"Fixed native unwind: static 40-byte allocation metadata verified; Windows runtime registration not tested.\n";
#endif
 std::cout<<"Fixed native PE fixture: two independent mappings; DIR64 relocation; R/RX/RW/no-access pages; zero fill; five native entry calls and two fixed host import calls returned expected values; no external target executed.\n";return 0;
#else
 std::cout<<"Fixed native PE fixture: skipped on unsupported host.\n";return 77;
#endif
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
