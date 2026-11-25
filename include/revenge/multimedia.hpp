#pragma once
#include <cstdint>
#include <filesystem>
#include <iosfwd>
#include <span>
#include <vector>
namespace revenge::media {
using Bytes=std::vector<uint8_t>;
struct Stick {float x{},y{};};
Stick normalize_stick(int16_t x,int16_t y,unsigned deadzone);
float normalize_trigger(uint8_t value,unsigned threshold=30);
// 48 kHz stereo signed PCM, interleaved. Gain in Q15 (0..32768).
std::vector<int16_t> mix(std::span<const int16_t> a,std::span<const int16_t> b,unsigned gain_a,unsigned gain_b);
std::vector<int16_t> tone_fixture(unsigned milliseconds=1000);
// Trusted caller-selected local file, one writer, bounded checksummed snapshot.
void save(const std::filesystem::path&,std::span<const uint8_t>);
Bytes load(const std::filesystem::path&);
int storage_fixture(std::ostream&,const std::filesystem::path&,bool write);
void play_pcm(std::span<const int16_t> pcm);
struct Pad {uint32_t connected{},buttons{},packet{};float lx{},ly{},rx{},ry{},lt{},rt{};};
Pad poll_pad(unsigned index);
int audio_test(std::ostream&);
int input_test(std::ostream&,unsigned seconds,bool rumble);
}
