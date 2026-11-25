#include "revenge/render_pass_fixture.hpp"
#include <iostream>
#include <stdexcept>
using namespace revenge::passes;
int main(){try{auto a=reference();unsigned inside=0;for(unsigned y=0;y<height;++y)for(unsigned x=0;x<width;++x){auto i=y*width+x;bool in=x>=3&&x<13&&y>=2&&y<10;inside+=in;
if(a[0][i]!=(in?Pixel{128,0,128,255}:Pixel{0,0,0,0})||a[1][i]!=(in?Pixel{0,255,0,255}:Pixel{0,0,0,0})||a[2][i]!=(in?Pixel{128,255,128,255}:Pixel{0,0,0,255}))throw std::runtime_error("render pass oracle mismatch");}if(inside!=80)throw std::runtime_error("coverage");std::cout<<"Render-pass CPU reference: 768 pixels across three surfaces; 80 covered pixels; depth, stencil, scissor, blend and composition known answers.\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
