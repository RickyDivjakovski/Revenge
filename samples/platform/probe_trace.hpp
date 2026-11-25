#pragma once
#include "../game/arena_core.hpp"
#include <cstdio>
#include <cstddef>
namespace revenge_probe {
// Same output contract for the independent desktop client and the official GDK source probe.
inline int format_trace(char* dst, std::size_t size, const revenge_arena::Trace& t){
 return std::snprintf(dst,size,"REVENGE-PROBE-V1 collision=%u masks=%u,%u,%u,%u score=%u",unsigned(t[0]),unsigned(t[1]),unsigned(t[2]),unsigned(t[3]),unsigned(t[4]),unsigned(t[5]));
}
}
