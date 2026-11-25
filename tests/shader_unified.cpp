#include "revenge/shader_flow.hpp"
#include "revenge/shader_memory.hpp"
#include "revenge/pipeline_cache.hpp"
#include <iostream>
#include <algorithm>
#include <random>
#include <stdexcept>
using namespace revenge;
void require(bool b){if(!b)throw std::runtime_error("unified/cache assertion failed");}
void reject(std::vector<uint32_t> w){bool failed=false;try{flow::decode_unified(w);}catch(const std::runtime_error&){failed=true;}require(failed);}
int main(){try{
 auto words=flow::unified_words();auto p=flow::decode_unified(words);auto in=memory_shader::fixture_input();std::mt19937 rng(42);
 for(unsigned trial=0;trial<9;++trial){for(unsigned i=0;i<4096;++i)require(flow::evaluate_unified(p,i,in)==flow::unified_expected(i,in));for(auto& v:in)v=rng();}
 auto text=flow::emit(p);require(text.find("sourceData")!=std::string::npos&&text.find("output[id.x] = v1")!=std::string::npos);
 auto bad=words;bad[4]=0xbf800000u;reject(bad); // missing wait
 bad=words;bad[1]=0x80000100u;reject(bad); // wrong descriptor
 bad=words;bad[6]=0xbf840004u;reject(bad); // branch into second word of store
 bad=words;bad[8]=0xbf820003u;reject(bad); // branch skips store
 bad=words;bad.insert(bad.end()-1,{0xe0702000u,0x80020100u});reject(bad); // duplicate store
 bad=words;bad[7]=0x4a000501u;reject(bad); // writes immutable v0
 // independently verify mixed FP32, buffer and branch emission, with finite inputs
 auto fp=words;fp[7]=0x06020501u;fp[9]=0x10020501u;auto f=flow::decode_unified(fp);std::fill(in.begin(),in.end(),0x3f800000u);for(unsigned i=0;i<4095;++i)require(flow::evaluate_unified(f,i,in)==(i<2048?0x40000000u:0x3f800000u));
 std::vector<uint8_t> key{1,2,3},blob{4,5,6,7};auto file=pipeline_cache::pack(key,blob);require(pipeline_cache::unpack(key,file)==blob);auto wrong=key;wrong[0]++;require(pipeline_cache::unpack(wrong,file).empty());for(size_t i=0;i<file.size();++i){auto x=file;x[i]^=1;require(pipeline_cache::unpack(key,x).empty());}for(size_t i=0;i<file.size();++i)require(pipeline_cache::unpack(key,std::span(file).first(i)).empty());file.push_back(0);require(pipeline_cache::unpack(key,file).empty());
 std::cout<<"36864 mixed integer/buffer/branch outputs, 4095 FP32 outputs, invalid paths and cache integrity passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
