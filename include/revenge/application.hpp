#pragma once
#include <cstdint>
#include <functional>
#include <filesystem>
#include <iosfwd>
#include <memory>
#include <string>
#include <variant>
#include <vector>
namespace revenge::package {struct Programs;}
namespace revenge::app {
using Bytes=std::vector<uint8_t>;
struct Handle {uint64_t id{};};
struct Start{}; struct Stop{};
struct Allocate{size_t bytes;}; struct Free{Handle handle;};
struct Read{Handle handle;size_t offset,bytes;}; struct Write{Handle handle;size_t offset;Bytes data;};
struct EventCreate{}; struct EventSet{Handle handle;}; struct EventReset{Handle handle;};
struct EventWait{Handle handle;unsigned timeout_ms;}; struct EventClose{Handle handle;};
struct MutexCreate{}; struct MutexLock{Handle handle;unsigned timeout_ms;}; struct MutexUnlock{Handle handle;}; struct MutexClose{Handle handle;};
struct SemaphoreCreate{unsigned initial,maximum;}; struct SemaphoreWait{Handle handle;unsigned timeout_ms;}; struct SemaphoreRelease{Handle handle;unsigned count;}; struct SemaphoreClose{Handle handle;};
struct SyncCount{};
struct Store{std::string name;Bytes data;}; struct Load{std::string name;}; struct Erase{std::string name;};
struct Persist{std::string name;}; struct Restore{std::string name;};
struct Clock{}; struct Render{unsigned width,height;};
using Request=std::variant<Start,Stop,Allocate,Free,Read,Write,EventCreate,EventSet,EventReset,EventWait,EventClose,MutexCreate,MutexLock,MutexUnlock,MutexClose,SemaphoreCreate,SemaphoreWait,SemaphoreRelease,SemaphoreClose,SyncCount,Store,Load,Erase,Persist,Restore,Clock,Render>;
using Reply=std::variant<std::monostate,Handle,Bytes,bool,uint64_t>;
using Renderer=std::function<void(unsigned,unsigned)>;
struct Stats {size_t memory_handles,events,files,storage_bytes;uint64_t committed;bool running;};
// Trusted in-process fixture ABI. Render callback is synchronous and must not re-enter.
// Join all callers before destruction. No guest pointers. Optional single host-selected snapshot path.
class Runtime {
 struct Impl; std::unique_ptr<Impl> p_;
public:
 explicit Runtime(Renderer renderer={},std::filesystem::path snapshot={}); ~Runtime();
 Runtime(const Runtime&)=delete;Runtime& operator=(const Runtime&)=delete;
 Reply dispatch(Request request); Stats stats();
};
int fixture(std::ostream&,unsigned width,unsigned height,Renderer renderer={},const package::Programs* programs=nullptr);
}
