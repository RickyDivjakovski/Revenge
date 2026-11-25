#include "revenge/presentation.hpp"
#include <stdexcept>
#include <cmath>
#include <iostream>
using namespace revenge::presentation;
void require(bool ok){if(!ok)throw std::runtime_error("presentation test failed");}
template<class F>void rejects(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}require(failed);}
int main(){try{rejects([]{BackBuffers b(1);});BackBuffers b(3);b.acquire(2,0);rejects([&]{b.acquire(0,0);});require(b.submit()==1);b.acquire(0,0);require(b.submit()==2);rejects([&]{b.acquire(2,0);});rejects([&]{b.resized(1);});b.acquire(2,1);require(b.submit()==3);rejects([&]{b.acquire(1,0);});rejects([&]{b.acquire(3,3);});b.resized(3);require(b.required(2)==0);for(unsigned i=0;i<12000;++i){unsigned index=(i*7+i/5)%3;b.acquire(index,b.last());require(b.submit()==i+4);if(i%101==0)b.resized(b.last());}auto x=scene(0,1280,720);require(x[0]==1&&x[1]==0&&x[2]==.5625f&&x[3]==1);auto y=scene(1,720,1280);require(y[2]==1&&y[3]==.5625f);require(std::abs(y[0]*y[0]+y[1]*y[1]-1)<1e-6);rejects([]{scene(0,0,720);});rejects([]{scene(-1,720,720);});std::cout<<"12000 back-buffer lifecycle iterations passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
