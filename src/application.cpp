#include "revenge/application.hpp"
#include "revenge/multimedia.hpp"
#include "revenge/virtual_memory.hpp"
#include <atomic>
#include <chrono>
#include <condition_variable>
#include <map>
#include <mutex>
#include <stdexcept>
#include <type_traits>
namespace revenge::app {
namespace {
using namespace std::chrono;
constexpr size_t storage_limit=1u<<20, memory_limit=8u<<20, handle_limit=128;
std::atomic<uint64_t> serial{1};
Handle next(){auto n=serial.load(std::memory_order_relaxed);for(;;){if(n==UINT64_MAX)throw std::overflow_error("app handle exhaustion");if(serial.compare_exchange_weak(n,n+1,std::memory_order_relaxed))return {n};}}
uint64_t thread_token(){thread_local const uint64_t token=next().id;return token;}
void check_name(const std::string& name){if(name.empty()||name.size()>64||name=="."||name=="..")throw std::invalid_argument("invalid storage key");for(unsigned char c:name)if(!((c>='a'&&c<='z')||(c>='A'&&c<='Z')||(c>='0'&&c<='9')||c=='_'||c=='-'||c=='.'))throw std::invalid_argument("invalid storage key");}
template<class Map> auto lookup(Map& m,Handle h){auto i=m.find(h.id);if(i==m.end())throw std::invalid_argument("foreign, closed or wrong-kind app handle");return i;}
}
struct Runtime::Impl {
 enum class State {created,running,stopped}; State state=State::created;
 struct Block{vm::Handle native;size_t logical;};
 vm::Arena arena{memory_limit,memory_limit}; std::map<uint64_t,Block> blocks;
 struct Semaphore {unsigned count,maximum;};
 std::map<uint64_t,uint64_t> mutexes; std::map<uint64_t,Semaphore> semaphores;
 std::map<uint64_t,bool> events;std::map<std::string,Bytes> files;size_t stored=0;
 Renderer render;std::filesystem::path snapshot;std::mutex mutex;std::condition_variable cv;steady_clock::time_point start;
 explicit Impl(Renderer r,std::filesystem::path path):render(std::move(r)),snapshot(std::move(path)){}
 void store(const std::string& name,Bytes data){check_name(name);auto i=files.find(name);auto old=i==files.end()?0:i->second.size();if((i==files.end()&&files.size()>=handle_limit)||data.size()>storage_limit-(stored-old))throw std::runtime_error("storage budget exceeded");auto n=data.size();files.insert_or_assign(name,std::move(data));stored=stored-old+n;}
 void running(){if(state!=State::running)throw std::logic_error("application not running");}
 Block block(Handle h,size_t offset,size_t count){auto b=lookup(blocks,h)->second;if(offset>b.logical||count>b.logical-offset)throw std::out_of_range("app memory range");return b;}
};
Runtime::Runtime(Renderer r,std::filesystem::path path):p_(std::make_unique<Impl>(std::move(r),std::move(path))){}
Runtime::~Runtime()=default;
Reply Runtime::dispatch(Request request){auto& p=*p_;std::unique_lock lock(p.mutex);return std::visit([&](auto& r)->Reply{
 using T=std::decay_t<decltype(r)>;
 if constexpr(std::is_same_v<T,Start>){if(p.state!=Impl::State::created)throw std::logic_error("application already started or stopped");p.start=steady_clock::now();p.state=Impl::State::running;}
 else if constexpr(std::is_same_v<T,Stop>){p.running();p.state=Impl::State::stopped;p.cv.notify_all();for(auto& [id,b]:p.blocks)p.arena.release(b.native);p.blocks.clear();p.events.clear();p.mutexes.clear();p.semaphores.clear();p.files.clear();p.stored=0;}
 else {p.running();
  if constexpr(std::is_same_v<T,Allocate>){if(!r.bytes||r.bytes>memory_limit||p.blocks.size()>=handle_limit)throw std::invalid_argument("app allocation limit");auto h=next();auto native=p.arena.reserve(r.bytes);try{auto page=p.arena.page_size();p.arena.commit(native,0,(r.bytes+page-1)/page*page);p.blocks.emplace(h.id,Impl::Block{native,r.bytes});}catch(...){p.arena.release(native);throw;}return h;}
  else if constexpr(std::is_same_v<T,Free>){auto i=lookup(p.blocks,r.handle);p.arena.release(i->second.native);p.blocks.erase(i);}
  else if constexpr(std::is_same_v<T,Read>){auto b=p.block(r.handle,r.offset,r.bytes);Bytes data(r.bytes);p.arena.read(b.native,r.offset,data);return data;}
  else if constexpr(std::is_same_v<T,Write>){auto b=p.block(r.handle,r.offset,r.data.size());p.arena.write(b.native,r.offset,r.data);}
  else if constexpr(std::is_same_v<T,EventCreate>){if(p.events.size()>=handle_limit)throw std::runtime_error("event limit");auto h=next();p.events.emplace(h.id,false);return h;}
  else if constexpr(std::is_same_v<T,EventSet>){lookup(p.events,r.handle)->second=true;p.cv.notify_all();}
  else if constexpr(std::is_same_v<T,EventReset>){lookup(p.events,r.handle)->second=false;}
  else if constexpr(std::is_same_v<T,EventClose>){p.events.erase(lookup(p.events,r.handle));p.cv.notify_all();}
  else if constexpr(std::is_same_v<T,EventWait>){if(r.timeout_ms>10000)throw std::invalid_argument("event wait limit");lookup(p.events,r.handle);auto ready=[&]{auto i=p.events.find(r.handle.id);return p.state!=Impl::State::running||i==p.events.end()||i->second;};p.cv.wait_for(lock,milliseconds(r.timeout_ms),ready);p.running();return lookup(p.events,r.handle)->second;}
  else if constexpr(std::is_same_v<T,MutexCreate>){if(p.mutexes.size()>=handle_limit)throw std::out_of_range("mutex limit");auto h=next();p.mutexes.emplace(h.id,0);return h;}
  else if constexpr(std::is_same_v<T,MutexLock>){if(r.timeout_ms>10000)throw std::out_of_range("mutex timeout");auto token=thread_token();if(lookup(p.mutexes,r.handle)->second==token)throw std::logic_error("nonrecursive mutex");p.cv.wait_for(lock,milliseconds(r.timeout_ms),[&]{auto i=p.mutexes.find(r.handle.id);return p.state!=Impl::State::running||i==p.mutexes.end()||i->second==0;});p.running();auto i=lookup(p.mutexes,r.handle);if(i->second)return false;i->second=token;return true;}
  else if constexpr(std::is_same_v<T,MutexUnlock>){auto i=lookup(p.mutexes,r.handle);if(i->second!=thread_token())throw std::logic_error("mutex not owned by caller");i->second=0;p.cv.notify_all();}
  else if constexpr(std::is_same_v<T,MutexClose>){auto i=lookup(p.mutexes,r.handle);if(i->second)throw std::logic_error("mutex owned");p.mutexes.erase(i);p.cv.notify_all();}
  else if constexpr(std::is_same_v<T,SemaphoreCreate>){if(!r.maximum||r.initial>r.maximum||r.maximum>65535||p.semaphores.size()>=handle_limit)throw std::out_of_range("semaphore limit");auto h=next();p.semaphores.emplace(h.id,Impl::Semaphore{r.initial,r.maximum});return h;}
  else if constexpr(std::is_same_v<T,SemaphoreWait>){if(r.timeout_ms>10000)throw std::out_of_range("semaphore timeout");lookup(p.semaphores,r.handle);p.cv.wait_for(lock,milliseconds(r.timeout_ms),[&]{auto i=p.semaphores.find(r.handle.id);return p.state!=Impl::State::running||i==p.semaphores.end()||i->second.count>0;});p.running();auto& sem=lookup(p.semaphores,r.handle)->second;if(!sem.count)return false;--sem.count;return true;}
  else if constexpr(std::is_same_v<T,SemaphoreRelease>){auto& sem=lookup(p.semaphores,r.handle)->second;if(!r.count||r.count>sem.maximum-sem.count)throw std::out_of_range("semaphore overflow");auto previous=sem.count;sem.count+=r.count;p.cv.notify_all();return uint64_t(previous);}
  else if constexpr(std::is_same_v<T,SemaphoreClose>){p.semaphores.erase(lookup(p.semaphores,r.handle));p.cv.notify_all();}
  else if constexpr(std::is_same_v<T,SyncCount>){return uint64_t(p.mutexes.size()+p.semaphores.size());}
  else if constexpr(std::is_same_v<T,Store>){p.store(r.name,std::move(r.data));}
  else if constexpr(std::is_same_v<T,Persist>){check_name(r.name);if(p.snapshot.empty())throw std::logic_error("persistent storage unavailable");media::save(p.snapshot,p.files.at(r.name));}
  else if constexpr(std::is_same_v<T,Restore>){check_name(r.name);if(p.snapshot.empty())throw std::logic_error("persistent storage unavailable");p.store(r.name,media::load(p.snapshot));}
  else if constexpr(std::is_same_v<T,Load>){check_name(r.name);return p.files.at(r.name);}
  else if constexpr(std::is_same_v<T,Erase>){check_name(r.name);auto i=p.files.find(r.name);if(i==p.files.end())throw std::invalid_argument("missing storage key");p.stored-=i->second.size();p.files.erase(i);}
  else if constexpr(std::is_same_v<T,Clock>){return static_cast<uint64_t>(duration_cast<nanoseconds>(steady_clock::now()-p.start).count());}
  else if constexpr(std::is_same_v<T,Render>){if(r.width<320||r.width>3840||r.height<240||r.height>2160)throw std::invalid_argument("invalid fixture extent");if(!p.render)throw std::logic_error("graphics service unavailable");p.render(r.width,r.height);}
 }
 return std::monostate{};
 },request);}
Stats Runtime::stats(){auto& p=*p_;std::lock_guard lock(p.mutex);return {p.blocks.size(),p.events.size(),p.files.size(),p.stored,p.arena.stats().committed,p.state==Impl::State::running};}
}
