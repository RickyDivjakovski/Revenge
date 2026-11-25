#pragma once
#include <cstdint>
#include <filesystem>
#include <span>
#include <vector>
namespace revenge {
// Single-queue, single-producer frame lifecycle. A slot cannot be reused until
// its submission fence has completed. The caller performs the actual wait.
class FrameRing {
 std::vector<uint64_t> fences_;
 uint64_t next_=0, completed_=0;
 bool acquired_=false;
public:
 explicit FrameRing(unsigned slots);
 unsigned slot() const noexcept;
 uint64_t required_fence() const noexcept;
 void acquire(uint64_t completed);
 uint64_t submit();
 uint64_t last_fence() const noexcept {return next_;}
};
struct FrameStats { double mean_ms, median_ms, p95_ms; };
FrameStats frame_stats(std::span<const double> durations);
// Writes a top-down 24-bit BMP from RGBA8 rows; padding in source is ignored.
void write_frame_bmp(const std::filesystem::path&, unsigned width,unsigned height,size_t pitch,std::span<const uint8_t> rgba);
int gpu_render(unsigned frames,unsigned slots,const std::filesystem::path& output,const std::filesystem::path& cache={});
}
