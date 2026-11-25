#include "revenge/transfers.hpp"
#include <iostream>
#include <stdexcept>
using namespace revenge::transfer;
static void require(bool b){if(!b)throw std::runtime_error("transfer test failed");}
template<class F>void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}require(caught);}
int main(){try{
 rejects([]{Pool p(0,4096,4096);});rejects([]{Pool p(2,4096,24575);});rejects([]{Pool p(8,UINT64_MAX,UINT64_MAX);});
 Pool p(2,4096,24576);require(p.live_bytes()==24576);rejects([&]{p.begin(2);});rejects([&]{p.upload(0);});p.begin(0);rejects([&]{p.begin(0);});rejects([&]{p.download(0);});p.upload(0);rejects([&]{p.submit(0);});p.download(0);require(p.submit(0,true)==1);rejects([&]{p.consume(0);});rejects([&]{p.complete(2);});p.complete(1);require(p.consume(0));require(p.live_bytes()==12288);rejects([&]{p.begin(0);});rejects([&]{p.consume(0);});rejects([&]{p.complete(0);});
 for(unsigned n=1;n<=8;++n){Pool ring(n,4096,n*12288);for(unsigned i=0;i<1000;++i){auto j=i%n;if(ring.slot(j).phase==Phase::submitted){ring.complete(ring.slot(j).fence);require(!ring.consume(j));}ring.begin(j);ring.upload(j);ring.download(j);require(ring.submit(j,i+n>=1000)==i+1);}ring.complete(1000);for(unsigned j=0;j<n;++j)require(ring.consume(j));require(ring.live_bytes()==0);}
 std::vector<uint8_t> b(8193);pattern(b,1);require(matches(b,1));require(!matches(b,2));b.back()^=1;require(!matches(b,1));pattern(b,2);require(matches(b,2));
 std::cout<<"Transfer pool: 8000 simulated submissions, lifetime and pattern checks passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
