#pragma once
#include "revenge/core.hpp"
namespace revenge {
struct ExportSymbol {uint32_t ordinal{},rva{};std::vector<std::string> names;std::string forwarder;};
struct ExportTable {std::string module;std::vector<ExportSymbol> symbols;};
ExportTable inspect_exports(std::span<const uint8_t>);
ExportTable inspect_exports_file(const std::filesystem::path&);
std::string export_report(const ExportTable&);
// Explicit file inventory only; no search, loading or binding.
std::string dependency_report(const Image&,const std::vector<std::pair<std::string,ExportTable>>&);
std::string dependency_chain_report(const Image&,const std::vector<std::pair<std::string,ExportTable>>&);
}
