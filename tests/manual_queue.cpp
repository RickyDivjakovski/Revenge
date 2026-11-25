#include "../samples/platform/queue_workload.hpp"
#include <iostream>
#include <stdexcept>
using namespace revenge::gaming;
static void check(bool b){if(!b)throw std::runtime_error("manual queue invariant failed");}
int main(){try {
    for(unsigned i=0;i<64;++i){ManualQueue q;QueueWorkload w;check(w.run(q));q.stop();if(i==0)w.print(std::cout);}
    ManualQueue q; QueueWorkload::State s; QueueWorkload::Item item{&s,3};
    for(unsigned i=0;i<128;++i)check(q.submit(Port::work,&item,QueueWorkload::callback));
    check(!q.submit(Port::work,&item,QueueWorkload::callback));
    check(q.submit(Port::completion,&item,QueueWorkload::callback));
    check(!q.submit(Port::work,&item,nullptr));check(!q.dispatch(static_cast<Port>(8)));
    check(q.dispatch(Port::work)&&s.count==1&&s.checksum==3);
    q.stop();q.stop();check(s.canceled==128&&!q.dispatch(Port::work)&&!q.submit(Port::completion,&item,QueueWorkload::callback));
    std::cout<<"Independent manual queue: 4096 dispatched callbacks, port isolation, 128 cancellations, bounded capacity and stop passed. Not a GDK ABI.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
