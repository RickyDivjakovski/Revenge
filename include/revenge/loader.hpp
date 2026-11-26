#pragma once
#include "revenge/core.hpp"
namespace revenge {
// Non-executable, in-memory PE layout for research. Never invokes target code.
struct ImportSlot {std::string dll, symbol; uint32_t rva;bool by_ordinal{};};
struct LoadPlan {
 Image image;
 uint64_t mapped_base{};
 uint32_t header_bytes{};
 std::vector<uint8_t> mapped;
 std::vector<ImportSlot> slots;
 uint64_t relocations{}, unwind_entries{}, tls_bytes{}, tls_zero_fill{}, tls_callbacks{};
 bool delay_imports{}, load_config{};
};
LoadPlan prepare_image(std::span<const uint8_t> bytes, uint64_t new_base=0);
LoadPlan prepare_file(const std::filesystem::path& path,uint64_t new_base=0);
struct PageSpan {uint32_t rva,bytes;uint8_t flags; bool operator==(const PageSpan&) const = default;};
// Flags: 1 read, 2 write, 4 execute, 8 occupied. Diagnostic only; never applies protections.
std::vector<PageSpan> page_layout(const LoadPlan&);
std::string init_metadata_report(const LoadPlan&);
// Offline TLS template model only: no OS TLS slots, callbacks or executable mappings.
struct TlsTemplate {bool present{}; uint32_t raw_rva{}, index_rva{}, alignment{1}, zero_fill{}; std::vector<uint8_t> initial;};
TlsTemplate tls_template(const LoadPlan&);
std::vector<uint8_t> tls_instance(const TlsTemplate&);
std::string tls_template_report(const LoadPlan&);
std::string layout_report(const LoadPlan&);
std::string import_plan_report(const LoadPlan&);
std::string preparation_report(const LoadPlan& plan,const std::filesystem::path& exe);
}
