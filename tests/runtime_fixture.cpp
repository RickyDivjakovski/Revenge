#include <windows.h>
#include <atomic>
#include <stdexcept>
#include <thread>
#include <iostream>
thread_local int value=7;
int main(){
    if(value!=7)return 11;
    value=19;std::atomic<bool> valid{false};
    std::thread t([&]{valid=(value==7);value=31;});t.join();
    if(!valid||value!=19)return 12;
    bool caught=false;try{throw std::runtime_error("fixture unwind");}catch(const std::runtime_error&){caught=true;}
    if(!caught)return 13;
    MEMORY_BASIC_INFORMATION mem{};if(!VirtualQuery(&value,&mem,sizeof(mem)))return 14;
    if((mem.Protect&0xff)!=PAGE_READWRITE)return 15;
    std::cout<<"Windows fixture: static TLS, thread separation, exception handling and writable non-executable TLS passed\n";
    return 0;
}
