#pragma once
#include <cstddef>
#include <cstdint>
#include <iosfwd>
#include <memory>
#include <span>
namespace revenge::vm {
enum class Access : uint8_t { reserved, no_access, read_only, read_write };
struct Handle { uint64_t offset{}, generation{}; bool operator==(const Handle&) const = default; };
struct Stats { uint64_t capacity{}, reserved{}, committed{}, peak_committed{}, free_bytes{}, largest_free{}, live{}, retired{}, completed_fence{}; };
// Owns a private host virtual address arena. No foreign pointers, executable pages,
// console ABI or GPU mappings. All public operations serialize internally.
class Arena {
    struct Impl; std::unique_ptr<Impl> impl_;
public:
    Arena(uint64_t capacity, uint64_t commit_budget);
    ~Arena();
    Arena(const Arena&)=delete; Arena& operator=(const Arena&)=delete;
    size_t page_size() const noexcept;
    Handle reserve(uint64_t bytes, uint64_t alignment=0);
    void commit(Handle, uint64_t offset, uint64_t bytes, Access=Access::read_write);
    void protect(Handle, uint64_t offset, uint64_t bytes, Access);
    void decommit(Handle, uint64_t offset, uint64_t bytes);
    void release(Handle);
    void read(Handle, uint64_t offset, std::span<uint8_t>);
    void write(Handle, uint64_t offset, std::span<const uint8_t>);
    Access page_access(Handle, uint64_t offset);
    bool aligned(Handle, uint64_t alignment);
    // Single logical timeline, caller supplies completed fence values. Not a GPU fence.
    void retire(Handle, uint64_t fence);
    uint64_t collect(uint64_t completed_fence);
    Stats stats();
};
int benchmark(std::ostream&, unsigned mib, unsigned threads);
}
