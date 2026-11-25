#include "probe_trace.hpp"
#include "revenge/native_abi.h"
#include <fstream>
#include <iostream>
#include <string>
#include <string_view>
#include <filesystem>
namespace {
using namespace revenge_arena;
struct Session{rv_session* s=nullptr;Session(){require(rv_open(RV_ABI_VERSION,&s)==RV_OK,"open");}~Session(){rv_close(s);}Session(const Session&)=delete;};
rv_request request(uint32_t op,uint64_t h=0){return {sizeof(rv_request),RV_ABI_VERSION,op,0,h,0,0,0,0};}
uint64_t call(rv_session* s,rv_request r,void* buf=nullptr,uint64_t size=0){uint64_t result=0;auto status=rv_dispatch(s,&r,buf,size,&result);if(status!=RV_OK)throw std::runtime_error(std::string("ABI op ")+std::to_string(r.op)+": "+rv_status_name(status));return result;}
std::string host_trace(){
 Session session;auto s=session.s;auto before=call(s,request(RV_CLOCK));
 auto t=scripted_trace();require(t==expected_trace,"game trace mismatch");
 auto r=request(RV_ALLOC);r.count=sizeof(t)+16;auto h=call(s,r);
 r=request(RV_WRITE,h);r.offset=8;r.count=sizeof(t);require(call(s,r,t.data(),sizeof(t))==sizeof(t),"trace write");
 Trace recovered{};r.op=RV_READ;require(call(s,r,recovered.data(),sizeof(recovered))==sizeof(recovered)&&t==recovered,"trace readback");
 auto tls=call(s,request(RV_TLS_CREATE));r=request(RV_TLS_SET,tls);r.arg0=recovered[5];call(s,r);require(call(s,request(RV_TLS_GET,tls))==4,"TLS score");call(s,request(RV_TLS_RELEASE,tls));
 require(call(s,request(RV_CLOCK))>=before,"monotonic clock");call(s,request(RV_FREE,h));call(s,request(RV_STOP));rv_stats stats{};call(s,request(RV_STATS),&stats,sizeof(stats));require(!stats.running&&!stats.committed_bytes&&!stats.memory_handles,"cleanup");
 char line[160]{};auto n=revenge_probe::format_trace(line,sizeof(line),recovered);require(n>0&&size_t(n)<sizeof(line),"trace formatting");return line;
}
void compare(const std::filesystem::path& path,const std::string& expected){
 std::ifstream f(path,std::ios::binary|std::ios::ate);require(bool(f),"trace file open");auto end=f.tellg();require(end>0&&end<=1024*1024,"trace file size (max 1 MiB)");f.seekg(0);std::string line;unsigned found=0;constexpr std::string_view prefix="REVENGE-PROBE-V1 ";
 while(std::getline(f,line)){if(!line.empty()&&line.back()=='\r')line.pop_back();auto pos=line.find(prefix);if(pos==std::string::npos)continue;require(line.substr(pos)==expected,"probe trace mismatch");++found;}
 require(!f.bad()&&found==1,"expected exactly one probe trace");
}
}
int main(int argc,char** argv){try{auto trace=host_trace();std::cout<<trace<<'\n';if(argc==3&&std::string_view(argv[1])=="--compare"){compare(argv[2],trace);std::cout<<"Supplied trace matches independent gameplay checkpoints (not proof of console origin or binary compatibility).\n";}else require(argc==1,"usage: RevengePlatformProbe [--compare trace.txt]");std::cout<<"Host ABI: memory, TLS, clock, game checkpoints and cleanup passed.\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
