#include "revenge/scene.hpp"
#include <stdexcept>
#include <iostream>
#include <algorithm>
using namespace revenge::scene3d;
void require(bool b){if(!b)throw std::runtime_error("scene test failed");}
template<class F>void rejects(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}require(bad);}
int main(){try{auto g=geometry();auto c=commands();validate(g,c);auto bad=c;bad.erase(bad.begin()+1);rejects([&]{validate(g,bad);});bad=c;bad[3].count=15;rejects([&]{validate(g,bad);});bad=c;bad.push_back({Op::begin});rejects([&]{validate(g,bad);});bad=c;bad.pop_back();rejects([&]{validate(g,bad);});auto wrong=g;wrong.indices[0]=99;rejects([&]{validate(wrong,c);});
 size_t total=0;for(auto w:{64u,127u,320u,1920u}){unsigned h=w/2+7;size_t pitch=w*4+16;std::vector<uint8_t> data(pitch*h);for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){Pixel p;if(expected(x,y,w,h,p))std::copy(p.begin(),p.end(),data.begin()+y*pitch+x*4);}total+=verify(data,w,h,pitch);data[0]^=1;rejects([&]{verify(data,w,h,pitch);});rejects([&]{verify(std::span<const uint8_t>(data).first(10),w,h,pitch);});}
 Pixel p;require(expected(30,30,100,100,p)&&p==palette[0]);require(expected(70,70,100,100,p)&&p==palette[3]);require(expected(50,15,100,100,p)&&p==palette[3]);require(expected(5,5,100,100,p)&&p==Pixel{0,0,0,255});std::cout<<total<<" oracle interior pixels checked; malformed commands rejected\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
