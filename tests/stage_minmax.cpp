#include "revenge/shader_stage.hpp"
#include "revenge/shader_package.hpp"
#include <algorithm>
#include <array>
#include <iostream>
#include <stdexcept>
using namespace revenge;
void check(bool b){if(!b)throw std::runtime_error("shader minmax assertion");}
int main(){try{auto p=package::minmax_fixture();check(package::decode(package::encode(p))==p);auto h=stage::emit(stage::Stage::vertex,p.vertex);check(h.find("min(")!=std::string::npos&&h.find("max(")!=std::string::npos);for(int i=-1024;i<=1024;++i){float x=i/128.f;auto r=stage::evaluate(stage::Stage::vertex,p.vertex,std::array<float,5>{x,0,2,0,0});check(r.position[0]==x*.5f);}auto f=stage::fixture(stage::Stage::vertex);f.insert(f.begin(),{0x1e0000f4u,0x200000f5u});for(int i=-1024;i<=1024;++i){float x=i/128.f;auto r=stage::evaluate(stage::Stage::vertex,f,std::array<float,5>{x,0,2,0,0});check(r.position[0]==std::clamp(x,-2.f,2.f));}constexpr std::array<float,8> constants{.5f,-.5f,1.f,-1.f,2.f,-2.f,4.f,-4.f};for(unsigned i=0;i<8;++i){auto v=stage::fixture(stage::Stage::vertex);v.insert(v.begin(),0x7e0002f0u+i);check(stage::evaluate(stage::Stage::vertex,v,std::array<float,5>{0,0,2,0,0}).position[0]==constants[i]);}auto ps=stage::fixture(stage::Stage::pixel);ps.insert(ps.begin()+4,{0x1e0c0cf0u,0x200c0cf0u});for(unsigned i=0;i<64;++i)check(stage::evaluate(stage::Stage::pixel,ps,std::array<float,2>{(i+.5f)/64,.25f}).color[0]==.5f);std::cout<<"Stage min/max: 4098 vertex comparisons, 64 pixel comparisons, eight inline constants and package roundtrip passed.\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
