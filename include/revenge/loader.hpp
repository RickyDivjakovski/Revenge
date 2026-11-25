#pragma once
#include "revenge/core.hpp"
namespace revenge {
// Non-executable, in-memory PE layout for research. Never invokes target code.
struct ImportSlot {std::string dll, symbol; uint32_t rva;};
struct LoadPlan {
 Image image;
 uint64_t mapped_base{};
 std::vector<uint8_t> mapped;
 std::vector<ImportSlot> slots;
 uint64_t relocations{}, unwind_entries{}, tls_bytes{}, tls_zero_fill{}, tls_callbacks{};
 bool delay_imports{}, load_config{};
};
LoadPlan prepare_image(std::span<const uint8_t> bytes, uint64_t new_base=0);
LoadPlan prepare_file(const std::filesystem::path& path,uint64_t new_base=0);
std::string preparation_report(const LoadPlan& plan,const std::filesystem::path& exe);
}
