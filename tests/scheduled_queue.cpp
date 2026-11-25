#include "revenge/scheduled_queue.hpp"
#include <iostream>
#include <stdexcept>
#include <array>
using namespace revenge::gaming;
static void check(bool ok){if(!ok)throw std::runtime_error("scheduled queue invariant failed");}
struct Trace {std::array<unsigned,8> order{}; unsigned n=0,cancel=0;};
struct Item {Trace* t;unsigned value;};
static void cb(void* p,bool canceled){auto& i=*static_cast<Item*>(p);if(canceled)++i.t->cancel;else {check(i.t->n<i.t->order.size());i.t->order[i.t->n++]=i.value;}}
int main(){try{
  for(unsigned iter=0;iter<256;++iter){ScheduledQueues q; Trace t;Item a{&t,1},b{&t,2},c{&t,3},d{&t,4};
    auto h=q.create(),alias=q.duplicate(h),other=q.create();check(h&&alias&&other&&h!=alias&&q.handles()==3);
    check(q.submit(h,Port::work,&a,cb,20)&&q.submit(alias,Port::work,&b,cb,10)&&q.submit(alias,Port::work,&c,cb,10)&&q.submit(h,Port::completion,&d,cb,5));
    check(q.close(h)&&!q.close(h)&&!q.submit(h,Port::work,&a,cb)&&!q.close(alias));
    check(!q.dispatch(alias,Port::work)&&q.advance(other,100)&&!q.dispatch(other,Port::work)&&!q.dispatch(alias,Port::completion));
    check(q.advance(alias,5)&&q.dispatch(alias,Port::completion)&&!q.dispatch(alias,Port::work));
    check(q.advance(alias,5)&&q.dispatch(alias,Port::work)&&q.dispatch(alias,Port::work)&&!q.dispatch(alias,Port::work));
    check(q.advance(alias,10)&&q.dispatch(alias,Port::work)&&t.n==4&&t.order==std::array<unsigned,8>{4,2,3,1});
    check(q.submit(alias,Port::work,&a,cb,99999)&&q.submit(alias,Port::completion,&b,cb)&&q.stop(alias)&&q.stop(alias)&&t.cancel==2);
    check(!q.advance(alias,1)&&!q.dispatch(alias,Port::work)&&!q.submit(alias,Port::work,&a,cb)&&q.close(alias)&&q.close(other)&&q.handles()==0);
  }
  ScheduledQueues q;auto h=q.create();Trace t;Item a{&t,1};
  check(!q.duplicate(0)&&!q.stop(0)&&!q.submit(h,static_cast<Port>(9),&a,cb)&&!q.submit(h,Port::work,&a,nullptr));
  check(q.advance(h,UINT64_MAX)&&!q.advance(h,1)&&!q.submit(h,Port::work,&a,cb,1));
  for(unsigned i=0;i<128;++i)check(q.submit(h,Port::work,&a,cb));
  check(!q.submit(h,Port::work,&a,cb));
  for(unsigned i=1;i<64;++i)check(q.duplicate(h)!=0);
  check(!q.duplicate(h)&&!q.create());check(q.stop(h)&&t.cancel==128);
  // Same observable workload as the real PC GDK reference, with virtual time.
  ScheduledQueues comparison;Trace observed;Item work{&observed,7},completion{&observed,11},pending{&observed,13};
  auto original=comparison.create(),alias=comparison.duplicate(original);
  check(original&&alias&&comparison.close(original)&&comparison.submit(alias,Port::work,&work,cb,20));
  check(comparison.submit(alias,Port::completion,&completion,cb)&&comparison.dispatch(alias,Port::completion));
  check(!comparison.dispatch(alias,Port::work)&&comparison.advance(alias,20)&&comparison.dispatch(alias,Port::work));
  check(comparison.submit(alias,Port::work,&pending,cb,60000)&&comparison.stop(alias)&&comparison.close(alias));
  check(observed.n==2&&observed.order[0]==11&&observed.order[1]==7&&observed.cancel==1);
  std::cout<<"REVENGE-DELAY-V1 work=1 completion=1 sum=18 canceled=1 alias=1\n";
  std::cout<<"Scheduled queues: 256 isolated workloads, 1024 dispatched, 640 canceled, alias close, stale handles, virtual deadlines, stable order, bounds and overflow passed. Independent model only.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
