#pragma once
#include <cstdint>
#include <cstddef>
#include <span>
#include <string>
#include <vector>
#include <filesystem>
namespace revenge::flow {
// Fixture ABI: v0=dispatch id, s0=threadgroup id, all lanes active.
// Public GFX10.3 subset; no shader container or console bindings.
enum class Op { nop, end, mov, add, or_b, add_f, sub_f, subrev_f, mul_f, eq, ne, gt, ge, lt, le, jump, scc0, scc1, load, store, wait, scalar_add };
struct Operand { bool immediate=true; uint32_t value=0; };
struct Instruction { Op op{}; uint32_t dst=0; Operand a{},b{}; size_t word=0, target=0; };
struct Program { std::vector<Instruction> code; bool buffers=false; bool loops=false; };
Program decode(std::span<const uint32_t>);
Program decode_loop(std::span<const uint32_t>);
std::vector<uint32_t> loop_words();
uint32_t loop_expected(uint32_t id);
int loop_gpu_test();
std::vector<uint32_t> wave_fixture_expected(unsigned width);
std::string wave_fixture_hlsl(unsigned width);
int wave_gpu_test(unsigned width);
Program decode_unified(std::span<const uint32_t>);
uint32_t evaluate_unified(const Program&, uint32_t id, std::span<const uint32_t> input);
std::vector<uint32_t> unified_words();
uint32_t unified_expected(uint32_t id, std::span<const uint32_t> input);
int unified_gpu_test(const std::filesystem::path& cache={});
uint32_t evaluate(const Program&,uint32_t id);
std::string emit(const Program&);
std::string describe(const Program&);
std::vector<uint32_t> fixture_words();
uint32_t fixture_expected(uint32_t id);
int gpu_test();
// Explicit CPU-only lane-mask abstraction. Does not decode wave instructions.
class WaveMask {
 unsigned width_; uint64_t exec_, vcc_=0;
public:
 explicit WaveMask(unsigned width);
 uint64_t exec() const noexcept {return exec_;}
 uint64_t vcc() const noexcept {return vcc_;}
 uint64_t set_exec(uint64_t mask) noexcept;
 void compare_equal(std::span<const uint32_t> a,std::span<const uint32_t> b);
 void masked_write(std::span<uint32_t> dst,std::span<const uint32_t> src) const;
};
}
