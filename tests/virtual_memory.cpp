#include "revenge/virtual_memory.hpp"
#include <array>
#include <atomic>
#include <iostream>
#include <random>
#include <stdexcept>
#include <thread>
#include <vector>
using namespace revenge::vm;
void require(bool b){if(!b)throw std::runtime_error("virtual memory test failed");}
template<class F>void rejects(F f){bool ok=false;try{f();}catch(const std::exception&){ok=true;}require(ok);}
int main(){
 Arena probe(1ull<<20,1ull<<20);auto p=probe.page_size();Arena a(p*64,p*16);
 auto h=a.reserve(p*3, p*4);require(a.aligned(h,p*4));require(a.page_access(h,0)==Access::reserved);std::array<uint8_t,4> bytes{1,2,3,4},got{};
 rejects([&]{a.read(h,0,got);});rejects([&]{a.commit(h,1,p);});rejects([&]{a.protect(h,0,p,Access::read_only);});a.commit(h,0,p*2);require(a.stats().committed==p*2);a.read(h,p-2,got);require(got==std::array<uint8_t,4>{});a.write(h,p-2,bytes);a.read(h,p-2,got);require(got==bytes);
 a.protect(h,p,p,Access::read_only);rejects([&]{a.write(h,p-2,std::array<uint8_t,4>{9,9,9,9});});a.read(h,p-2,got);require(got==bytes);a.protect(h,p,p,Access::no_access);rejects([&]{a.read(h,p-2,got);});a.protect(h,p,p,Access::read_write);a.read(h,p-2,got);require(got==bytes);
 rejects([&]{a.commit(h,0,p);});rejects([&]{a.decommit(h,p,p*2);});a.decommit(h,p,p);a.commit(h,p,p);a.read(h,p-2,got);require((got==std::array<uint8_t,4>{1,2,0,0}));a.release(h);require(a.stats().committed==0);rejects([&]{a.read(h,0,got);});auto again=a.reserve(p*3,p*4);require(again.offset==h.offset&&again.generation!=h.generation);rejects([&]{a.release(h);});a.release(again);
 auto local=a.reserve(p);auto foreign=probe.reserve(p);rejects([&]{a.release(foreign);});rejects([&]{probe.release(local);});a.release(local);probe.release(foreign);
 auto big=a.reserve(p*20);rejects([&]{a.commit(big,0,p*17);});require(a.stats().committed==0);a.commit(big,0,p*16);a.retire(big,7);rejects([&]{a.release(big);});require(a.collect(6)==0&&a.stats().committed==p*16);require(a.collect(7)==1&&a.stats().committed==0);rejects([&]{a.collect(6);});
 auto x=a.reserve(p*16),y=a.reserve(p*16),z=a.reserve(p*16);a.release(y);require(a.stats().largest_free==p*16);auto middle=a.reserve(p*16);require(middle.offset==y.offset);a.release(x);a.release(z);a.release(middle);require(a.stats().largest_free==p*64);
 rejects([&]{a.reserve(0);});rejects([&]{a.reserve(UINT64_MAX);});rejects([&]{a.reserve(p,3);});rejects([&]{a.reserve(p*65);});
 Arena concurrent(p*64,p*64);std::atomic<unsigned> verified{0};std::atomic<bool> failed{false};std::vector<std::jthread> jobs;
 for(unsigned t=0;t<8;++t)jobs.emplace_back([&,t]{try{std::mt19937 rng(t+100);for(unsigned k=0;k<250;++k){auto n=1+rng()%4;auto v=concurrent.reserve(n*p);concurrent.commit(v,0,n*p);std::vector<uint8_t> data(n*p),out(n*p);for(auto& b:data)b=static_cast<uint8_t>(rng());concurrent.write(v,0,data);concurrent.read(v,0,out);require(data==out);concurrent.release(v);verified+=1;}}catch(...){failed=true;}});jobs.clear();require(!failed&&verified==2000);auto s=concurrent.stats();require(s.reserved==0&&s.committed==0&&s.largest_free==s.capacity&&s.live==0&&s.retired==0);
 std::cout<<"Native reservation, permissions, zero-on-recommit, stale handles, budgets, fragmentation, fence retirement and "<<verified<<" concurrent allocation cycles passed.\n";
}
