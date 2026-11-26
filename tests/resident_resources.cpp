#include "revenge/native_abi.h"
#include <vector>
#include <memory>
#include <iostream>
#include <stdexcept>
#include <string>
static void check(bool b){if(!b)throw std::runtime_error("resident check failed");}
int main(int argc,char** argv){try{
 rv_session* raw=nullptr;check(rv_open(RV_ABI_VERSION,&raw)==RV_OK);std::unique_ptr<rv_session,decltype(&rv_close)> s(raw,rv_close);
 auto call=[&](uint32_t op,uint64_t h=0,uint64_t n=0,uint64_t a=0,uint64_t b=0,void* p=nullptr,size_t cap=0,uint32_t expected=RV_OK){rv_request r{sizeof(r),RV_ABI_VERSION,op,0,h,0,n,a,b};uint64_t result=99;auto st=rv_dispatch(s.get(),&r,p,cap,&result);if(st!=expected)throw std::runtime_error("op "+std::to_string(op)+" status "+std::to_string(st)+" expected "+std::to_string(expected));if(st!=RV_OK)check(!result);return result;};
 bool compiled=(call(RV_GFX_CAPS)&RV_GFX_RESIDENT_TRANSFER)!=0;
 if(argc==1){if(!compiled)call(RV_GPU_ALLOC,0,16,0,0,nullptr,0,RV_UNSUPPORTED);else{call(RV_GPU_ALLOC,0,0,0,0,nullptr,0,RV_RANGE);call(RV_GPU_ALLOC,0,RV_MAX_TRANSFER+1,0,0,nullptr,0,RV_RANGE);call(RV_GPU_ALLOC,0,16,1,0,nullptr,0,RV_RANGE);call(RV_GPU_ALLOC,0,16,2,3,nullptr,0,RV_RANGE);check(call(RV_GPU_USAGE)==0);}std::cout<<"Resident capability and non-device validation passed\n";return 0;}
 check(argc==2&&std::string(argv[1])=="--gpu"&&compiled);
 uint64_t checked=0;for(auto w:{0u,13u,64u}){unsigned h=w?7:0;size_t n=w?w*h*4:4096;std::vector<uint8_t> input(n),out(n,0xa5);auto x=call(RV_GPU_ALLOC,0,n,w,h),y=call(RV_GPU_ALLOC,0,n,w,h);check(x!=y&&call(RV_GPU_USAGE)==2*n);call(RV_GPU_READ,x,n,0,0,out.data(),n,RV_STATE);call(RV_GPU_COPY,y,0,x,0,nullptr,0,RV_STATE);for(unsigned frame=0;frame<8;++frame){for(size_t i=0;i<n;++i)input[i]=static_cast<uint8_t>(i*37+frame*19);check(call(RV_GPU_UPLOAD,x,n,0,0,input.data(),n)==n);check(call(RV_GPU_COPY,y,0,x)==n);check(call(RV_GPU_READ,y,n,0,0,out.data(),n)==n&&out==input);checked+=n;}
 call(RV_GPU_COPY,x,0,x,0,nullptr,0,RV_INVALID);call(RV_GPU_RELEASE,x);call(RV_GPU_RELEASE,x,0,0,0,nullptr,0,RV_INVALID);check(call(RV_GPU_USAGE)==n);call(RV_GPU_RELEASE,y);check(call(RV_GPU_USAGE)==0);}
 auto live=call(RV_GPU_ALLOC,0,32);check(live!=0);call(RV_STOP);check(call(RV_GPU_USAGE)==0);call(RV_GPU_ALLOC,0,32,0,0,nullptr,0,RV_STATE);std::cout<<"Resident GPU transfers: "<<checked<<" bytes verified, 24 upload/copy/readback cycles, buffer and RGBA8 textures, cleanup passed\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
