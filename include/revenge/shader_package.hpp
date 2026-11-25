#pragma once
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>
namespace revenge::package {
// Independent RSP1 format, not a console container. CRC is integrity only.
struct Programs {std::vector<uint32_t> compute, vertex, pixel; bool operator==(const Programs&) const = default;};
Programs fixture();
Programs color_fixture();
Programs narrow_fixture();
Programs minmax_fixture();
void require_custom(const Programs&);
void validate(const Programs&);
void require_scene_fixture(const Programs&);
std::vector<uint8_t> encode(const Programs&);
Programs decode(std::span<const uint8_t>);
Programs read(const std::filesystem::path&);
void write(const std::filesystem::path&,const Programs&);
std::string describe(const Programs&);
}
