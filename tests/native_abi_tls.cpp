#include "revenge/native_abi.h"
#include <atomic>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
void check(bool b){if(!b)throw std::runtime_error("TLS fixture mismatch");}
using Session=std::unique_ptr<rv_session,decltype(&rv_close)>;
Session open(){rv_session* p=nullptr;check(rv_open(RV_ABI_VERSION,&p)==RV_OK);return Session(p,rv_close);}
uint64_t call(rv_session* s,unsigned op,uint64_t h=0,uint64_t a=0,unsigned expected=RV_OK){rv_request r{sizeof(r),RV_ABI_VERSION,op,0,h,0,0,a,0};uint64_t v=99;auto status=rv_dispatch(s,&r,nullptr,0,&v);if(status!=expected)throw std::runtime_error("unexpected TLS status for "+std::to_string(op));if(status!=RV_OK)check(v==0);return v;}
int main(){try{auto one=open(),two=open();auto s=one.get();auto h=call(s,RV_TLS_CREATE),other=call(two.get(),RV_TLS_CREATE);check(call(s,RV_TLS_GET,h)==0);call(s,RV_TLS_SET,h,77);call(two.get(),RV_TLS_SET,other,88);call(two.get(),RV_TLS_GET,h,0,RV_INVALID);
 std::atomic<unsigned> done=0;std::vector<std::jthread> threads;for(unsigned k=0;k<8;++k)threads.emplace_back([&,k]{try{check(call(s,RV_TLS_GET,h)==0);for(unsigned i=1;i<=512;++i){uint64_t v=uint64_t(k+1)*10000+i;call(s,RV_TLS_SET,h,v);check(call(s,RV_TLS_GET,h)==v);}call(s,RV_TLS_DETACH);check(call(s,RV_TLS_GET,h)==0);++done;}catch(...){}});threads.clear();check(done==8&&call(s,RV_TLS_GET,h)==77&&call(two.get(),RV_TLS_GET,other)==88);
 std::jthread old([&]{call(s,RV_TLS_SET,h,123);});old.join();std::atomic<bool> fresh=false;std::jthread next([&]{fresh=call(s,RV_TLS_GET,h)==0;});next.join();check(fresh);
 call(s,RV_TLS_SET,h,0);check(call(s,RV_TLS_GET,h)==0);
 // One undetached value remains from old; fill the remaining 127 entries with distinct thread identities.
 for(unsigned i=0;i<127;++i){std::atomic<bool> ok=false;std::jthread t([&]{try{call(s,RV_TLS_SET,h,1);ok=true;}catch(...){}});t.join();check(ok);}
 std::atomic<bool> full=false;std::jthread excess([&]{try{call(s,RV_TLS_SET,h,1,RV_RANGE);full=call(s,RV_TLS_GET,h)==0;}catch(...){}});excess.join();check(full);
 std::vector<uint64_t> slots;for(unsigned i=1;i<64;++i)slots.push_back(call(s,RV_TLS_CREATE));check(call(s,RV_TLS_COUNT)==64);call(s,RV_TLS_CREATE,0,0,RV_RANGE);for(auto x:slots)call(s,RV_TLS_RELEASE,x);call(s,RV_TLS_RELEASE,h);call(s,RV_TLS_GET,h,0,RV_INVALID);check(call(s,RV_TLS_COUNT)==0);call(s,RV_TLS_CREATE);call(s,RV_STOP);call(s,RV_TLS_COUNT,0,0,RV_STATE);
 std::cout<<"Native TLS: 4096 isolated set/get pairs across 8 workers; detach, thread identity, slot and thread-value limits, stale/foreign handles and stop passed.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
