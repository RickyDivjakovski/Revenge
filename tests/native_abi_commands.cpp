#include "revenge/native_abi.h"
#include <array>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <limits>
#include <memory>
#include <stdexcept>
#include <string>
#include <vector>
namespace {
void check(bool b,const char* m){if(!b)throw std::runtime_error(m);}
using Session=std::unique_ptr<rv_session,decltype(&rv_close)>;
Session open(){rv_session* p=nullptr;check(rv_open(RV_ABI_VERSION,&p)==RV_OK&&p,"open");return {p,rv_close};}
rv_request req(uint32_t op,uint64_t handle=0){return {sizeof(rv_request),RV_ABI_VERSION,op,0,handle,0,0,0,0};}
uint64_t call(rv_session* s,rv_request r,void* data=nullptr,size_t n=0,uint32_t expect=RV_OK){uint64_t result=~uint64_t(0);auto status=rv_dispatch(s,&r,data,n,&result);if(status!=expect)throw std::runtime_error("op "+std::to_string(r.op)+": "+rv_status_name(status)+" expected "+rv_status_name(expect));if(status!=RV_OK)check(!result,"error result");return result;}
uint64_t create(rv_session* s,uint32_t kind,void* p,size_t n,uint64_t w=0,uint64_t h=0,uint32_t expect=RV_OK){auto r=req(RV_RES_CREATE);r.arg0=kind;r.arg1=w;r.offset=h;r.count=n;return call(s,r,p,n,expect);}
struct Commands {rv_draw_header header;rv_indexed_draw draws[2];};
static_assert(sizeof(Commands)==56);
using Pixel=std::array<uint8_t,4>;
size_t verify(const std::vector<uint8_t>& frame,unsigned w,unsigned h,Pixel front,Pixel rear,double scale=1.0){
 size_t checked=0;for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){double px=(2.0*x+1)/w-1,py=1-(2.0*y+1)/h;bool edge=false;for(double e:{-.75,-.5,0.,.75})edge|=std::abs(px-e*scale)<4.0/w;for(double e:{-.75,-.5,.5,.75})edge|=std::abs(py-e)<4.0/h;if(edge)continue;
 Pixel p{0,0,0,255};if(std::abs(px)<.75*scale&&std::abs(py)<.75)p=rear;if(px>-.5*scale&&px<0&&std::abs(py)<.5)p=front;
 if(std::memcmp(p.data(),frame.data()+(size_t(y)*w+x)*4,4))throw std::runtime_error("client pixel mismatch at "+std::to_string(x)+","+std::to_string(y));++checked;}return checked;
}
}
int main(int argc,char** argv){try{
 std::string mode=argc==3?argv[2]:"";
 if(argc!=2&&!(argc==3&&(mode=="--gpu"||mode=="--async"||mode=="--async-gpu"||mode=="--async-stress"||mode=="--async-gpu-stress"||mode=="--vertex"||mode=="--vertex-gpu"))){std::cerr<<"Usage: revenge_abi_commands_fixture <scene.rsp> [--gpu|--async|--async-gpu|--async-stress|--async-gpu-stress|--vertex|--vertex-gpu]\n";return 2;}
 std::ifstream f(std::filesystem::path(argv[1]),std::ios::binary|std::ios::ate);auto n=f.tellg();check(n>=32&&n<=49184,"package length");std::vector<uint8_t> package(static_cast<size_t>(n));f.seekg(0);check(bool(f.read(reinterpret_cast<char*>(package.data()),n)),"package read");
 bool narrow=mode=="--vertex"||mode=="--vertex-gpu";auto s=open(),foreign=open();auto load=req(narrow?RV_GFX_LOAD_CUSTOM:RV_GFX_LOAD);load.count=package.size();auto p=call(s.get(),load,package.data(),package.size());
 rv_vertex vertices[]={{-.5f,.5f,1,.5f,.5f},{0,.5f,1,.5f,.5f},{-.5f,-.5f,1,.5f,.5f},{0,-.5f,1,.5f,.5f},{-1.5f,1.5f,2,5.f/6,.5f},{1.5f,1.5f,2,5.f/6,.5f},{-1.5f,-1.5f,2,5.f/6,.5f},{1.5f,-1.5f,2,5.f/6,.5f}};
 uint16_t indices[]{0,1,2,2,1,3,4,5,6,6,5,7};Pixel texture[]={{1,2,3,255},{17,90,201,255},{240,50,9,255}};
 auto v=create(s.get(),RV_VERTEX,vertices,sizeof(vertices));auto i=create(s.get(),RV_INDEX,indices,sizeof(indices));auto t=create(s.get(),RV_TEXTURE,texture,sizeof(texture),3,1);
 Commands cmds{{v,i,t,p,2,0},{{0,6},{6,6}}};auto c=create(s.get(),RV_COMMANDS,&cmds,sizeof(cmds));check(call(s.get(),req(RV_CMD_VALIDATE,c))==2,"draw count");check(call(s.get(),req(RV_RES_COUNT))==4,"resource count");
 call(foreign.get(),req(RV_CMD_VALIDATE,c),nullptr,0,RV_INVALID);call(s.get(),req(RV_CMD_VALIDATE,v),nullptr,0,RV_INVALID);call(s.get(),req(RV_GFX_RELEASE,t),nullptr,0,RV_INVALID);create(s.get(),RV_TEXTURE,texture,sizeof(texture),4,1,RV_RANGE);
 auto old=vertices[0].x;vertices[0].x=std::numeric_limits<float>::quiet_NaN();create(s.get(),RV_VERTEX,vertices,sizeof(vertices),0,0,RV_INVALID);vertices[0].x=old;
 auto bad=cmds;bad.draws[1].count=9;auto b=create(s.get(),RV_COMMANDS,&bad,sizeof(bad));call(s.get(),req(RV_CMD_VALIDATE,b),nullptr,0,RV_INVALID);call(s.get(),req(RV_RES_RELEASE,b));
 bad=cmds;bad.header.texture=v;b=create(s.get(),RV_COMMANDS,&bad,sizeof(bad));call(s.get(),req(RV_CMD_VALIDATE,b),nullptr,0,RV_INVALID);call(s.get(),req(RV_RES_RELEASE,b));
 bad=cmds;bad.header.draw_count=3;create(s.get(),RV_COMMANDS,&bad,sizeof(bad),0,0,RV_INVALID);
 unsigned w=320,h=240;std::vector<uint8_t> frame(size_t(w)*h*4);auto draw=req(RV_CMD_SUBMIT,c);draw.arg0=w;draw.arg1=h;call(s.get(),draw,frame.data(),frame.size()-1,RV_RANGE);
 // Build an independent integer-coordinate reference to exercise the client oracle
 // without claiming a GPU result on the portable test path.
 for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){Pixel color{0,0,0,255};if(x>=(narrow?100u:40u)&&x<(narrow?220u:280u)&&y>=30&&y<210)color={9,50,240,255};if(x>=(narrow?120u:80u)&&x<160&&y>=60&&y<180)color={201,90,17,255};std::memcpy(frame.data()+(size_t(y)*w+x)*4,color.data(),4);}
 auto oracle_count=verify(frame,w,h,{201,90,17,255},{9,50,240,255},narrow?.5:1.);check(oracle_count>60000,"oracle coverage");frame[0]^=1;bool rejected=false;try{verify(frame,w,h,{201,90,17,255},{9,50,240,255},narrow?.5:1.);}catch(const std::runtime_error&){rejected=true;}check(rejected,"oracle corruption rejection");std::cout<<"CPU client oracle: "<<oracle_count<<" reference pixels checked; corruption rejected.\n";
 auto caps=call(s.get(),req(RV_GFX_CAPS));if(!(caps&RV_GFX_CUSTOM))call(s.get(),draw,frame.data(),frame.size(),RV_UNSUPPORTED);
 if(narrow){bool gpu=mode=="--vertex-gpu";size_t verified=0;if(gpu){check(caps&RV_GFX_CUSTOM,"GPU unavailable");check(call(s.get(),draw,frame.data(),frame.size())==frame.size(),"narrow sync readback");verified+=verify(frame,w,h,{201,90,17,255},{9,50,240,255},.5);}
  auto submit=req(RV_JOB_SUBMIT,c);submit.arg0=w;submit.arg1=h;submit.offset=gpu?0:1;auto job=call(s.get(),submit);auto wait=req(RV_JOB_WAIT,job);wait.arg0=10000;check(call(s.get(),wait)==RV_JOB_COMPLETE,"narrow job completion");if(gpu){check(call(s.get(),req(RV_JOB_READ,job),frame.data(),frame.size())==frame.size(),"narrow async readback");verified+=verify(frame,w,h,{201,90,17,255},{9,50,240,255},.5);}else check(call(s.get(),req(RV_JOB_READ,job))==2,"narrow draw count");call(s.get(),req(RV_JOB_RELEASE,job));call(s.get(),req(RV_STOP));check(!call(s.get(),req(RV_JOB_COUNT))&&!call(s.get(),req(RV_RES_COUNT))&&!call(s.get(),req(RV_GFX_COUNT)),"narrow cleanup");std::cout<<"Custom vertex: horizontal scale 0.5; "<<verified<<" GPU oracle pixels; validation and cleanup passed.\n";return 0;
 }
 if(mode=="--gpu"){check(caps&RV_GFX_CUSTOM,"GPU backend unavailable");check(call(s.get(),draw,frame.data(),frame.size())==frame.size(),"readback length");auto checked=verify(frame,w,h,{201,90,17,255},{9,50,240,255});
 Pixel updated[]={{1,2,3,255},{6,100,55,255},{81,3,199,255}};auto t2=create(s.get(),RV_TEXTURE,updated,sizeof(updated),3,1);auto changed=cmds;changed.header.texture=t2;auto c2=create(s.get(),RV_COMMANDS,&changed,sizeof(changed));draw.handle=c2;check(call(s.get(),draw,frame.data(),frame.size())==frame.size(),"second readback length");auto second=verify(frame,w,h,{55,100,6,255},{199,3,81,255});call(s.get(),req(RV_RES_RELEASE,c2));call(s.get(),req(RV_RES_RELEASE,t2));std::cout<<"Client verified "<<checked<<" + "<<second<<" framebuffer pixels across two application-defined submissions.\n";}
 if(mode=="--async-stress"||mode=="--async-gpu-stress"){
  bool gpu=mode=="--async-gpu-stress";check(caps&RV_GFX_ASYNC,"async capability");if(gpu)check(caps&RV_GFX_CUSTOM,"GPU unavailable");
  size_t verified=0;auto start=std::chrono::steady_clock::now();
  for(unsigned batch=0;batch<16;++batch){
   unsigned bw=(batch/4)%2?384:320,bh=(batch/4)%2?256:240;
   auto submit=req(RV_JOB_SUBMIT,c);submit.arg0=bw;submit.arg1=bh;submit.offset=gpu?0:1;
   std::array<uint64_t,4> jobs{};std::array<Pixel,4> fronts{},rears{};
   for(unsigned k=0;k<jobs.size();++k){auto n=batch*4+k;texture[1]={uint8_t(17+n),90,uint8_t(201-n),255};texture[2]={uint8_t(240-n),50,uint8_t(9+n),255};fronts[k]={texture[1][2],90,texture[1][0],255};rears[k]={texture[2][2],50,texture[2][0],255};auto upd=req(RV_RES_UPDATE,t);upd.count=sizeof(texture);check(call(s.get(),upd,texture,sizeof(texture))==sizeof(texture),"stress update");jobs[k]=call(s.get(),submit);}
   check(call(s.get(),req(RV_JOB_COUNT))==4,"stress queue size");call(s.get(),submit,nullptr,0,RV_RANGE);
   for(unsigned k:{3u,1u,2u,0u}){auto wait=req(RV_JOB_WAIT,jobs[k]);wait.arg0=10000;uint64_t status=RV_JOB_QUEUED;for(unsigned attempt=0;attempt<6&&status<RV_JOB_COMPLETE;++attempt)status=call(s.get(),wait);check(status==RV_JOB_COMPLETE,"stress completion");if(gpu){frame.resize(size_t(bw)*bh*4);check(call(s.get(),req(RV_JOB_READ,jobs[k]),frame.data(),frame.size())==frame.size(),"stress readback");verified+=verify(frame,bw,bh,fronts[k],rears[k]);}else check(call(s.get(),req(RV_JOB_READ,jobs[k]))==2,"stress draw count");call(s.get(),req(RV_JOB_RELEASE,jobs[k]));call(s.get(),req(RV_JOB_POLL,jobs[k]),nullptr,0,RV_INVALID);}
   check(!call(s.get(),req(RV_JOB_COUNT)),"stress batch cleanup");
  }
  call(s.get(),req(RV_STOP));check(!call(s.get(),req(RV_JOB_COUNT))&&!call(s.get(),req(RV_RES_COUNT))&&!call(s.get(),req(RV_GFX_COUNT)),"stress stop cleanup");
  auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();std::cout<<"Async stress "<<(gpu?"GPU":"validation")<<": 64 jobs completed; "<<verified<<" GPU pixels verified; two extents; out-of-order collection; zero retained resources; elapsed ms "<<ms<<".\n";return 0;
 }
 if(mode=="--async"||mode=="--async-gpu"){
  bool gpu=mode=="--async-gpu";check(caps&RV_GFX_ASYNC,"async capability");if(gpu)check(caps&RV_GFX_CUSTOM,"GPU unavailable");
  auto submit=req(RV_JOB_SUBMIT,c);submit.arg0=w;submit.arg1=h;submit.offset=gpu?0:1;
  if(!gpu&&!(caps&RV_GFX_CUSTOM)){auto wrong=submit;wrong.offset=0;call(s.get(),wrong,nullptr,0,RV_UNSUPPORTED);}
  std::array<uint64_t,4> jobs{};std::array<Pixel,4> fronts{},rears{};
  for(unsigned k=0;k<jobs.size();++k){texture[1]={uint8_t(17+k),90,uint8_t(201-k),255};texture[2]={uint8_t(240-k),50,uint8_t(9+k),255};fronts[k]={texture[1][2],90,texture[1][0],255};rears[k]={texture[2][2],50,texture[2][0],255};auto upd=req(RV_RES_UPDATE,t);upd.count=sizeof(texture);check(call(s.get(),upd,texture,sizeof(texture))==sizeof(texture),"update bytes");jobs[k]=call(s.get(),submit);}
  check(call(s.get(),req(RV_JOB_COUNT))==4,"four retained jobs");call(s.get(),submit,nullptr,0,RV_RANGE);
  call(foreign.get(),req(RV_JOB_POLL,jobs[0]),nullptr,0,RV_INVALID);
  auto too_long=req(RV_JOB_WAIT,jobs[0]);too_long.arg0=10001;call(s.get(),too_long,nullptr,0,RV_RANGE);
  auto upd=req(RV_RES_UPDATE,t);upd.count=sizeof(texture)-1;call(s.get(),upd,texture,sizeof(texture)-1,RV_RANGE);
  call(s.get(),req(RV_RES_RELEASE,t));call(s.get(),req(RV_RES_RELEASE,c));call(s.get(),req(RV_RES_RELEASE,v));call(s.get(),req(RV_RES_RELEASE,i));call(s.get(),req(RV_GFX_RELEASE,p));
  size_t verified=0;for(unsigned k=0;k<jobs.size();++k){auto status=call(s.get(),req(RV_JOB_POLL,jobs[k]));check(status>=RV_JOB_QUEUED&&status<=RV_JOB_FAILED,"poll state");auto wait=req(RV_JOB_WAIT,jobs[k]);wait.arg0=10000;for(unsigned attempt=0;attempt<6&&status<RV_JOB_COMPLETE;++attempt)status=call(s.get(),wait);check(status==RV_JOB_COMPLETE,"job completed");if(gpu){call(s.get(),req(RV_JOB_READ,jobs[k]),frame.data(),frame.size()-1,RV_RANGE);check(call(s.get(),req(RV_JOB_READ,jobs[k]),frame.data(),frame.size())==frame.size(),"async readback bytes");verified+=verify(frame,w,h,fronts[k],rears[k]);}else check(call(s.get(),req(RV_JOB_READ,jobs[k]))==2,"validation draw count");call(s.get(),req(RV_JOB_RELEASE,jobs[k]));call(s.get(),req(RV_JOB_POLL,jobs[k]),nullptr,0,RV_INVALID);}
  check(!call(s.get(),req(RV_JOB_COUNT))&&!call(s.get(),req(RV_RES_COUNT))&&!call(s.get(),req(RV_GFX_COUNT)),"async cleanup");call(s.get(),req(RV_STOP));check(!call(s.get(),req(RV_JOB_COUNT)),"stop cleanup");std::cout<<"Async "<<(gpu?"GPU":"validation")<<": 4 jobs completed; "<<verified<<" GPU pixels verified; snapshot release, limits and cleanup passed.\n";return 0;
 }
 call(s.get(),req(RV_RES_RELEASE,t));call(s.get(),req(RV_CMD_VALIDATE,c),nullptr,0,RV_INVALID);call(s.get(),req(RV_RES_RELEASE,t),nullptr,0,RV_INVALID);call(s.get(),req(RV_RES_RELEASE,c));call(s.get(),req(RV_RES_RELEASE,v));call(s.get(),req(RV_RES_RELEASE,i));check(!call(s.get(),req(RV_RES_COUNT)),"cleanup");
 std::vector<uint8_t> big(RV_MAX_TRANSFER);for(unsigned j=0;j<8;++j)create(s.get(),RV_TEXTURE,big.data(),big.size(),512,512);create(s.get(),RV_TEXTURE,texture,sizeof(texture),3,1,RV_RANGE);call(s.get(),req(RV_STOP));check(!call(s.get(),req(RV_RES_COUNT))&&!call(s.get(),req(RV_GFX_COUNT)),"stop cleanup");
 for(unsigned j=0;j<64;++j)create(foreign.get(),RV_INDEX,indices,sizeof(indices));create(foreign.get(),RV_INDEX,indices,sizeof(indices),0,0,RV_RANGE);call(foreign.get(),req(RV_STOP));check(!call(foreign.get(),req(RV_RES_COUNT)),"foreign cleanup");
 std::cout<<"Command ABI: application-defined vertex/index/texture resources, two indexed draws, bounds, stale/foreign handles, byte and handle budgets, cleanup passed.\n";return 0;
 }catch(const std::exception& e){std::cerr<<"Command ABI fixture: "<<e.what()<<'\n';return 1;}}
