#pragma once
#include "revenge/manual_queue.hpp"
#include <algorithm>
#include <cstdint>
#include <limits>
#include <map>
#include <memory>
#include <vector>
namespace revenge::gaming {
// Independent single-thread virtual-time model, not the GDK ABI.
class ScheduledQueues {
public:
    using Handle=std::uint64_t;
private:
    struct Item { std::uint64_t due; void* context; Callback callback; };
    struct State { std::uint64_t now=0; bool stopped=false; std::array<std::vector<Item>,2> ports; };
    std::map<Handle,std::shared_ptr<State>> handles_;
    Handle next_=1;
    std::shared_ptr<State> lookup(Handle h) const { auto i=handles_.find(h);return i==handles_.end()?nullptr:i->second; }
    Handle insert(std::shared_ptr<State> state) {
        if(handles_.size()>=64||next_==std::numeric_limits<Handle>::max())return 0;
        auto h=next_++;handles_.emplace(h,std::move(state));return h;
    }
public:
    ScheduledQueues()=default;
    ScheduledQueues(const ScheduledQueues&)=delete;
    ScheduledQueues& operator=(const ScheduledQueues&)=delete;
    Handle create(){return insert(std::make_shared<State>());}
    Handle duplicate(Handle h){auto s=lookup(h);return s?insert(s):0;}
    bool close(Handle h){auto s=lookup(h);if(!s)return false;
        if(std::count_if(handles_.begin(),handles_.end(),[&](const auto& e){return e.second==s;})==1&&(!s->ports[0].empty()||!s->ports[1].empty()))return false;
        return handles_.erase(h)==1;
    }
    bool submit(Handle h,Port p,void* context,Callback cb,std::uint64_t delay=0){
        auto s=lookup(h);auto i=static_cast<std::size_t>(p);
        if(!s||s->stopped||i>=2||!cb||s->ports[i].size()>=128||delay>std::numeric_limits<std::uint64_t>::max()-s->now)return false;
        s->ports[i].push_back({s->now+delay,context,cb});return true;
    }
    bool advance(Handle h,std::uint64_t ms){auto s=lookup(h);if(!s||s->stopped||ms>std::numeric_limits<std::uint64_t>::max()-s->now)return false;s->now+=ms;return true;}
    bool dispatch(Handle h,Port p){auto s=lookup(h);auto i=static_cast<std::size_t>(p);if(!s||s->stopped||i>=2)return false;
        auto& port=s->ports[i];auto it=std::min_element(port.begin(),port.end(),[](auto& a,auto& b){return a.due<b.due;});
        if(it==port.end()||it->due>s->now)return false;
        auto item=*it;port.erase(it);item.callback(item.context,false);return true;
    }
    bool stop(Handle h){auto s=lookup(h);if(!s)return false;if(s->stopped)return true;s->stopped=true;
        std::array<std::vector<Item>,2> pending;pending.swap(s->ports);
        for(auto& port:pending)for(auto& item:port)item.callback(item.context,true);return true;
    }
    std::size_t handles()const noexcept{return handles_.size();}
};
}
