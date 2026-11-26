// Fixed, independently authored PE fixture. No user paths or arbitrary machine code.
#include "revenge/loader.hpp"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <vector>
#include <thread>
#include <exception>
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
constexpr unsigned process_attach=1,thread_attach=2,thread_detach=3,process_detach=0;
constexpr uint32_t import_entry=0x1040, import_slot=0x2040;
unsigned fixture_answer() noexcept {return 0x5256u;}
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
void put(std::vector<uint8_t>& b,size_t p,uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)b.at(p+i)=static_cast<uint8_t>(v>>(8*i));}
// Typed, fixed fixture binder. Validate complete plan before mutating any bytes.
using HostAnswer=unsigned(*)() noexcept;
void bind_fixture(LoadPlan& plan,bool lifecycle=false){
 check(plan.slots.size()==1&&plan.mapped.size()==extent,"unexpected fixture import count or extent");
 check(!plan.delay_imports&&!plan.load_config&&plan.image.tls==lifecycle,"unsupported fixture initialisation");
 const auto& slot=plan.slots.front();
 check(slot.dll=="rv_fixture_host.dll"&&slot.symbol=="rv_fixture_answer"&&slot.rva==import_slot&&!slot.by_ordinal,"unknown fixture import");
 const HostAnswer answer=&fixture_answer;
 put(plan.mapped,import_slot,reinterpret_cast<uintptr_t>(answer),8);
}
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
// A second, independently authored fixture adds one static TLS descriptor and
// a callback with no dependencies. Guest TLS storage is an explicit reference
// model, not a replacement for the host's static TLS vector.
std::vector<uint8_t> lifecycle_fixture(){
 auto b=fixture();constexpr size_t o=0x98;
 put(b,o+184,0x21a0,4);put(b,o+188,40,4);put(b,o+116+5*8,32,4);
 put(b,0x5a0,preferred+0x21d0,8);put(b,0x5a8,preferred+0x21d4,8);
 put(b,0x5b0,preferred+0x2010,8);put(b,0x5b8,preferred+0x21e0,8);
 put(b,0x5c0,12,4);put(b,0x5c4,0x500000,4);put(b,0x5d0,0x12345678,4);
 put(b,0x5e0,preferred+0x1080,8);
 put(b,0x60c,0x2000,4);put(b,0x610,20,4);
 unsigned i=0;for(unsigned off:{0x1a0u,0x1a8u,0x1b0u,0x1b8u,0x1e0u})put(b,0x614+2*i++,0xa000|off,2);
 // Callback records the host ABI reason and increments its invocation count.
 constexpr std::array<uint8_t,20> callback={0xf3,0x0f,0x1e,0xfa,
#ifdef _WIN32
  0x89,0xd0,
#else
  0x89,0xf0,
#endif
  0x89,0x05,0x88,0x0f,0,0,0xf0,0xff,0x05,0x85,0x0f,0,0,0xc3};
 std::copy(callback.begin(),callback.end(),b.begin()+0x280);return b;
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
 TlsTemplate prepare(bool lifecycle=false){auto bytes=lifecycle?lifecycle_fixture():fixture();auto plan=prepare_image(bytes,reinterpret_cast<uintptr_t>(p));
 check(plan.relocations==(lifecycle?6u:1u)&&plan.slots.size()==1&&plan.image.tls==lifecycle&&plan.image.unwind&&plan.unwind_entries==1&&!plan.delay_imports&&!plan.load_config,"unexpected fixed fixture metadata");
 check(init_metadata_report(plan).find("maximum local stack adjustment=40")!=std::string::npos,"fixed unwind metadata mismatch");
 check(page_layout(plan)==std::vector<PageSpan>{{0,4096,9},{4096,4096,13},{8192,4096,11},{12288,4096,0},{16384,4096,9}},"fixture page layout mismatch");
 check(std::all_of(plan.mapped.begin()+0x2200,plan.mapped.begin()+0x3000,[](uint8_t v){return v==0;}),"zero-fill mismatch");
 auto tls=tls_template(plan);
 if(lifecycle){check(tls.present&&tls.initial==std::vector<uint8_t>{0x78,0x56,0x34,0x12}&&tls.zero_fill==12&&tls.alignment==16&&tls.index_rva==0x2010&&plan.tls_callbacks==1,"unexpected TLS metadata");put(plan.mapped,tls.index_rva,1,4);}
 bind_fixture(plan,lifecycle);
 std::memcpy(p,plan.mapped.data(),extent);for(auto span:page_layout(plan))protect(span.rva,span.bytes,span.flags);
#ifdef _WIN32
 check(FlushInstructionCache(GetCurrentProcess(),p+0x1000,4096)!=0,"instruction cache flush failed");
#else
 __builtin___clear_cache(reinterpret_cast<char*>(p+0x1000),reinterpret_cast<char*>(p+0x2000));
#endif
 return tls;
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
void integrated_lifecycle(bool inject_failure=false){
 Mapping m;const auto t=m.prepare(true);
#ifdef _WIN32
 auto* table=reinterpret_cast<PRUNTIME_FUNCTION>(m.p+0x4080);
 auto base=static_cast<DWORD64>(reinterpret_cast<uintptr_t>(m.p));
 check(RtlAddFunctionTable(table,1,base)!=0,"lifecycle unwind registration");
 struct Registration{PRUNTIME_FUNCTION table;~Registration(){if(table)RtlDeleteFunctionTable(table);}} reg{table};
 DWORD64 found=0;check(RtlLookupFunctionEntry(base+import_entry+8,&found,nullptr)==table&&found==base,"lifecycle unwind lookup");
#endif
 auto read32=[&](size_t r){uint32_t v;std::memcpy(&v,m.p+r,4);return v;};
 uintptr_t target=0,terminator=1;std::memcpy(&target,m.p+0x21e0,8);std::memcpy(&terminator,m.p+0x21e8,8);check(target==reinterpret_cast<uintptr_t>(m.p+0x1080)&&terminator==0,"relocated fixed callback table");
 unsigned events=0;bool process_live=false,thread_live=false;
 auto callback=[&](unsigned reason){
  switch(reason){case process_attach:check(!process_live&&!events,"duplicate process attach");process_live=true;break;case thread_attach:check(process_live&&!thread_live,"invalid thread attach");thread_live=true;break;case thread_detach:check(process_live&&thread_live,"invalid thread detach");thread_live=false;break;case process_detach:check(process_live&&!thread_live,"invalid process detach");process_live=false;break;default:throw std::runtime_error("invalid callback reason");}
  using Callback=void(*)(void*,unsigned,void*);reinterpret_cast<Callback>(target)(m.p,reason,nullptr);++events;check(read32(0x2014)==reason&&read32(0x2018)==events,"callback order or count");
 };
 unsigned rejected=0;auto reject=[&](unsigned reason){auto before=events;try{callback(reason);}catch(const std::runtime_error&){check(events==before,"invalid callback mutated state");++rejected;return;}throw std::runtime_error("accepted invalid callback order");};
 reject(thread_attach);reject(process_detach);reject(99);
 alignas(16) std::array<uint8_t,16> parent{};auto initial=tls_instance(t);check(initial.size()==parent.size(),"initial TLS size");std::copy(initial.begin(),initial.end(),parent.begin());check(parent[0]==0x78&&parent[15]==0&&read32(0x2010)==1,"initial TLS reference");
 callback(process_attach);reject(process_attach);reject(thread_detach);check(m.invoke()==42&&m.invoke_import()==0x5256u,"lifecycle entry/import");
 unsigned failures=0;
 for(unsigned i=0;i<8;++i){std::exception_ptr failure;std::thread worker([&]{try{
   alignas(16) std::array<uint8_t,16> storage{};auto instance=tls_instance(t);std::copy(instance.begin(),instance.end(),storage.begin());
   check(storage[0]==0x78&&storage[15]==0,"worker TLS template");callback(thread_attach);try{storage[0]=uint8_t(i);storage[15]=0xff;
   check(m.invoke_import()==0x5256u&&storage[0]==i&&parent[0]==0x78&&parent[15]==0,"worker isolation/import");
   if(inject_failure&&i==3)throw std::logic_error("injected worker failure");
   }catch(...){callback(thread_detach);throw;}callback(thread_detach);
  }catch(...){failure=std::current_exception();}});worker.join();if(failure){try{std::rethrow_exception(failure);}catch(const std::logic_error& e){if(!inject_failure||i!=3||std::string(e.what())!="injected worker failure")throw;++failures;}}}
 check(failures==(inject_failure?1u:0u)&&m.invoke()==43&&parent[0]==0x78&&parent[15]==0,"lifecycle final state");
 callback(process_detach);reject(thread_attach);reject(process_detach);check(events==18&&rejected==7&&!process_live&&!thread_live,"lifecycle termination state");
#ifdef _WIN32
 check(RtlDeleteFunctionTable(table)!=0,"lifecycle unwind deregistration");reg.table=nullptr;
 found=0;check(RtlLookupFunctionEntry(base+import_entry+8,&found,nullptr)==nullptr,"lifecycle stale unwind registration");
#endif
 if(inject_failure)std::cout<<"Integrated fixed image lifecycle: injected worker failure cleaned up; 7 invalid callback transitions rejected.\n";
 else std::cout<<"Integrated fixed image lifecycle: 6 relocations, typed import, 8 worker reference TLS instances, 18 native callbacks, entry and cleanup passed. No guest static TLS installation or external target execution.\n";
}
#endif
}
int main(){try{
#if (defined(_WIN32) && (defined(_M_X64)||defined(__x86_64__))) || (defined(__linux__) && defined(__x86_64__))
 auto baseline=prepare_image(fixture(),preferred);unsigned rejected=0;
 for(unsigned variant=0;variant<9;++variant){auto bad=baseline;switch(variant){case 0:bad.slots[0].dll="wrong.dll";break;case 1:bad.slots[0].symbol="wrong";break;case 2:bad.slots[0].rva+=8;break;case 3:bad.slots.push_back(bad.slots[0]);break;case 4:bad.slots.clear();break;case 5:bad.delay_imports=true;break;case 6:bad.image.tls=true;break;case 7:bad.load_config=true;break;case 8:bad.slots[0].by_ordinal=true;break;}auto before=bad.mapped;bool failed=false;try{bind_fixture(bad);}catch(const std::runtime_error&){failed=true;}check(failed&&before==bad.mapped,"invalid fixture binding modified image");++rejected;}
 std::cout<<"Fixed typed binding: "<<rejected<<" malformed plans rejected without mutation.\n";
 integrated_lifecycle();integrated_lifecycle(true);
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
