#pragma once
#include <cstdint>
#include <cstddef>
#include <span>
#include <vector>
namespace revenge::pipeline_cache {
// Local, disposable driver-cache envelope. Integrity check is not authentication.
constexpr size_t max_blob=16*1024*1024;
constexpr size_t max_key=1024*1024;
std::vector<uint8_t> pack(std::span<const uint8_t> key,std::span<const uint8_t> blob);
std::vector<uint8_t> unpack(std::span<const uint8_t> key,std::span<const uint8_t> file);
}
