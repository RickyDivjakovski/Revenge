#include "revenge/native_abi.h"
#include <array>
#include <algorithm>
#include <vector>
#include <memory>
#include <iostream>
#include <stdexcept>
#include <string>
static void check(bool b){if(!b)throw std::runtime_error("resident draw mismatch");}
int main(int argc,char** argv){try{
 rv_session* raw=nullptr;check(rv_open(RV_ABI_VERSION,&raw)==RV_OK);std::unique_ptr<rv_session,decltype(&rv_close)> s(raw,rv_close);
 auto call=[&](uint32_t op,uint64_t h=0,uint64_t n=0,uint64_t a=0,uint64_t b=0,void* p=nullptr,size_t cap=0,uint32_t expected=RV_OK,uint64_t offset=0){rv_request r{sizeof(r),RV_ABI_VERSION,op,0,h,offset,n,a,b};uint64_t result=99;auto st=rv_dispatch(s.get(),&r,p,cap,&result);if(st!=expected)throw std::runtime_error("op "+std::to_string(op)+" status "+std::to_string(st)+" expected "+std::to_string(expected));if(st!=RV_OK)check(!result);return result;};
 const uint32_t vs[]={0x100c04ffu,0x40800000u,0x0a0c0cffu,0x40000000u,0x100c0cffu,0x3e924925u,0x7e0a0280u,0xf800020fu,0x05050403u,0xf80008cfu,0x02060100u,0xbf810000u};
 const uint32_t ps[]={0x7e040280u,0xf0900f08u,0x00400400u,0xbf8c3f70u,0xf800180fu,0x07040506u,0xbf810000u};
 rv_gpu_program program{};program.vertex_count=sizeof(vs)/sizeof(*vs);program.pixel_count=sizeof(ps)/sizeof(*ps);std::copy(vs,vs+program.vertex_count,program.vertex);std::copy(ps,ps+program.pixel_count,program.pixel);
 bool compiled=(call(RV_GFX_CAPS)&RV_GFX_RESIDENT_DRAW)!=0;check(bool(call(RV_GFX_CAPS)&RV_GFX_RESIDENT_EXTENDED)==compiled);
 check(bool(call(RV_GFX_CAPS)&RV_GFX_RESIDENT_PROGRAM)==compiled);check(call(RV_GPU_PROGRAM_COUNT,0,0,0,0,nullptr,0,compiled?RV_OK:RV_UNSUPPORTED)==0);
 check(call(RV_GPU_PROGRAM_HANDLES,0,0,0,0,nullptr,0,compiled?RV_OK:RV_UNSUPPORTED)==0);
 check(call(RV_GPU_PROGRAM_CLEAR,0,0,0,0,nullptr,0,compiled?RV_OK:RV_UNSUPPORTED)==0);
 check(bool(call(RV_GFX_CAPS)&RV_GFX_RESIDENT_BATCH)==compiled);
 check(bool(call(RV_GFX_CAPS)&RV_GFX_RESIDENT_ASYNC)==compiled);
 call(RV_GPU_BATCH_POLL,0,0,0,0,nullptr,0,RV_INVALID);
 call(RV_GPU_BATCH_WAIT,1,0,10001,0,nullptr,0,RV_RANGE);
 call(RV_GPU_BATCH_POLL,1,0,0,0,nullptr,0,compiled?RV_INVALID:RV_UNSUPPORTED);
 rv_gpu_batch_draw malformed{1,1,2,3,0,6,0};call(RV_GPU_DRAW_BATCH,0,1,0,0,&malformed,sizeof(malformed),RV_INVALID);call(RV_GPU_BATCH_SUBMIT,0,1,0,0,&malformed,sizeof(malformed),RV_INVALID);malformed.texture=4;call(RV_GPU_DRAW_BATCH,0,1,0,0,&malformed,sizeof(malformed),compiled?RV_INVALID:RV_UNSUPPORTED);
 if(argc==1){auto bad=program;bad.pixel_count=257;call(RV_GPU_DRAW_PROGRAM,0,0,0,0,&bad,sizeof(bad),RV_INVALID);bad=program;bad.pixel[0]=0xffffffff;call(RV_GPU_DRAW_PROGRAM,0,0,0,0,&bad,sizeof(bad),RV_INVALID);call(RV_GPU_DRAW_PROGRAM,0,0,0,0,&program,sizeof(program),compiled?RV_INVALID:RV_UNSUPPORTED);call(RV_GPU_PROGRAM_CREATE,1,0,0,0,&program,sizeof(program),RV_INVALID);call(RV_GPU_PROGRAM_CREATE,0,0,0,0,&bad,sizeof(bad),RV_INVALID);bad=program;--bad.vertex_count;call(RV_GPU_DRAW_PROGRAM,0,0,0,0,&bad,sizeof(bad),RV_INVALID);call(RV_GPU_PROGRAM_CREATE,0,0,0,0,&program,sizeof(program)-1,RV_INVALID);call(RV_GPU_DRAW_INDEXED,0,0,0,0,nullptr,0,compiled?RV_INVALID:RV_UNSUPPORTED);call(RV_GPU_DRAW_EXTENDED,0,0,0,0,nullptr,0,compiled?RV_INVALID:RV_UNSUPPORTED);std::cout<<"Resident draw capability contract passed\n";return 0;}
 check(argc==2&&std::string(argv[1])=="--gpu"&&compiled);
 auto handle=call(RV_GPU_PROGRAM_CREATE,0,0,0,0,&program,sizeof(program));check(handle!=0);check(call(RV_GPU_PROGRAM_COUNT)==1);check(call(RV_GPU_PROGRAM_HANDLES)==1);
 auto alias=call(RV_GPU_PROGRAM_CREATE,0,0,0,0,&program,sizeof(program));check(alias!=handle);check(call(RV_GPU_PROGRAM_COUNT)==1);check(call(RV_GPU_PROGRAM_HANDLES)==2);call(RV_GPU_PROGRAM_RELEASE,alias);call(RV_GPU_PROGRAM_RELEASE,alias,0,0,0,nullptr,0,RV_INVALID);check(call(RV_GPU_PROGRAM_HANDLES)==1);
 call(RV_GPU_PROGRAM_CLEAR,0,0,0,0,nullptr,0,RV_STATE);
 std::array<uint64_t,15> aliases{};for(auto& id:aliases)id=call(RV_GPU_PROGRAM_CREATE,0,0,0,0,&program,sizeof(program));check(call(RV_GPU_PROGRAM_HANDLES)==16);check(call(RV_GPU_PROGRAM_COUNT)==1);call(RV_GPU_PROGRAM_CREATE,0,0,0,0,&program,sizeof(program),RV_RANGE);for(auto id:aliases)call(RV_GPU_PROGRAM_RELEASE,id);
 rv_session* other_raw=nullptr;check(rv_open(RV_ABI_VERSION,&other_raw)==RV_OK);std::unique_ptr<rv_session,decltype(&rv_close)> other_session(other_raw,rv_close);rv_request foreign{sizeof(rv_request),RV_ABI_VERSION,RV_GPU_PROGRAM_RELEASE,0,handle,0,0,0,0};uint64_t foreign_result=99;check(rv_dispatch(other_session.get(),&foreign,nullptr,0,&foreign_result)==RV_INVALID&&!foreign_result);other_session.reset();
 std::array<float,8> vertices{-1,-1,-1,1,1,1,1,-1};std::array<uint16_t,6> indices{0,1,2,0,2,3};
 auto v=call(RV_GPU_ALLOC,0,sizeof(vertices)),i=call(RV_GPU_ALLOC,0,sizeof(indices));
 call(RV_GPU_UPLOAD,v,sizeof(vertices),0,0,vertices.data(),sizeof(vertices));call(RV_GPU_UPLOAD,i,sizeof(indices),0,0,indices.data(),sizeof(indices));
 size_t pixels=0,program_pixels=0;for(unsigned w:{13u,32u}){unsigned h=9;size_t n=w*h*4;std::vector<uint8_t> input(n),output(n);auto x=call(RV_GPU_ALLOC,0,n,w,h),t=call(RV_GPU_ALLOC,0,n,w,h);
 call(RV_GPU_DRAW_INDEXED,t,6,v,i,nullptr,0,RV_STATE,x);
 call(RV_GPU_DRAW_INDEXED,t,7,v,i,nullptr,0,RV_RANGE,x);
 call(RV_GPU_DRAW_INDEXED,t,6,v,i,nullptr,0,RV_INVALID,t);
 for(unsigned frame=0;frame<6;++frame){for(size_t k=0;k<n;++k)input[k]=static_cast<uint8_t>(k*31+frame*17);call(RV_GPU_UPLOAD,x,n,0,0,input.data(),n);check(call(RV_GPU_DRAW_INDEXED,t,6,v,i,nullptr,0,RV_OK,x)==6);call(RV_GPU_READ,t,n,0,0,output.data(),n);check(output==input);pixels+=w*h;
 check(call(RV_GPU_DRAW_INDEXED,x,6,v,i,nullptr,0,RV_OK,t)==6);call(RV_GPU_READ,x,n,0,0,output.data(),n);check(output==input);pixels+=w*h;}
 // A single triangle must cover only part of the target, proving the index count is used.
 check(call(RV_GPU_DRAW_INDEXED,t,3,v,i,nullptr,0,RV_OK,x)==3);call(RV_GPU_READ,t,n,0,0,output.data(),n);
 for(size_t k=0;k<4;++k){check(output[k]==input[k]);check(output[n-4+k]==0);}
 auto second=call(RV_GPU_ALLOC,0,n,w,h);std::vector<uint8_t> other(n,64);std::fill(input.begin(),input.end(),128);call(RV_GPU_UPLOAD,x,n,0,0,input.data(),n);call(RV_GPU_UPLOAD,second,n,0,0,other.data(),n);
 rv_gpu_draw_options opt{x,4,0xffffffffu};call(RV_GPU_DRAW_EXTENDED,t,6,v,i,&opt,sizeof(opt),RV_INVALID,x);
 for(unsigned mode:{0u,1u,2u,3u,0u}){opt={mode==2?second:x,mode,0xff00ff00u};check(call(RV_GPU_DRAW_EXTENDED,t,6,v,i,&opt,sizeof(opt),RV_OK,x)==6);call(RV_GPU_READ,t,n,0,0,output.data(),n);for(size_t k=0;k<n;++k){auto channel=k%4;unsigned expected=mode==1?(channel==3?128:127):mode==2?96:mode==3?(channel%2?128:0):128;check(output[k]==expected);}}
 opt={t,2,0};call(RV_GPU_DRAW_EXTENDED,t,6,v,i,&opt,sizeof(opt),RV_INVALID,x);call(RV_GPU_RELEASE,second);
 // Alternate translated programs, including a vertex program that halves clip-space X.
 for(size_t k=0;k<n;k+=4){input[k]=17;input[k+1]=61;input[k+2]=113;input[k+3]=255;}
 call(RV_GPU_UPLOAD,x,n,0,0,input.data(),n);
 for(unsigned variant:{0u,1u,2u,3u,4u,0u}){
  auto packet=program;if(variant==1)packet.pixel[5]=0x07060504u;
  if(variant==2){std::move_backward(packet.vertex,packet.vertex+packet.vertex_count,packet.vertex+packet.vertex_count+2);packet.vertex[0]=0x100000ffu;packet.vertex[1]=0x3f000000u;packet.vertex_count+=2;}
  if(variant==3){std::move_backward(packet.pixel+4,packet.pixel+packet.pixel_count,packet.pixel+packet.pixel_count+2);packet.pixel[4]=(27u<<25)|(6u<<17)|(6u<<9)|255u;packet.pixel[5]=0;packet.pixel_count+=2;}
  if(variant==4){std::move_backward(packet.pixel+4,packet.pixel+packet.pixel_count,packet.pixel+packet.pixel_count+2);packet.pixel[4]=(19u<<25)|(6u<<17)|(6u<<9)|255u;packet.pixel[5]=0x3e800000u;packet.pixel_count+=2;}
  if(variant==0){check(call(RV_GPU_DRAW_PIPELINE,t,6,v,i,&handle,sizeof(handle),RV_OK,x)==6);call(RV_GPU_DRAW_PIPELINE,t,6,v,i,&alias,sizeof(alias),RV_INVALID,x);}else check(call(RV_GPU_DRAW_PROGRAM,t,6,v,i,&packet,sizeof(packet),RV_OK,x)==6);call(RV_GPU_READ,t,n,0,0,output.data(),n);
  for(unsigned yy=0;yy<h;++yy)for(unsigned xx=0;xx<w;++xx){bool inside=variant!=2||(float(xx)+0.5f>float(w)*0.25f&&float(xx)+0.5f<float(w)*0.75f);size_t at=(yy*w+xx)*4;
   const std::array<uint8_t,4> expected=inside?(variant==1?std::array<uint8_t,4>{17,61,113,255}:variant==3?std::array<uint8_t,4>{0,61,17,255}:variant==4?std::array<uint8_t,4>{64,61,17,255}:std::array<uint8_t,4>{113,61,17,255}):std::array<uint8_t,4>{0,0,0,0};
   for(unsigned c=0;c<4;++c)check(output[at+c]==expected[c]);}program_pixels+=w*h;
 }
 // Two translated swizzles restore the original color, with distinct descriptor slots.
 std::array<rv_gpu_batch_draw,4> batch{{{t,x,v,i,handle,6,0},{x,t,v,i,handle,6,0},{t,x,v,i,0,6,0},{x,t,v,i,0,6,0}}};
 auto before=call(RV_GPU_SUBMISSION_COUNT);auto bad_batch=batch;bad_batch[3].pipeline=alias;
 call(RV_GPU_DRAW_BATCH,0,bad_batch.size(),0,0,bad_batch.data(),sizeof(bad_batch),RV_INVALID);check(call(RV_GPU_SUBMISSION_COUNT)==before);
 call(RV_GPU_READ,x,n,0,0,output.data(),n);check(output==input);before=call(RV_GPU_SUBMISSION_COUNT);
 check(call(RV_GPU_DRAW_BATCH,0,batch.size(),0,0,batch.data(),sizeof(batch))==batch.size());check(call(RV_GPU_SUBMISSION_COUNT)==before+1);
 call(RV_GPU_READ,x,n,0,0,output.data(),n);check(output==input);call(RV_GPU_READ,t,n,0,0,output.data(),n);check(output==input);
 // Non-blocking submission; retain resources until observed fence completion.
 before=call(RV_GPU_SUBMISSION_COUNT);
 call(RV_GPU_BATCH_SUBMIT,0,bad_batch.size(),0,0,bad_batch.data(),sizeof(bad_batch),RV_INVALID);check(call(RV_GPU_SUBMISSION_COUNT)==before);
 auto ticket=call(RV_GPU_BATCH_SUBMIT,0,batch.size(),0,0,batch.data(),sizeof(batch));check(ticket!=0);check(call(RV_GPU_SUBMISSION_COUNT)==before+1);
 call(RV_GPU_BATCH_RELEASE,ticket,0,0,0,nullptr,0,RV_STATE);
 call(RV_GPU_BATCH_SUBMIT,0,batch.size(),0,0,batch.data(),sizeof(batch),RV_STATE);
 call(RV_GPU_RELEASE,x,0,0,0,nullptr,0,RV_STATE);
 call(RV_GPU_PROGRAM_RELEASE,handle,0,0,0,nullptr,0,RV_STATE);
 call(RV_GPU_READ,t,n,0,0,output.data(),n,RV_STATE);
 auto status=call(RV_GPU_BATCH_POLL,ticket);check(status<=1);
 check(call(RV_GPU_BATCH_WAIT,ticket,0,10000)==1);
 check(call(RV_GPU_BATCH_POLL,ticket)==1);
 call(RV_GPU_BATCH_SUBMIT,0,batch.size(),0,0,batch.data(),sizeof(batch),RV_STATE);
 call(RV_GPU_READ,x,n,0,0,output.data(),n);check(output==input);call(RV_GPU_READ,t,n,0,0,output.data(),n);check(output==input);
 rv_session* foreign_raw=nullptr;check(rv_open(RV_ABI_VERSION,&foreign_raw)==RV_OK);std::unique_ptr<rv_session,decltype(&rv_close)> foreign_owner(foreign_raw,rv_close);
 rv_request ticket_request{sizeof(rv_request),RV_ABI_VERSION,RV_GPU_BATCH_POLL,0,ticket,0,0,0,0};uint64_t ticket_result=99;check(rv_dispatch(foreign_owner.get(),&ticket_request,nullptr,0,&ticket_result)==RV_INVALID&&ticket_result==0);
 call(RV_GPU_BATCH_RELEASE,ticket);call(RV_GPU_BATCH_POLL,ticket,0,0,0,nullptr,0,RV_INVALID);
 check(call(RV_GPU_PROGRAM_COUNT)==5);call(RV_GPU_RELEASE,x);call(RV_GPU_RELEASE,t);call(RV_GPU_DRAW_INDEXED,t,6,v,i,nullptr,0,RV_INVALID,x);}
 call(RV_GPU_PROGRAM_RELEASE,handle);check(call(RV_GPU_PROGRAM_HANDLES)==0);check(call(RV_GPU_PROGRAM_COUNT)==5);check(call(RV_GPU_PROGRAM_CLEAR)==5);check(call(RV_GPU_PROGRAM_COUNT)==0);auto final_handle=call(RV_GPU_PROGRAM_CREATE,0,0,0,0,&program,sizeof(program));check(final_handle!=handle);check(call(RV_GPU_PROGRAM_COUNT)==1);check(call(RV_GPU_PROGRAM_HANDLES)==1);call(RV_GPU_RELEASE,v);call(RV_GPU_RELEASE,i);check(call(RV_GPU_USAGE)==0);
 // Stop with a retained, unobserved submission: teardown must drain the fence.
 v=call(RV_GPU_ALLOC,0,sizeof(vertices));i=call(RV_GPU_ALLOC,0,sizeof(indices));
 call(RV_GPU_UPLOAD,v,sizeof(vertices),0,0,vertices.data(),sizeof(vertices));call(RV_GPU_UPLOAD,i,sizeof(indices),0,0,indices.data(),sizeof(indices));
 std::array<uint8_t,4> pixel{17,61,113,255};auto stop_src=call(RV_GPU_ALLOC,0,4,1,1),stop_dst=call(RV_GPU_ALLOC,0,4,1,1);
 call(RV_GPU_UPLOAD,stop_src,4,0,0,pixel.data(),pixel.size());rv_gpu_batch_draw stop_draw{stop_dst,stop_src,v,i,final_handle,6,0};
 auto stop_ticket=call(RV_GPU_BATCH_SUBMIT,0,1,0,0,&stop_draw,sizeof(stop_draw));check(stop_ticket!=0);call(RV_STOP);check(call(RV_GPU_USAGE)==0);call(RV_GPU_BATCH_POLL,stop_ticket,0,0,0,nullptr,0,RV_INVALID);check(call(RV_GPU_PROGRAM_COUNT)==0);check(call(RV_GPU_PROGRAM_HANDLES)==0);std::cout<<"Resident indexed drawing: "<<pixels<<" pixels verified, 24 full-target draws plus two partial draws, persistent vertex/index/texture bindings, ping-pong targets, ten extended draws and cleanup passed; translated stage draws: "<<program_pixels<<" pixels verified; resident batches: 8 draws in 2 submissions verified; async resident batches: 8 draws, 2 tickets verified; pending stop drain passed; stage bitwise mask: 405 pixels verified; stage integer min: 405 pixels verified\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
