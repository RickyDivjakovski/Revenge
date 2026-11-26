#include "revenge/queue_abi.h"
#include <atomic>
#include <iostream>
#include <latch>
#include <stdexcept>
#include <thread>
namespace {
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
struct Data {std::atomic<unsigned> work{0},done{0},state{0};};
uint32_t RQ_CALL work(void* p,uint64_t* r){++static_cast<Data*>(p)->work;*r=77;return RQ_OK;}
void RQ_CALL done(void* p,rq_operation,const rq_operation_snapshot* s){auto& d=*static_cast<Data*>(p);d.state=s->state;++d.done;}
struct Block {std::latch entered{1},leave{1},notified{1},finish{1};};
uint32_t RQ_CALL blocking(void* p,uint64_t* r){auto& b=*static_cast<Block*>(p);b.entered.count_down();b.leave.wait();*r=91;return RQ_OK;}
void RQ_CALL notification(void* p,rq_operation,const rq_operation_snapshot*){auto& b=*static_cast<Block*>(p);b.notified.count_down();b.finish.wait();}
uint32_t RQ_CALL throwing(void*,uint64_t*){throw std::runtime_error("work");}
void RQ_CALL throwing_done(void*,rq_operation,const rq_operation_snapshot*){throw std::runtime_error("completion");}
}
int main(){try{rq_context* c=nullptr;rq_handle h=0;check(rq_open(RQ_VERSION,&c)==RQ_OK&&rq_create(c,&h)==RQ_OK,"open");
 for(unsigned i=0;i<256;++i){Data d;rq_operation id=0;check(rq_operation_submit(c,h,0,work,done,&d,&id)==RQ_OK,"submit");std::latch go{1};uint32_t dispatched=RQ_FAILURE,canceled=RQ_FAILURE;std::thread a([&]{go.wait();dispatched=rq_dispatch(c,h,RQ_WORK,0);}),b([&]{go.wait();canceled=rq_operation_cancel(c,h,id);});go.count_down();a.join();b.join();rq_operation_snapshot s{sizeof(s)};check(rq_operation_query(c,h,id,&s)==RQ_OK&&s.notification_done&&d.done==1&&d.state==s.state,"exactly one completion");if(s.state==RQ_OP_SUCCEEDED)check(dispatched==RQ_OK&&canceled==RQ_STATE&&d.work==1&&s.result==77,"dispatch wins");else check(s.state==RQ_OP_CANCELED&&dispatched==RQ_TIMEOUT&&canceled==RQ_OK&&!d.work&&!s.result,"cancel wins");check(rq_operation_release(c,h,id)==RQ_OK,"release");}
 Block block;rq_operation id=0;check(rq_operation_submit(c,h,0,blocking,notification,&block,&id)==RQ_OK,"blocking submit");uint32_t dispatch=RQ_FAILURE;std::thread t([&]{dispatch=rq_dispatch(c,h,RQ_WORK,0);});block.entered.wait();rq_operation_snapshot s{sizeof(s)};bool running=rq_operation_query(c,h,id,&s)==RQ_OK&&s.state==RQ_OP_RUNNING&&rq_operation_cancel(c,h,id)==RQ_STATE&&rq_operation_wait(c,h,id,1,&s)==RQ_TIMEOUT;
 block.leave.count_down();block.notified.wait();bool terminal=rq_operation_wait(c,h,id,100,&s)==RQ_OK&&s.state==RQ_OP_SUCCEEDED&&s.result==91&&!s.notification_done&&rq_operation_release(c,h,id)==RQ_STATE;block.finish.count_down();t.join();check(running&&terminal&&dispatch==RQ_OK&&rq_operation_release(c,h,id)==RQ_OK,"running/notification lifetime");
 check(rq_operation_submit(c,h,0,throwing,throwing_done,nullptr,&id)==RQ_OK&&rq_dispatch(c,h,RQ_WORK,0)==RQ_FAILURE,"exception containment");check(rq_operation_query(c,h,id,&s)==RQ_OK&&s.state==RQ_OP_FAILED&&s.work_status==RQ_FAILURE&&s.notification_status==RQ_FAILURE&&s.notification_done,"exception result");check(rq_operation_release(c,h,id)==RQ_OK&&rq_stop(c,h)==RQ_OK&&rq_close(c,h)==RQ_OK&&rq_destroy(c)==RQ_OK,"cleanup");std::cout<<"Retained operations: 256 cancel/dispatch races, running wait, notification lifetime and exception containment passed.\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
