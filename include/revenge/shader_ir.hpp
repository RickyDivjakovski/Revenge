#pragma once
#include <cstdint>
#include <filesystem>
#include <span>
#include <string>
#include <vector>
namespace revenge::shader {
// Independent straight-line GFX10.3 ALU fixture; no console shader ABI.
enum class Op { mov, add, sub, subrev, mul24, min_u, max_u, shr, shl, and_b, or_b, xor_b, xnor_b };
struct Value { bool immediate; uint32_t value; }; // otherwise register: v0..v255, s0..s105 = 256..361
struct Instruction { Op op; uint32_t dst; Value a,b; size_t word; };
struct Program { std::vector<Instruction> instructions; };
Program decode(std::span<const uint32_t> words);
std::vector<uint32_t> read_words(const std::filesystem::path&);
Program optimize(const Program&); // only observable fixture result v0
uint32_t evaluate(const Program&, uint32_t input);
std::string hlsl(const Program&);
std::string hlsl_function(const Program&);
std::string describe(const Program&);
std::vector<uint32_t> fixture_words();
int gpu_shader_fixture();
}
