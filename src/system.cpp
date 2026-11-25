#include "revenge/system.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <stdexcept>
#include <iostream>
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
#include <intrin.h>
#elif defined(__x86_64__) || defined(__i386__)
#include <cpuid.h>
#endif
namespace revenge {namespace {
constexpr uint32_t unmapped=UINT32_MAX;
size_t arena_size(size_t n,size_t p){if(!n||p<256||p>65536||(p&(p-1))||n>(256ull*1024*1024)/p)throw std::invalid_argument("invalid arena size (max 256 MiB)");return n*p;}
size_t virtual_count(size_t n,size_t p){arena_size(n,p);return n;}
std::string lower(std::string_view s){std::string r(s);for(auto& c:r)if(c>='A'&&c<='Z')c=char(c-'A'+'a');return r;}
void bounds(size_t start,size_t n,size_t end){if(start>end||n>end-start)throw std::out_of_range("page range");}
}
MemoryArena::MemoryArena(size_t physical,size_t virt,size_t p):page_(p),bytes_(arena_size(physical,p)),refs_(physical),map_(virtual_count(virt,p),unmapped),allocated_(physical),dirty_(bytes_.size(),p){}
std::vector<uint32_t> MemoryArena::allocate(size_t n){if(!n||n>allocated_.size()-used_)throw std::length_error("physical pages exhausted or zero request");std::vector<uint32_t> result;result.reserve(n);for(size_t i=0;i<allocated_.size()&&result.size()<n;++i)if(!allocated_[i])result.push_back(static_cast<uint32_t>(i));for(auto i:result){allocated_[i]=true;std::memset(bytes_.data()+size_t(i)*page_,0,page_);dirty_.mark(size_t(i)*page_,page_);}used_+=n;return result;}
void MemoryArena::release(std::span<const uint32_t> pages){std::vector<uint32_t> sorted(pages.begin(),pages.end());std::sort(sorted.begin(),sorted.end());if(std::adjacent_find(sorted.begin(),sorted.end())!=sorted.end())throw std::invalid_argument("duplicate release");for(auto i:sorted)if(i>=allocated_.size()||!allocated_[i]||refs_[i])throw std::invalid_argument("page not allocated or still mapped");for(auto i:sorted){allocated_[i]=false;std::memset(bytes_.data()+size_t(i)*page_,0,page_);dirty_.mark(size_t(i)*page_,page_);}used_-=sorted.size();}
void MemoryArena::map(size_t v,std::span<const uint32_t> pages){bounds(v,pages.size(),map_.size());for(size_t i=0;i<pages.size();++i)if(pages[i]>=allocated_.size()||!allocated_[pages[i]]||map_[v+i]!=unmapped)throw std::invalid_argument("invalid physical page or occupied mapping");for(size_t i=0;i<pages.size();++i){map_[v+i]=pages[i];++refs_[pages[i]];}}
void MemoryArena::unmap(size_t v,size_t n){bounds(v,n,map_.size());for(size_t i=0;i<n;++i)if(map_[v+i]==unmapped)throw std::invalid_argument("unmapped page");for(size_t i=0;i<n;++i){--refs_[map_[v+i]];map_[v+i]=unmapped;}}
void MemoryArena::validate(size_t off,size_t n)const{bounds(off,n,map_.size()*page_);if(!n)return;for(size_t i=off/page_;i<=(off+n-1)/page_;++i)if(map_[i]==unmapped)throw std::invalid_argument("unmapped address");}
void MemoryArena::write(size_t off,std::span<const uint8_t> data){validate(off,data.size());while(!data.empty()){auto p=size_t(map_[off/page_])*page_+off%page_;auto n=std::min(data.size(),page_-off%page_);std::memcpy(bytes_.data()+p,data.data(),n);dirty_.mark(p,n);off+=n;data=data.subspan(n);}}
void MemoryArena::read(size_t off,std::span<uint8_t> data)const{validate(off,data.size());while(!data.empty()){auto p=size_t(map_[off/page_])*page_+off%page_;auto n=std::min(data.size(),page_-off%page_);std::memcpy(data.data(),bytes_.data()+p,n);off+=n;data=data.subspan(n);}}
std::span<const ApiEntry> api_registry()noexcept{static constexpr ApiEntry entries[]={{"xmem.dll","memory","CPU page model only; ABI not implemented"},{"d3d12_xs.dll","graphics","not implemented"},{"d3d12_x.dll","graphics","not implemented"},{"xg_xs.dll","texture layout","not implemented"},{"xg_x.dll","texture layout","not implemented"},{"XboxBvhBuilder.dll","ray tracing","not implemented"},{"XCRDAPI.dll","storage","not implemented"},{"amd_fidelityfx_loader_xbox.dll","graphics effects","not implemented"},{"chad.dll","audio","not implemented"},{"xapu.dll","audio","not implemented"},{"AcpHal.dll","audio","not implemented"},{"Bink2Scarlett.dll","video","not implemented"},{"libHttpClient.GDK.dll","networking","external; not verified"}};return entries;}
bool auditable_contract(std::string_view name){auto s=lower(name);static constexpr std::string_view names[]={"api-ms-win-core-file-l1-1-0.dll","api-ms-win-core-file-l2-1-0.dll","api-ms-win-core-processthreads-l1-1-0.dll","api-ms-win-core-processthreads-l1-1-1.dll","api-ms-win-core-processthreads-l1-1-2.dll","api-ms-win-core-libraryloader-l1-2-0.dll","api-ms-win-core-libraryloader-l1-2-1.dll","api-ms-win-core-memory-l1-1-0.dll","api-ms-win-core-synch-l1-1-0.dll","api-ms-win-core-synch-l1-2-0.dll","api-ms-win-core-synch-l1-2-1.dll","api-ms-win-core-handle-l1-1-0.dll","api-ms-win-core-errorhandling-l1-1-0.dll","api-ms-win-core-profile-l1-1-0.dll","api-ms-win-core-sysinfo-l1-1-0.dll","api-ms-win-core-sysinfo-l1-2-0.dll","api-ms-win-core-com-l1-1-0.dll","api-ms-win-crt-stdio-l1-1-0.dll","api-ms-win-crt-runtime-l1-1-0.dll","api-ms-win-crt-locale-l1-1-0.dll","api-ms-win-crt-time-l1-1-0.dll","api-ms-win-crt-math-l1-1-0.dll","api-ms-win-crt-convert-l1-1-0.dll","api-ms-win-crt-filesystem-l1-1-0.dll","api-ms-win-crt-string-l1-1-0.dll","api-ms-win-crt-utility-l1-1-0.dll","api-ms-win-crt-heap-l1-1-0.dll"};return std::find(std::begin(names),std::end(names),s)!=std::end(names);}
std::filesystem::path translate_path(const std::filesystem::path& root,std::string_view input){if(!root.is_absolute()||input.size()<3||input.size()>4096||input[1]!=':'||(input[2]!='/'&&input[2]!='\\'))throw std::invalid_argument("expected absolute root and drive path");char drive=lower(input.substr(0,1))[0];std::filesystem::path result=root;switch(drive){case 'g':result/="content";break;case 't':result/="temp";break;case 'u':result/="user";break;case 'd':result/="data";break;default:throw std::invalid_argument("unconfigured drive");} // Synthetic fixture mapping, not verified Xbox drive semantics.
size_t pos=3;while(pos<input.size()){auto end=input.find_first_of("/\\",pos);if(end==std::string_view::npos)end=input.size();auto s=input.substr(pos,end-pos);if(s.empty()||s=="."||s==".."||s.back()=='.'||s.back()==' ')throw std::invalid_argument("invalid path component");for(unsigned char c:s)if(c<32||c>=127||std::string_view(":*?\"<>|").find(char(c))!=std::string_view::npos)throw std::invalid_argument("unsupported path character");auto stem=lower(s.substr(0,s.find('.')));while(!stem.empty()&&stem.back()==' ')stem.pop_back();if(stem=="con"||stem=="prn"||stem=="aux"||stem=="nul"||stem=="conin$"||stem=="conout$"||(stem.size()==4&&(stem.starts_with("com")||stem.starts_with("lpt"))&&stem[3]>='0'&&stem[3]<='9'))throw std::invalid_argument("reserved device path");result/=std::string(s);pos=end+1;}return result;}
unsigned choose_slots(std::span<const TuneSample> samples,double fraction){if(!std::isfinite(fraction)||fraction<=0||fraction>1)throw std::invalid_argument("invalid fraction");double best=0;for(auto s:samples)if(s.slots&&s.slots<=8&&std::isfinite(s.throughput)&&s.throughput>best)best=s.throughput;if(best<=0)throw std::invalid_argument("no valid samples");unsigned chosen=9;for(auto s:samples)if(s.slots&&s.slots<=8&&std::isfinite(s.throughput)&&s.throughput>=best*fraction)chosen=std::min(chosen,s.slots);return chosen;}
unsigned slot_budget(uint64_t free_bytes,uint64_t transfer_bytes){if(!transfer_bytes)return 0;auto cap=std::min<uint64_t>(free_bytes/16,256ull*1024*1024);auto count=cap/transfer_bytes;return count>2?static_cast<unsigned>(std::min<uint64_t>(8,count-2)):0;}
int cpu_info(std::ostream& out){char vendor[13]{};bool avx2=false;
#if defined(_MSC_VER) && (defined(_M_X64) || defined(_M_IX86))
int r[4]{};__cpuid(r,0);int max=r[0];std::memcpy(vendor,&r[1],4);std::memcpy(vendor+4,&r[3],4);std::memcpy(vendor+8,&r[2],4);if(max>=7){__cpuid(r,1);if((r[2]&(1<<27))&&(r[2]&(1<<28))&&(_xgetbv(0)&6)==6){__cpuidex(r,7,0);avx2=(r[1]&(1<<5))!=0;}}
#elif defined(__x86_64__) || defined(__i386__)
unsigned a,b,c,d;if(__get_cpuid(0,&a,&b,&c,&d)){std::memcpy(vendor,&b,4);std::memcpy(vendor+4,&d,4);std::memcpy(vendor+8,&c,4);}__builtin_cpu_init();avx2=__builtin_cpu_supports("avx2");
#endif
out<<"CPU vendor: "<<(vendor[0]?vendor:"unknown")<<"; AVX2 available to process: "<<avx2<<"\nPortable baseline build; no vendor-specific instruction path selected.\n";return 0;}
}
