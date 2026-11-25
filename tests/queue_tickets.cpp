#include "revenge/queue_abi.h"
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>
namespace {
void check(bool value,const char* message){if(!value)throw std::runtime_error(message);}
struct Counts{std::atomic<unsigned> ran{0},canceled{0};};
void RQ_CALL callback(void* data,uint32_t canceled){auto& c=*static_cast<Counts*>(data);if(canceled)++c.canceled;else ++c.ran;}
struct Reenter{rq_context* q;rq_handle h;Counts* counts;bool ok=false;};
void RQ_CALL canceled_callback(void* data,uint32_t canceled){auto& r=*static_cast<Reenter*>(data);r.ok=canceled==1&&rq_submit(r.q,r.h,RQ_COMPLETION,0,callback,r.counts)==RQ_OK;}
}
int main(){try{
    rq_context* q=nullptr;rq_handle h=0,a=0,other=0;check(rq_open(RQ_VERSION,&q)==RQ_OK&&rq_create(q,&h)==RQ_OK&&rq_duplicate(q,h,&a)==RQ_OK&&rq_create(q,&other)==RQ_OK,"create");
    Counts c;rq_ticket t=0;Reenter r{q,a,&c};
    check(rq_submit_tracked(q,h,RQ_WORK,60000,canceled_callback,&r,&t)==RQ_OK&&t,"tracked");
    check(rq_cancel(q,other,t)==RQ_INVALID&&rq_cancel(q,a,t)==RQ_OK&&r.ok,"cancel via alias and reenter");
    check(rq_cancel(q,h,t)==RQ_INVALID&&rq_dispatch(q,h,RQ_COMPLETION,0)==RQ_OK&&c.ran==1,"exactly once");
    rq_snapshot snap{sizeof(rq_snapshot)};check(rq_query(q,a,&snap)==RQ_OK&&snap.active==0&&snap.pending_work==0,"empty snapshot");
    rq_snapshot malformed{};check(rq_query(q,a,&malformed)==RQ_INVALID&&malformed.size==0,"size check");
    for(unsigned i=0;i<128;++i){
        check(rq_submit_tracked(q,h,RQ_WORK,0,callback,&c,&t)==RQ_OK,"submit race");
        uint32_t dispatched=RQ_FAILURE;std::thread worker([&]{dispatched=rq_dispatch(q,a,RQ_WORK,0);});
        const auto canceled=rq_cancel(q,h,t);worker.join();
        check((dispatched==RQ_OK&&canceled==RQ_INVALID)||(dispatched==RQ_TIMEOUT&&canceled==RQ_OK),"dispatch/cancel race");
        check(c.ran+c.canceled==i+2,"exactly once race");
    }
    check(rq_submit_tracked(q,h,RQ_COMPLETION,60000,callback,&c,&t)==RQ_OK,"pending stop");
    check(rq_stop(q,a)==RQ_OK&&rq_cancel(q,h,t)==RQ_INVALID&&c.ran+c.canceled==130,"stop claims ticket");
    check(rq_submit_tracked(q,a,RQ_WORK,0,callback,&c,&t)==RQ_STATE&&t==0,"stopped submit");
    check(rq_close(q,a)==RQ_OK&&rq_close(q,h)==RQ_OK&&rq_close(q,other)==RQ_OK&&rq_destroy(q)==RQ_OK,"cleanup");
    std::cout<<"Tracked queue: 128 dispatch/cancel races, alias cancellation, reentrant completion, stale/foreign tickets and stop passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
