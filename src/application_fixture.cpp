#include "revenge/shader_package.hpp"
#include "revenge/application.hpp"
#include "revenge/shader_ir.hpp"
#include "revenge/scene.hpp"
#include <exception>
#include <ostream>
#include <stdexcept>
#include <thread>
namespace revenge::app {
namespace {
void put(Bytes& b,uint32_t n){for(unsigned i=0;i<4;++i)b.push_back(static_cast<uint8_t>(n>>(8*i)));}
uint32_t get(const Bytes& b,size_t i){uint32_t n=0;for(unsigned j=0;j<4;++j)n|=uint32_t(b.at(i+j))<<(8*j);return n;}
}
int fixture(std::ostream& out,unsigned w,unsigned h,Renderer renderer,const package::Programs* programs){
 if(programs)package::require_scene_fixture(*programs);
 if(w<320||w>3840||h<240||h>2160)throw std::invalid_argument("invalid app extent");
 bool gpu=bool(renderer);unsigned renders=0;
 Runtime runtime([&](unsigned x,unsigned y){if(x!=w||y!=h)throw std::runtime_error("application config mismatch");if(renderer)renderer(x,y);else scene3d::validate(scene3d::geometry(),scene3d::commands());++renders;});
 runtime.dispatch(Start{});auto t0=std::get<uint64_t>(runtime.dispatch(Clock{}));
 Bytes config;put(config,w);put(config,h);runtime.dispatch(Store{"scene.cfg",config});
 auto memory=std::get<Handle>(runtime.dispatch(Allocate{4096*4}));auto event=std::get<Handle>(runtime.dispatch(EventCreate{}));
 Bytes input;input.reserve(4096*4);for(unsigned i=0;i<4096;++i)put(input,i);runtime.dispatch(Write{memory,0,std::move(input)});
 std::exception_ptr failure;
 std::jthread worker([&]{try{auto data=std::get<Bytes>(runtime.dispatch(Read{memory,0,4096*4}));auto p=shader::optimize(shader::decode(programs?programs->compute:shader::fixture_words()));Bytes result;result.reserve(data.size());for(size_t i=0;i<data.size();i+=4)put(result,shader::evaluate(p,get(data,i)));runtime.dispatch(Write{memory,0,std::move(result)});}catch(...){failure=std::current_exception();}try{runtime.dispatch(EventSet{event});}catch(...){if(!failure)failure=std::current_exception();}});
 auto signaled=std::get<bool>(runtime.dispatch(EventWait{event,10000}));worker.join();if(failure)std::rethrow_exception(failure);if(!signaled)throw std::runtime_error("application worker timed out");
 auto result=std::get<Bytes>(runtime.dispatch(Read{memory,0,4096*4}));for(unsigned i=0;i<4096;++i)if(get(result,i*4)!=3*i+1)throw std::runtime_error("application shader result mismatch");
 runtime.dispatch(Store{"result.bin",result});if(std::get<Bytes>(runtime.dispatch(Load{"result.bin"}))!=result)throw std::runtime_error("application storage mismatch");
 auto loaded=std::get<Bytes>(runtime.dispatch(Load{"scene.cfg"}));
 if(programs){auto encoded=package::encode(*programs);runtime.dispatch(Store{"scene.rsp",encoded});auto recovered=package::decode(std::get<Bytes>(runtime.dispatch(Load{"scene.rsp"})));if(recovered!=*programs)throw std::runtime_error("package service roundtrip mismatch");runtime.dispatch(Erase{"scene.rsp"});out<<"Application shader package round-trip and three stage validation completed.\n";}
 out<<"Application worker: verified 4096 translated CPU ALU results; event signaled; storage round-trip verified (16384 bytes).\n";
 runtime.dispatch(Render{get(loaded,0),get(loaded,4)});if(renders!=1)throw std::runtime_error("application render not dispatched");
 runtime.dispatch(Free{memory});runtime.dispatch(EventClose{event});runtime.dispatch(Erase{"scene.cfg"});runtime.dispatch(Erase{"result.bin"});
 auto t1=std::get<uint64_t>(runtime.dispatch(Clock{}));runtime.dispatch(Stop{});auto s=runtime.stats();if(s.running||s.memory_handles||s.events||s.files||s.storage_bytes||s.committed||t1<t0)throw std::runtime_error("application shutdown leak");
 out<<"Application lifecycle: started, worker joined, graphics "<<(gpu?"GPU fixture":"CPU command validation")<<" completed, stopped.\nRemaining memory handles: 0; events: 0; storage entries: 0; committed bytes: 0.\nElapsed service time ms: "<<double(t1-t0)/1e6<<"\nIndependent in-process fixture; no Xbox ABI or gameplay.\n";return 0;
}
}
