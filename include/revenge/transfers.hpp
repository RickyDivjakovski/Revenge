#pragma once
#include <cstdint>
#include <cstddef>
#include <span>
#include <vector>
namespace revenge::transfer {
// Single producer, one queue, fixed buffer triplets. Not a console API.
enum class Phase { idle, recording, copy_dest, copy_source, submitted, released };
struct Slot { Phase phase=Phase::idle; uint64_t fence=0; bool retire=false; };
class Pool {
 std::vector<Slot> slots_; uint64_t bytes_, issued_=0, completed_=0, live_;
 Slot& get(unsigned);
public:
 Pool(unsigned slots,uint64_t bytes_per_buffer,uint64_t logical_budget);
 const Slot& slot(unsigned) const;
 void begin(unsigned);
 void upload(unsigned);
 void download(unsigned);
 uint64_t submit(unsigned,bool retire=false);
 void complete(uint64_t);
 // Caller must verify the readback before consuming. Returns true if retired.
 bool consume(unsigned);
 uint64_t live_bytes() const noexcept{return live_;}
 uint64_t issued() const noexcept{return issued_;}
 uint64_t completed() const noexcept{return completed_;}
};
void pattern(std::span<uint8_t>,uint64_t sequence);
bool matches(std::span<const uint8_t>,uint64_t sequence);
int gpu_test(unsigned rounds,unsigned slots,unsigned mib);
}
