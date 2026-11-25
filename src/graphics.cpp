#include "revenge/graphics.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <numeric>
#include <stdexcept>
namespace revenge {
FrameRing::FrameRing(unsigned slots) {if(!slots||slots>8)throw std::invalid_argument("frame slots must be 1..8");fences_.resize(slots);}
unsigned FrameRing::slot() const noexcept {return static_cast<unsigned>(next_%fences_.size());}
uint64_t FrameRing::required_fence() const noexcept {return fences_[slot()];}
void FrameRing::acquire(uint64_t completed){if(acquired_||completed<completed_||completed>next_||completed<required_fence())throw std::logic_error("frame slot unavailable or invalid fence");completed_=completed;acquired_=true;}
uint64_t FrameRing::submit(){if(!acquired_||next_==UINT64_MAX-1)throw std::logic_error("invalid frame submission");fences_[slot()]=next_+1;acquired_=false;return ++next_;}
FrameStats frame_stats(std::span<const double> durations){if(durations.empty())throw std::invalid_argument("no samples");std::vector<double> v(durations.begin(),durations.end());for(auto x:v)if(!std::isfinite(x)||x<0)throw std::invalid_argument("invalid sample");std::sort(v.begin(),v.end());auto mid=v.size()/2;double median=v.size()%2?v[mid]:(v[mid-1]+v[mid])/2;return {std::accumulate(v.begin(),v.end(),0.0)/v.size(),median,v[static_cast<size_t>(std::ceil(v.size()*0.95))-1]};}
void write_frame_bmp(const std::filesystem::path& path,unsigned w,unsigned h,size_t pitch,std::span<const uint8_t> rgba){
 if(!w||!h||w>8192||h>8192||pitch<size_t(w)*4||pitch>rgba.size()/h)throw std::invalid_argument("invalid RGBA8 layout");
 uint32_t stride=(w*3+3)&~3u, bytes=stride*h;std::ofstream out(path,std::ios::binary|std::ios::trunc);if(!out)throw std::runtime_error("cannot create BMP");
 auto le=[&](uint32_t v,unsigned n){for(unsigned i=0;i<n;++i)out.put(static_cast<char>((v>>(8*i))&255));};
 out.write("BM",2);le(54+bytes,4);le(0,4);le(54,4);le(40,4);le(w,4);le(uint32_t(0)-h,4);le(1,2);le(24,2);le(0,4);le(bytes,4);for(int i=0;i<4;++i)le(0,4);
 std::vector<char> row(stride);for(unsigned y=0;y<h;++y){auto p=rgba.data()+size_t(y)*pitch;for(unsigned x=0;x<w;++x){row[x*3]=static_cast<char>(p[x*4+2]);row[x*3+1]=static_cast<char>(p[x*4+1]);row[x*3+2]=static_cast<char>(p[x*4]);}out.write(row.data(),row.size());}out.flush();if(!out)throw std::runtime_error("BMP write failed");
}
}
