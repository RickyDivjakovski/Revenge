#include "revenge/shader_memory.hpp"
#include <iostream>
#include <stdexcept>
#include <random>
using namespace revenge::memory_shader;
void require(bool b){if(!b)throw std::runtime_error("buffer test failed");}
void reject(std::vector<uint32_t> w){try{decode(w);}catch(const std::runtime_error&){return;}throw std::runtime_error("bad program accepted");}
int main(){try{auto w=fixture_words();auto p=decode(w);require(p.code().size()==6);auto input=fixture_input();require(evaluate(p,input)==fixture_expected(input));auto h=emit(p);require(h.find("sourceData[v0 + 1u]")!=std::string::npos&&h.find("outputData[v0] = v1")!=std::string::npos);std::mt19937 rng(431);for(int k=0;k<16;++k){for(auto& x:input)x=rng();require(evaluate(p,input)==fixture_expected(input));}auto b=w;b.erase(b.begin()+4);reject(b);b=w;b[1]|=0x10000;reject(b);b=w;b[2]|=1;reject(b);b=w;b[0]|=0x4000;reject(b);b=w;b[1]=0x80000000;reject(b);b=w;b[7]=0x80000100;reject(b);b=w;b[6]|=4;reject(b);b=w;b.pop_back();reject(b);reject({0xe0302000});b=w;b.insert(b.end()-1,0xbf8c3f70);reject(b);b=w;b[5]=0x4a020503;reject(b);b=w;b[5]=0x4a000501;reject(b);std::cout<<"69632 buffer outputs checked, malformed instruction and wait dependencies rejected.\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
