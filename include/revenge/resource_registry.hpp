#pragma once
#include <cstdint>
#include <span>
#include <vector>
#include <string>
#include <filesystem>
namespace revenge::resources {
enum class Kind { texture, buffer };
struct Handle { uint64_t owner=0,generation=0; unsigned slot=0; };
// Single graphics-queue timeline. Descriptor slots are not recycled before retirement.
// Thread confinement: one command recording/retirement owner.
class Registry {
 struct Entry { uint64_t generation=0,last_use=0; Kind kind=Kind::texture; enum State {free,live,retired} state=free; };
 uint64_t owner_,submitted_=0,completed_=0;
 std::vector<Entry> entries_;
 Entry& resolve(Handle,Kind);
public:
 explicit Registry(unsigned capacity);
 Registry(const Registry&)=delete; Registry& operator=(const Registry&)=delete;
 Handle allocate(Kind);
 unsigned descriptor(Handle,Kind);
 void submitted(Handle,Kind,uint64_t fence);
 void retire(Handle,Kind);
 unsigned collect(uint64_t completed);
 unsigned occupied() const noexcept;
};
// Generated compute shader uses the independently decoded ALU fixture to select texels.
std::string texture_compute_hlsl(unsigned row_pitch,std::span<const uint32_t> words={});
uint32_t texture_word(unsigned index);
}
