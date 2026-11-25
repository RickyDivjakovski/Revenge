// Independent trusted client; only the public C ABI is linked.
#include "revenge/native_abi.h"
#include <array>
#include <atomic>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <thread>
#include <vector>
namespace {
void check(bool b,const char* what){if(!b)throw std::runtime_error(what);}
using Session=std::unique_ptr<rv_session,decltype(&rv_close)>;
Session open(){rv_session* s=nullptr;check(rv_open(RV_ABI_VERSION,&s)==RV_OK&&s,"open");return Session(s,rv_close);}
uint64_t call(rv_session* s,uint32_t op,uint64_t h=0,uint64_t a=0,uint64_t b=0,uint64_t n=0,void* buf=nullptr,uint64_t cap=0,uint32_t expected=RV_OK){rv_request r{sizeof(rv_request),RV_ABI_VERSION,op,0,h,0,n,a,b};uint64_t out=~uint64_t{};auto status=rv_dispatch(s,&r,buf,cap,&out);if(status!=expected)throw std::runtime_error("op "+std::to_string(op)+": "+rv_status_name(status)+" expected "+rv_status_name(expected));if(status!=RV_OK)check(out==0,"error result");return out;}
}
int main(){try{auto session=open(),foreign=open();auto s=session.get();auto m=call(s,RV_MUTEX_CREATE),sem=call(s,RV_SEM_CREATE,0,0,3);check(m!=sem&&call(s,RV_SYNC_COUNT)==2,"sync handles");
 check(call(s,RV_MUTEX_LOCK,m)==1,"lock");call(s,RV_MUTEX_LOCK,m,0,0,0,nullptr,0,RV_STATE);call(s,RV_MUTEX_CLOSE,m,0,0,0,nullptr,0,RV_STATE);
 std::atomic<bool> good=false;std::jthread probe([&]{try{call(s,RV_MUTEX_UNLOCK,m,0,0,0,nullptr,0,RV_STATE);check(call(s,RV_MUTEX_LOCK,m,5)==0,"locked mutex timeout");good=true;}catch(...){}});probe.join();check(good,"foreign thread ownership");call(s,RV_MUTEX_UNLOCK,m);
 auto mem=call(s,RV_ALLOC,0,0,0,sizeof(uint64_t));uint64_t zero=0;call(s,RV_WRITE,mem,0,0,sizeof(zero),&zero,sizeof(zero));std::atomic<unsigned> workers=0;std::vector<std::jthread> threads;
 for(unsigned k=0;k<4;++k)threads.emplace_back([&]{try{for(unsigned i=0;i<1024;++i){check(call(s,RV_MUTEX_LOCK,m,3000)==1,"worker lock");uint64_t v=0;call(s,RV_READ,mem,0,0,sizeof(v),&v,sizeof(v));++v;call(s,RV_WRITE,mem,0,0,sizeof(v),&v,sizeof(v));call(s,RV_MUTEX_UNLOCK,m);}++workers;}catch(...){}});
 threads.clear();uint64_t total=0;call(s,RV_READ,mem,0,0,sizeof(total),&total,sizeof(total));check(workers==4&&total==4096,"protected shared memory increments");
 check(call(s,RV_SEM_WAIT,sem)==0,"empty semaphore");check(call(s,RV_SEM_RELEASE,sem,2)==0,"previous semaphore count");call(s,RV_SEM_RELEASE,sem,2,0,0,nullptr,0,RV_RANGE);check(call(s,RV_SEM_WAIT,sem)==1&&call(s,RV_SEM_WAIT,sem)==1&&call(s,RV_SEM_WAIT,sem)==0,"semaphore counts");
 good=false;std::jthread waiter([&]{try{good=call(s,RV_SEM_WAIT,sem,3000)==1;}catch(...){}});call(s,RV_SEM_RELEASE,sem,1);waiter.join();check(good,"semaphore cross-thread acquire");
 call(s,RV_SEM_CREATE,0,4,3,0,nullptr,0,RV_RANGE);call(s,RV_SEM_WAIT,sem,10001,0,0,nullptr,0,RV_RANGE);call(s,RV_MUTEX_LOCK,sem,0,0,0,nullptr,0,RV_INVALID);call(foreign.get(),RV_SEM_WAIT,sem,0,0,0,nullptr,0,RV_INVALID);
 std::array<uint8_t,12> record{'s','a','v','e',0,1,2,3,4,5,6,255};auto original=record;
 check(call(s,RV_STORE_PUT,0,4,0,8,record.data(),record.size())==8,"store put");check(call(s,RV_STORE_SIZE,0,4,0,0,record.data(),4)==8,"store size");call(s,RV_STORE_GET,0,4,0,0,record.data(),4,RV_RANGE);check(record==original,"short get modified data");
 check(call(s,RV_STORE_GET,0,4,0,0,record.data(),record.size())==8&&!std::memcmp(record.data(),original.data()+4,8),"binary storage roundtrip");record=original;call(foreign.get(),RV_STORE_SIZE,0,4,0,0,record.data(),4,RV_RANGE);
 record[0]='/';call(s,RV_STORE_PUT,0,4,0,8,record.data(),record.size(),RV_INVALID);record=original;call(s,RV_STORE_ERASE,0,4,0,0,record.data(),4);call(s,RV_STORE_SIZE,0,4,0,0,record.data(),4,RV_RANGE);
 call(s,RV_STORE_PUT,0,4,0,0,record.data(),4);check(call(s,RV_STORE_SIZE,0,4,0,0,record.data(),4)==0,"empty value");call(s,RV_STORE_ERASE,0,4,0,0,record.data(),4);
 call(s,RV_RENDER_VALIDATE,0,1920,1080);call(s,RV_FREE,mem);call(s,RV_MUTEX_CLOSE,m);call(s,RV_SEM_CLOSE,sem);check(call(s,RV_SYNC_COUNT)==0,"sync cleanup");call(s,RV_MUTEX_LOCK,m,0,0,0,nullptr,0,RV_INVALID);
 auto pending=call(s,RV_SEM_CREATE,0,0,1);good=false;std::jthread stopping([&]{try{call(s,RV_SEM_WAIT,pending,5000,0,0,nullptr,0,RV_STATE);good=true;}catch(...){}});call(s,RV_STOP);stopping.join();check(good,"stop cancels wait");rv_stats stats{};call(s,RV_STATS,0,0,0,0,&stats,sizeof(stats));check(!stats.running&&!stats.memory_handles&&!stats.storage_entries&&!stats.committed_bytes,"stop cleanup");call(s,RV_SYNC_COUNT,0,0,0,0,nullptr,0,RV_STATE);
 std::cout<<"System ABI: 4096 mutex-protected memory updates across 4 workers; semaphore counts, ownership, timeouts and stop cancellation passed.\nSession storage: binary roundtrip, short buffer, invalid key, empty value, isolation and cleanup passed.\nNative graphics validation dispatched; no Xbox ABI or game execution.\n";return 0;
 }catch(const std::exception& e){std::cerr<<"System ABI fixture: "<<e.what()<<'\n';return 1;}}
