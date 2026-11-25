#include "revenge/native_abi.h"
#include "revenge/application.hpp"
#include "revenge/render_pass_fixture.hpp"
#include "revenge/multimedia.hpp"
#include <filesystem>
#include <string>
#include "revenge/scene.hpp"
#include "revenge/shader_package.hpp"
#include <atomic>
#include <algorithm>
#include <map>
#include <mutex>
#include <cstring>
#include <memory>
#include <stdexcept>
#include <cstddef>
#include <cmath>
#include <vector>
#include <thread>
#include <condition_variable>
#include <deque>
#include <chrono>
#include <array>
#include <iostream>
static_assert(sizeof(void*)==8,"Revenge native ABI requires a 64-bit host");
static_assert(sizeof(rv_request)==56 && offsetof(rv_request,handle)==16 && sizeof(rv_stats)==48 && sizeof(rv_frame_stats)==56 && sizeof(rv_pad_state)==40);
namespace a=revenge::app;
struct GraphicsResource {uint64_t kind=0; unsigned width=0,height=0; std::vector<uint8_t> bytes;};
static_assert(sizeof(revenge::scene3d::Vertex)==20);
static_assert(sizeof(rv_vertex)==20 && sizeof(rv_draw_header)==40 && sizeof(rv_indexed_draw)==8);
namespace {
std::atomic<uint64_t> package_serial{1};
#ifdef _WIN32
std::mutex gpu_mutex;
#endif
struct Job {
 unsigned w=0,h=0;bool validate_only=false;uint64_t draws=0;
 revenge::scene3d::CustomScene scene;revenge::package::Programs program;
 uint32_t state=RV_JOB_QUEUED;
};
struct JobQueue {
 std::mutex mutex;std::condition_variable cv;std::map<uint64_t,std::shared_ptr<Job>> jobs;
 std::deque<std::shared_ptr<Job>> pending;std::thread worker;bool stopping=false;
 ~JobQueue(){shutdown();}
 void run(){
#ifdef _WIN32
  std::shared_ptr<revenge::scene3d::CustomDevice> device;
  revenge::package::Programs active_program;
#endif
  for(;;){std::array<std::shared_ptr<Job>,2> batch{};unsigned count=0;
   {std::unique_lock lock(mutex);cv.wait(lock,[&]{return stopping||!pending.empty();});if(pending.empty())return;
    // Brief bounded coalescing for GPU jobs; preserve FIFO and do not delay shutdown.
    if(!stopping&&!pending.front()->validate_only&&pending.size()<2)cv.wait_for(lock,std::chrono::milliseconds(2),[&]{return stopping||pending.size()>=2;});
    while(count<batch.size()&&!pending.empty()){batch[count]=pending.front();pending.pop_front();batch[count++]->state=RV_JOB_RUNNING;}}
   std::array<bool,2> ok{},submitted{};
#ifdef _WIN32
   std::lock_guard gpu_lock(gpu_mutex);
#endif
   try{
    unsigned gpu_count=0;
    for(unsigned i=0;i<count;++i){auto& j=*batch[i];revenge::scene3d::validate(j.scene.mesh,j.scene.stream);
     if(j.validate_only)ok[i]=true;
#ifdef _WIN32
     else{if(device&&active_program!=j.program){ // Drain previous shader's slots before switching pipeline.
      for(unsigned k=0;k<i;++k)if(submitted[k]){auto& prior=*batch[k];revenge::scene3d::collect_custom(*device,k,prior.scene);ok[k]=prior.scene.framebuffer.size()==size_t(prior.w)*prior.h*4;submitted[k]=false;}
      device.reset();gpu_count=0;
     }if(!device){device=revenge::scene3d::create_custom_device();active_program=j.program;}if(revenge::scene3d::gpu_test(j.w,j.h,{},false,true,&j.program,{},0,nullptr,nullptr,&j.scene,device.get(),i))throw std::runtime_error("slot submit failed");submitted[i]=true;++gpu_count;}
#endif
    }
#ifdef _WIN32
    if(gpu_count&&std::any_of(batch.begin(),batch.begin()+count,[](const auto& j){return !j->validate_only&&!j->scene.quiet;}))std::cout<<"GPU batch: "<<gpu_count<<" submitted before collection.\n";
    for(unsigned i=0;i<count;++i)if(submitted[i]){auto& j=*batch[i];revenge::scene3d::collect_custom(*device,i,j.scene);ok[i]=j.scene.framebuffer.size()==size_t(j.w)*j.h*4;}
#endif
   }catch(...){
#ifdef _WIN32
    device.reset();
#endif
   }
   {std::lock_guard lock(mutex);for(unsigned i=0;i<count;++i)batch[i]->state=ok[i]?RV_JOB_COMPLETE:RV_JOB_FAILED;cv.notify_all();}
  }
 }
 bool full(){std::lock_guard lock(mutex);return jobs.size()>=4;}
 void submit(uint64_t id,std::shared_ptr<Job> j){std::lock_guard lock(mutex);if(stopping||jobs.size()>=4)throw std::logic_error("queue unavailable");if(!worker.joinable())worker=std::thread([this]{run();});jobs.emplace(id,j);try{pending.push_back(j);}catch(...){jobs.erase(id);throw;}cv.notify_one();}
 void shutdown(){ {std::lock_guard lock(mutex);stopping=true;cv.notify_all();}if(worker.joinable())worker.join();std::lock_guard lock(mutex);jobs.clear();pending.clear();}
};
}
struct ManagedThread { std::thread worker; uint32_t state=RV_THREAD_RUNNING, code=0; };
struct rv_session { std::mutex thread_mutex; std::condition_variable thread_cv; std::map<uint64_t,std::shared_ptr<ManagedThread>> threads; bool closing=false; std::mutex tls_mutex; std::map<uint64_t,std::map<uint64_t,uint64_t>> tls_slots; std::mutex gfx_mutex; std::map<uint64_t,revenge::package::Programs> packages; std::map<uint64_t,GraphicsResource> resources; size_t resource_bytes=0; a::Runtime runtime{[](unsigned,unsigned){revenge::scene3d::validate(revenge::scene3d::geometry(),revenge::scene3d::commands());}};JobQueue queue;
 #ifdef _WIN32
 std::shared_ptr<revenge::passes::PassCache> pass_cache;
 #endif
 ~rv_session(){ // No external calls may race close; managed callbacks may finish naturally.
  {std::lock_guard lock(thread_mutex);closing=true;}
  for(auto& entry:threads)if(entry.second->worker.joinable())entry.second->worker.join();
  queue.shutdown();
#ifdef _WIN32
  {std::lock_guard lock(gpu_mutex);pass_cache.reset();}
#endif
 }
};
namespace {
uint64_t next_package(){auto n=package_serial.load();for(;;){if(n>=(UINT64_C(1)<<63))throw std::overflow_error("package handle exhaustion");if(package_serial.compare_exchange_weak(n,n+1))return n|(UINT64_C(1)<<63);}}
uint64_t tls_identity(){static thread_local const uint64_t key=next_package();return key;}
uint32_t error() noexcept {try{throw;}catch(const std::out_of_range&){return RV_RANGE;}catch(const std::invalid_argument&){return RV_INVALID;}catch(const std::logic_error&){return RV_STATE;}catch(...){return RV_FAILURE;}}
}
extern "C" {
uint32_t RV_CALL rv_abi_version(void){return RV_ABI_VERSION;}
uint32_t RV_CALL rv_open(uint32_t version,rv_session** out){if(!out)return RV_INVALID;*out=nullptr;if(version!=RV_ABI_VERSION)return RV_VERSION;try{auto s=std::make_unique<rv_session>();s->runtime.dispatch(a::Start{});*out=s.release();return RV_OK;}catch(...){return error();}}
void RV_CALL rv_close(rv_session* s){delete s;}
const char* RV_CALL rv_status_name(uint32_t status){switch(status){case RV_OK:return "ok";case RV_INVALID:return "invalid request or handle";case RV_VERSION:return "ABI version mismatch";case RV_UNSUPPORTED:return "unsupported operation";case RV_RANGE:return "range or buffer limit";case RV_STATE:return "invalid lifecycle state";case RV_FAILURE:return "service failure";default:return "unknown status";}}
uint32_t RV_CALL rv_dispatch(rv_session* s,const rv_request* request,void* buffer,uint64_t capacity,uint64_t* result){
 if(!result)return RV_INVALID;*result=0;
 if(!s||!request)return RV_INVALID;
 if(request->size!=sizeof(rv_request))return RV_INVALID;
 const auto r=*request;
 if(r.version!=RV_ABI_VERSION)return RV_VERSION;
 if(r.reserved)return RV_INVALID;
 if(capacity>RV_MAX_TRANSFER)return RV_RANGE;
 if(capacity&&!buffer)return RV_INVALID;
 if(r.op==RV_PASS_RENDER){
  if(r.handle||r.offset||r.arg1||r.arg0>1||!buffer)return RV_INVALID;
  if(r.count<sizeof(rv_pass_header)||r.count>capacity)return RV_RANGE;
  rv_pass_header h{};std::memcpy(&h,buffer,sizeof(h));
  if(h.reserved)return RV_INVALID;
  if(!h.width||!h.height||h.width>128||h.height>128||!h.draw_count||h.draw_count>64)return RV_RANGE;
  const size_t input=sizeof(h)+size_t(h.draw_count)*sizeof(rv_pass_draw),output=size_t(h.width)*h.height*12;
  if(r.count!=input||capacity!=std::max(input,output))return RV_RANGE;
  try{
   std::lock_guard lock(s->gfx_mutex);if(!s->runtime.stats().running)return RV_STATE;
   std::vector<revenge::passes::Draw> commands;commands.reserve(h.draw_count);
   for(unsigned i=0;i<h.draw_count;++i){rv_pass_draw d{};std::memcpy(&d,static_cast<const uint8_t*>(buffer)+sizeof(h)+size_t(i)*sizeof(d),sizeof(d));if(d.depth256>256)return RV_INVALID;commands.push_back({{d.left,d.top,d.right,d.bottom},float(d.depth256)/256,d.mode});}
   revenge::passes::validate(h.width,h.height,commands);revenge::passes::Images images;
   if(!r.arg0)images=revenge::passes::reference(h.width,h.height,commands);
   else{
#ifdef _WIN32
    std::lock_guard gpu_lock(gpu_mutex);if(!s->pass_cache)s->pass_cache=revenge::passes::create_pass_cache();images=revenge::passes::render_gpu(*s->pass_cache,h.width,h.height,commands);
#else
    return RV_UNSUPPORTED;
#endif
   }
   const size_t pixels=size_t(h.width)*h.height;
   for(const auto& image:images)if(image.size()!=pixels)return RV_FAILURE;
   static_assert(sizeof(revenge::passes::Pixel)==4);
   for(size_t i=0;i<images.size();++i)std::memcpy(static_cast<uint8_t*>(buffer)+i*pixels*4,images[i].data(),pixels*4);
   *result=output;return RV_OK;
  }catch(const std::exception& e){if(r.arg0)std::cerr<<"[ABI pass] "<<e.what()<<std::endl;return error();}catch(...){if(r.arg0)std::cerr<<"[ABI pass] unknown exception"<<std::endl;return error();}
 }
 if(r.op>=RV_MEDIA_CAPS&&r.op<=RV_RES_PATCH){
  try{
   if(!s->runtime.stats().running)return RV_STATE;
   if(r.op==RV_RES_PATCH){
    if(r.arg0||r.arg1||!buffer||!capacity||r.count!=capacity)return RV_INVALID;
    std::lock_guard lock(s->gfx_mutex);if(!s->runtime.stats().running)return RV_STATE;
    auto it=s->resources.find(r.handle);if(it==s->resources.end()||it->second.kind==RV_COMMANDS)return RV_INVALID;
    auto& v=it->second;auto unit=v.kind==RV_VERTEX?sizeof(rv_vertex):v.kind==RV_INDEX?sizeof(uint16_t):size_t(4);
    if(r.offset%unit||capacity%unit||r.offset>v.bytes.size()||capacity>v.bytes.size()-r.offset)return RV_RANGE;
    if(v.kind==RV_VERTEX)for(size_t j=0;j<capacity;j+=unit){rv_vertex x;std::memcpy(&x,static_cast<const uint8_t*>(buffer)+j,unit);if(!std::isfinite(x.x)||!std::isfinite(x.y)||!std::isfinite(x.z)||!std::isfinite(x.u)||!std::isfinite(x.v)||x.z<=0)return RV_INVALID;}
    std::memcpy(v.bytes.data()+r.offset,buffer,capacity);*result=capacity;return RV_OK;
   }
   if(r.handle||r.offset||r.arg1)return RV_INVALID;
   if(r.op==RV_GFX_LOAD_CUSTOM){
    if(r.arg0||!buffer||r.count!=capacity||capacity<32||capacity>49184)return RV_RANGE;
    std::lock_guard lock(s->gfx_mutex);if(!s->runtime.stats().running)return RV_STATE;if(s->packages.size()>=16)return RV_RANGE;
    revenge::package::Programs p;try{p=revenge::package::decode({static_cast<const uint8_t*>(buffer),static_cast<size_t>(capacity)});revenge::package::require_custom(p);}catch(const std::exception&){return RV_INVALID;}
    auto id=next_package();s->packages.emplace(id,std::move(p));*result=id;return RV_OK;
   }
   if(r.op==RV_MEDIA_CAPS){if(r.count||r.arg0||buffer||capacity)return RV_INVALID;*result=RV_MEDIA_STORAGE;
#ifdef _WIN32
    *result|=RV_MEDIA_AUDIO|RV_MEDIA_PAD;
#endif
    return RV_OK;}
   if(r.op==RV_PAD_POLL){if(r.count||r.arg0>3||!buffer||capacity!=sizeof(rv_pad_state))return RV_INVALID;
#ifdef _WIN32
    auto p=revenge::media::poll_pad(static_cast<unsigned>(r.arg0));rv_pad_state state{p.connected,p.buttons,p.packet,0,p.lx,p.ly,p.rx,p.ry,p.lt,p.rt};std::memcpy(buffer,&state,sizeof(state));*result=sizeof(state);return RV_OK;
#else
    return RV_UNSUPPORTED;
#endif
   }
   if(r.op==RV_AUDIO_PLAY){if(r.arg0||!buffer||r.count!=capacity||capacity%4||capacity<3840||capacity>384000)return RV_RANGE;
#ifdef _WIN32
    std::vector<int16_t> pcm(static_cast<size_t>(capacity/2));std::memcpy(pcm.data(),buffer,capacity);revenge::media::play_pcm(pcm);*result=capacity/4;return RV_OK;
#else
    return RV_UNSUPPORTED;
#endif
   }
   if(!buffer||!r.arg0||r.arg0>1024||r.arg0>capacity)return RV_INVALID;
   auto bytes=static_cast<uint8_t*>(buffer);std::string path(reinterpret_cast<const char*>(bytes),static_cast<size_t>(r.arg0));if(path.find('\0')!=std::string::npos)return RV_INVALID;
   auto file=std::filesystem::u8path(path);
   if(r.op==RV_SNAPSHOT_SAVE){if(r.count!=capacity-r.arg0)return RV_RANGE;revenge::media::save(file,{bytes+r.arg0,static_cast<size_t>(r.count)});*result=r.count;return RV_OK;}
   if(r.count)return RV_INVALID;auto data=revenge::media::load(file);if(data.size()>capacity)return RV_RANGE;if(!data.empty())std::memcpy(buffer,data.data(),data.size());*result=data.size();return RV_OK;
  }catch(...){return error();}
 }
 if(r.op>=RV_THREAD_CREATE&&r.op<=RV_THREAD_COUNT){
  const bool create=r.op==RV_THREAD_CREATE, wait=r.op==RV_THREAD_WAIT, count=r.op==RV_THREAD_COUNT;
  if(r.offset||r.count||r.arg1||(!wait&&r.arg0)||((create||count)&&r.handle)||(!create&&(buffer||capacity)))return RV_INVALID;
  if(wait&&r.arg0>10000)return RV_RANGE;
  if(create&&(!buffer||capacity!=sizeof(rv_thread_start)))return RV_INVALID;
  try{std::unique_lock lock(s->thread_mutex);
   if(s->closing||!s->runtime.stats().running)return RV_STATE;
   if(count){*result=s->threads.size();return RV_OK;}
   if(create){
    rv_thread_start start{};std::memcpy(&start,buffer,sizeof(start));if(!start.proc)return RV_INVALID;
    if(s->threads.size()>=16)return RV_RANGE;
    auto id=next_package();auto t=std::make_shared<ManagedThread>();s->threads.emplace(id,t);
    try{t->worker=std::thread([s,t,start]{
     uint32_t state=RV_THREAD_COMPLETE, code=0;
     try{code=start.proc(s,start.argument);}catch(...){state=RV_THREAD_FAILED;}
     try{std::lock_guard tls_lock(s->tls_mutex);auto key=tls_identity();for(auto& e:s->tls_slots)e.second.erase(key);}catch(...){state=RV_THREAD_FAILED;}
     {std::lock_guard guard(s->thread_mutex);t->code=code;t->state=state;s->thread_cv.notify_all();}
    });}catch(...){s->threads.erase(id);throw;}
    *result=id;return RV_OK;
   }
   auto it=s->threads.find(r.handle);if(it==s->threads.end())return RV_INVALID;auto t=it->second;
   if(wait){if(t->worker.get_id()==std::this_thread::get_id())return RV_STATE;s->thread_cv.wait_for(lock,std::chrono::milliseconds(r.arg0),[&]{return t->state!=RV_THREAD_RUNNING;});*result=t->state;return RV_OK;}
   if(r.op==RV_THREAD_POLL){*result=t->state;return RV_OK;}
   if(t->state==RV_THREAD_RUNNING)return RV_STATE;
   if(r.op==RV_THREAD_EXIT){if(t->state==RV_THREAD_FAILED)return RV_FAILURE;*result=t->code;return RV_OK;}
   // Terminal state is published after callback and TLS cleanup; worker takes no further locks.
   t->worker.join();s->threads.erase(it);return RV_OK;
  }catch(...){return error();}
 }
 if(r.op>=RV_JOB_POLL&&r.op<=RV_JOB_COUNT){
  if(r.offset||r.count||r.arg1||(r.op!=RV_JOB_WAIT&&r.arg0)||(r.op!=RV_JOB_READ&&(buffer||capacity)))return RV_INVALID;
  if(r.op==RV_JOB_WAIT&&r.arg0>10000)return RV_RANGE;
  try{auto& q=s->queue;std::unique_lock lock(q.mutex);
   if(r.op==RV_JOB_COUNT){if(r.handle)return RV_INVALID;*result=q.jobs.size();return RV_OK;}
   auto it=q.jobs.find(r.handle);if(it==q.jobs.end())return RV_INVALID;auto j=it->second;
   auto terminal=[&]{return j->state==RV_JOB_COMPLETE||j->state==RV_JOB_FAILED;};
   if(r.op==RV_JOB_WAIT){q.cv.wait_for(lock,std::chrono::milliseconds(r.arg0),terminal);*result=j->state;return RV_OK;}
   if(r.op==RV_JOB_POLL){*result=j->state;return RV_OK;}
   if(!terminal())return RV_STATE;
   if(r.op==RV_JOB_RELEASE){q.jobs.erase(it);return RV_OK;}
   if(j->state==RV_JOB_FAILED)return RV_FAILURE;
   if(j->validate_only){if(buffer||capacity)return RV_INVALID;*result=j->draws;return RV_OK;}
   if(!buffer||capacity!=j->scene.framebuffer.size())return RV_RANGE;
   std::memcpy(buffer,j->scene.framebuffer.data(),capacity);*result=capacity;return RV_OK;
  }catch(...){return error();}
 }
 if(r.op>=RV_RES_CREATE&&r.op<=RV_JOB_SUBMIT){
  try{std::lock_guard lock(s->gfx_mutex);
   if(r.op==RV_RES_COUNT){if(r.handle||r.offset||r.count||r.arg0||r.arg1||buffer||capacity)return RV_INVALID;*result=s->resources.size();return RV_OK;}
   if(!s->runtime.stats().running)return RV_STATE;
   if(r.op==RV_RES_CREATE){
    if(r.handle||!buffer||r.count!=capacity||!capacity)return RV_INVALID;
    if(s->resources.size()>=64||capacity>8u*1024u*1024u-s->resource_bytes)return RV_RANGE;
    if(r.arg0<RV_VERTEX||r.arg0>RV_COMMANDS)return RV_INVALID;
    if(r.arg0!=RV_TEXTURE&&(r.arg1||r.offset))return RV_INVALID;
    if(r.arg0==RV_VERTEX){if(capacity%sizeof(rv_vertex)||capacity/sizeof(rv_vertex)>65536)return RV_RANGE;
      for(size_t j=0;j<capacity;j+=sizeof(rv_vertex)){rv_vertex v;std::memcpy(&v,static_cast<const uint8_t*>(buffer)+j,sizeof(v));if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||!std::isfinite(v.u)||!std::isfinite(v.v)||v.z<=0)return RV_INVALID;}}
    if(r.arg0==RV_INDEX&&capacity%sizeof(uint16_t))return RV_RANGE;
    if(r.arg0==RV_TEXTURE&&(r.arg1<1||r.arg1>512||r.offset<1||r.offset>512||r.arg1*r.offset*4!=capacity))return RV_RANGE;
    if(r.arg0==RV_COMMANDS){if(capacity<sizeof(rv_draw_header))return RV_RANGE;rv_draw_header h;std::memcpy(&h,buffer,sizeof(h));if(h.reserved||!h.draw_count||h.draw_count>1024||capacity!=sizeof(h)+size_t(h.draw_count)*sizeof(rv_indexed_draw))return RV_INVALID;}
    GraphicsResource v{r.arg0,static_cast<unsigned>(r.arg1),static_cast<unsigned>(r.offset),std::vector<uint8_t>(static_cast<const uint8_t*>(buffer),static_cast<const uint8_t*>(buffer)+capacity)};
    auto id=next_package();s->resources.emplace(id,std::move(v));s->resource_bytes+=capacity;*result=id;return RV_OK;
   }
   if(r.op==RV_RES_UPDATE){
    if(r.arg0||r.arg1||r.offset||!buffer||r.count!=capacity)return RV_INVALID;
    auto i=s->resources.find(r.handle);if(i==s->resources.end()||i->second.kind==RV_COMMANDS)return RV_INVALID;
    if(capacity!=i->second.bytes.size())return RV_RANGE;
    if(i->second.kind==RV_VERTEX){for(size_t j=0;j<capacity;j+=sizeof(rv_vertex)){rv_vertex v;std::memcpy(&v,static_cast<const uint8_t*>(buffer)+j,sizeof(v));if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||!std::isfinite(v.u)||!std::isfinite(v.v)||v.z<=0)return RV_INVALID;}}
    std::memcpy(i->second.bytes.data(),buffer,capacity);*result=capacity;return RV_OK;
   }
   if(r.count||(r.op!=RV_JOB_SUBMIT&&r.offset))return RV_INVALID;
   auto it=s->resources.find(r.handle);if(it==s->resources.end())return RV_INVALID;
   if(r.op==RV_RES_RELEASE){if(r.arg0||r.arg1||buffer||capacity)return RV_INVALID;s->resource_bytes-=it->second.bytes.size();s->resources.erase(it);return RV_OK;}
   if(it->second.kind!=RV_COMMANDS)return RV_INVALID;
   if(r.op==RV_CMD_VALIDATE){if(r.arg0||r.arg1||buffer||capacity)return RV_INVALID;}
   else if(r.op==RV_JOB_SUBMIT){if(buffer||capacity||r.offset>2)return RV_INVALID;if(r.arg0<320||r.arg0>512||r.arg1<240||r.arg1>512)return RV_RANGE;if(s->queue.full())return RV_RANGE;}
   else if(r.arg0<320||r.arg0>512||r.arg1<240||r.arg1>512||!buffer||capacity!=r.arg0*r.arg1*4)return RV_RANGE;
   rv_draw_header h;std::memcpy(&h,it->second.bytes.data(),sizeof(h));
   auto get=[&](uint64_t id,uint64_t kind)->const GraphicsResource&{auto i=s->resources.find(id);if(i==s->resources.end()||i->second.kind!=kind)throw std::invalid_argument("invalid command resource");return i->second;};
   const auto& v=get(h.vertex,RV_VERTEX);const auto& ix=get(h.index,RV_INDEX);const auto& t=get(h.texture,RV_TEXTURE);auto p=s->packages.find(h.package);if(p==s->packages.end())return RV_INVALID;
   revenge::scene3d::CustomScene scene;scene.mesh.vertices.resize(v.bytes.size()/sizeof(rv_vertex));scene.mesh.indices.resize(ix.bytes.size()/sizeof(uint16_t));std::memcpy(scene.mesh.vertices.data(),v.bytes.data(),v.bytes.size());std::memcpy(scene.mesh.indices.data(),ix.bytes.data(),ix.bytes.size());
   scene.stream={{revenge::scene3d::Op::begin},{revenge::scene3d::Op::bind_mesh},{revenge::scene3d::Op::bind_texture}};
   for(unsigned j=0;j<h.draw_count;++j){rv_indexed_draw draw;std::memcpy(&draw,it->second.bytes.data()+sizeof(h)+size_t(j)*sizeof(draw),sizeof(draw));scene.stream.push_back({revenge::scene3d::Op::draw_indexed,draw.first,draw.count});}scene.stream.push_back({revenge::scene3d::Op::end});
   revenge::scene3d::validate(scene.mesh,scene.stream);
   if(r.op==RV_CMD_VALIDATE){*result=h.draw_count;return RV_OK;}
   if(r.op==RV_JOB_SUBMIT){
#ifndef _WIN32
    if(r.offset!=1)return RV_UNSUPPORTED;
#endif
    scene.texture_width=t.width;scene.texture_height=t.height;scene.rgba=t.bytes;scene.quiet=r.offset==2;
    auto j=std::make_shared<Job>();j->w=static_cast<unsigned>(r.arg0);j->h=static_cast<unsigned>(r.arg1);j->validate_only=r.offset==1;j->draws=h.draw_count;j->scene=std::move(scene);j->program=p->second;
    auto id=next_package();s->queue.submit(id,std::move(j));*result=id;return RV_OK;
   }
#ifdef _WIN32
   scene.texture_width=t.width;scene.texture_height=t.height;scene.rgba=t.bytes;
   std::lock_guard gpu_lock(gpu_mutex);if(revenge::scene3d::gpu_test(static_cast<unsigned>(r.arg0),static_cast<unsigned>(r.arg1),{},false,true,&p->second,{},0,nullptr,nullptr,&scene)||scene.framebuffer.size()!=capacity)return RV_FAILURE;
   std::memcpy(buffer,scene.framebuffer.data(),scene.framebuffer.size());*result=capacity;return RV_OK;
#else
   (void)t;return RV_UNSUPPORTED;
#endif
  }catch(...){return error();}
 }
 if(r.op==RV_GFX_RUN){
  if(!buffer||capacity!=sizeof(rv_frame_stats))return RV_RANGE;
  if(r.arg0<320||r.arg0>3840||r.arg1<240||r.arg1>2160||r.count<1||r.count>30||r.offset>240)return RV_RANGE;
  try{std::lock_guard lock(s->gfx_mutex);if(!s->runtime.stats().running)return RV_STATE;
   auto i=s->packages.find(r.handle);if(i==s->packages.end())return RV_INVALID;if(i->second!=revenge::package::fixture())return RV_INVALID;
#ifdef _WIN32
   std::lock_guard gpu_lock(gpu_mutex);revenge::scene3d::Continuous c{};c.seconds=static_cast<unsigned>(r.count);c.fps_cap=static_cast<unsigned>(r.offset);
   if(revenge::scene3d::gpu_test(static_cast<unsigned>(r.arg0),static_cast<unsigned>(r.arg1),{},true,true,&i->second,{},0,nullptr,&c))return RV_FAILURE;
   rv_frame_stats stats{c.submitted,c.presented,c.elapsed_ns,c.gpu_ns,c.first_pixels,c.last_pixels,c.last_shift};std::memcpy(buffer,&stats,sizeof(stats));*result=sizeof(stats);return RV_OK;
#else
   return RV_UNSUPPORTED;
#endif
  }catch(...){return error();}
 }
 if(r.op>=RV_GFX_CAPS&&r.op<=RV_GFX_COUNT){
  const bool load=r.op==RV_GFX_LOAD, draw=r.op==RV_GFX_DRAW, release=r.op==RV_GFX_RELEASE;
  if((!draw&&!release&&r.handle)||r.offset||(!load&&!draw&&r.count)||(!draw&&(r.arg0||r.arg1))||(!load&&(buffer||capacity)))return RV_INVALID;
  if(load&&(r.count<32||r.count>49184||r.count!=capacity))return RV_RANGE;
  if(draw&&(r.arg0<320||r.arg0>3840||r.arg1<240||r.arg1>2160||r.count>30))return RV_RANGE;
  try{std::lock_guard lock(s->gfx_mutex);
   if(r.op==RV_GFX_COUNT){*result=s->packages.size();return RV_OK;}
   if(!s->runtime.stats().running)return RV_STATE;
   if(r.op==RV_GFX_CAPS){
    *result=RV_GFX_ASYNC;
#ifdef _WIN32
    *result|=RV_GFX_GPU|RV_GFX_PRESENT|RV_GFX_CONTINUOUS|RV_GFX_CUSTOM|RV_GFX_MULTIPASS;
#endif
    return RV_OK;
   }
   if(load){if(s->packages.size()>=16)return RV_RANGE;revenge::package::Programs p;try{p=revenge::package::decode({static_cast<const uint8_t*>(buffer),static_cast<size_t>(capacity)});revenge::package::require_scene_fixture(p);}catch(const std::exception&){return RV_INVALID;}auto id=next_package();s->packages.emplace(id,std::move(p));*result=id;return RV_OK;}
   auto i=s->packages.find(r.handle);if(i==s->packages.end())return RV_INVALID;
   if(release){s->packages.erase(i);return RV_OK;}
   if(i->second!=revenge::package::fixture())return RV_INVALID;
#ifdef _WIN32
   std::lock_guard gpu_lock(gpu_mutex);size_t verified=0;
   if(revenge::scene3d::gpu_test(static_cast<unsigned>(r.arg0),static_cast<unsigned>(r.arg1),{},true,true,&i->second,{},static_cast<unsigned>(r.count),&verified)||!verified)return RV_FAILURE;
   *result=verified;return RV_OK;
#else
   return RV_UNSUPPORTED;
#endif
  }catch(...){*result=0;return error();}
 }
 if(r.op>=RV_TLS_CREATE&&r.op<=RV_TLS_COUNT){
  if(r.offset||r.count||r.arg1||buffer||capacity||(r.op!=RV_TLS_SET&&r.arg0)||((r.op==RV_TLS_CREATE||r.op==RV_TLS_DETACH||r.op==RV_TLS_COUNT)&&r.handle))return RV_INVALID;
  try{std::lock_guard lock(s->tls_mutex);if(!s->runtime.stats().running)return RV_STATE;
   auto& slots=s->tls_slots;const auto thread_key=tls_identity();
   if(r.op==RV_TLS_CREATE){if(slots.size()>=64)return RV_RANGE;auto id=next_package();slots.emplace(id,std::map<uint64_t,uint64_t>{});*result=id;return RV_OK;}
   if(r.op==RV_TLS_COUNT){*result=slots.size();return RV_OK;}
   if(r.op==RV_TLS_DETACH){for(auto& entry:slots)entry.second.erase(thread_key);return RV_OK;}
   auto it=slots.find(r.handle);if(it==slots.end())return RV_INVALID;
   if(r.op==RV_TLS_RELEASE){slots.erase(it);return RV_OK;}
   auto& values=it->second;auto v=values.find(thread_key);
   if(r.op==RV_TLS_GET){if(v!=values.end())*result=v->second;return RV_OK;}
   if(!r.arg0){values.erase(thread_key);return RV_OK;}
   if(v==values.end()&&values.size()>=128)return RV_RANGE;
   values[thread_key]=r.arg0;return RV_OK;
  }catch(...){*result=0;return error();}
 }
 if(r.op>=RV_MUTEX_CREATE&&r.op<=RV_SYNC_COUNT){
  try{auto& rt=s->runtime;const a::Handle h{r.handle};
   if(r.op>=RV_STORE_PUT&&r.op<=RV_STORE_ERASE){
    if(r.handle||r.offset||r.arg1||!buffer||!r.arg0||r.arg0>64||r.arg0>capacity)return RV_INVALID;
    if(r.op==RV_STORE_PUT){if(r.count!=capacity-r.arg0)return RV_RANGE;}
    else if(r.count||(r.op!=RV_STORE_GET&&capacity!=r.arg0))return RV_INVALID;
    auto bytes=static_cast<uint8_t*>(buffer);std::string key(reinterpret_cast<const char*>(bytes),static_cast<size_t>(r.arg0));
    if(r.op==RV_STORE_PUT){rt.dispatch(a::Store{key,a::Bytes(bytes+r.arg0,bytes+capacity)});*result=r.count;}
    else if(r.op==RV_STORE_ERASE)rt.dispatch(a::Erase{key});
    else {auto value=std::get<a::Bytes>(rt.dispatch(a::Load{key}));if(r.op==RV_STORE_GET){if(value.size()>capacity)return RV_RANGE;if(!value.empty())std::memcpy(buffer,value.data(),value.size());}*result=value.size();}
    return RV_OK;
   }
   bool create=r.op==RV_MUTEX_CREATE||r.op==RV_SEM_CREATE||r.op==RV_SYNC_COUNT;
   bool arg=r.op==RV_MUTEX_LOCK||r.op==RV_SEM_WAIT||r.op==RV_SEM_RELEASE||r.op==RV_SEM_CREATE;
   if(r.offset||r.count||buffer||capacity||(create&&r.handle)||(!arg&&r.arg0)||(r.op!=RV_SEM_CREATE&&r.arg1))return RV_INVALID;
   if((r.op==RV_MUTEX_LOCK||r.op==RV_SEM_WAIT)&&r.arg0>10000)return RV_RANGE;
   if((r.op==RV_SEM_CREATE||r.op==RV_SEM_RELEASE)&&(r.arg0>65535||r.arg1>65535))return RV_RANGE;
   switch(r.op){
    case RV_MUTEX_CREATE:*result=std::get<a::Handle>(rt.dispatch(a::MutexCreate{})).id;break;
    case RV_MUTEX_LOCK:*result=std::get<bool>(rt.dispatch(a::MutexLock{h,static_cast<unsigned>(r.arg0)}));break;
    case RV_MUTEX_UNLOCK:rt.dispatch(a::MutexUnlock{h});break;
    case RV_MUTEX_CLOSE:rt.dispatch(a::MutexClose{h});break;
    case RV_SEM_CREATE:*result=std::get<a::Handle>(rt.dispatch(a::SemaphoreCreate{static_cast<unsigned>(r.arg0),static_cast<unsigned>(r.arg1)})).id;break;
    case RV_SEM_WAIT:*result=std::get<bool>(rt.dispatch(a::SemaphoreWait{h,static_cast<unsigned>(r.arg0)}));break;
    case RV_SEM_RELEASE:*result=std::get<uint64_t>(rt.dispatch(a::SemaphoreRelease{h,static_cast<unsigned>(r.arg0)}));break;
    case RV_SEM_CLOSE:rt.dispatch(a::SemaphoreClose{h});break;
    case RV_SYNC_COUNT:*result=std::get<uint64_t>(rt.dispatch(a::SyncCount{}));break;
    default:return RV_UNSUPPORTED;
   }return RV_OK;
  }catch(...){*result=0;return error();}
 }
 const bool data=r.op==RV_READ||r.op==RV_WRITE;
 const bool uses_handle=data||r.op==RV_FREE||(r.op>=RV_EVENT_SET&&r.op<=RV_EVENT_CLOSE);
 if((!uses_handle&&r.handle)||(!data&&r.offset)||(!data&&r.op!=RV_ALLOC&&r.count)||((r.op!=RV_EVENT_WAIT&&r.op!=RV_RENDER_VALIDATE)&&r.arg0)||(r.op!=RV_RENDER_VALIDATE&&r.arg1))return RV_INVALID;
 if(!data&&r.op!=RV_STATS&&(buffer||capacity))return RV_INVALID;
 if(data&&(r.count>capacity||r.count>RV_MAX_TRANSFER||r.offset>8u*1024u*1024u))return RV_RANGE;
 auto& rt=s->runtime;const a::Handle h{r.handle};
 try{switch(r.op){
 case RV_ALLOC: if(!r.count||r.count>8u*1024u*1024u)return RV_RANGE;*result=std::get<a::Handle>(rt.dispatch(a::Allocate{static_cast<size_t>(r.count)})).id;break;
 case RV_FREE:rt.dispatch(a::Free{h});break;
 case RV_READ:{auto b=std::get<a::Bytes>(rt.dispatch(a::Read{h,static_cast<size_t>(r.offset),static_cast<size_t>(r.count)}));if(!b.empty())std::memcpy(buffer,b.data(),b.size());*result=b.size();break;}
 case RV_WRITE:{a::Bytes b(static_cast<size_t>(r.count));if(!b.empty())std::memcpy(b.data(),buffer,b.size());rt.dispatch(a::Write{h,static_cast<size_t>(r.offset),std::move(b)});*result=r.count;break;}
 case RV_EVENT_CREATE:*result=std::get<a::Handle>(rt.dispatch(a::EventCreate{})).id;break;
 case RV_EVENT_SET:rt.dispatch(a::EventSet{h});break;
 case RV_EVENT_RESET:rt.dispatch(a::EventReset{h});break;
 case RV_EVENT_WAIT:if(r.arg0>10000)return RV_RANGE;*result=std::get<bool>(rt.dispatch(a::EventWait{h,static_cast<unsigned>(r.arg0)}));break;
 case RV_EVENT_CLOSE:rt.dispatch(a::EventClose{h});break;
 case RV_CLOCK:*result=std::get<uint64_t>(rt.dispatch(a::Clock{}));break;
 case RV_RENDER_VALIDATE:if(r.arg0>3840||r.arg1>2160)return RV_RANGE;rt.dispatch(a::Render{static_cast<unsigned>(r.arg0),static_cast<unsigned>(r.arg1)});break;
 case RV_STATS:{if(capacity<sizeof(rv_stats))return RV_RANGE;auto v=rt.stats();rv_stats stats{v.memory_handles,v.events,v.files,v.storage_bytes,v.committed,v.running?1u:0u};std::memcpy(buffer,&stats,sizeof(stats));*result=sizeof(stats);break;}
 case RV_STOP:{std::lock_guard thread_lock(s->thread_mutex);if(s->closing||!s->threads.empty())return RV_STATE;std::lock_guard lock(s->gfx_mutex);{std::lock_guard tls_lock(s->tls_mutex);rt.dispatch(a::Stop{});s->tls_slots.clear();}s->queue.shutdown();
#ifdef _WIN32
 {std::lock_guard gpu_lock(gpu_mutex);s->pass_cache.reset();}
#endif
 s->packages.clear();s->resources.clear();s->resource_bytes=0;break;}
 default:return RV_UNSUPPORTED;
 }return RV_OK;}catch(...){*result=0;return error();}
}
}
