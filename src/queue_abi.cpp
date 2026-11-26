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
struct Operation {
    rq_operation id; rq_operation_work work; rq_operation_completion completion; void* data;
    rq_operation_snapshot snapshot{sizeof(rq_operation_snapshot),RQ_OP_QUEUED,RQ_STATE,0,RQ_OK,0,0};
};
struct Item { Clock::time_point due; rq_callback callback; void* data; rq_ticket ticket=0; std::shared_ptr<Operation> operation{}; };
struct Queue { bool stopped=false,canceling=false; size_t active=0,aliases=0; std::array<std::vector<Item>,2> ports; std::map<rq_operation,std::shared_ptr<Operation>> operations; };
}
struct rq_context { std::mutex mutex; std::condition_variable changed; std::map<rq_handle,std::shared_ptr<Queue>> handles; rq_handle next=1; rq_ticket next_ticket=1; rq_operation next_operation=1; };
namespace {
std::shared_ptr<Queue> lookup(rq_context& c,rq_handle h){auto i=c.handles.find(h);return i==c.handles.end()?nullptr:i->second;}
uint32_t insert(rq_context& c,const std::shared_ptr<Queue>& q,rq_handle* out){if(c.handles.size()>=64||c.next==std::numeric_limits<rq_handle>::max())return RQ_LIMIT;c.handles.emplace(c.next,q);++q->aliases;*out=c.next++;return RQ_OK;}
bool terminal(uint32_t state){return state>=RQ_OP_SUCCEEDED;}
std::shared_ptr<Operation> lookup_operation(const std::shared_ptr<Queue>& q,rq_operation id){if(!q)return {};auto i=q->operations.find(id);return i==q->operations.end()?nullptr:i->second;}
uint32_t call(rq_context& c,const Item& i,uint32_t canceled) {
    if(!i.operation){try{i.callback(i.data,canceled);return RQ_OK;}catch(...){return RQ_FAILURE;}}
    const auto& op=i.operation;uint64_t result=0;uint32_t status=RQ_STATE;
    if(!canceled){try{status=op->work(op->data,&result);}catch(...){status=RQ_FAILURE;}}
    rq_operation_snapshot notice{};
    {std::lock_guard lock(c.mutex);auto& s=op->snapshot;s.state=canceled?RQ_OP_CANCELED:status==RQ_OK?RQ_OP_SUCCEEDED:RQ_OP_FAILED;s.work_status=status;s.result=(!canceled&&status==RQ_OK)?result:0;notice=s;c.changed.notify_all();}
    uint32_t notify_status=RQ_OK;
    if(op->completion){try{op->completion(op->data,op->id,&notice);}catch(...){notify_status=RQ_FAILURE;}}
    {std::lock_guard lock(c.mutex);op->snapshot.notification_status=notify_status;op->snapshot.notification_done=1;c.changed.notify_all();}
    return notify_status;
}
}
uint32_t RQ_CALL rq_open(uint32_t version,rq_context** out){if(!out)return RQ_INVALID;*out=nullptr;if(version!=RQ_VERSION)return RQ_VERSION_ERROR;try{*out=new rq_context;return RQ_OK;}catch(...){return RQ_FAILURE;}}
uint32_t RQ_CALL rq_destroy(rq_context* c){if(!c)return RQ_INVALID;try{{std::lock_guard lock(c->mutex);if(!c->handles.empty())return RQ_STATE;}delete c;return RQ_OK;}catch(...){return RQ_FAILURE;}}
uint32_t RQ_CALL rq_create(rq_context* c,rq_handle* out){if(!out)return RQ_INVALID;*out=0;if(!c)return RQ_INVALID;try{std::lock_guard lock(c->mutex);return insert(*c,std::make_shared<Queue>(),out);}catch(...){return RQ_FAILURE;}}
uint32_t RQ_CALL rq_duplicate(rq_context* c,rq_handle h,rq_handle* out){if(!out)return RQ_INVALID;*out=0;if(!c)return RQ_INVALID;try{std::lock_guard lock(c->mutex);auto q=lookup(*c,h);return q?insert(*c,q,out):RQ_INVALID;}catch(...){return RQ_FAILURE;}}
uint32_t RQ_CALL rq_close(rq_context* c,rq_handle h){if(!c)return RQ_INVALID;try{std::lock_guard lock(c->mutex);auto q=lookup(*c,h);if(!q)return RQ_INVALID;if(q->aliases==1&&(q->active||q->canceling||!q->ports[0].empty()||!q->ports[1].empty()||!q->operations.empty()))return RQ_STATE;--q->aliases;c->handles.erase(h);c->changed.notify_all();return RQ_OK;}catch(...){return RQ_FAILURE;}}
uint32_t RQ_CALL rq_submit(rq_context* c,rq_handle h,uint32_t port,uint32_t delay,rq_callback cb,void* data){if(!c||port>1||!cb||delay>60000)return RQ_INVALID;try{std::lock_guard lock(c->mutex);auto q=lookup(*c,h);if(!q)return RQ_INVALID;if(q->stopped)return RQ_STATE;auto& p=q->ports[port];if(p.size()>=128)return RQ_LIMIT;p.push_back({Clock::now()+std::chrono::milliseconds(delay),cb,data});c->changed.notify_all();return RQ_OK;}catch(...){return RQ_FAILURE;}}
uint32_t RQ_CALL rq_dispatch(rq_context* c,rq_handle h,uint32_t port,uint32_t timeout){if(!c||port>1||timeout>10000)return RQ_INVALID;try{std::unique_lock lock(c->mutex);const auto end=Clock::now()+std::chrono::milliseconds(timeout);for(;;){auto q=lookup(*c,h);if(!q)return RQ_INVALID;if(q->stopped)return RQ_STATE;auto& p=q->ports[port];auto it=std::min_element(p.begin(),p.end(),[](const Item& a,const Item& b){return a.due<b.due;});auto now=Clock::now();if(it!=p.end()&&it->due<=now){auto item=*it;p.erase(it);++q->active;if(item.operation)item.operation->snapshot.state=RQ_OP_RUNNING;lock.unlock();auto status=call(*c,item,0);lock.lock();--q->active;c->changed.notify_all();return status;}if(now>=end)return RQ_TIMEOUT;c->changed.wait_until(lock,it==p.end()?end:std::min(end,it->due));}}catch(...){return RQ_FAILURE;}}
uint32_t RQ_CALL rq_stop(rq_context* c,rq_handle h){if(!c)return RQ_INVALID;try{std::unique_lock lock(c->mutex);auto q=lookup(*c,h);if(!q)return RQ_INVALID;if(q->canceling)return RQ_STATE;if(q->stopped)return RQ_OK;q->stopped=true;q->canceling=true;std::array<std::vector<Item>,2> pending;pending.swap(q->ports);for(auto& p:pending)for(auto& i:p)if(i.operation)i.operation->snapshot.state=RQ_OP_CANCELING;c->changed.notify_all();lock.unlock();uint32_t status=RQ_OK;for(auto& p:pending)for(auto& i:p)if(call(*c,i,1)!=RQ_OK)status=RQ_FAILURE;lock.lock();q->canceling=false;c->changed.notify_all();return status;}catch(...){return RQ_FAILURE;}}

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
            auto item=*it;p.erase(it);++q->active;c->changed.notify_all();lock.unlock();auto status=call(*c,item,1);lock.lock();--q->active;c->changed.notify_all();return status;}
        return RQ_INVALID;
    }catch(...){return RQ_FAILURE;}
}

uint32_t RQ_CALL rq_operation_submit(rq_context* c,rq_handle h,uint32_t delay,rq_operation_work work,rq_operation_completion completion,void* data,rq_operation* out){
    if(!out)return RQ_INVALID;*out=0;if(!c||!work||delay>60000)return RQ_INVALID;
    try{std::lock_guard lock(c->mutex);auto q=lookup(*c,h);if(!q)return RQ_INVALID;if(q->stopped)return RQ_STATE;
        auto& p=q->ports[RQ_WORK];if(q->operations.size()>=RQ_OPERATION_LIMIT||p.size()>=128||c->next_operation==std::numeric_limits<rq_operation>::max())return RQ_LIMIT;
        auto op=std::make_shared<Operation>();op->id=c->next_operation;op->work=work;op->completion=completion;op->data=data;
        q->operations.emplace(op->id,op);try{p.push_back({Clock::now()+std::chrono::milliseconds(delay),nullptr,nullptr,0,op});}catch(...){q->operations.erase(op->id);throw;}
        *out=c->next_operation++;c->changed.notify_all();return RQ_OK;
    }catch(...){return RQ_FAILURE;}
}
uint32_t RQ_CALL rq_operation_query(rq_context* c,rq_handle h,rq_operation id,rq_operation_snapshot* out){
    if(!c||!out||out->size!=sizeof(*out))return RQ_INVALID;
    try{std::lock_guard lock(c->mutex);auto op=lookup_operation(lookup(*c,h),id);if(!op)return RQ_INVALID;*out=op->snapshot;return RQ_OK;}catch(...){return RQ_FAILURE;}
}
uint32_t RQ_CALL rq_operation_wait(rq_context* c,rq_handle h,rq_operation id,uint32_t timeout,rq_operation_snapshot* out){
    if(!c||!out||out->size!=sizeof(*out)||timeout>10000)return RQ_INVALID;
    try{std::unique_lock lock(c->mutex);const auto end=Clock::now()+std::chrono::milliseconds(timeout);
        for(;;){auto op=lookup_operation(lookup(*c,h),id);if(!op)return RQ_INVALID;if(terminal(op->snapshot.state)){*out=op->snapshot;return RQ_OK;}if(Clock::now()>=end)return RQ_TIMEOUT;c->changed.wait_until(lock,end);}
    }catch(...){return RQ_FAILURE;}
}
uint32_t RQ_CALL rq_operation_cancel(rq_context* c,rq_handle h,rq_operation id){
    if(!c||!id)return RQ_INVALID;
    try{std::unique_lock lock(c->mutex);auto q=lookup(*c,h);auto op=lookup_operation(q,id);if(!op)return RQ_INVALID;if(op->snapshot.state!=RQ_OP_QUEUED)return RQ_STATE;
        auto& p=q->ports[RQ_WORK];auto it=std::find_if(p.begin(),p.end(),[&](const Item& i){return i.operation==op;});if(it==p.end())return RQ_STATE;
        auto item=*it;p.erase(it);++q->active;op->snapshot.state=RQ_OP_CANCELING;c->changed.notify_all();lock.unlock();auto status=call(*c,item,1);lock.lock();--q->active;c->changed.notify_all();return status;
    }catch(...){return RQ_FAILURE;}
}
uint32_t RQ_CALL rq_operation_release(rq_context* c,rq_handle h,rq_operation id){
    if(!c)return RQ_INVALID;
    try{std::lock_guard lock(c->mutex);auto q=lookup(*c,h);auto op=lookup_operation(q,id);if(!op)return RQ_INVALID;if(!terminal(op->snapshot.state)||!op->snapshot.notification_done)return RQ_STATE;q->operations.erase(id);c->changed.notify_all();return RQ_OK;}catch(...){return RQ_FAILURE;}
}
uint32_t RQ_CALL rq_operation_count(rq_context* c,rq_handle h,uint32_t* out){
    if(!c||!out)return RQ_INVALID;
    try{std::lock_guard lock(c->mutex);auto q=lookup(*c,h);if(!q)return RQ_INVALID;*out=static_cast<uint32_t>(q->operations.size());return RQ_OK;}catch(...){return RQ_FAILURE;}
}
