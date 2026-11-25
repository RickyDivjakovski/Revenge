#include "revenge/queue_abi.h"
#include "revenge/native_abi.h"
#include <array>
#include <algorithm>
#include <atomic>
#include <latch>
#include <cstring>
#include <iostream>
#include <stdexcept>
#include <thread>
#include <vector>
#include <string_view>
namespace {
void check(bool b,const char* why){if(!b)throw std::runtime_error(why);}
struct Counts {std::atomic<unsigned> work{0},cancel{0},error{0};};
void RQ_CALL count(void* p,uint32_t cancel){auto& c=*static_cast<Counts*>(p);if(cancel)++c.cancel;else ++c.work;}
struct Reenter {rq_context* q;rq_handle h;Counts* counts;};
void RQ_CALL reenter(void* p,uint32_t cancel){auto& r=*static_cast<Reenter*>(p);if(cancel||rq_submit(r.q,r.h,RQ_COMPLETION,0,count,r.counts)!=RQ_OK)++r.counts->error;}
struct Block {std::latch entered{1},release{1};};
void RQ_CALL block(void* p,uint32_t canceled){if(canceled)return;auto& b=*static_cast<Block*>(p);b.entered.count_down();b.release.wait();}
struct Worker {rq_context* q;rq_handle h;unsigned count;};
uint32_t RV_CALL worker(rv_session*,uint64_t v){auto& w=*reinterpret_cast<Worker*>(static_cast<uintptr_t>(v));for(unsigned i=0;i<w.count;++i){const auto result=rq_dispatch(w.q,w.h,RQ_WORK,1000);if(result!=RQ_OK)return result;}return RQ_OK;}
uint64_t native(rv_session* s,uint32_t op,uint64_t h=0,uint64_t a=0,void* data=nullptr,uint64_t n=0){rv_request req{sizeof(req),RV_ABI_VERSION,op,0,h,0,0,a,0};uint64_t result=0;check(rv_dispatch(s,&req,data,n,&result)==RV_OK,"native thread operation");return result;}
struct Render {rv_session* session;unsigned mode;rq_context* q;rq_handle handle;unsigned width,height;bool passed=false,completed=false;};
void RQ_CALL render_complete(void* p,uint32_t canceled){auto& r=*static_cast<Render*>(p);r.completed=!canceled&&r.passed;}
void RQ_CALL render(void* p,uint32_t canceled){auto& r=*static_cast<Render*>(p);if(canceled)return;
    rv_pass_header h{r.width,r.height,1,0};rv_pass_draw d{0,0,r.width,r.height,128,0};const auto pixels=size_t(r.width)*r.height,output=pixels*12;
    std::vector<uint8_t> bytes(std::max(sizeof h+sizeof d,output));std::memcpy(bytes.data(),&h,sizeof h);std::memcpy(bytes.data()+sizeof h,&d,sizeof d);
    rv_request req{};req.size=sizeof req;req.version=RV_ABI_VERSION;req.op=RV_PASS_RENDER;req.arg0=r.mode;req.count=sizeof h+sizeof d;uint64_t result=0;
    const auto status=rv_dispatch(r.session,&req,bytes.data(),bytes.size(),&result);if(status!=RV_OK||result!=output){std::cerr<<"[queue render] status="<<rv_status_name(status)<<" bytes="<<result<<std::endl;return;}
    const std::array<std::array<uint8_t,4>,3> expected{{{255,0,0,255},{0,255,0,255},{255,255,0,255}}};
    for(size_t surface=0;surface<3;++surface)for(size_t i=0;i<pixels;++i)for(size_t k=0;k<4;++k)if(bytes[(surface*pixels+i)*4+k]!=expected[surface][k])return;
    r.passed=true;if(rq_submit(r.q,r.handle,RQ_COMPLETION,0,render_complete,&r)!=RQ_OK)r.passed=false;
}
}
int main(int argc,char** argv){try{
 const bool gpu=argc==2&&std::string_view(argv[1])=="--gpu";check(argc==1||gpu,"usage");if(gpu)std::cerr<<"[queue fixture] start"<<std::endl;rq_context* q=nullptr;rq_handle h=0;check(rq_open(RQ_VERSION,&q)==RQ_OK&&rq_create(q,&h)==RQ_OK,"open");Counts c;
 for(unsigned round=0;round<32;++round){std::vector<std::thread> threads;for(unsigned t=0;t<4;++t)threads.emplace_back([&]{for(unsigned j=0;j<32;++j)if(rq_submit(q,h,RQ_WORK,0,count,&c)!=RQ_OK)++c.error;});for(auto& t:threads)t.join();threads.clear();for(unsigned t=0;t<4;++t)threads.emplace_back([&]{for(unsigned j=0;j<32;++j)if(rq_dispatch(q,h,RQ_WORK,0)!=RQ_OK)++c.error;});for(auto& t:threads)t.join();}
 check(c.work==4096&&c.error==0,"parallel counts");check(rq_dispatch(q,h,RQ_WORK,0)==RQ_TIMEOUT,"empty");
 Reenter r{q,h,&c};check(rq_submit(q,h,RQ_WORK,0,reenter,&r)==RQ_OK&&rq_dispatch(q,h,RQ_WORK,100)==RQ_OK&&rq_dispatch(q,h,RQ_COMPLETION,100)==RQ_OK&&c.work==4097&&c.error==0,"reentrant submit");
 std::atomic<uint32_t> status{RQ_FAILURE};std::thread waiter([&]{status=rq_dispatch(q,h,RQ_WORK,2000);});auto submitted=rq_submit(q,h,RQ_WORK,5,count,&c);waiter.join();check(submitted==RQ_OK&&status==RQ_OK&&c.work==4098,"timed wait");
 rv_session* session=nullptr;check(rv_open(RV_ABI_VERSION,&session)==RV_OK,"graphics open");
 const unsigned mode=gpu?1u:0u;
 std::array<Render,6> jobs{{{session,mode,q,h,8,8},{session,mode,q,h,8,8},{session,mode,q,h,13,7},{session,mode,q,h,13,7},{session,mode,q,h,1,1},{session,mode,q,h,8,8}}};
 for(auto& job:jobs)check(rq_submit(q,h,RQ_WORK,0,render,&job)==RQ_OK,"render submission");
 Worker w{q,h,static_cast<unsigned>(jobs.size())};rv_thread_start start{worker,reinterpret_cast<uintptr_t>(&w)};auto thread=native(session,RV_THREAD_CREATE,0,0,&start,sizeof start);
 auto worker_state=native(session,RV_THREAD_WAIT,thread,10000);for(unsigned retry=0;gpu&&worker_state==RV_THREAD_RUNNING&&retry<5;++retry){std::cerr<<"[queue fixture] waiting for GPU worker"<<std::endl;worker_state=native(session,RV_THREAD_WAIT,thread,10000);}if(worker_state==RV_THREAD_RUNNING){rv_close(session);throw std::runtime_error("managed render worker timed out");}check(worker_state==RV_THREAD_COMPLETE&&native(session,RV_THREAD_EXIT,thread)==RQ_OK,"managed render worker");native(session,RV_THREAD_RELEASE,thread);check(native(session,RV_THREAD_COUNT)==0,"thread cleanup");
 rq_snapshot completed{sizeof(rq_snapshot)};check(rq_query(q,h,&completed)==RQ_OK&&completed.pending_work==0&&completed.pending_completion==jobs.size()&&completed.active==0,"render completion queue");
 size_t rendered_pixels=0;for(auto& job:jobs){check(rq_dispatch(q,h,RQ_COMPLETION,0)==RQ_OK&&job.passed&&job.completed,"render completion and pixels");rendered_pixels+=size_t(job.width)*job.height*3;}
 check(rq_query(q,h,&completed)==RQ_OK&&completed.pending_completion==0,"completion drain");if(gpu)std::cerr<<"[queue fixture] stopping and closing session"<<std::endl;native(session,RV_STOP);rv_close(session);if(gpu)std::cerr<<"[queue fixture] session closed"<<std::endl;
 // Closing a last handle while a callback is in flight must fail.
 Block b;check(rq_submit(q,h,RQ_WORK,0,block,&b)==RQ_OK,"block submit");std::thread active([&]{status=rq_dispatch(q,h,RQ_WORK,100);});b.entered.wait();rq_snapshot snap{sizeof(rq_snapshot)};auto queried=rq_query(q,h,&snap);auto close_active=rq_close(q,h);b.release.count_down();active.join();check(close_active==RQ_STATE&&status==RQ_OK&&queried==RQ_OK&&snap.active==1,"active close guard and snapshot");
 for(unsigned i=0;i<128;++i)check(rq_submit(q,h,RQ_WORK,60000,count,&c)==RQ_OK,"capacity");check(rq_submit(q,h,RQ_WORK,60000,count,&c)==RQ_LIMIT,"bounded");
 std::thread stop_waiter([&]{status=rq_dispatch(q,h,RQ_COMPLETION,2000);});auto stopped=rq_stop(q,h);stop_waiter.join();check(stopped==RQ_OK&&status==RQ_STATE&&c.cancel==128,"stop wake");check(rq_submit(q,h,RQ_WORK,0,count,&c)==RQ_STATE&&rq_close(q,h)==RQ_OK&&rq_destroy(q)==RQ_OK,"close");
 std::cout<<"Native C queue: 4096 parallel dispatches, 128 cancellations, timed wake, reentrant submission and stop wake; "<<rendered_pixels<<" "<<(gpu?"desktop GPU":"CPU")<<" pixels via managed native thread and existing graphics ABI passed; six render completions, two same-size reuse opportunities and three size changes."<<std::endl;return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
