#include "revenge/native_abi.h"
#include <array>
#include <atomic>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
void check(bool b){if(!b)throw std::runtime_error("managed thread fixture mismatch");}
using Session=std::unique_ptr<rv_session,decltype(&rv_close)>;
Session open(){rv_session* p=nullptr;check(rv_open(RV_ABI_VERSION,&p)==RV_OK);return Session(p,rv_close);}
uint64_t call(rv_session* s,unsigned op,uint64_t h=0,uint64_t a=0,unsigned expected=RV_OK,void* b=nullptr,uint64_t n=0){rv_request r{sizeof(r),RV_ABI_VERSION,op,0,h,0,0,a,0};uint64_t v=99;auto status=rv_dispatch(s,&r,b,n,&v);if(status!=expected)throw std::runtime_error("unexpected thread status op "+std::to_string(op)+": "+rv_status_name(status));if(status!=RV_OK)check(v==0);return v;}
struct Context{uint64_t slot,gate;std::atomic<unsigned> verified{0};};
struct Arg{Context* ctx;unsigned id;};
uint32_t RV_CALL work(rv_session* s,uint64_t v){auto& arg=*reinterpret_cast<Arg*>(static_cast<uintptr_t>(v));auto& c=*arg.ctx;check(call(s,RV_TLS_GET,c.slot)==0);if(c.gate)check(call(s,RV_EVENT_WAIT,c.gate,10000)==1);for(unsigned j=1;j<=256;++j){auto n=uint64_t(arg.id)*1000+j;call(s,RV_TLS_SET,c.slot,n);check(call(s,RV_TLS_GET,c.slot)==n);++c.verified;}rv_request render{sizeof(render),RV_ABI_VERSION,RV_RENDER_VALIDATE,0,0,0,0,320,240};uint64_t result=0;check(rv_dispatch(s,&render,nullptr,0,&result)==RV_OK);return arg.id+100;}
uint32_t RV_CALL fail(rv_session*,uint64_t){throw std::runtime_error("fixture callback failure");}
uint64_t launch(rv_session* s,rv_thread_proc fn,uint64_t arg,unsigned expected=RV_OK){rv_thread_start start{fn,arg};return call(s,RV_THREAD_CREATE,0,0,expected,&start,sizeof(start));}
int main(){try{
 Context c{};Arg a{&c,0};std::array<Arg,16> args{};auto one=open(),two=open();auto s=one.get();c.slot=call(s,RV_TLS_CREATE);c.gate=call(s,RV_EVENT_CREATE);
 std::vector<uint64_t> handles;handles.reserve(16);
 for(unsigned i=0;i<16;++i){args[i]={&c,i};handles.push_back(launch(s,work,reinterpret_cast<uintptr_t>(&args[i])));}
 check(call(s,RV_THREAD_COUNT)==16);launch(s,work,0,RV_RANGE);launch(s,nullptr,0,RV_INVALID);
 check(call(s,RV_THREAD_POLL,handles[0])==RV_THREAD_RUNNING);check(call(s,RV_THREAD_WAIT,handles[0],0)==RV_THREAD_RUNNING);call(s,RV_THREAD_WAIT,handles[0],10001,RV_RANGE);call(s,RV_THREAD_RELEASE,handles[0],0,RV_STATE);call(s,RV_THREAD_EXIT,handles[0],0,RV_STATE);call(two.get(),RV_THREAD_POLL,handles[0],0,RV_INVALID);call(s,RV_STOP,0,0,RV_STATE);
 call(s,RV_EVENT_SET,c.gate);
 for(unsigned i=0;i<16;++i){auto h=handles[i];check(call(s,RV_THREAD_WAIT,h,10000)==RV_THREAD_COMPLETE);check(call(s,RV_THREAD_EXIT,h)==i+100);call(s,RV_THREAD_RELEASE,h);call(s,RV_THREAD_POLL,h,0,RV_INVALID);}
 check(c.verified==4096&&call(s,RV_TLS_GET,c.slot)==0);call(s,RV_EVENT_CLOSE,c.gate);c.gate=0;
 for(unsigned i=0;i<160;++i){a.id=i;auto h=launch(s,work,reinterpret_cast<uintptr_t>(&a));check(call(s,RV_THREAD_WAIT,h,10000)==RV_THREAD_COMPLETE);check(call(s,RV_THREAD_EXIT,h)==i+100);call(s,RV_THREAD_RELEASE,h);}
 check(c.verified==45056);auto h=launch(s,fail,0);check(call(s,RV_THREAD_WAIT,h,10000)==RV_THREAD_FAILED);call(s,RV_THREAD_EXIT,h,0,RV_FAILURE);call(s,RV_THREAD_RELEASE,h);check(call(s,RV_THREAD_COUNT)==0);call(s,RV_TLS_RELEASE,c.slot);call(s,RV_STOP);call(s,RV_THREAD_COUNT,0,0,RV_STATE);
 std::cout<<"Managed threads: 176 successful callbacks, 45056 TLS set/get pairs, automatic cleanup, exit codes, capacity, timeout, callback failure and lifecycle passed.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
