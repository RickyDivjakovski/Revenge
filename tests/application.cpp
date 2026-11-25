#include "revenge/application.hpp"
#include <atomic>
#include <iostream>
#include <stdexcept>
#include <thread>
using namespace revenge::app;
void require(bool b){if(!b)throw std::runtime_error("application test failure");}
template<class F> void rejects(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}require(bad);}
int main(){try{
 Runtime a,b;rejects([&]{a.dispatch(Allocate{1});});a.dispatch(Start{});b.dispatch(Start{});rejects([&]{a.dispatch(Start{});});
 auto m=std::get<Handle>(a.dispatch(Allocate{3}));require(std::get<Bytes>(a.dispatch(Read{m,0,3}))==Bytes(3));a.dispatch(Write{m,0,{1,2,3}});require(std::get<Bytes>(a.dispatch(Read{m,1,2}))==Bytes({2,3}));rejects([&]{a.dispatch(Read{m,3,1});});rejects([&]{b.dispatch(Read{m,0,1});});rejects([&]{a.dispatch(EventSet{m});});a.dispatch(Free{m});rejects([&]{a.dispatch(Free{m});});
 auto e=std::get<Handle>(a.dispatch(EventCreate{}));require(!std::get<bool>(a.dispatch(EventWait{e,0})));a.dispatch(EventSet{e});require(std::get<bool>(a.dispatch(EventWait{e,0})));a.dispatch(EventReset{e});require(!std::get<bool>(a.dispatch(EventWait{e,1})));std::atomic<bool> ok=false;std::jthread waiter([&]{try{ok=std::get<bool>(a.dispatch(EventWait{e,1000}));}catch(...){}});a.dispatch(EventSet{e});waiter.join();require(ok);a.dispatch(EventClose{e});rejects([&]{a.dispatch(EventWait{e,0});});
 for(unsigned i=0;i<2000;++i){auto h=std::get<Handle>(a.dispatch(Allocate{17}));a.dispatch(Write{h,16,{uint8_t(i)}});require(std::get<Bytes>(a.dispatch(Read{h,16,1}))==Bytes({uint8_t(i)}));a.dispatch(Free{h});}
 a.dispatch(Store{"config",{1,2,3}});a.dispatch(Store{"config",{9}});require(a.stats().storage_bytes==1&&std::get<Bytes>(a.dispatch(Load{"config"}))==Bytes({9}));rejects([&]{a.dispatch(Store{"../escape",{1}});});rejects([&]{a.dispatch(Store{"huge",Bytes(1u<<20)});});require(a.stats().storage_bytes==1);rejects([&]{a.dispatch(Render{1280,720});});
 auto big=std::get<Handle>(a.dispatch(Allocate{8u<<20}));rejects([&]{a.dispatch(Allocate{1});});a.dispatch(Free{big});
 a.dispatch(Allocate{4});a.dispatch(EventCreate{});a.dispatch(Stop{});auto s=a.stats();require(!s.running&&!s.memory_handles&&!s.events&&!s.files&&!s.storage_bytes&&!s.committed);rejects([&]{a.dispatch(Start{});});rejects([&]{a.dispatch(Clock{});});b.dispatch(Stop{});
 unsigned calls=0;Runtime r([&](unsigned w,unsigned h){require(w==1280&&h==720);++calls;});r.dispatch(Start{});rejects([&]{r.dispatch(Render{1,1});});r.dispatch(Render{1280,720});r.dispatch(Stop{});require(calls==1);
 std::cout<<"2000 allocation cycles, event waits, storage budgets, lifecycle and graphics dispatch verified\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
