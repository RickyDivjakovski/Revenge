#pragma once
#include <array>
#include <cstdint>
#include <vector>
#include <span>
#include <memory>
namespace revenge::passes {
inline constexpr unsigned width=16,height=16;
struct Rect { unsigned left,top,right,bottom; };
struct Draw { Rect scissor; float depth; unsigned mode; };
inline constexpr Rect region{3,2,13,10}, full{0,0,width,height};
inline constexpr std::array<Draw,3> draws{{{region,0.25f,0},{full,0.5f,1},{full,0.125f,2}}};
using Pixel=std::array<std::uint8_t,4>;
using Images=std::array<std::vector<Pixel>,3>;
void validate(unsigned w,unsigned h,std::span<const Draw> commands);
Images reference(unsigned w,unsigned h,std::span<const Draw> commands);
Images reference();
#ifdef _WIN32
struct PassCache;
std::shared_ptr<PassCache> create_pass_cache();
Images render_gpu(PassCache& cache,unsigned w,unsigned h,std::span<const Draw> commands);
#endif
int gpu_test();
}
