#include "revenge/queue_abi.h"
#include <algorithm>
#include <array>
#include <chrono>
#include <condition_variable>
#include <limits>
#include <map>
#include <memory>
#include <mutex>
#include <vector>
namespace {
using Clock=std::chrono::steady_clock;
struct Item { Clock::time_point due; rq_callback callback; void* data; rq_ticket ticket=0; };
struct Queue { bool stopped=false,canceling=false; size_t active=0,aliases=0; std::array<std::vector<Item>,2> ports; };
}
struct rq_context { std::mutex mutex; std::condition_variable changed; std::map<rq_handle,std::shared_ptr<Queue>> handles; rq_handle next=1; rq_ticket next_ticket=1; };
namespace {
std::shared_ptr<Queue> lookup(rq_context& c,rq_handle h){auto i=c.handles.find(h);return i==c.handles.end()?nullptr:i->second;}
uint32_t insert(rq_context& c,const std::shared_ptr<Queue>& q,rq_handle* out){if(c.handles.size()>=64||c.next==std::numeric_limits<rq_handle>::max())return RQ_LIMIT;c.handles.emplace(c.next,q);++q->aliases;*out=c.next++;return RQ_OK;}
uint32_t call(const Item& i,uint32_t canceled) noexcept {try{i.callback(i.data,canceled);return RQ_OK;}catch(...){return RQ_FAILURE;}}
}
uint32_t RQ_CALL rq_open(uint32_t version,rq_context** out){if(!out)return RQ_INVALID;*out=nullptr;if(version!=RQ_VERSION)return RQ_VERSION_ERROR;try{*out=new rq_context;return RQ_OK;}catch(...){return RQ_FAILURE;}}
uint32_t RQ_CALL rq_destroy(rq_context* c){if(!c)return RQ_INVALID;try{{std::lock_guard lock(c->mutex);if(!c->handles.empty())return RQ_STATE;}delete c;return RQ_OK;}catch(...){return RQ_FAILURE;}}
uint32_t RQ_CALL rq_create(rq_context* c,rq_handle* out){if(!out)return RQ_INVALID;*out=0;if(!c)return RQ_INVALID;try{std::lock_guard lock(c->mutex);return insert(*c,std::make_shared<Queue>(),out);}catch(...){return RQ_FAILURE;}}
uint32_t RQ_CALL rq_duplicate(rq_context* c,rq_handle h,rq_handle* out){if(!out)return RQ_INVALID;*out=0;if(!c)return RQ_INVALID;try{std::lock_guard lock(c->mutex);auto q=lookup(*c,h);return q?insert(*c,q,out):RQ_INVALID;}catch(...){return RQ_FAILURE;}}
uint32_t RQ_CALL rq_close(rq_context* c,rq_handle h){if(!c)return RQ_INVALID;try{std::lock_guard lock(c->mutex);auto q=lookup(*c,h);if(!q)return RQ_INVALID;if(q->aliases==1&&(q->active||q->canceling||!q->ports[0].empty()||!q->ports[1].empty()))return RQ_STATE;--q->aliases;c->handles.erase(h);c->changed.notify_all();return RQ_OK;}catch(...){return RQ_FAILURE;}}
uint32_t RQ_CALL rq_submit(rq_context* c,rq_handle h,uint32_t port,uint32_t delay,rq_callback cb,void* data){if(!c||port>1||!cb||delay>60000)return RQ_INVALID;try{std::lock_guard lock(c->mutex);auto q=lookup(*c,h);if(!q)return RQ_INVALID;if(q->stopped)return RQ_STATE;auto& p=q->ports[port];if(p.size()>=128)return RQ_LIMIT;p.push_back({Clock::now()+std::chrono::milliseconds(delay),cb,data});c->changed.notify_all();return RQ_OK;}catch(...){return RQ_FAILURE;}}
uint32_t RQ_CALL rq_dispatch(rq_context* c,rq_handle h,uint32_t port,uint32_t timeout){if(!c||port>1||timeout>10000)return RQ_INVALID;try{std::unique_lock lock(c->mutex);const auto end=Clock::now()+std::chrono::milliseconds(timeout);for(;;){auto q=lookup(*c,h);if(!q)return RQ_INVALID;if(q->stopped)return RQ_STATE;auto& p=q->ports[port];auto it=std::min_element(p.begin(),p.end(),[](const Item& a,const Item& b){return a.due<b.due;});auto now=Clock::now();if(it!=p.end()&&it->due<=now){auto item=*it;p.erase(it);++q->active;lock.unlock();auto status=call(item,0);lock.lock();--q->active;c->changed.notify_all();return status;}if(now>=end)return RQ_TIMEOUT;c->changed.wait_until(lock,it==p.end()?end:std::min(end,it->due));}}catch(...){return RQ_FAILURE;}}
uint32_t RQ_CALL rq_stop(rq_context* c,rq_handle h){if(!c)return RQ_INVALID;try{std::unique_lock lock(c->mutex);auto q=lookup(*c,h);if(!q)return RQ_INVALID;if(q->canceling)return RQ_STATE;if(q->stopped)return RQ_OK;q->stopped=true;q->canceling=true;std::array<std::vector<Item>,2> pending;pending.swap(q->ports);c->changed.notify_all();lock.unlock();uint32_t status=RQ_OK;for(auto& p:pending)for(auto& i:p)if(call(i,1)!=RQ_OK)status=RQ_FAILURE;lock.lock();q->canceling=false;c->changed.notify_all();return status;}catch(...){return RQ_FAILURE;}}

uint32_t RQ_CALL rq_query(rq_context* c,rq_handle h,rq_snapshot* out){
    if(!c||!out||out->size!=sizeof(rq_snapshot))return RQ_INVALID;
    try{std::lock_guard lock(c->mutex);auto q=lookup(*c,h);if(!q)return RQ_INVALID;
        *out={sizeof(rq_snapshot),uint32_t(q->stopped),uint32_t(q->canceling),uint32_t(q->aliases),uint32_t(q->active),uint32_t(q->ports[0].size()),uint32_t(q->ports[1].size())};return RQ_OK;
    }catch(...){return RQ_FAILURE;}
}

uint32_t RQ_CALL rq_submit_tracked(rq_context* c,rq_handle h,uint32_t port,uint32_t delay,rq_callback cb,void* data,rq_ticket* out){
    if(!out)return RQ_INVALID;*out=0;
    if(!c||port>1||!cb||delay>60000)return RQ_INVALID;
    try{std::lock_guard lock(c->mutex);auto q=lookup(*c,h);if(!q)return RQ_INVALID;if(q->stopped)return RQ_STATE;
        auto& p=q->ports[port];if(p.size()>=128||c->next_ticket==std::numeric_limits<rq_ticket>::max())return RQ_LIMIT;
        p.push_back({Clock::now()+std::chrono::milliseconds(delay),cb,data,c->next_ticket});*out=c->next_ticket++;c->changed.notify_all();return RQ_OK;
    }catch(...){return RQ_FAILURE;}
}
uint32_t RQ_CALL rq_cancel(rq_context* c,rq_handle h,rq_ticket ticket){
    if(!c||!ticket)return RQ_INVALID;
    try{std::unique_lock lock(c->mutex);auto q=lookup(*c,h);if(!q)return RQ_INVALID;
        for(auto& p:q->ports){auto it=std::find_if(p.begin(),p.end(),[&](const Item& i){return i.ticket==ticket;});if(it==p.end())continue;
            auto item=*it;p.erase(it);++q->active;c->changed.notify_all();lock.unlock();auto status=call(item,1);lock.lock();--q->active;c->changed.notify_all();return status;}
        return RQ_INVALID;
    }catch(...){return RQ_FAILURE;}
}
