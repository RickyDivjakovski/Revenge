#include "revenge/shader_stage.hpp"
#include <array>
#include <bit>
#include <iostream>
#include <stdexcept>
using namespace revenge::stage;
int main(){try{unsigned tested=0;for(unsigned sel=128;sel<=208;++sel){uint32_t expected=sel<=192?sel-128:0u-(sel-192);auto w=fixture(Stage::vertex);w.insert(w.begin(),0x7e000200u|sel);auto result=evaluate(Stage::vertex,w,std::array<float,5>{0,0,2,0,0});if(std::bit_cast<uint32_t>(result.position[0])!=expected||emit(Stage::vertex,w).find("asfloat("+std::to_string(expected)+"u)")==std::string::npos)throw std::runtime_error("integer inline bits mismatch");++tested;}auto w=fixture(Stage::vertex);w.insert(w.begin(),0x7e0002d1u);try{(void)emit(Stage::vertex,w);throw std::logic_error("reserved selector accepted");}catch(const std::invalid_argument&){}std::cout<<"Stage integer inline constants: "<<tested<<" bit-exact CPU MOV and HLSL emission checks; reserved selector rejected. No GPU exceptional FP claim.\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
