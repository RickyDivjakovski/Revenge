#pragma once
#include <array>
#include <cstddef>
#include <deque>
namespace revenge::gaming {
enum class Port { work, completion };
using Callback = void(*)(void*, bool);
// Independent single-thread manual reference; no GDK ABI, timing or concurrency claim.
class ManualQueue {
    struct Item { void* context; Callback callback; };
    std::array<std::deque<Item>,2> ports_;
    bool stopped_ = false;
    static constexpr std::size_t limit_ = 128;
public:
    ManualQueue() = default;
    ManualQueue(const ManualQueue&) = delete;
    ManualQueue& operator=(const ManualQueue&) = delete;
    bool submit(Port port, void* context, Callback callback) {
        auto i=static_cast<std::size_t>(port);
        if (stopped_ || !callback || i>=ports_.size() || ports_[i].size()>=limit_) return false;
        ports_[i].push_back({context,callback}); return true;
    }
    bool dispatch(Port port) {
        auto i=static_cast<std::size_t>(port);
        if(stopped_ || i>=ports_.size() || ports_[i].empty()) return false;
        auto item=ports_[i].front(); ports_[i].pop_front(); item.callback(item.context,false); return true;
    }
    // Explicit cancellation for this model; callbacks must not throw.
    void stop() {
        if(stopped_) return;
        stopped_=true;
        for(auto& p:ports_) while(!p.empty()) { auto item=p.front();p.pop_front();item.callback(item.context,true); }
    }
};
}
