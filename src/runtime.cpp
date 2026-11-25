#include "revenge/runtime.hpp"
#include "revenge/launch.hpp"
#include <algorithm>
#include <array>
#include <cctype>
#include <iostream>
namespace revenge {
DependencyKind classify_dependency(std::string_view name) {
    if(name.empty() || name.size()>255) return DependencyKind::invalid;
    std::string n;
    for(unsigned char c:name) {
        if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='-'||c=='_'||c=='.')) return DependencyKind::invalid;
        n+=static_cast<char>(std::tolower(c));
    }
    if(!n.ends_with(".dll") || n.find("..")!=std::string::npos || n.front()=='.')return DependencyKind::invalid;
    constexpr std::array console={"d3d12_x.dll","d3d12_xs.dll","xg_x.dll","xg_xs.dll","xmem.dll","xboxbvhbuilder.dll","xcrdapi.dll","chad.dll","xapu.dll","acphal.dll","bink2scarlett.dll","amd_fidelityfx_loader_xbox.dll"};
    if(std::find(console.begin(),console.end(),n)!=console.end())return DependencyKind::console;
    if(n.starts_with("api-ms-")||n.starts_with("ext-ms-"))return DependencyKind::api_contract;
    constexpr std::array sys={"kernel32.dll","kernelbase.dll","ntdll.dll","user32.dll","gdi32.dll","advapi32.dll","bcrypt.dll","ws2_32.dll","ole32.dll","oleaut32.dll","shell32.dll","shlwapi.dll","winhttp.dll","mfplat.dll","mfreadwrite.dll","d3d12.dll","dxgi.dll","ucrtbase.dll","msvcp140.dll","msvcp140_atomic_wait.dll","vcruntime140.dll","vcruntime140_1.dll","msvcrt.dll","combase.dll","gameinput.dll","xaudio2_9.dll"};
    if(std::find(sys.begin(),sys.end(),n)!=sys.end())return DependencyKind::system_candidate;
    return DependencyKind::other;
}
std::string_view dependency_label(DependencyKind k){switch(k){case DependencyKind::console:return "console ABI unavailable";case DependencyKind::api_contract:return "Windows API-set contract";case DependencyKind::system_candidate:return "Windows system candidate";case DependencyKind::other:return "external dependency (unverified)";default:return "invalid DLL name";}}
bool windows_candidate(const Image& x,const std::filesystem::path& path){
    if(x.machine!=0x8664 || !x.entry || (x.subsystem!=2&&x.subsystem!=3) || has_console_markers(x,path))return false;
    for(auto& d:x.imports){auto k=classify_dependency(d.dll);if(k==DependencyKind::console||k==DependencyKind::invalid)return false;}return true;
}
#ifndef _WIN32
int audit_runtime(const std::filesystem::path& path,std::ostream& out){auto x=inspect_file(path);out<<"Offline direct-import classification (no DLLs loaded)\n";for(auto& d:x.imports)out<<d.dll<<": "<<dependency_label(classify_dependency(d.dll))<<" ("<<d.symbols.size()<<")\n";out<<"Windows execution candidate: "<<windows_candidate(x,path)<<"\nHost export checks require Windows.\n";return 0;}
int run_windows(const std::filesystem::path&,unsigned,bool,std::ostream& out){out<<"Windows process runner is unavailable on this platform.\n";return 2;}
#endif
}
