#include "revenge/presentation.hpp"
#include <algorithm>
#include <cmath>
#include <stdexcept>
namespace revenge::presentation {
BackBuffers::BackBuffers(unsigned count){if(count<2||count>8)throw std::invalid_argument("back-buffer count 2..8");fences_.resize(count);}
uint64_t BackBuffers::required(unsigned index)const{return fences_.at(index);}
void BackBuffers::acquire(unsigned index,uint64_t completed){if(recording_||completed<completed_||completed>submitted_||completed<required(index))throw std::logic_error("back buffer unavailable or invalid fence");active_=index;completed_=completed;recording_=true;}
uint64_t BackBuffers::submit(){if(!recording_||submitted_==UINT64_MAX-1)throw std::logic_error("invalid present submission");recording_=false;return fences_[active_]=++submitted_;}
void BackBuffers::resized(uint64_t completed){if(recording_||completed!=submitted_)throw std::logic_error("resize requires drained queue");completed_=completed;std::fill(fences_.begin(),fences_.end(),0);}
std::array<float,4> scene(double seconds,unsigned w,unsigned h){if(!std::isfinite(seconds)||seconds<0||!w||!h||w>16384||h>16384)throw std::invalid_argument("invalid scene extent/time");auto side=double(std::min(w,h));return {float(std::cos(seconds)),float(std::sin(seconds)),float(side/w),float(side/h)};}
}
