#pragma once
#include "revenge/core.hpp"
#include <iosfwd>
#include <string_view>
namespace revenge {
enum class DependencyKind { console, api_contract, system_candidate, other, invalid };
DependencyKind classify_dependency(std::string_view name);
std::string_view dependency_label(DependencyKind kind);
// Conservative gate; absence of known markers is not proof of compatibility.
bool windows_candidate(const Image&, const std::filesystem::path&);
int audit_runtime(const std::filesystem::path&, std::ostream&);
// Only for trusted, authorised Windows executables. Uses the native OS loader.
// Process lifetime control is NOT a security sandbox.
int run_windows(const std::filesystem::path&, unsigned timeout_seconds, bool diagnostics, std::ostream&);
}
