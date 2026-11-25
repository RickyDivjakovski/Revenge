#include <windows.h>
#include <XGameRuntime.h>
#include <XGameRuntimeInit.h>
#include <XSystem.h>
#include <XTaskQueue.h>
#include "queue_workload.hpp"
#include "module_snapshot.hpp"
#include "probe_trace.hpp"
#include <iostream>
#include <atomic>
#include <cstdlib>
#include <chrono>
#include <thread>
using namespace revenge::gaming;
namespace {
struct Runtime { bool active=false; ~Runtime(){if(active)XGameRuntimeUninitialize();} };
struct Queue {
    XTaskQueueHandle h=nullptr;
    HRESULT last=S_OK;
    static XTaskQueuePort convert(Port p){return p==Port::work?XTaskQueuePort::Work:XTaskQueuePort::Completion;}
    bool submit(Port p,void* context,Callback cb){last=XTaskQueueSubmitCallback(h,convert(p),context,cb);return SUCCEEDED(last);}
    bool dispatch(Port p){return XTaskQueueDispatch(h,convert(p),0);}
    static void CALLBACK terminated(void* context){static_cast<std::atomic_bool*>(context)->store(true);}
    bool close(){
        std::atomic_bool done{false};
        auto hr=XTaskQueueTerminate(h,false,&done,terminated);
        if(FAILED(hr)){std::cerr<<"XTaskQueueTerminate HRESULT=0x"<<std::hex<<static_cast<unsigned long>(hr)<<'\n';return false;}
        auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(5);
        while(!done.load()&&std::chrono::steady_clock::now()<deadline){
            XTaskQueueDispatch(h,XTaskQueuePort::Work,0);XTaskQueueDispatch(h,XTaskQueuePort::Completion,0);
            if(!done.load())std::this_thread::sleep_for(std::chrono::milliseconds(1));
        }
        if(!done.load())return false;
        XTaskQueueCloseHandle(h);h=nullptr;return true;
    }
};
bool delayed_reference(){
    Queue q; QueueWorkload::State state, pending;
    QueueWorkload::Item delayed{&state,7}, immediate{&state,11}, canceled{&pending,13};
    if(FAILED(XTaskQueueCreate(XTaskQueueDispatchMode::Manual,XTaskQueueDispatchMode::Manual,&q.h)))return false;
    XTaskQueueHandle alias=nullptr;
    bool ok=SUCCEEDED(XTaskQueueDuplicateHandle(q.h,&alias));
    if(ok){XTaskQueueCloseHandle(q.h);q.h=alias;}
    if(ok)ok=SUCCEEDED(XTaskQueueSubmitDelayedCallback(q.h,XTaskQueuePort::Work,20,&delayed,QueueWorkload::callback));
    if(ok)ok=q.submit(Port::completion,&immediate,QueueWorkload::callback)&&q.dispatch(Port::completion)&&state.count==1&&state.checksum==11;
    if(ok)ok=XTaskQueueDispatch(q.h,XTaskQueuePort::Work,2000)&&state.count==2&&state.checksum==18&&!state.canceled;
    if(ok)ok=SUCCEEDED(XTaskQueueSubmitDelayedCallback(q.h,XTaskQueuePort::Work,60000,&canceled,QueueWorkload::callback));
    if(!q.close()){std::cerr<<"Delayed queue cleanup failed\n"<<std::flush;std::_Exit(2);}
    ok=ok&&pending.canceled==1&&!pending.count;
    if(ok)std::cout<<"REVENGE-DELAY-V1 work=1 completion=1 sum=18 canceled=1 alias=1\nGDK delayed reference: alias survives original close; work and completion dispatched; delayed pending cancellation passed.\n";
    return ok;
}
}
int main(){
    auto before=own_modules();
    Runtime runtime;auto hr=XGameRuntimeInitialize();
    std::cout<<"XGameRuntimeInitialize HRESULT=0x"<<std::hex<<static_cast<unsigned long>(hr)<<std::dec<<std::endl;
    if(FAILED(hr)){std::cerr<<"Windows Gaming Runtime initialisation failed; check Gaming Services and installed SDK/runtime.\n";return 1;}
    runtime.active=true;
    auto initialized=own_modules();report_modules("initialise",before,initialized);
    auto device=XSystemGetDeviceType();std::cout<<"XSystemGetDeviceType="<<static_cast<unsigned>(device)<<'\n';
    if(device!=XSystemDeviceType::Pc){std::cerr<<"Unexpected device type for PC reference\n";return 1;}
    QueueWorkload workload;Queue queue;
    hr=XTaskQueueCreate(XTaskQueueDispatchMode::Manual,XTaskQueueDispatchMode::Manual,&queue.h);
    if(FAILED(hr)){std::cerr<<"XTaskQueueCreate HRESULT=0x"<<std::hex<<static_cast<unsigned long>(hr)<<'\n';return 1;}
    bool ok=workload.run(queue);
    QueueWorkload::State pending; QueueWorkload::Item work{&pending,1}, completion{&pending,2};
    if(ok) ok=queue.submit(Port::work,&work,QueueWorkload::callback)&&queue.submit(Port::completion,&completion,QueueWorkload::callback);
    if(!queue.close()){std::cerr<<"Task queue cleanup failed; exiting reference process\n"<<std::flush;std::_Exit(2);}
    if(!ok||pending.canceled!=2||pending.count){std::cerr<<"GDK manual queue mismatch, last submit HRESULT=0x"<<std::hex<<static_cast<unsigned long>(queue.last)<<'\n';return 1;}
    workload.print(std::cout);
    std::cout<<"GDK pending callbacks canceled on termination: "<<pending.canceled<<'\n';
    if(!delayed_reference()){std::cerr<<"GDK delayed/duplicate queue mismatch\n";return 1;}
    auto exercised=own_modules();report_modules("workload",initialized,exercised);report_public_exports(exercised);
    const auto trace=revenge_arena::scripted_trace();
    char line[160]{};
    const auto n=revenge_probe::format_trace(line,sizeof(line),trace);
    if(trace!=revenge_arena::expected_trace||n<=0||static_cast<size_t>(n)>=sizeof(line))return 1;
    std::cout<<line<<'\n';
    XGameRuntimeUninitialize();runtime.active=false;
    report_modules("shutdown",exercised,own_modules());
    std::cout<<"Public Windows GDK: runtime lifecycle, PC device type, manual task queue and cleanup passed. OS-loaded PC application, not Xbox execution.\n";
    return 0;
}
