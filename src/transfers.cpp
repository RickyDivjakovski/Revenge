#include "revenge/transfers.hpp"
#include <limits>
#include <stdexcept>
namespace revenge::transfer {
Pool::Pool(unsigned n,uint64_t bytes,uint64_t budget):bytes_(bytes),live_(0){
 if(!n||n>8||!bytes||bytes>std::numeric_limits<uint64_t>::max()/(3*n))throw std::invalid_argument("invalid transfer pool dimensions");
 live_=3*n*bytes;if(live_>budget)throw std::runtime_error("transfer pool logical budget exhausted");slots_.resize(n);
}
Slot& Pool::get(unsigned i){return slots_.at(i);}
const Slot& Pool::slot(unsigned i)const{return slots_.at(i);}
void Pool::begin(unsigned i){auto& s=get(i);if(s.phase!=Phase::idle)throw std::logic_error("transfer slot not idle");s.phase=Phase::recording;}
void Pool::upload(unsigned i){auto& s=get(i);if(s.phase!=Phase::recording)throw std::logic_error("upload requires recording slot");s.phase=Phase::copy_dest;}
void Pool::download(unsigned i){auto& s=get(i);if(s.phase!=Phase::copy_dest)throw std::logic_error("download requires upload");s.phase=Phase::copy_source;}
uint64_t Pool::submit(unsigned i,bool retire){auto& s=get(i);if(s.phase!=Phase::copy_source||issued_==std::numeric_limits<uint64_t>::max()-1)throw std::logic_error("invalid transfer submission");s.phase=Phase::submitted;s.fence=++issued_;s.retire=retire;return issued_;}
void Pool::complete(uint64_t value){if(value<completed_||value>issued_)throw std::logic_error("invalid transfer fence completion");completed_=value;}
bool Pool::consume(unsigned i){auto& s=get(i);if(s.phase!=Phase::submitted||s.fence>completed_)throw std::logic_error("readback not complete");if(s.retire){s.phase=Phase::released;live_-=3*bytes_;return true;}s={};return false;}
static uint8_t byte_at(std::size_t i,uint64_t seq){uint64_t v=uint64_t(i)^(seq*0x9e3779b97f4a7c15ull);v^=v>>30;v*=0xbf58476d1ce4e5b9ull;v^=v>>27;v*=0x94d049bb133111ebull;v^=v>>31;return static_cast<uint8_t>(v);}
void pattern(std::span<uint8_t> out,uint64_t seq){for(std::size_t i=0;i<out.size();++i)out[i]=byte_at(i,seq);}
bool matches(std::span<const uint8_t> in,uint64_t seq){for(std::size_t i=0;i<in.size();++i)if(in[i]!=byte_at(i,seq))return false;return true;}
}
