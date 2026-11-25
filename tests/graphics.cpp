#include "revenge/graphics.hpp"
#include <array>
#include <fstream>
#include <iterator>
#include <iostream>
#include <stdexcept>
using namespace revenge;
static void require(bool p){if(!p)throw std::runtime_error("graphics test failed");}
template<class F> static void rejects(F f){bool thrown=false;try{f();}catch(const std::exception&){thrown=true;}require(thrown);}
int main(){try{
 rejects([]{FrameRing r(0);});FrameRing r(2);require(r.slot()==0);r.acquire(0);rejects([&]{r.acquire(0);});require(r.submit()==1);require(r.slot()==1);r.acquire(0);require(r.submit()==2);require(r.required_fence()==1);rejects([&]{r.acquire(0);});r.acquire(1);require(r.submit()==3);require(r.required_fence()==2);rejects([&]{r.acquire(4);});r.acquire(3);require(r.submit()==4);rejects([&]{r.submit();});rejects([&]{r.acquire(2);});
 for(unsigned slots=1;slots<=8;++slots){FrameRing ring(slots);for(unsigned i=0;i<1000;++i){ring.acquire(ring.required_fence());require(ring.submit()==i+1);}}
 std::array<double,4> t{4,1,3,2};auto s=frame_stats(t);require(s.mean_ms==2.5&&s.median_ms==2.5&&s.p95_ms==4);rejects([]{frame_stats({});});
 std::array<uint8_t,8> pixel{10,20,30,255,99,99,99,99};auto path=std::filesystem::current_path()/"revenge-graphics-test.bmp";write_frame_bmp(path,1,1,8,pixel);std::ifstream f(path,std::ios::binary);std::vector<uint8_t> data((std::istreambuf_iterator<char>(f)),{});f.close();std::filesystem::remove(path);require(data.size()==58&&data[0]=='B'&&data[1]=='M'&&data[22]==255&&data[54]==30&&data[55]==20&&data[56]==10&&data[57]==0);rejects([&]{write_frame_bmp(path,2,2,8,pixel);});
 std::cout<<"Graphics foundation tests passed\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
