#include "revenge/native_abi.h"
#include <algorithm>
#include <array>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string_view>
#include <vector>
namespace {
using Pixel=std::array<uint8_t,4>;
static_assert(sizeof(rv_pass_header)==16 && sizeof(rv_pass_draw)==24);
void check(bool ok,const char* why){if(!ok)throw std::runtime_error(why);}
using Session=std::unique_ptr<rv_session,decltype(&rv_close)>;
Session open(){rv_session* s=nullptr;check(rv_open(RV_ABI_VERSION,&s)==RV_OK&&s,"open");return Session(s,rv_close);}
rv_request request(uint32_t op){rv_request r{};r.size=sizeof(r);r.version=RV_ABI_VERSION;r.op=op;return r;}
struct Packet {rv_request r;std::vector<uint8_t> bytes;};
Packet packet(unsigned w,unsigned h,const std::vector<rv_pass_draw>& ds,unsigned mode){
    rv_pass_header header{w,h,static_cast<uint32_t>(ds.size()),0};auto r=request(RV_PASS_RENDER);r.arg0=mode;r.count=sizeof(header)+ds.size()*sizeof(rv_pass_draw);
    std::vector<uint8_t> b(std::max<size_t>(r.count,size_t(w)*h*12),0xcd);std::memcpy(b.data(),&header,sizeof(header));if(!ds.empty())std::memcpy(b.data()+sizeof(header),ds.data(),ds.size()*sizeof(rv_pass_draw));return {r,std::move(b)};
}
void invoke(rv_session* s,Packet& p,uint32_t expected,uint64_t output=0){uint64_t result=123;auto original=p.bytes;auto status=rv_dispatch(s,&p.r,p.bytes.data(),p.bytes.size(),&result);if(status!=expected)throw std::runtime_error(std::string("status: ")+rv_status_name(status));check(result==output,"result size");if(expected!=RV_OK)check(original==p.bytes,"failure modified packet");}
unsigned compare(const Packet& p,unsigned w,unsigned h,unsigned variant,unsigned tolerance){
    unsigned worst=0;for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){
        Pixel a{},b{};if(variant==0){if(x>=1&&x<w-1&&y>=1&&y<h-1){a={255,0,0,255};b={0,255,0,255};if(x>=w/2){a={128,0,128,255};}}}
        else if(variant==1){if(x<w/2){a={255,255,0,255};b={255,0,0,255};}else {a={255,0,0,255};b={0,255,0,255};}}
        // Variant 2: no stencil coverage, all draws fail; composition is opaque black.
        Pixel c{a[0],b[1],a[2],255};std::array<Pixel,3> expected{a,b,c};
        for(size_t t=0;t<3;++t)for(size_t k=0;k<4;++k){const auto i=(t*size_t(w)*h+y*w+x)*4+k;const unsigned delta=unsigned(std::abs(int(p.bytes[i])-int(expected[t][k])));worst=std::max(worst,delta);check(delta<=tolerance,"pixel mismatch");}
    }return worst;
}
}
int main(int argc,char** argv){try{
    const bool gpu=argc==2&&std::string_view(argv[1])=="--gpu";check(argc==1||gpu,"usage");if(gpu)std::cerr<<"[ABI fixture] start"<<std::endl;auto s=open(),other=open();uint64_t caps=0;auto cap=request(RV_GFX_CAPS);check(rv_dispatch(s.get(),&cap,nullptr,0,&caps)==RV_OK,"caps");if(gpu)check(caps&RV_GFX_MULTIPASS,"multipass unavailable");
    unsigned worst=0;size_t pixels=0;
    for(unsigned run=0;run<(gpu?6u:48u);++run){unsigned v=run%3,w=7+run%11,h=5+run%9;if(run==0){w=128;h=128;}if(run==2){w=1;h=1;}
        std::vector<rv_pass_draw> ds;
        if(v==0)ds={{1,1,w-1,h-1,128,0},{0,0,w,h,192,1},{w/2,0,w,h,64,2}};
        else if(v==1)ds={{0,0,w,h,192,0},{0,0,w/2,h,128,1},{0,0,w,h,224,0}};
        else ds={{0,0,w,h,0,1},{0,0,w,h,0,2},{0,0,w,h,256,0}};
        if(run==3){ds.insert(ds.end(),61,rv_pass_draw{0,0,w,h,256,0});}
        auto p=packet(w,h,ds,0);const auto bytes=size_t(w)*h*12;auto* caller=run%2?s.get():other.get();invoke(caller,p,RV_OK,bytes);compare(p,w,h,v,0);
        if(gpu){auto q=packet(w,h,ds,1);invoke(caller,q,RV_OK,bytes);worst=std::max(worst,compare(q,w,h,v,1));for(size_t i=0;i<bytes;++i)check(std::abs(int(q.bytes[i])-int(p.bytes[i]))<=1,"CPU/GPU mismatch");}pixels+=size_t(w)*h*3;
    }
    const std::vector<rv_pass_draw> good{{0,0,3,3,128,0}};
    auto bad=packet(3,3,good,0);bad.r.handle=1;invoke(s.get(),bad,RV_INVALID);
    bad=packet(3,3,good,2);invoke(s.get(),bad,RV_INVALID);
    bad=packet(3,3,good,0);bad.r.count--;invoke(s.get(),bad,RV_RANGE);
    bad=packet(3,3,good,0);bad.bytes.push_back(0);invoke(s.get(),bad,RV_RANGE);
    bad=packet(0,3,good,0);invoke(s.get(),bad,RV_RANGE);
    bad=packet(129,3,good,0);invoke(s.get(),bad,RV_RANGE);
    bad=packet(3,3,{},0);invoke(s.get(),bad,RV_RANGE);
    bad=packet(3,3,std::vector<rv_pass_draw>(65,good[0]),0);invoke(s.get(),bad,RV_RANGE);
    bad=packet(3,3,{{0,0,4,3,128,0}},0);invoke(s.get(),bad,RV_INVALID);
    bad=packet(3,3,{{1,0,1,3,128,0}},0);invoke(s.get(),bad,RV_INVALID);
    bad=packet(3,3,{{0,0,3,3,257,0}},0);invoke(s.get(),bad,RV_INVALID);
    bad=packet(3,3,{{0,0,3,3,128,3}},0);invoke(s.get(),bad,RV_INVALID);
    bad=packet(3,3,good,0);bad.bytes[12]=1;invoke(s.get(),bad,RV_INVALID);
    if(!(caps&RV_GFX_MULTIPASS)){bad=packet(3,3,good,1);invoke(s.get(),bad,RV_UNSUPPORTED);}
    auto stop=request(RV_STOP);uint64_t out=0;check(rv_dispatch(s.get(),&stop,nullptr,0,&out)==RV_OK,"stop");bad=packet(3,3,good,0);invoke(s.get(),bad,RV_STATE);invoke(other.get(),bad,RV_OK,108);
    if(gpu)std::cerr<<"[ABI fixture] closing sessions"<<std::endl; s.reset();other.reset();
    {auto fresh=open();auto again=packet(3,3,good,gpu?1:0);invoke(fresh.get(),again,RV_OK,108);auto cpu=packet(3,3,good,0);invoke(fresh.get(),cpu,RV_OK,108);for(size_t i=0;i<108;++i)check(std::abs(int(again.bytes[i])-int(cpu.bytes[i]))<=(gpu?1:0),"reopened session mismatch");fresh.reset();if(gpu)std::cerr<<"[ABI fixture] reopened session verified and closed"<<std::endl;}
    std::cout<<"Native ABI multipass: "<<(gpu?6:48)<<" workloads, "<<pixels<<" pixels across three surfaces; 13 invalid packets rejected unchanged, separate sessions and stop passed; "<<(gpu?"desktop GPU and CPU":"CPU only")<<", max byte error "<<worst<<"."<<std::endl;return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
