// Independently built Windows runtime lifecycle reference. OS loader only.
#include <windows.h>
#include <atomic>
#include <stdexcept>
#include <thread>
#include <iostream>
#include <array>

namespace {
volatile LONG process_attach=0, thread_attach=0, thread_detach=0;
void NTAPI lifecycle(PVOID, DWORD reason, PVOID) noexcept {
    // Loader-lock callback: no allocation, I/O, locks or calls into the CRT.
    if(reason==DLL_PROCESS_ATTACH) InterlockedIncrement(&process_attach);
    else if(reason==DLL_THREAD_ATTACH) InterlockedIncrement(&thread_attach);
    else if(reason==DLL_THREAD_DETACH) InterlockedIncrement(&thread_detach);
}
LONG read_count(volatile LONG* p){return InterlockedCompareExchange(p,0,0);}
}
#if defined(_MSC_VER) && defined(_M_X64)
#pragma section(".CRT$XLB", long, read)
extern "C" {
__declspec(allocate(".CRT$XLB")) PIMAGE_TLS_CALLBACK revenge_fixture_tls_callback = lifecycle;
}
#pragma comment(linker,"/INCLUDE:_tls_used")
#pragma comment(linker,"/INCLUDE:revenge_fixture_tls_callback")
#endif

thread_local int value=7;
thread_local std::array<unsigned,16> scratch{23,47};
int main(){
    if(value!=7||scratch[0]!=23||scratch[1]!=47||scratch[15]!=0)return 11;
#if defined(_MSC_VER) && defined(_M_X64)
    if(read_count(&process_attach)!=1)return 16;
#endif
    value=19;scratch[0]=99;
    const auto attach_before=read_count(&thread_attach), detach_before=read_count(&thread_detach);
    std::atomic<unsigned> valid{0};
    std::array<std::thread,8> workers;
    for(unsigned k=0;k<workers.size();++k)workers[k]=std::thread([&,k]{
        if(value!=7||scratch[0]!=23||scratch[1]!=47||scratch[15]!=0)return;
        value=31+int(k);scratch[0]=k+200;
        bool caught=false;try{throw std::runtime_error("worker unwind");}catch(const std::runtime_error&){caught=true;}
        MEMORY_BASIC_INFORMATION mem{};
        if(caught&&value==31+int(k)&&scratch[0]==k+200&&VirtualQuery(&value,&mem,sizeof(mem))&&(mem.Protect&0xff)==PAGE_READWRITE)++valid;
    });
    for(auto& t:workers)t.join();
    if(valid!=8||value!=19||scratch[0]!=99)return 12;
#if defined(_MSC_VER) && defined(_M_X64)
    if(read_count(&thread_attach)-attach_before<8||read_count(&thread_detach)-detach_before<8)return 17;
#endif
    bool caught=false;try{throw std::runtime_error("fixture unwind");}catch(const std::runtime_error&){caught=true;}
    if(!caught)return 13;
    MEMORY_BASIC_INFORMATION mem{};if(!VirtualQuery(&value,&mem,sizeof(mem)))return 14;
    if((mem.Protect&0xff)!=PAGE_READWRITE)return 15;
    std::cout<<"Windows fixture: static TLS, 8 worker lifecycles, thread separation, exception handling and writable non-executable TLS passed\n";
#if defined(_MSC_VER) && defined(_M_X64)
    std::cout<<"Windows loader TLS callbacks: process attach and at least 8 thread attach/detach events verified.\n";
#else
    std::cout<<"Windows loader TLS callbacks: MSVC x64 reference unavailable.\n";
#endif
    return 0;
}
