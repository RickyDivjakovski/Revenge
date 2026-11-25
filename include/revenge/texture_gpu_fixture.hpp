#pragma once
#include "revenge/texture.hpp"
namespace revenge::textures {
struct SamplingImage { Descriptor descriptor; std::vector<uint8_t> upload; };
struct SamplingQuery { unsigned texture, sampler, layer; float u,v,lod; };
struct SamplingFixture { std::vector<SamplingImage> images; std::vector<Sampler> samplers; std::vector<SamplingQuery> queries; std::vector<std::array<float,4>> expected; std::string hlsl; };
SamplingFixture sampling_fixture();
int sampling_gpu_test();
}
