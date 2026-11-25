#include "revenge/texture_gpu_fixture.hpp"
#include <iostream>
#include <cmath>
#include <stdexcept>
using namespace revenge::textures;
int main(){try{auto f=sampling_fixture();if(f.images.size()!=5||f.samplers.size()!=4||f.expected.size()!=320||f.queries.size()!=320)throw std::runtime_error("fixture size");for(auto c:f.expected)for(auto x:c)if(!std::isfinite(x)||x<0||x>1)throw std::runtime_error("invalid component");if(f.expected[0]!=std::array<float,4>{1,0,0,1}||f.expected[8]!=std::array<float,4>{0,1,0,1}||f.expected[64]!=std::array<float,4>{1,0,0,1})throw std::runtime_error("known colour mismatch");if(f.hlsl.find("result[319]")==std::string::npos)throw std::runtime_error("missing query");std::cout<<"GPU sampling workload: 320 CPU oracle samples, five formats, two mips and two layers, four samplers. GPU execution separate.\n";return 0;}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
