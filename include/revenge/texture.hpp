#pragma once
#include <array>
#include <cstdint>
#include <cstddef>
#include <span>
#include <string>
#include <vector>
#include <filesystem>
namespace revenge::textures {
// Independent fixture layout, not a console descriptor or hardware swizzle.
enum class Format { r8, rgba8, bgra8, bc1, bc3, bc4, bc5 };
enum class Layout { linear, morton8 };
struct Descriptor { uint32_t width,height,mips=1,layers=1; Format format=Format::rgba8; Layout layout=Layout::linear; };
struct Footprint { uint32_t layer,mip,width,height,blocks_x,blocks_y,block_bytes; size_t source_offset,source_size,upload_offset,row_pitch,row_bytes; };
struct Plan { std::vector<Footprint> subresources; size_t source_bytes=0,upload_bytes=0; };
Plan plan(Descriptor d);
std::vector<uint8_t> convert(Descriptor d,std::span<const uint8_t> source);
uint32_t morton8(uint32_t x,uint32_t y);
enum class Address { clamp, wrap, mirror, border };
enum class Filter { point, linear };
// Normalized CPU reference; explicit mip/layer; BC1/3/4/5 UNORM.
// Border is transparent black.
std::array<float,4> sample(Descriptor d,std::span<const uint8_t> upload,uint32_t mip,uint32_t layer,float u,float v,Address address,Filter filter);
struct Sampler { Address u=Address::clamp, v=Address::clamp; Filter spatial=Filter::point, mip=Filter::point; };
std::array<float,4> sample_lod(Descriptor d,std::span<const uint8_t> upload,uint32_t layer,float u,float v,float lod,Sampler sampler);
std::vector<uint32_t> fixture_input();
std::vector<uint32_t> fixture_expected();
std::string fixture_hlsl();
int gpu_test(const std::filesystem::path& cache);
}
