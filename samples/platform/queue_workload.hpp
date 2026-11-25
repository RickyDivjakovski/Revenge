#pragma once
#include "revenge/manual_queue.hpp"
#include <array>
#include <cstdint>
#include <ostream>
namespace revenge::gaming {
struct QueueWorkload {
    struct State { unsigned count=0, canceled=0; uint64_t checksum=0; };
    struct Item { State* state=nullptr; unsigned value=0; };
    std::array<State,2> states{};
    std::array<Item,64> items{};
    static void callback(void* context,bool canceled) {
        auto& item=*static_cast<Item*>(context); auto& s=*item.state;
        if(canceled) { ++s.canceled; return; }
        ++s.count; s.checksum+=item.value;
    }
    template<class Queue> bool run(Queue& q) {
        if(q.dispatch(Port::work)||q.dispatch(Port::completion)) return false;
        for(unsigned i=0;i<32;++i) {
            items[2*i]={&states[0],i+1}; items[2*i+1]={&states[1],100+i};
            if(!q.submit(Port::work,&items[2*i],callback)||!q.submit(Port::completion,&items[2*i+1],callback)) return false;
        }
        if(states[0].count||states[1].count) return false;
        for(unsigned i=0;i<32;++i) if(!q.dispatch(Port::work)) return false;
        if(q.dispatch(Port::work)||states[1].count||states[0].count!=32||states[0].checksum!=528) return false;
        for(unsigned i=0;i<32;++i) if(!q.dispatch(Port::completion)) return false;
        return !q.dispatch(Port::completion)&&states[1].count==32&&states[1].checksum==3696&&!states[0].canceled&&!states[1].canceled;
    }
    void print(std::ostream& out) const {out<<"REVENGE-QUEUE-V1 work="<<states[0].count<<" completion="<<states[1].count<<" sums="<<states[0].checksum<<','<<states[1].checksum<<" canceled="<<states[0].canceled+states[1].canceled<<'\n';}
};
}
