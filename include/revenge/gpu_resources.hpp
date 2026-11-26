#pragma once
#include <cstdint>
#include <memory>
#include <span>
#include <string>
namespace revenge::resident {
class Arena;
struct Draw { uint64_t target,texture,vertices,indices; unsigned count; uint64_t second; unsigned mode,tint; std::string vs,ps; };
void draw_batch(Arena&, std::span<const Draw>, bool asynchronous=false);
bool batch_complete(Arena&, unsigned timeout_ms);
uint64_t submission_count(const Arena&);
std::shared_ptr<Arena> create();
// All operations externally serialised. Resources remain in COMMON between calls.
void allocate(Arena&, uint64_t id, uint64_t bytes, unsigned width, unsigned height);
void upload(Arena&, uint64_t id, std::span<const uint8_t> bytes);
void read(Arena&, uint64_t id, std::span<uint8_t> bytes);
void copy(Arena&, uint64_t dst, uint64_t src);
void release(Arena&, uint64_t id);
uint64_t program_count(const Arena&);
uint64_t clear_programs(Arena&);
void prepare_program(Arena&, const std::string& vertex_source, const std::string& pixel_source);
void draw(Arena&, uint64_t target, uint64_t texture, uint64_t vertices, uint64_t indices, unsigned count, uint64_t second_texture, unsigned mode, unsigned tint, const std::string& vertex_source={}, const std::string& pixel_source={});
}
