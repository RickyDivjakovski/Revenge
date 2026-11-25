#include "native_reference.h"
#include "revenge/native_abi.h"
#include <array>
#include <cstdint>
#include <iostream>
#include <stdexcept>
#include <string>
#include <string_view>
namespace {
void check(bool yes,const char* why){if(!yes)throw std::runtime_error(why);}
rv_request req(uint32_t op,uint64_t h=0,uint64_t arg=0){return {sizeof(rv_request),RV_ABI_VERSION,op,0,h,0,0,arg,0};}
uint64_t call(rv_session* s,rv_request r,void* buffer=nullptr,uint64_t size=0){uint64_t out=0;auto status=rv_dispatch(s,&r,buffer,size,&out);if(status!=RV_OK)throw std::runtime_error("reference ABI op "+std::to_string(r.op)+": "+std::to_string(status));return out;}
struct Session{rv_session* p=nullptr;Session(){check(rv_open(RV_ABI_VERSION,&p)==RV_OK,"open");}~Session(){rv_close(p);}};
uint32_t RV_CALL worker(rv_session* s,uint64_t slot){uint32_t value=0,unwind=0;check(ref_step(0,&value)==0&&value==17,"TLS initial template");check(call(s,req(RV_TLS_GET,slot))==0,"ABI TLS initially empty");for(uint32_t i=1;i<=256;++i){check(ref_step(1,&value)==0&&value==17+i,"compiler TLS isolation");call(s,req(RV_TLS_SET,slot,value));check(call(s,req(RV_TLS_GET,slot))==value,"ABI TLS mismatch");check(ref_unwind(value,&unwind)==0&&unwind==value+2,"nested unwind cleanup");}return value;}
}
int main(int argc,char** argv){if(argc==2&&std::string_view(argv[1])=="--log-probe"){std::cout<<"reference-stdout-marker"<<std::endl;std::cerr<<"reference-stderr-marker"<<std::endl;return 7;}if(argc!=1)return 2;try{Session session;auto s=session.p;auto slot=call(s,req(RV_TLS_CREATE));auto before=ref_retired();uint32_t main_value=0;check(ref_step(5,&main_value)==0&&main_value==22,"main TLS");call(s,req(RV_TLS_SET,slot,22));for(unsigned round=0;round<4;++round){std::array<uint64_t,8> handles{};for(auto& h:handles){rv_thread_start start{worker,slot};h=call(s,req(RV_THREAD_CREATE),&start,sizeof(start));}for(auto h:handles){check(call(s,req(RV_THREAD_WAIT,h,10000))==RV_THREAD_COMPLETE,"worker completion");check(call(s,req(RV_THREAD_EXIT,h))==273,"worker exit");call(s,req(RV_THREAD_RELEASE,h));}check(ref_retired()==before+(round+1)*8,"TLS destructor after join");check(call(s,req(RV_THREAD_COUNT))==0,"thread leak");}check(ref_step(0,&main_value)==0&&main_value==22,"main TLS changed");check(call(s,req(RV_TLS_GET,slot))==22,"main ABI TLS changed");call(s,req(RV_TLS_RELEASE,slot));call(s,req(RV_STOP));std::cout<<"Compiler-built native reference: 32 managed threads, 8192 TLS comparisons, 8192 nested exception unwinds, 32 TLS destructors and independent main thread passed. OS loader, not Xbox execution.\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
