#include "revenge/runtime.hpp"
#include "revenge/system.hpp"
#include <windows.h>
#include <charconv>
#include <chrono>
#include <iomanip>
#include <iostream>
#include <stdexcept>
#include <system_error>
namespace revenge { namespace {
struct Handle { HANDLE h{}; explicit Handle(HANDLE value=nullptr):h(value){} ~Handle(){if(h&&h!=INVALID_HANDLE_VALUE)CloseHandle(h);} Handle(const Handle&)=delete; Handle& operator=(const Handle&)=delete; };
struct Module {HMODULE h{}; ~Module(){if(h)FreeLibrary(h);}};
[[noreturn]] void fail(const char* what){throw std::system_error(static_cast<int>(GetLastError()),std::system_category(),what);}
void ensure(BOOL ok,const char* what){if(!ok)fail(what);}
struct ChildGuard {HANDLE process;bool finished=false;~ChildGuard(){if(!finished){TerminateProcess(process,1);WaitForSingleObject(process,2000);}}};
struct ErrorMode {UINT old=SetErrorMode(SEM_FAILCRITICALERRORS|SEM_NOGPFAULTERRORBOX|SEM_NOOPENFILEERRORBOX);~ErrorMode(){SetErrorMode(old);}};
FARPROC symbol(HMODULE h,const std::string& s){if(!s.starts_with('#'))return GetProcAddress(h,s.c_str());unsigned n=0;auto [end,err]=std::from_chars(s.data()+1,s.data()+s.size(),n);if(err!=std::errc{}||end!=s.data()+s.size()||n==0||n>65535)return nullptr;return GetProcAddress(h,MAKEINTRESOURCEA(n));}
}
int audit_runtime(const std::filesystem::path& path,std::ostream& out){
    auto x=inspect_file(path);size_t resolved=0,missing=0,unchecked=0;
    out<<"Direct import audit: system-directory candidates and allowlisted API-set contracts only; no game DLLs loaded.\n";
    for(auto& d:x.imports){auto kind=classify_dependency(d.dll);out<<d.dll<<": "<<dependency_label(kind)<<" ("<<d.symbols.size()<<")\n";
        // Resolve only allowlisted contracts through the OS loader; never load title DLLs.
        if(kind!=DependencyKind::system_candidate && !(kind==DependencyKind::api_contract && auditable_contract(d.dll))){unchecked+=d.symbols.size();continue;}
        std::wstring name(d.dll.begin(),d.dll.end());Module m{LoadLibraryExW(name.c_str(),nullptr,LOAD_LIBRARY_SEARCH_SYSTEM32)};
        if(!m.h){out<<"  system load unavailable ("<<GetLastError()<<")\n";unchecked+=d.symbols.size();continue;}
        for(auto& s:d.symbols){if(symbol(m.h,s))++resolved;else{++missing;out<<"  missing export: "<<s<<"\n";}}
    }
    out<<"Export names resolved: "<<resolved<<"; missing: "<<missing<<"; not checked: "<<unchecked<<"\nWindows execution candidate: "<<windows_candidate(x,path)<<"\nExport presence is not ABI compatibility. Delay, dynamic and transitive imports not audited.\n";return 0;
}
int run_windows(const std::filesystem::path& input,unsigned seconds,bool diagnostics,std::ostream& out){
    if(seconds==0||seconds>86400)throw std::runtime_error("timeout must be 1..86400 seconds");
    auto path=std::filesystem::canonical(input);auto image=inspect_file(path);
    if(!windows_candidate(image,path)){out<<"Execution refused: known console markers or unsupported PE. Xbox ABI is not implemented.\n";return 2;}
    Handle job(CreateJobObjectW(nullptr,nullptr));if(!job.h)fail("CreateJobObject");
    JOBOBJECT_EXTENDED_LIMIT_INFORMATION limits{};limits.BasicLimitInformation.LimitFlags=JOB_OBJECT_LIMIT_KILL_ON_JOB_CLOSE;
    ensure(SetInformationJobObject(job.h,JobObjectExtendedLimitInformation,&limits,sizeof(limits)),"SetInformationJobObject");
    STARTUPINFOW startup{};startup.cb=sizeof(startup);PROCESS_INFORMATION pi{};
    std::wstring cmd=L"\""+path.wstring()+L"\"";DWORD flags=CREATE_SUSPENDED|(diagnostics?DEBUG_ONLY_THIS_PROCESS:0);
    {ErrorMode mode;ensure(CreateProcessW(path.c_str(),cmd.data(),nullptr,nullptr,FALSE,flags,nullptr,path.parent_path().c_str(),&startup,&pi),"CreateProcessW");}
    Handle process(pi.hProcess),thread(pi.hThread);ChildGuard guard{process.h};
    ensure(AssignProcessToJobObject(job.h,process.h),"AssignProcessToJobObject");
    if(ResumeThread(thread.h)==DWORD(-1))fail("ResumeThread");
    out<<"Native Windows process started, PID "<<pi.dwProcessId<<"; timeout "<<seconds<<"s. Not Xbox emulation.\n";
    const auto end=std::chrono::steady_clock::now()+std::chrono::seconds(seconds);
    DWORD exit_code=0;bool done=false,initial_breakpoint=false;
    if(!diagnostics){DWORD w=WaitForSingleObject(process.h,seconds*1000u);if(w==WAIT_FAILED)fail("WaitForSingleObject");done=w==WAIT_OBJECT_0;}
    else while(std::chrono::steady_clock::now()<end){
        DEBUG_EVENT event{};if(!WaitForDebugEvent(&event,50)){if(GetLastError()==ERROR_SEM_TIMEOUT)continue;fail("WaitForDebugEvent");}
        DWORD status=DBG_CONTINUE;
        switch(event.dwDebugEventCode){
        case CREATE_PROCESS_DEBUG_EVENT:if(event.u.CreateProcessInfo.hFile)CloseHandle(event.u.CreateProcessInfo.hFile);break;
        case LOAD_DLL_DEBUG_EVENT:if(event.u.LoadDll.hFile)CloseHandle(event.u.LoadDll.hFile);break;
        case EXCEPTION_DEBUG_EVENT:{const auto& e=event.u.Exception;const auto code=e.ExceptionRecord.ExceptionCode;
            if(!initial_breakpoint&&code==EXCEPTION_BREAKPOINT&&e.dwFirstChance){initial_breakpoint=true;break;}
            status=DBG_EXCEPTION_NOT_HANDLED;
            if(!e.dwFirstChance)out<<"Unhandled exception: 0x"<<std::hex<<code<<" at "<<e.ExceptionRecord.ExceptionAddress<<std::dec<<"\n";
            break;}
        case EXIT_PROCESS_DEBUG_EVENT:exit_code=event.u.ExitProcess.dwExitCode;done=true;break;
        default:break;
        }
        ensure(ContinueDebugEvent(event.dwProcessId,event.dwThreadId,status),"ContinueDebugEvent");if(done)break;
    }
    if(!done){ensure(TerminateJobObject(job.h,124),"TerminateJobObject");out<<"Time limit exceeded; job terminated.\n";return 124;}
    if(WaitForSingleObject(process.h,2000)!=WAIT_OBJECT_0)throw std::runtime_error("process did not terminate after exit notification");
    ensure(GetExitCodeProcess(process.h,&exit_code),"GetExitCodeProcess");guard.finished=true;
    out<<"Process exit code: 0x"<<std::hex<<exit_code<<std::dec<<"\n";
    // Use small, portable status codes while reporting the full NTSTATUS above.
    return exit_code==0?0:3;
}
}
