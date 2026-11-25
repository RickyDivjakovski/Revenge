#include "revenge/pipeline_cache.hpp"
#include <algorithm>
#include <array>
#include <stdexcept>
namespace revenge::pipeline_cache {namespace {
constexpr std::array<uint8_t,8> magic{'R','V','P','S','O',1,0,0};
void put(std::vector<uint8_t>& out,uint64_t n){for(unsigned i=0;i<8;++i)out.push_back(uint8_t(n>>(8*i)));}
uint64_t get(std::span<const uint8_t> in,size_t at){uint64_t n=0;for(unsigned i=0;i<8;++i)n|=uint64_t(in[at+i])<<(8*i);return n;}
uint64_t checksum(std::span<const uint8_t> s){uint64_t h=14695981039346656037ull;for(auto b:s){h^=b;h*=1099511628211ull;}return h;}
}
std::vector<uint8_t> pack(std::span<const uint8_t> key,std::span<const uint8_t> blob){if(key.empty()||key.size()>max_key||blob.empty()||blob.size()>max_blob)throw std::invalid_argument("invalid cache size");std::vector<uint8_t> out;out.reserve(32+key.size()+blob.size());for(auto b:magic)out.push_back(b);put(out,key.size());put(out,blob.size());put(out,checksum(blob));out.insert(out.end(),key.begin(),key.end());out.insert(out.end(),blob.begin(),blob.end());return out;}
std::vector<uint8_t> unpack(std::span<const uint8_t> key,std::span<const uint8_t> file){if(file.size()<32||!std::equal(magic.begin(),magic.end(),file.begin()))return {};auto k=get(file,8),b=get(file,16);if(!k||!b||k>max_key||b>max_blob||k!=key.size()||file.size()!=32+k+b)return {};if(!std::equal(key.begin(),key.end(),file.begin()+32))return {};auto blob=file.subspan(32+size_t(k),size_t(b));if(checksum(blob)!=get(file,24))return {};return {blob.begin(),blob.end()};}
}
