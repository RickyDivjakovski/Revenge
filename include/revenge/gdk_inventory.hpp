#pragma once
#include "revenge/core.hpp"
#include <iosfwd>
#include <string_view>
namespace revenge::gaming {
struct Api { std::string_view symbol, area, state; };
std::span<const Api> catalog() noexcept;
void report(const Image&,std::ostream&);
}
