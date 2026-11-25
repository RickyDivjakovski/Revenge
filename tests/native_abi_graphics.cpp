// Independent native graphics client: only the public ABI is linked.
#include "revenge/native_abi.h"
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
#include <filesystem>
namespace {
void check(bool b,const char* msg){if(!b)throw std::runtime_error(msg);}
using Session=std::unique_ptr<rv_session,decltype(&rv_close)>;
Session open(){rv_session* p=nullptr;check(rv_open(RV_ABI_VERSION,&p)==RV_OK&&p,"open");return {p,rv_close};}
rv_request req(uint32_t op,uint64_t h=0,uint64_t n=0,uint64_t w=0,uint64_t ht=0){return {sizeof(rv_request),RV_ABI_VERSION,op,0,h,0,n,w,ht};}
uint64_t call(rv_session* s,rv_request r,void* b=nullptr,uint64_t n=0,uint32_t expected=RV_OK){uint64_t out=UINT64_MAX;auto status=rv_dispatch(s,&r,b,n,&out);if(status!=expected)throw std::runtime_error("op "+std::to_string(r.op)+": "+rv_status_name(status)+"; expected "+rv_status_name(expected));if(status!=RV_OK)check(out==0,"error result not zero");return out;}
uint64_t number(const char* s){std::string x(s);size_t pos=0;if(x.empty()||x.find_first_not_of("0123456789")!=std::string::npos)throw std::invalid_argument("expected unsigned integer");auto n=std::stoull(x,&pos);check(pos==x.size(),"invalid integer");return n;}
}
int main(int argc,char** argv){try{
 if(argc!=2&&argc!=5&&argc!=6){std::cerr<<"Usage: revenge_abi_graphics_fixture <scene.rsp> [width height present_seconds [fps_cap]]\nWithout dimensions: portable validation only.\n";return 2;}
 std::ifstream f(std::filesystem::path(argv[1]),std::ios::binary|std::ios::ate);auto size=f.tellg();check(size>=32&&size<=49184,"package size");std::vector<uint8_t> bytes(static_cast<size_t>(size));f.seekg(0);check(bool(f.read(reinterpret_cast<char*>(bytes.data()),size)),"read package");
 auto s=open(),other=open();auto caps=call(s.get(),req(RV_GFX_CAPS));auto load=[&](rv_session* session){return call(session,req(RV_GFX_LOAD,0,bytes.size()),bytes.data(),bytes.size());};
 auto bad=bytes;bad.back()^=1;call(s.get(),req(RV_GFX_LOAD,0,bad.size()),bad.data(),bad.size(),RV_INVALID);call(s.get(),req(RV_GFX_LOAD,0,bytes.size()-1),bytes.data(),bytes.size(),RV_RANGE);
 auto h=load(s.get());check(h&&call(s.get(),req(RV_GFX_COUNT))==1,"package count");call(other.get(),req(RV_GFX_RELEASE,h),nullptr,0,RV_INVALID);call(s.get(),req(RV_FREE,h),nullptr,0,RV_INVALID);
 call(s.get(),req(RV_GFX_DRAW,h,0,319,240),nullptr,0,RV_RANGE);call(s.get(),req(RV_GFX_DRAW,h,31,320,240),nullptr,0,RV_RANGE);call(other.get(),req(RV_GFX_DRAW,h,0,320,240),nullptr,0,RV_INVALID);
 if(!(caps&RV_GFX_GPU))call(s.get(),req(RV_GFX_DRAW,h,0,320,240),nullptr,0,RV_UNSUPPORTED);
 rv_frame_stats stats{};auto run=req(RV_GFX_RUN,h,2,320,240);run.offset=60;
 auto invalid=run;invalid.offset=241;call(s.get(),invalid,&stats,sizeof(stats),RV_RANGE);invalid=run;invalid.count=0;call(s.get(),invalid,&stats,sizeof(stats),RV_RANGE);call(s.get(),run,&stats,sizeof(stats)-1,RV_RANGE);call(other.get(),run,&stats,sizeof(stats),RV_INVALID);
 if(!(caps&RV_GFX_CONTINUOUS))call(s.get(),run,&stats,sizeof(stats),RV_UNSUPPORTED);
 if(argc==6){check(caps&RV_GFX_CONTINUOUS,"continuous backend unavailable");run.arg0=number(argv[2]);run.arg1=number(argv[3]);run.count=number(argv[4]);run.offset=number(argv[5]);check(call(s.get(),run,&stats,sizeof(stats))==sizeof(stats),"stats size");check(stats.submitted&&stats.presented&&stats.presented<=stats.submitted&&stats.elapsed_ns&&stats.first_pixels&&stats.last_pixels,"continuous stats");
 std::cout<<"Continuous ABI: submitted "<<stats.submitted<<"; successful Presents "<<stats.presented<<"; elapsed seconds "<<double(stats.elapsed_ns)/1e9<<"; Present/s "<<double(stats.presented)*1e9/double(stats.elapsed_ns)<<"; mean GPU draw+copy ms "<<double(stats.gpu_ns)/double(stats.submitted)/1e6<<".\nFirst oracle pixels: "<<stats.first_pixels<<"; last oracle pixels: "<<stats.last_pixels<<"; last x offset: "<<stats.last_shift<<".\n";}
 if(argc==5){check(caps&RV_GFX_GPU,"D3D12 GPU backend unavailable");auto w=number(argv[2]),ht=number(argv[3]),seconds=number(argv[4]);auto pixels=call(s.get(),req(RV_GFX_DRAW,h,seconds,w,ht));check(pixels>0,"zero verified GPU pixels");std::cout<<"Native ABI graphics: "<<pixels<<" verified GPU interior pixels at "<<w<<'x'<<ht<<"; requested presentation seconds: "<<seconds<<".\n";}
 call(s.get(),req(RV_GFX_RELEASE,h));call(s.get(),run,&stats,sizeof(stats),RV_INVALID);call(s.get(),req(RV_GFX_DRAW,h,0,320,240),nullptr,0,RV_INVALID);call(s.get(),req(RV_GFX_RELEASE,h),nullptr,0,RV_INVALID);
 for(unsigned i=0;i<16;++i){auto next=load(s.get());check(next!=h,"stale handle reused");}call(s.get(),req(RV_GFX_LOAD,0,bytes.size()),bytes.data(),bytes.size(),RV_RANGE);check(call(s.get(),req(RV_GFX_COUNT))==16,"capacity");
 call(s.get(),req(RV_STOP));check(call(s.get(),req(RV_GFX_COUNT))==0,"stop cleanup");call(s.get(),req(RV_GFX_LOAD,0,bytes.size()),bytes.data(),bytes.size(),RV_STATE);call(other.get(),req(RV_STOP));
 std::cout<<"Native ABI graphics: package integrity, capacity, bounds, stale/foreign handles and shutdown passed; zero package handles remaining.\nCompiled backend flags: "<<caps<<". Independent scene fixture; no Xbox ABI or gameplay.\n";return 0;
 }catch(const std::exception& e){std::cerr<<"ABI graphics fixture: "<<e.what()<<'\n';return 1;}}
