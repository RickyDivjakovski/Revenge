#pragma once
#include <array>
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>
#include <memory>
namespace revenge::package {struct Programs;}
namespace revenge::scene3d {
struct Vertex {float x,y,z,u,v;};
struct Geometry {std::vector<Vertex> vertices;std::vector<uint16_t> indices;};
enum class Op {begin, bind_mesh, bind_texture, draw_indexed, end};
struct Command {Op op;unsigned first=0,count=0;};
using Pixel=std::array<uint8_t,4>;
inline constexpr std::array<Pixel,4> palette{{{255,64,32,255},{32,255,64,255},{64,32,255,255},{255,255,32,255}}};
Geometry geometry();
std::vector<Command> commands();
void validate(const Geometry&,std::span<const Command>);
// Pixel-centre oracle; excludes rasterization boundaries and point-sampling ties.
bool expected(unsigned x,unsigned y,unsigned w,unsigned h,Pixel& result);
size_t verify(std::span<const uint8_t> image,unsigned w,unsigned h,size_t pitch,bool swapped=false,int shift_x=0);
// A bounded, deterministic triangle-wave translation in integer screen pixels.
int motion_offset(unsigned width,uint64_t frame);
struct CustomScene { Geometry mesh; std::vector<Command> stream; unsigned texture_width=0,texture_height=0; std::vector<uint8_t> rgba; std::vector<uint8_t> framebuffer; bool quiet=false; };
struct Continuous {unsigned seconds=0, fps_cap=0;uint64_t submitted=0,presented=0,elapsed_ns=0,gpu_ns=0,first_pixels=0,last_pixels=0;int last_shift=0;};
struct CustomDevice;
std::shared_ptr<CustomDevice> create_custom_device();
int gpu_test(unsigned w,unsigned h,const std::filesystem::path& output,bool integrated=false,bool translated=false,const package::Programs* programs=nullptr,const std::filesystem::path& cache={},unsigned present_seconds=0,size_t* verified=nullptr,Continuous* continuous=nullptr,CustomScene* custom=nullptr,CustomDevice* device=nullptr,unsigned async_slot=~0u);
void collect_custom(CustomDevice&,unsigned slot,CustomScene&);
}
