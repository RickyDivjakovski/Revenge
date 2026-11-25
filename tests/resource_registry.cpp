#include "revenge/resource_registry.hpp"
#include "revenge/shader_ir.hpp"
#include <stdexcept>
#include <iostream>
using namespace revenge::resources;
void require(bool b){if(!b)throw std::runtime_error("registry test failure");}
template<class F>void rejects(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}require(bad);}
int main(){try{
 Registry r(2),other(1);auto a=r.allocate(Kind::texture),b=r.allocate(Kind::buffer);require(r.occupied()==2);rejects([&]{r.allocate(Kind::texture);});rejects([&]{r.descriptor(a,Kind::buffer);});rejects([&]{other.descriptor(a,Kind::texture);});
 r.submitted(a,Kind::texture,1);r.submitted(b,Kind::buffer,2);r.retire(a,Kind::texture);rejects([&]{r.descriptor(a,Kind::texture);});require(r.collect(0)==0);rejects([&]{r.allocate(Kind::texture);});require(r.collect(1)==1);auto c=r.allocate(Kind::texture);require(c.slot==a.slot&&c.generation!=a.generation);rejects([&]{r.descriptor(a,Kind::texture);});rejects([&]{r.submitted(c,Kind::texture,1);});r.retire(b,Kind::buffer);require(r.collect(2)==1);r.retire(c,Kind::texture);require(r.collect(2)==1&&r.occupied()==0);rejects([&]{r.collect(1);});rejects([&]{r.collect(3);});
 for(uint64_t i=3;i<10003;++i){auto h=r.allocate(Kind::texture);r.submitted(h,Kind::texture,i);r.retire(h,Kind::texture);require(r.collect(i-1)==0);require(r.collect(i)==1);}
 auto hlsl=texture_compute_hlsl(256);require(hlsl.find("uint translated_alu(uint v0_input)")!=std::string::npos&&hlsl.find("target.Store")!=std::string::npos&&hlsl.find("case 10u:")!=std::string::npos);require(texture_word(0)==0xff2040ffu&&texture_word(1)==0xff40ff20u&&texture_word(2)==0xffff2040u&&texture_word(3)==0xff20ffffu);rejects([]{texture_compute_hlsl(4);});rejects([]{texture_word(4);});
 auto p=revenge::shader::optimize(revenge::shader::decode(revenge::shader::fixture_words()));for(unsigned i=0;i<4096;++i)require(revenge::shader::evaluate(p,i)==3*i+1);
 std::cout<<"10000 descriptor retirement cycles, 4096 translated ALU oracle results and texture packing verified\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
