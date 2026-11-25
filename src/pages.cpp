#include "revenge/core.hpp"
#include <algorithm>
#include <bit>
#include <stdexcept>
namespace revenge {
DirtyPages::DirtyPages(uint64_t length,uint64_t page):length_(length),page_(page){if(!page || page> (1ull<<32) || (page&(page-1)))throw std::invalid_argument("page must be a power of two");auto count=length/page+(length%page!=0);auto words=count/64+(count%64!=0);if(words>16ull*1024*1024)throw std::length_error("dirty map too large");words_.resize(static_cast<size_t>(words));}
void DirtyPages::mark(uint64_t offset,uint64_t size){if(offset>length_ || size>length_-offset)throw std::out_of_range("dirty range");if(!size)return;auto first=offset/page_,last=(offset+size-1)/page_; auto a=first/64,b=last/64;uint64_t lo=~uint64_t(0)<<(first%64),hi=~uint64_t(0)>>(63-last%64);if(a==b){words_[a]|=lo&hi;return;}words_[a]|=lo;std::fill(words_.begin()+a+1,words_.begin()+b,~uint64_t(0));words_[b]|=hi;}
std::vector<Range> DirtyPages::ranges()const{std::vector<Range> out;for(size_t i=0;i<words_.size();++i){uint64_t w=words_[i];while(w){auto start=std::countr_zero(w);auto run=std::countr_one(w>>start);uint64_t begin=(uint64_t(i)*64+start)*page_;uint64_t bytes=std::min<uint64_t>(length_-begin,uint64_t(run)*page_);if(!out.empty() && out.back().offset+out.back().size==begin)out.back().size+=bytes;else out.push_back({begin,bytes});if(run==64)w=0;else w&=~(((uint64_t(1)<<run)-1)<<start);}}return out;}
void DirtyPages::clear()noexcept{std::fill(words_.begin(),words_.end(),0);}
}
