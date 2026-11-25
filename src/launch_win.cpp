#include "revenge/launch.hpp"
#include "revenge/loader.hpp"
#include "revenge/runtime.hpp"
#include <sstream>
#include <fstream>
#include <windows.h>
#include <shobjidl.h>
#include <wrl/client.h>
#include <optional>
#include <algorithm>
#include <stdexcept>
#include <system_error>
#include <string>
using Microsoft::WRL::ComPtr;
namespace revenge { namespace {
struct Apartment {
    HRESULT hr=CoInitializeEx(nullptr,COINIT_APARTMENTTHREADED|COINIT_DISABLE_OLE1DDE);
    ~Apartment(){if(SUCCEEDED(hr)) CoUninitialize();}
};
void check(HRESULT hr){if(FAILED(hr)) throw std::runtime_error("Windows selection dialog failed (HRESULT " + std::to_string(static_cast<unsigned long>(hr)) + ").");}
std::wstring widen(const std::string& text){return std::wstring(text.begin(),text.end());}
std::optional<std::filesystem::path> select_path(bool folder,const std::filesystem::path& initial={}) {
    ComPtr<IFileOpenDialog> dlg; check(CoCreateInstance(CLSID_FileOpenDialog,nullptr,CLSCTX_INPROC_SERVER,IID_PPV_ARGS(dlg.GetAddressOf())));
    DWORD options{};check(dlg->GetOptions(&options));check(dlg->SetOptions(options|FOS_FORCEFILESYSTEM|FOS_PATHMUSTEXIST|FOS_FILEMUSTEXIST|(folder?FOS_PICKFOLDERS:0)));
    check(dlg->SetTitle(folder?L"Revenge - Select game directory":L"Revenge - Select game executable"));
    if(!folder){const COMDLG_FILTERSPEC filter{L"Game executables (*.exe)",L"*.exe"};check(dlg->SetFileTypes(1,&filter));}
    if(!initial.empty()){ComPtr<IShellItem> item;if(SUCCEEDED(SHCreateItemFromParsingName(initial.c_str(),nullptr,IID_PPV_ARGS(item.GetAddressOf())))) check(dlg->SetFolder(item.Get()));}
    const HRESULT hr=dlg->Show(nullptr);if(hr==HRESULT_FROM_WIN32(ERROR_CANCELLED)) return std::nullopt;check(hr);
    ComPtr<IShellItem> item;check(dlg->GetResult(item.GetAddressOf()));PWSTR raw=nullptr;check(item->GetDisplayName(SIGDN_FILESYSPATH,&raw));
    std::filesystem::path result(raw);CoTaskMemFree(raw);return result;
}
void message(const std::wstring& text,UINT flags=MB_ICONINFORMATION){MessageBoxW(nullptr,text.c_str(),L"Revenge",MB_OK|flags|MB_SETFOREGROUND);}
}
int launch_dialog(){
    // Preserve terminal visibility when launched from an existing shell.
    DWORD processes[2]{};if(GetConsoleProcessList(processes,2)==1) if(auto window=GetConsoleWindow()) ShowWindow(window,SW_HIDE);
    try {
        Apartment apartment;check(apartment.hr);
        auto directory=select_path(true);if(!directory)return 0;
        auto candidates=game_candidates(*directory);
        if(candidates.empty()){message(L"No .exe files were found in this folder. Select the folder directly containing the game executable.",MB_ICONWARNING);return 1;}
        auto exe=candidates.size()==1?std::optional<std::filesystem::path>(candidates.front()):select_path(false,*directory);
        if(!exe)return 0;
        if(std::find(candidates.begin(),candidates.end(),*exe)==candidates.end()) { // use filesystem equivalence for Windows casing
            bool found=false;for(const auto& p:candidates){std::error_code ec;if(std::filesystem::equivalent(p,*exe,ec)&&!ec){found=true;break;}}
            if(!found){message(L"Choose an executable directly inside the selected game folder.",MB_ICONWARNING);return 1;}
        }
        auto image=inspect_file(*exe);
        if(has_console_markers(image,*exe)){
            auto plan=prepare_file(*exe);
            auto report=std::filesystem::temp_directory_path()/L"Revenge-loader-report.txt";
            std::ofstream out(report,std::ios::trunc);
            if(!out)throw std::runtime_error("Unable to create diagnostic report");
            out<<preparation_report(plan,*exe);out.close();
            if(!out)throw std::runtime_error("Unable to write diagnostic report");
            message(L"Xbox/Scarlett executable prepared (offline):\n"+exe->wstring()+L"\n\nUnbound imports: "+std::to_wstring(plan.slots.size())+L"\nReport: "+report.wstring()+L"\n\nGame execution is not supported yet. No game code was executed.",MB_ICONWARNING);return 2;
        }
        if(image.subsystem!=2 && image.subsystem!=3){message(L"This executable is not a recognised Windows GUI or console application.",MB_ICONWARNING);return 2;}
        const auto prompt=L"No known Xbox markers were found. This does not prove Windows compatibility.\n\nLaunch this trusted Windows executable in a supervised process (not Xbox emulation)?\nThe process will have a one-hour limit.\n"+exe->wstring();
        if(MessageBoxW(nullptr,prompt.c_str(),L"Revenge - Confirm launch",MB_YESNO|MB_ICONQUESTION|MB_DEFBUTTON2|MB_SETFOREGROUND)!=IDYES)return 0;
        std::ostringstream result;
        const int status=run_windows(*exe,3600,false,result);
        message(widen(result.str()),status?MB_ICONWARNING:MB_ICONINFORMATION);return status;
    }catch(const std::exception& e){message(widen(e.what()),MB_ICONERROR);return 1;}
}
}
