#pragma once
#include <windows.h>
#include <tlhelp32.h>
#include <map>
#include <string>
#include <iostream>
namespace revenge::gaming {
struct ModuleSnapshot {
    bool valid=false;
    std::map<std::wstring,std::wstring> modules; // path -> basename; paths never printed
};
inline std::string utf8(const std::wstring& value){
    if(value.empty())return {};
    int n=WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),nullptr,0,nullptr,nullptr);
    if(n<=0)return "[unavailable]";
    std::string s(static_cast<size_t>(n),'\0');
    if(!WideCharToMultiByte(CP_UTF8,0,value.data(),static_cast<int>(value.size()),s.data(),n,nullptr,nullptr))return "[unavailable]";
    return s;
}
inline ModuleSnapshot own_modules(){
    ModuleSnapshot result;
    HANDLE snapshot=INVALID_HANDLE_VALUE;
    for(unsigned i=0;i<8;++i){snapshot=CreateToolhelp32Snapshot(TH32CS_SNAPMODULE,GetCurrentProcessId());if(snapshot!=INVALID_HANDLE_VALUE||GetLastError()!=ERROR_BAD_LENGTH)break;}
    if(snapshot==INVALID_HANDLE_VALUE){std::cout<<"Module snapshot unavailable, Win32 error="<<GetLastError()<<'\n';return result;}
    struct Close {HANDLE h;~Close(){CloseHandle(h);}} guard{snapshot};
    MODULEENTRY32W entry{};entry.dwSize=sizeof(entry);
    if(!Module32FirstW(snapshot,&entry)){std::cout<<"Module enumeration unavailable, Win32 error="<<GetLastError()<<'\n';return result;}
    do{result.modules.emplace(entry.szExePath,entry.szModule);entry.dwSize=sizeof(entry);}while(Module32NextW(snapshot,&entry));
    result.valid=GetLastError()==ERROR_NO_MORE_FILES;
    return result;
}
inline void report_modules(const char* phase,const ModuleSnapshot& before,const ModuleSnapshot& after){
    if(!before.valid||!after.valid){std::cout<<"Module delta "<<phase<<": unavailable\n";return;}
    unsigned added=0,removed=0;
    for(const auto& [path,name]:after.modules)if(!before.modules.contains(path)){++added;std::cout<<"  added "<<utf8(name)<<'\n';}
    for(const auto& [path,name]:before.modules)if(!after.modules.contains(path)){++removed;std::cout<<"  removed "<<utf8(name)<<'\n';}
    std::cout<<"Module delta "<<phase<<": before="<<before.modules.size()<<" after="<<after.modules.size()<<" added="<<added<<" removed="<<removed<<'\n';
}
inline void report_public_exports(const ModuleSnapshot& snap){
    if(!snap.valid)return;
    std::cout<<"Loaded module inventory (basenames only): "<<snap.modules.size()<<'\n';
    for(const auto& [path,name]:snap.modules)std::cout<<"  module "<<utf8(name)<<'\n';
    constexpr const char* names[]={"XGameRuntimeInitialize","XGameRuntimeUninitialize","XSystemGetDeviceType","XTaskQueueCreate","XTaskQueueDuplicateHandle","XTaskQueueSubmitDelayedCallback","GameInputCreate"};
    unsigned matches=0,unavailable=0;
    for(const auto& [path,name]:snap.modules){
        HMODULE module=nullptr;
        // Acquire a reference only to an already loaded module; never load an arbitrary DLL.
        if(!GetModuleHandleExW(0,path.c_str(),&module)){++unavailable;continue;}
        // No calls through these pointers, no addresses in the log.
        for(auto symbol:names)if(GetProcAddress(module,symbol)){++matches;std::cout<<"  visible export "<<utf8(name)<<'!'<<symbol<<'\n';}
        FreeLibrary(module);
    }
    std::cout<<"Public export visibility: "<<matches<<" matches; "<<unavailable<<" modules no longer accessible. Not an API call trace or binding plan.\n";
}
}
