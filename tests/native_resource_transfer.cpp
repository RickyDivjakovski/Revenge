#include "revenge/native_abi.h"
#include <array>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>
void check(bool b){if(!b)throw std::runtime_error("resource assertion");}
int main(){try{
 rv_session *raw=nullptr;check(rv_open(RV_ABI_VERSION,&raw)==RV_OK);std::unique_ptr<rv_session,decltype(&rv_close)> s(raw,rv_close);
 auto call=[&](uint32_t op,uint64_t h=0,void* p=nullptr,size_t n=0,uint64_t off=0,uint64_t count=0,uint64_t a=0,uint64_t b=0,uint32_t expected=RV_OK){rv_request r{sizeof(rv_request),RV_ABI_VERSION,op,0,h,off,count,a,b};uint64_t result=0;auto status=rv_dispatch(s.get(),&r,p,n,&result);if(status!=expected)throw std::runtime_error("op "+std::to_string(op)+" status "+std::to_string(status));if(status!=RV_OK)check(!result);return result;};
 check(call(RV_GFX_CAPS)&RV_GFX_RESOURCE_TRANSFER);
 std::array<uint16_t,4> data{1,2,3,4},out{};auto h=call(RV_RES_CREATE,0,data.data(),8,0,8,RV_INDEX);auto c=call(RV_RES_CLONE,h);check(c!=h&&call(RV_RES_USAGE)==16);rv_resource_info info{};check(call(RV_RES_INFO,c,&info,sizeof(info))==sizeof(info)&&info.kind==RV_INDEX&&info.bytes==8);
 call(RV_RES_COPY,c,nullptr,0,2,6,c);call(RV_RES_READ,c,out.data(),8,0,8);check((out==std::array<uint16_t,4>{1,1,2,3}));call(RV_RES_READ,h,out.data(),8,0,8);check(out==data);
 call(RV_RES_COPY,c,nullptr,0,0,8,h);call(RV_RES_COPY,c,nullptr,0,1,2,h,0,RV_RANGE);call(RV_RES_COPY,c,nullptr,0,UINT64_MAX,2,h,0,RV_RANGE);call(RV_RES_READ,c,out.data(),8,0,8);check(out==data);call(RV_RES_READ,c,out.data(),8,UINT64_MAX,8,0,0,RV_RANGE);check(out==data);
 call(RV_RES_RELEASE,h);call(RV_RES_CLONE,h,nullptr,0,0,0,0,0,RV_INVALID);check(call(RV_RES_USAGE)==8);call(RV_STOP);check(call(RV_RES_USAGE)==0);call(RV_RES_INFO,c,&info,sizeof(info),0,0,0,0,RV_STATE);
 std::cout<<"Resource transfers: metadata, clone isolation, overlap, alignment, overflow, stale handles and cleanup passed.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
