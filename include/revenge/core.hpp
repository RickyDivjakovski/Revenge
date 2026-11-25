#pragma once
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>
namespace revenge {
struct Import {std::string dll; std::vector<std::string> symbols;};
struct Section {std::string name; uint32_t rva, virtual_size, raw, raw_size;};
struct Image {uint16_t machine, subsystem; uint32_t entry, image_size; uint64_t base; bool tls, unwind, reloc; std::vector<Section> sections; std::vector<Import> imports;};
Image inspect(std::span<const uint8_t> bytes);
Image inspect_file(const std::filesystem::path& path);
struct Range {uint64_t offset, size; bool operator==(const Range&) const = default;};
// Per-resource dirty tracking: writes are recorded by the caller; no OS page hooks.
class DirtyPages {
 uint64_t length_, page_; std::vector<uint64_t> words_;
public:
 DirtyPages(uint64_t length, uint64_t page=4096);
 void mark(uint64_t offset,uint64_t size);
 std::vector<Range> ranges() const;
 void clear() noexcept;
};
int gpu_info();
int gpu_compute(const std::filesystem::path& shader);
int gpu_bench(unsigned frames,unsigned slots,unsigned mib);
}
