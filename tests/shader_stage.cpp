#include "revenge/shader_stage.hpp"
#include "revenge/scene.hpp"
#include <cmath>
#include <bit>
#include <iostream>
#include <stdexcept>
using namespace revenge::stage;
void require(bool ok){if(!ok)throw std::runtime_error("stage assertion failed");}
template<class F>void reject(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}require(bad);}
int main(){try{
 auto vs=fixture(Stage::vertex),ps=fixture(Stage::pixel);
 for(auto v:revenge::scene3d::geometry().vertices){auto r=evaluate(Stage::vertex,vs,std::array<float,5>{v.x,v.y,v.z,v.u,v.v});require(r.position[0]==v.x&&r.position[1]==v.y&&std::abs(r.position[2]-(4*v.z-2)/3.5f)<1e-6f&&r.position[3]==v.z&&r.parameter[0]==v.u&&r.parameter[1]==v.v);}
 for(unsigned y=0;y<64;++y)for(unsigned x=0;x<64;++x){auto r=evaluate(Stage::pixel,ps,std::array<float,2>{(x+.5f)/64,(y+.5f)/64});auto p=revenge::scene3d::palette[(x>=32?1:0)+(y>=32?2:0)];require(r.color[0]==p[2]/255.f&&r.color[1]==p[1]/255.f&&r.color[2]==p[0]/255.f&&r.color[3]==1.f);}
 require(emit(Stage::vertex,vs).find("outv.p = float4(v0, v1, v6, v2)")!=std::string::npos);require(emit(Stage::pixel,ps).find("color = float4(v6, v5, v4, v7)")!=std::string::npos);require(emit(Stage::pixel,ps).find("SampleLevel")!=std::string::npos);
 reject([&]{emit(Stage::pixel,vs);});reject([&]{emit(Stage::vertex,ps);});auto bad=ps;bad.erase(bad.begin()+3);reject([&]{emit(Stage::pixel,bad);});bad=ps;bad[2]|=0x10000;reject([&]{emit(Stage::pixel,bad);});bad=ps;bad[0]=0x7e040281;reject([&]{emit(Stage::pixel,bad);});bad=ps;bad[4]^=1;reject([&]{emit(Stage::pixel,bad);});bad=vs;bad.pop_back();reject([&]{emit(Stage::vertex,bad);});bad=vs;bad[10]=0xffffffff;reject([&]{emit(Stage::vertex,bad);});
 // Repeated dependent texture operations, with a wait between samples.
 auto multi=ps;multi.insert(multi.begin()+4,{0xf0900f08u,0x00400800u,0xbf8c3f70u});multi[8]=0x0b08090au;
 auto mr=evaluate(Stage::pixel,multi,std::array<float,2>{.75f,.25f});auto sr=evaluate(Stage::pixel,ps,std::array<float,2>{.75f,.25f});require(mr.color==sr.color);auto text=emit(Stage::pixel,multi);require(text.find("sampled_1")!=std::string::npos&&text.find("sampled_4")!=std::string::npos);auto pending=multi;pending.erase(pending.begin()+3);reject([&]{emit(Stage::pixel,pending);});
 // Bitwise VOP2 on the raw representation of clip-space X.
 struct BitCase{unsigned op;uint32_t source,input,expected;};
 for(auto c:std::array<BitCase,7>{{{27,0x7fffffffu,0xbf800000u,0x3f800000u},{28,0x80000000u,0x3f800000u,0xbf800000u},{29,0x80000000u,0xbf800000u,0x3f800000u},{30,0x407fffffu,0x80000000u,0x3f800000u},{22,33u,0x7f000000u,0x3f800000u},{26,33u,0x1fc00000u,0x3f800000u},{27,0u,0x3f800000u,0u}}}){auto f=vs;f.insert(f.begin(),{uint32_t(c.op<<25)|255u,c.source});auto r=evaluate(Stage::vertex,f,std::array<float,5>{std::bit_cast<float>(c.input),0,2,0,0});require(std::bit_cast<uint32_t>(r.position[0])==c.expected);require(emit(Stage::vertex,f).find("asuint(v0)")!=std::string::npos);}
 for(auto c:std::array<BitCase,8>{{{37,0x80000000u,0xbf800000u,0x3f800000u},{38,0x3f800000u,0x3f800000u,0u},{39,0x80000000u,0xbf800000u,0x3f800000u},{19,0x3f800000u,0xbf800000u,0x3f800000u},{20,0x3f800000u,0xbf800000u,0xbf800000u},{11,0xff000003u,0x3f000002u,6u},{37,0xffffffffu,0x3f800000u,0x3f7fffffu},{38,0u,0x3f800000u,0xc0800000u}}}){auto f=vs;f.insert(f.begin(),{uint32_t(c.op<<25)|255u,c.source});auto r=evaluate(Stage::vertex,f,std::array<float,5>{std::bit_cast<float>(c.input),0,2,0,0});require(std::bit_cast<uint32_t>(r.position[0])==c.expected);require(emit(Stage::vertex,f).find("asuint(v0)")!=std::string::npos);}
 constexpr unsigned w=320,h=240;std::vector<uint8_t> image(w*h*4);for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){revenge::scene3d::Pixel p;if(revenge::scene3d::expected(x,y,w,h,p)){auto j=(y*w+x)*4;image[j]=p[2];image[j+1]=p[1];image[j+2]=p[0];image[j+3]=p[3];}}auto n=revenge::scene3d::verify(image,w,h,w*4,true);require(n>0);reject([&]{revenge::scene3d::verify(image,w,h,w*4);});std::cout<<"8 vertex exports; 4096 pixel oracle results; "<<n<<" BGR framebuffer pixels; invalid exports, descriptors, and waits rejected; 7 bitwise/shift and 8 integer known answers passed.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
