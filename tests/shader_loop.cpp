#include "revenge/shader_flow.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
using namespace revenge::flow;
void require(bool b){if(!b)throw std::runtime_error("loop/wave assertion failed");}
template<typename F> void reject(F f){bool failed=false;try{f();}catch(const std::exception&){failed=true;}require(failed);}
int main(){try{
 auto p=decode_loop(loop_words());require(p.code.size()==6&&p.code[4].target==1);for(uint32_t i=0;i<4096;++i)require(evaluate(p,i)==i+i/64+1);
 auto h=emit(p);require(h.find("step<1024u")!=std::string::npos&&h.find("continue;")!=std::string::npos&&h.find("scc = s1 < lhs")!=std::string::npos);
 reject([]{decode(loop_words());});
 reject([]{decode_loop(std::array<uint32_t,2>{0xbf82fffeu,0xbf810000u});}); // unreachable end
 reject([]{decode_loop(std::array<uint32_t,4>{0x7e0002ffu,0x12345678u,0xbf82fffeu,0xbf810000u});}); // literal target
 reject([]{decode_loop(std::array<uint32_t,2>{0xbf85ffffu,0xbf810000u});}); // undefined SCC
 auto infinite=decode_loop(std::array<uint32_t,3>{0xbf068000u,0xbf85ffffu,0xbf810000u});reject([&]{evaluate(infinite,0);});require(evaluate(infinite,64)==64);
 auto carry=decode_loop(std::array<uint32_t,5>{0xbe8103c1u,0x80018101u,0xbf850001u,0x7e000280u,0xbf810000u});require(evaluate(carry,17)==17);
 auto nocarry=decode_loop(std::array<uint32_t,5>{0xbe810381u,0x80018101u,0xbf850001u,0x7e000280u,0xbf810000u});require(evaluate(nocarry,17)==0);
 for(unsigned width:{32u,64u}){auto out=wave_fixture_expected(width);require(out.size()==4096);for(unsigned id=0;id<4096;++id){unsigned group=id/64,local=id%64,lane=local%width,bit=local%32;uint32_t e=0,v=0;for(unsigned j=0;j<32;++j){unsigned l=(lane/32)*32+j;if((l+group)%5!=0){e|=1u<<j;if((l*3+group)%7<3)v|=1u<<j;}}auto expected=bit==0?e:bit==1?v:((v>>bit)&1u)?id*3+1:0x12345678u;require(out[id]==expected);}require(wave_fixture_hlsl(width).find("GroupMemoryBarrierWithGroupSync")!=std::string::npos);}
 reject([]{wave_fixture_expected(48);});
 std::cout<<"4096 bounded-loop and 8192 logical wave-mask oracle outputs verified; carry, malformed branches and exhaustion tested.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
