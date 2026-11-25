#include "revenge/texture.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <iostream>
#include <random>
#include <stdexcept>
using namespace revenge::textures;
static void require(bool b){if(!b)throw std::runtime_error("texture test failed");}
template<class F> void rejects(F f){bool ok=false;try{f();}catch(const std::exception&){ok=true;}require(ok);}
int main(){try{
 std::array<bool,64> seen{};for(unsigned y=0;y<8;++y)for(unsigned x=0;x<8;++x){auto i=morton8(x,y);require(i<64&&!seen[i]);seen[i]=true;}require(morton8(1,0)==1&&morton8(0,1)==2&&morton8(7,7)==63);rejects([]{morton8(8,0);});
 std::mt19937 rng(1400);size_t checked=0;
 // Independent bit-plane formula for encoding, then compare every useful output byte.
 for(auto fmt:{Format::r8,Format::rgba8,Format::bgra8,Format::bc1,Format::bc3,Format::bc4,Format::bc5})for(unsigned k=0;k<30;++k){Descriptor d{uint32_t(1+rng()%65u),uint32_t(1+rng()%65u),1,2,fmt,Layout::morton8};d.mips=std::max(d.width,d.height)>=4?3:1;auto p=plan(d);std::vector<uint8_t> src(p.source_bytes,0xcc);auto l=d;l.layout=Layout::linear;auto lp=plan(l);std::vector<uint8_t> linear(lp.source_bytes);
 for(size_t j=0;j<p.subresources.size();++j){auto s=p.subresources[j],q=lp.subresources[j];require(s.upload_offset%512==0&&s.row_pitch%256==0);for(uint32_t y=0;y<s.blocks_y;++y)for(uint32_t x=0;x<s.blocks_x;++x){unsigned a=x%8,b=y%8;unsigned mort=(a&1)+2*(b&1)+2*(a&2)+4*(b&2)+4*(a&4)+8*(b&4);size_t ti=((y/8)*((s.blocks_x+7)/8)+x/8)*64+mort;for(unsigned z=0;z<s.block_bytes;++z){auto value=uint8_t(rng());src[s.source_offset+ti*s.block_bytes+z]=value;linear[q.source_offset+y*q.row_bytes+x*q.block_bytes+z]=value;++checked;}}}
 require(convert(d,src)==convert(l,linear));rejects([&]{convert(d,std::span(src).first(src.size()-1));});
 }
 Descriptor d{2,2,1,1,Format::rgba8,Layout::linear};std::array<uint8_t,16> px{255,0,0,255,0,255,0,255,0,0,255,255,255,255,255,255};auto up=convert(d,px);auto a=sample(d,up,0,0,.25f,.25f,Address::clamp,Filter::point);require(a[0]==1&&a[1]==0&&a[2]==0&&a[3]==1);a=sample(d,up,0,0,.5f,.5f,Address::clamp,Filter::linear);require(std::abs(a[0]-.5f)<1e-6f&&std::abs(a[1]-.5f)<1e-6f&&std::abs(a[2]-.5f)<1e-6f);require(sample(d,up,0,0,-.25f,.25f,Address::wrap,Filter::point)[1]==1);require(sample(d,up,0,0,2,.25f,Address::clamp,Filter::point)[1]==1);d.format=Format::bgra8;require(sample(d,up,0,0,.25f,.25f,Address::clamp,Filter::point)[2]==1);
 rejects([]{plan({0,1});});rejects([]{plan({1,1,2});});rejects([]{plan({16384,16384,1,2048});});rejects([&]{sample(d,up,1,0,0,0,Address::clamp,Filter::point);});
 auto src=fixture_input(),expected=fixture_expected();Descriptor f{64,64,1,1,Format::rgba8,Layout::morton8};auto bytes=std::span(reinterpret_cast<const uint8_t*>(src.data()),src.size()*4);auto converted=convert(f,bytes);require(converted.size()==expected.size()*4&&std::memcmp(converted.data(),expected.data(),converted.size())==0);
 require(fixture_hlsl().find("tile*64+m")!=std::string::npos);
 std::cout<<"Texture tests passed; checked "<<checked<<" payload bytes and 4096 fixture pixels.\n";return 0;}catch(const std::exception&e){std::cerr<<e.what()<<'\n';return 1;}}
