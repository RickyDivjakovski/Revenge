#include "revenge/virtual_memory.hpp"
#include <algorithm>
#include <atomic>
#include <bit>
#include <chrono>
#include <cstring>
#include <exception>
#include <map>
#include <mutex>
#include <ostream>
#include <stdexcept>
#include <thread>
#include <vector>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#elif defined(__linux__)
#include <sys/mman.h>
#include <unistd.h>
#else
#error Unsupported native virtual memory platform
#endif
namespace revenge::vm {
namespace {
std::atomic<uint64_t> next_generation{1};
uint64_t generation(){auto n=next_generation.load(std::memory_order_relaxed);for(;;){if(n==UINT64_MAX)throw std::overflow_error("allocation generation exhausted");if(next_generation.compare_exchange_weak(n,n+1,std::memory_order_relaxed))return n;}}
size_t host_page(){
#ifdef _WIN32
 SYSTEM_INFO i{}; GetSystemInfo(&i); return i.dwPageSize;
#else
 long n=sysconf(_SC_PAGESIZE); if(n<=0)throw std::runtime_error("cannot query page size"); return static_cast<size_t>(n);
#endif
}
uint8_t* reserve_os(size_t n){
#ifdef _WIN32
 auto p=VirtualAlloc(nullptr,n,MEM_RESERVE,PAGE_NOACCESS); if(!p)throw std::runtime_error("VirtualAlloc reserve failed");
#else
 auto p=mmap(nullptr,n,PROT_NONE,MAP_PRIVATE|MAP_ANONYMOUS,-1,0); if(p==MAP_FAILED)throw std::runtime_error("mmap reserve failed");
#endif
 return static_cast<uint8_t*>(p);
}
void release_os(uint8_t* p,size_t n) noexcept {
#ifdef _WIN32
 (void)n; VirtualFree(p,0,MEM_RELEASE);
#else
 munmap(p,n);
#endif
}
#ifdef _WIN32
DWORD native_access(Access a){return a==Access::read_write?PAGE_READWRITE:a==Access::read_only?PAGE_READONLY:PAGE_NOACCESS;}
#else
int native_access(Access a){return a==Access::read_write?(PROT_READ|PROT_WRITE):a==Access::read_only?PROT_READ:PROT_NONE;}
#endif
bool valid(Access a){return a==Access::no_access||a==Access::read_only||a==Access::read_write;}
bool change_os(uint8_t* p,size_t n,Access a,bool commit){
#ifdef _WIN32
 if(commit)return VirtualAlloc(p,n,MEM_COMMIT,native_access(a))==p;
 DWORD old{};return VirtualProtect(p,n,native_access(a),&old)!=0;
#else
 (void)commit;return mprotect(p,n,native_access(a))==0;
#endif
}
bool decommit_os(uint8_t* p,size_t n){
#ifdef _WIN32
 return VirtualFree(p,n,MEM_DECOMMIT)!=0;
#else
 return mprotect(p,n,PROT_NONE)==0 && madvise(p,n,MADV_DONTNEED)==0;
#endif
}
}
struct Arena::Impl {
 struct Region {uint64_t generation; std::vector<Access> pages; uint64_t fence=0;};
 size_t page=host_page(); uint64_t capacity,budget; uint8_t* base=nullptr;
 std::mutex lock; std::map<uint64_t,Region> regions;
 uint64_t reserved=0, committed=0, peak=0, completed=0; bool poisoned=false;
 Impl(uint64_t cap,uint64_t limit):capacity(cap),budget(limit){
  static_assert(sizeof(size_t)>=8,"64-bit host required");
  if(!std::has_single_bit(page)||!cap||cap%page||!limit||limit%page||limit>cap||cap>(64ull<<30))throw std::invalid_argument("invalid arena capacity/budget (page aligned, <=64 GiB)");
  base=reserve_os(static_cast<size_t>(cap));
 }
 ~Impl(){release_os(base,static_cast<size_t>(capacity));}
 void check() const {if(poisoned)throw std::runtime_error("arena unavailable after OS memory operation failure");}
 void os(bool ok){if(!ok){poisoned=true;throw std::runtime_error("native memory operation failed; arena disabled");}}
 auto get(Handle h,bool allow_retired=false){check();auto it=regions.find(h.offset);if(it==regions.end()||it->second.generation!=h.generation||(!allow_retired&&it->second.fence))throw std::invalid_argument("invalid, stale or retired allocation");return it;}
 uint64_t size(const Region& r)const{return r.pages.size()*uint64_t(page);}
 void range(const Region& r,uint64_t off,uint64_t n,bool pages)const{auto len=size(r);if(off>len||n>len-off||(pages&&(!n||off%page||n%page)))throw std::out_of_range("invalid allocation range");}
 uint64_t aligned_offset(uint64_t off,uint64_t alignment)const{auto mod=(reinterpret_cast<uintptr_t>(base)%alignment+off%alignment)%alignment;return off+(mod?alignment-mod:0);}
 void release_it(std::map<uint64_t,Region>::iterator it){auto n=size(it->second);auto count=std::count_if(it->second.pages.begin(),it->second.pages.end(),[](Access a){return a!=Access::reserved;});if(count)os(decommit_os(base+it->first,static_cast<size_t>(n)));committed-=uint64_t(count)*page;reserved-=n;regions.erase(it);}
};
Arena::Arena(uint64_t c,uint64_t b):impl_(std::make_unique<Impl>(c,b)){}
Arena::~Arena()=default;
size_t Arena::page_size()const noexcept{return impl_->page;}
Handle Arena::reserve(uint64_t bytes,uint64_t alignment){auto& p=*impl_;std::lock_guard guard(p.lock);p.check();if(!alignment)alignment=p.page;if(!std::has_single_bit(alignment)||alignment<p.page||alignment>(1ull<<30)||!bytes||bytes>p.capacity)throw std::invalid_argument("invalid allocation size/alignment");auto n=(bytes+p.page-1)/p.page*p.page;uint64_t off=p.aligned_offset(0,alignment);for(const auto& [start,r]:p.regions){if(off<=start&&n<=start-off)break;if(off<start+p.size(r))off=p.aligned_offset(start+p.size(r),alignment);}if(off>p.capacity||n>p.capacity-off)throw std::runtime_error("arena address space exhausted");auto id=generation();p.regions.emplace(off,Impl::Region{id,std::vector<Access>(static_cast<size_t>(n/p.page),Access::reserved),0});p.reserved+=n;return {off,id};}
void Arena::commit(Handle h,uint64_t off,uint64_t n,Access a){auto& p=*impl_;std::lock_guard guard(p.lock);auto& r=p.get(h)->second;p.range(r,off,n,true);if(!valid(a))throw std::invalid_argument("invalid protection");auto first=r.pages.begin()+off/p.page,last=first+n/p.page;if(!std::all_of(first,last,[](Access x){return x==Access::reserved;}))throw std::invalid_argument("commit requires reserved pages");if(n>p.budget-p.committed)throw std::runtime_error("commit budget exhausted");p.os(change_os(p.base+h.offset+off,static_cast<size_t>(n),a,true));std::fill(first,last,a);p.committed+=n;p.peak=std::max(p.peak,p.committed);}
void Arena::protect(Handle h,uint64_t off,uint64_t n,Access a){auto& p=*impl_;std::lock_guard guard(p.lock);auto& r=p.get(h)->second;p.range(r,off,n,true);if(!valid(a))throw std::invalid_argument("invalid protection");auto first=r.pages.begin()+off/p.page,last=first+n/p.page;if(std::find(first,last,Access::reserved)!=last)throw std::invalid_argument("protect requires committed pages");p.os(change_os(p.base+h.offset+off,static_cast<size_t>(n),a,false));std::fill(first,last,a);}
void Arena::decommit(Handle h,uint64_t off,uint64_t n){auto& p=*impl_;std::lock_guard guard(p.lock);auto& r=p.get(h)->second;p.range(r,off,n,true);auto first=r.pages.begin()+off/p.page,last=first+n/p.page;if(std::find(first,last,Access::reserved)!=last)throw std::invalid_argument("decommit requires committed pages");p.os(decommit_os(p.base+h.offset+off,static_cast<size_t>(n)));std::fill(first,last,Access::reserved);p.committed-=n;}
void Arena::release(Handle h){auto& p=*impl_;std::lock_guard guard(p.lock);p.release_it(p.get(h));}
void Arena::read(Handle h,uint64_t off,std::span<uint8_t> out){auto& p=*impl_;std::lock_guard guard(p.lock);auto& r=p.get(h)->second;p.range(r,off,out.size(),false);if(out.empty())return;for(auto i=off/p.page;i<=(off+out.size()-1)/p.page;++i)if(r.pages[i]!=Access::read_only&&r.pages[i]!=Access::read_write)throw std::invalid_argument("read protection violation");std::memcpy(out.data(),p.base+h.offset+off,out.size());}
void Arena::write(Handle h,uint64_t off,std::span<const uint8_t> in){auto& p=*impl_;std::lock_guard guard(p.lock);auto& r=p.get(h)->second;p.range(r,off,in.size(),false);if(in.empty())return;for(auto i=off/p.page;i<=(off+in.size()-1)/p.page;++i)if(r.pages[i]!=Access::read_write)throw std::invalid_argument("write protection violation");std::memcpy(p.base+h.offset+off,in.data(),in.size());}
Access Arena::page_access(Handle h,uint64_t off){auto& p=*impl_;std::lock_guard guard(p.lock);auto& r=p.get(h)->second;p.range(r,off,1,false);return r.pages[off/p.page];}
bool Arena::aligned(Handle h,uint64_t a){auto& p=*impl_;std::lock_guard guard(p.lock);p.get(h);if(!std::has_single_bit(a)||a>(1ull<<30))throw std::invalid_argument("invalid alignment");return p.aligned_offset(h.offset,a)==h.offset;}
void Arena::retire(Handle h,uint64_t fence){auto& p=*impl_;std::lock_guard guard(p.lock);auto& r=p.get(h)->second;if(fence<=p.completed)throw std::invalid_argument("retirement fence must be future");r.fence=fence;}
uint64_t Arena::collect(uint64_t fence){auto& p=*impl_;std::lock_guard guard(p.lock);p.check();if(fence<p.completed)throw std::invalid_argument("fence regressed");uint64_t count=0;for(auto it=p.regions.begin();it!=p.regions.end();){if(it->second.fence&&it->second.fence<=fence){auto old=it++;p.release_it(old);++count;}else ++it;}p.completed=fence;return count;}
Stats Arena::stats(){auto& p=*impl_;std::lock_guard guard(p.lock);p.check();Stats s{p.capacity,p.reserved,p.committed,p.peak,p.capacity-p.reserved,0,0,0,p.completed};uint64_t end=0;for(const auto& [start,r]:p.regions){s.largest_free=std::max(s.largest_free,start-end);end=start+p.size(r);if(r.fence)++s.retired;else ++s.live;}s.largest_free=std::max(s.largest_free,p.capacity-end);return s;}
int benchmark(std::ostream& out,unsigned mib,unsigned threads){if(!mib||mib>256||!threads||threads>16||uint64_t(mib)*threads>512)throw std::invalid_argument("benchmark size/thread count (maximum 512 MiB total)");uint64_t n=uint64_t(mib)<<20;Arena a(n*threads,n*threads);if(n%a.page_size())throw std::runtime_error("benchmark size not page aligned");std::vector<std::exception_ptr> errors(threads);std::vector<std::jthread> workers;auto begin=std::chrono::steady_clock::now();for(unsigned t=0;t<threads;++t)workers.emplace_back([&,t]{try{auto h=a.reserve(n);a.commit(h,0,n);std::vector<uint8_t> data(static_cast<size_t>(n),static_cast<uint8_t>(t+1)),got(data.size());a.write(h,0,data);a.read(h,0,got);if(data!=got)throw std::runtime_error("memory benchmark mismatch");a.release(h);}catch(...){errors[t]=std::current_exception();}});workers.clear();for(auto& e:errors)if(e)std::rethrow_exception(e);auto elapsed=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();auto s=a.stats();if(s.reserved||s.committed||s.live||s.retired)throw std::runtime_error("memory benchmark leak");out<<"Native page size: "<<a.page_size()<<" bytes\nVerified "<<n*threads<<" bytes across "<<threads<<" workers.\nPeak committed bytes: "<<s.peak_committed<<"\nElapsed ms (allocation, copies, comparison, release): "<<elapsed<<"\nAll reservations released. CPU-only fixture; no Xbox ABI or GPU mapping.\n";return 0;}
}
