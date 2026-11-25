#pragma once
#include <array>
#include <cstdint>
#include <span>
#include <string>
#include <vector>
namespace revenge::stage {
enum class Stage { vertex, pixel };
struct Result { std::array<float,4> position{}, parameter{}, color{}; };
// Fixed independent ABI: VS v0..4 = x,y,z,u,v; PS v0..1 = interpolated u,v.
std::vector<uint32_t> fixture(Stage);
std::string describe(Stage,std::span<const uint32_t>);
std::string emit(Stage,std::span<const uint32_t>);
Result evaluate(Stage,std::span<const uint32_t>,std::span<const float>);
}
