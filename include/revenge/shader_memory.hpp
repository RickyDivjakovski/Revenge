#pragma once
#include <cstdint>
#include <cstddef>
#include <utility>
#include <span>
#include <string>
#include <vector>
namespace revenge::memory_shader {
// Research ABI: v0 = element index, s[0:3] = read-only buffer, s[4:7] = output.
// Both buffers have stride 4 and 4096 elements. No descriptor bytes are decoded.
inline constexpr uint32_t count=4096;
enum class Op {load, wait, add, store, end};
struct Instruction {Op op; uint32_t dst=0, a=0,b=0,offset=0; bool immediate=false; size_t word=0;};
class Program {
 std::vector<Instruction> code_;
 explicit Program(std::vector<Instruction> c):code_(std::move(c)){}
 friend Program decode(std::span<const uint32_t>);
public:
 const std::vector<Instruction>& code() const noexcept {return code_;}
};
Program decode(std::span<const uint32_t> words);
std::string describe(const Program&);
std::string emit(const Program&);
std::vector<uint32_t> evaluate(const Program&,std::span<const uint32_t> input);
std::vector<uint32_t> fixture_words();
std::vector<uint32_t> fixture_input();
std::vector<uint32_t> fixture_expected(std::span<const uint32_t> input);
int gpu_test();
}
