#pragma once
#include <array>
#include <cstdint>
#include <vector>
namespace revenge::presentation {
// Tracks actual DXGI back-buffer indices, not frame-number modulo buffer count.
class BackBuffers {
 std::vector<uint64_t> fences_;
 uint64_t submitted_=0, completed_=0;
 unsigned active_=0;
 bool recording_=false;
public:
 explicit BackBuffers(unsigned count);
 uint64_t required(unsigned index) const;
 void acquire(unsigned index,uint64_t completed);
 uint64_t submit();
 void resized(uint64_t completed);
 uint64_t last() const noexcept{return submitted_;}
};
// Root constants: cosine, sine, horizontal scale, vertical scale.
std::array<float,4> scene(double seconds,unsigned width,unsigned height);
int run(unsigned seconds,unsigned width,unsigned height,bool vsync,bool fixed=false,unsigned fps_cap=0);
}
