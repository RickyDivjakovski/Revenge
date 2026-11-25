// Standalone native client: no C++ runtime headers or direct revenge_core linkage.
#include "revenge/native_abi.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <atomic>
static_assert(sizeof(rv_request)==56 && sizeof(rv_stats)==48);
namespace {
void check(bool good,const char* text){if(!good)throw std::runtime_error(text);}
using Session=std::unique_ptr<rv_session,decltype(&rv_close)>;
rv_request req(uint32_t op,uint64_t h=0,uint64_t offset=0,uint64_t count=0,uint64_t a=0,uint64_t b=0){return {sizeof(rv_request),RV_ABI_VERSION,op,0,h,offset,count,a,b};}
uint64_t call(rv_session* s,rv_request r,void* data=nullptr,uint64_t n=0,uint32_t expect=RV_OK){uint64_t out=~uint64_t{};auto status=rv_dispatch(s,&r,data,n,&out);if(status!=expect)throw std::runtime_error(std::string("operation ")+std::to_string(r.op)+": "+rv_status_name(status)+" expected "+rv_status_name(expect));if(status!=RV_OK)check(out==0,"failed request result");return out;}
Session open(){rv_session* s=nullptr;check(rv_open(RV_ABI_VERSION,&s)==RV_OK&&s,"open");return Session(s,rv_close);}
}
int main(){try{
 check(rv_abi_version()==RV_ABI_VERSION,"version negotiation");rv_session* bad=nullptr;check(rv_open(2,&bad)==RV_VERSION&&!bad,"wrong version");auto s=open();auto other=open();
 auto t0=call(s.get(),req(RV_CLOCK));auto h=call(s.get(),req(RV_ALLOC,0,0,16384));auto e=call(s.get(),req(RV_EVENT_CREATE));
 check(h&&e&&h!=e,"handles");std::array<uint32_t,4096> input{},output{};for(uint32_t i=0;i<input.size();++i)input[i]=i;
 check(call(s.get(),req(RV_WRITE,h,0,sizeof(input)),input.data(),sizeof(input))==sizeof(input),"write length");
 check(call(s.get(),req(RV_EVENT_WAIT,e),nullptr,0)==0,"initial event state");
 std::atomic<bool> worker_ok=false;
 std::jthread worker([&]{try{std::array<uint32_t,4096> values{};call(s.get(),req(RV_READ,h,0,sizeof(values)),values.data(),sizeof(values));for(auto& x:values)x=3*x+1;call(s.get(),req(RV_WRITE,h,0,sizeof(values)),values.data(),sizeof(values));worker_ok=true;call(s.get(),req(RV_EVENT_SET,e));}catch(...){worker_ok=false;}});
 check(call(s.get(),req(RV_EVENT_WAIT,e,0,0,3000))==1,"event wait");worker.join();check(worker_ok,"worker");
 call(s.get(),req(RV_READ,h,0,sizeof(output)),output.data(),sizeof(output));for(uint32_t i=0;i<output.size();++i)check(output[i]==3*i+1,"worker result");
 call(s.get(),req(RV_EVENT_RESET,e));check(call(s.get(),req(RV_EVENT_WAIT,e))==0,"reset event");
 auto r=req(RV_CLOCK);r.version=2;call(s.get(),r,nullptr,0,RV_VERSION);r=req(RV_CLOCK);r.size=0;call(s.get(),r,nullptr,0,RV_INVALID);r=req(RV_CLOCK);r.reserved=1;call(s.get(),r,nullptr,0,RV_INVALID);
 call(s.get(),req(9999),nullptr,0,RV_UNSUPPORTED);call(s.get(),req(RV_READ,h,16383,4),output.data(),4,RV_RANGE);call(s.get(),req(RV_READ,h,0,8),output.data(),4,RV_RANGE);call(s.get(),req(RV_READ,h,UINT64_MAX,4),output.data(),4,RV_RANGE);call(other.get(),req(RV_FREE,h),nullptr,0,RV_INVALID);call(s.get(),req(RV_FREE,e),nullptr,0,RV_INVALID);
 call(s.get(),req(RV_EVENT_WAIT,e,0,0,UINT64_MAX),nullptr,0,RV_RANGE);call(s.get(),req(RV_RENDER_VALIDATE,0,0,0,UINT64_MAX,1080),nullptr,0,RV_RANGE);
 call(s.get(),req(RV_RENDER_VALIDATE,0,0,0,1920,1080));check(call(s.get(),req(RV_CLOCK))>=t0,"monotonic clock");
 call(s.get(),req(RV_FREE,h));call(s.get(),req(RV_FREE,h),nullptr,0,RV_INVALID);call(s.get(),req(RV_EVENT_CLOSE,e));call(s.get(),req(RV_EVENT_SET,e),nullptr,0,RV_INVALID);
 // Leave live allocations for automatic stop cleanup, and ensure two sessions are independent.
 auto leak=call(s.get(),req(RV_ALLOC,0,0,4096));(void)leak;call(s.get(),req(RV_EVENT_CREATE));auto other_h=call(other.get(),req(RV_ALLOC,0,0,64));
 rv_stats stats{};call(s.get(),req(RV_STATS),&stats,sizeof(stats));check(stats.running==1&&stats.memory_handles==1&&stats.events==1&&stats.committed_bytes>=4096,"live stats");
 call(s.get(),req(RV_STOP));call(s.get(),req(RV_STATS),&stats,sizeof(stats));check(!stats.running&&!stats.memory_handles&&!stats.events&&!stats.storage_entries&&!stats.storage_bytes&&!stats.committed_bytes,"shutdown cleanup");call(s.get(),req(RV_CLOCK),nullptr,0,RV_STATE);call(s.get(),req(RV_STOP),nullptr,0,RV_STATE);
 call(other.get(),req(RV_FREE,other_h));call(other.get(),req(RV_STOP));
 std::cout<<"Native ABI v1: separate executable linked to Revenge shared library; 4096 worker results verified.\nVersion, bounds, stale/foreign handles, event sync, unsupported operations and lifecycle checks passed.\nCPU graphics command validation dispatched; zero tracked resources after stop.\nIndependent native ABI; no Xbox import binding, GPU presentation or gameplay.\n";return 0;
 }catch(const std::exception& e){std::cerr<<"Native ABI fixture: "<<e.what()<<'\n';return 1;}}
