#include "revenge/shader_ir.hpp"
#include <array>
#include <iostream>
#include <random>
#include <stdexcept>
using namespace revenge::shader;
void require(bool condition){if(!condition)throw std::runtime_error("shader IR test assertion failed");}
uint32_t vop(unsigned op,unsigned d,unsigned a,unsigned b){return (op<<25)|(d<<17)|(b<<9)|a;}
uint32_t mov(unsigned d,unsigned a){return 0x7e000200u|(d<<17)|a;}
void reject(std::initializer_list<uint32_t> words){bool rejected=false;try{decode(words);}catch(const std::runtime_error&){rejected=true;}require(rejected);}
int main(){try{
 constexpr uint32_t end=0xbf810000u;
 auto p=decode(fixture_words()),q=optimize(p);require(p.instructions.size()==4);require(q.instructions.size()==3);require(hlsl(q).find("output[id.x] = v0_2;")!=std::string::npos);
 for(uint32_t i=0;i<4096;++i){require(evaluate(p,i)==3*i+1);require(evaluate(q,i)==3*i+1);}
 std::mt19937 rng(138);for(int i=0;i<500;++i){uint32_t a=rng(),b=rng();auto check=[&](unsigned op,uint32_t expect){std::array<uint32_t,5> w{mov(1,255),b,vop(op,0,255,1),a,end};auto original=decode(w),optimized=optimize(original);require(evaluate(original,0)==expect);require(evaluate(optimized,0)==expect);require(optimized.instructions.size()==1);};check(11,(a&0xffffffu)*(b&0xffffffu));check(19,a<b?a:b);check(20,a>b?a:b);check(22,b>>(a&31));check(26,b<<(a&31));check(27,a&b);check(28,a|b);check(29,a^b);check(30,~(a^b));check(37,a+b);check(38,a-b);check(39,b-a);
 auto dynamic=decode(std::array<uint32_t,4>{mov(1,255),b,vop(37,0,256,1),end});require(evaluate(dynamic,a)==a+b);require(evaluate(optimize(dynamic),a)==a+b);}
 auto r=decode(std::array<uint32_t,6>{mov(1,129),mov(1,256),vop(37,0,257,0),mov(2,255),end,end});require(evaluate(r,14)==28);require(evaluate(optimize(r),14)==28);
 require(evaluate(decode(std::array<uint32_t,2>{mov(0,193),end}),0)==0xffffffffu);
 require(evaluate(decode(std::array<uint32_t,2>{0xbf80000fu,end}),77)==77);
 reject({});reject({mov(0,255)});reject({mov(0,129)});reject({end,end});reject({mov(0,257),end});reject({mov(0,249),end});reject({0xbf820000u,end});reject({0xbeff0380u,end});reject({vop(3,0,256,0),end});reject({0x80000000u,end});reject({mov(0,0),end});reject({0x7e000400u,end});reject({0x7c000000u,end});
 std::cout<<"4096 known-answer inputs, 6500 randomized arithmetic cases, constant folding, DCE and rejection tests passed.\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
