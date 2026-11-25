#include "revenge/scene.hpp"
#include <algorithm>
#include <stdexcept>
#include <iostream>
using namespace revenge::scene3d;
void check(bool b){if(!b)throw std::runtime_error("motion test failed");}
int main(){try{
 for(auto w:{320u,1920u,3840u}){for(uint64_t i=0;i<10000;++i){auto x=motion_offset(w,i);check(x>=-int(w/8)&&x<=int(w/8)&&x==motion_offset(w,i+120));}check(motion_offset(w,0)==-int(w/8)&&motion_offset(w,30)==0&&motion_offset(w,60)==int(w/8));}
 unsigned w=320,h=240;size_t pitch=w*4+16,total=0;
 for(int offset:{-40,-17,0,17,40}){
  std::vector<uint8_t> data(pitch*h);
  std::vector<uint8_t> base(pitch*h);for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){Pixel p{};if(expected(x,y,w,h,p)){std::swap(p[0],p[2]);std::copy(p.begin(),p.end(),base.begin()+y*pitch+x*4);}}
  for(unsigned y=0;y<h;++y){for(unsigned x=0;x<w;++x)data[y*pitch+x*4+3]=255;for(unsigned x=0;x<w;++x){int dest=int(x)+offset;if(dest>=0&&dest<int(w))std::copy_n(base.begin()+y*pitch+x*4,4,data.begin()+y*pitch+dest*4);}}
  total+=verify(data,w,h,pitch,true,offset);bool rejected=false;data[(w/2)*4]^=1;try{verify(data,w,h,pitch,true,offset);}catch(const std::runtime_error&){rejected=true;}check(rejected);
 }
 bool rejected=false;try{motion_offset(319,0);}catch(const std::invalid_argument&){rejected=true;}check(rejected);
 std::cout<<total<<" shifted oracle pixels checked; 30000 motion offsets validated.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
