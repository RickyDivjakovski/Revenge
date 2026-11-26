#pragma once
#include "revenge/exports.hpp"
namespace revenge {
struct DependencyFile { std::string name; Image image; ExportTable exports; };
struct DependencyAudit {size_t reachable{}, supplied{}, resolved{}, missing_modules{}, missing_symbols{}, malformed{}, forwarder_cycles{}, depth_limits{}, module_cycles{}; std::string report;};
// Explicit offline inventory. No module loading, function addresses or signature inference.
DependencyAudit audit_dependencies(const DependencyFile& root, const std::vector<DependencyFile>& files);
}
