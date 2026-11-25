#pragma once
#include "revenge/core.hpp"
#include <string_view>
#include <iosfwd>
namespace revenge {
// Experimental, CPU-only model. Page indices, not host addresses or Xbox ABI.
// Callers must serialize access, including dirty snapshot/reset.
class MemoryArena {
    size_t page_, used_=0;
    std::vector<uint8_t> bytes_;
    std::vector<uint32_t> refs_, map_;
    std::vector<bool> allocated_;
    DirtyPages dirty_;
    void validate(size_t offset,size_t count) const;
public:
    MemoryArena(size_t physical_pages,size_t virtual_pages,size_t page=4096);
    std::vector<uint32_t> allocate(size_t count);
    void release(std::span<const uint32_t> pages);
    void map(size_t virtual_page,std::span<const uint32_t> pages);
    void unmap(size_t virtual_page,size_t count);
    void write(size_t offset,std::span<const uint8_t> data);
    void read(size_t offset,std::span<uint8_t> data) const;
    size_t used_pages() const noexcept{return used_;}
    std::vector<Range> dirty_ranges() const{return dirty_.ranges();}
    void clear_dirty() noexcept{dirty_.clear();}
};
struct ApiEntry {std::string_view dll, area, state;};
std::span<const ApiEntry> api_registry() noexcept;
bool auditable_contract(std::string_view name);
// Lexical mapping only. No IO, no symlink/reparse protection, no sandbox.
std::filesystem::path translate_path(const std::filesystem::path& root,std::string_view path);
struct TuneSample {unsigned slots; double throughput;};
unsigned choose_slots(std::span<const TuneSample> samples, double fraction=0.95);
unsigned slot_budget(uint64_t free_bytes,uint64_t transfer_bytes);
int cpu_info(std::ostream& out);
int gpu_tune(unsigned frames,unsigned mib);
}
