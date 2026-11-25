#pragma once
#include "revenge/core.hpp"
namespace revenge {
// Conservative detection, not a certification of Windows compatibility.
bool has_console_markers(const Image& image, const std::filesystem::path& path);
std::vector<std::filesystem::path> game_candidates(const std::filesystem::path& directory);
int launch_dialog();
}
