#pragma once
#include "revenge/core.hpp"
#include <utility>
namespace revenge {
// Static direct-import reachability over an explicit file inventory only.
std::string dependency_graph_report(const std::string& root,const Image& image,const std::vector<std::pair<std::string,Image>>& supplied);
}
