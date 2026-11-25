#include "revenge/shader_package.hpp"
#include "revenge/shader_ir.hpp"
#include "revenge/shader_stage.hpp"
#include <array>
#include <fstream>
#include <sstream>
#include <stdexcept>
namespace revenge::package {namespace {
constexpr size_t max_words=4096, header=32, max_bytes=header+3*max_words*4;
void put(std::vector<uint8_t>& b,uint32_t n){for(unsigned i=0;i<4;++i)b.push_back(uint8_t(n>>(8*i)));}
uint32_t get(std::span<const uint8_t> b,size_t p){if(p>b.size()||b.size()-p<4)throw std::runtime_error("RSP1: truncated word");uint32_t n=0;for(unsigned i=0;i<4;++i)n|=uint32_t(b[p+i])<<(8*i);return n;}
uint32_t crc(std::span<const uint8_t> b){uint32_t v=~0u;for(auto x:b){v^=x;for(unsigned i=0;i<8;++i)v=(v>>1)^(0xedb88320u & (0u-(v&1u)));}return ~v;}
void sizes(const Programs& p){for(auto* s:{&p.compute,&p.vertex,&p.pixel})if(s->empty()||s->size()>max_words)throw std::runtime_error("RSP1: invalid stage length");}
}
Programs fixture(){return {shader::fixture_words(),stage::fixture(stage::Stage::vertex),stage::fixture(stage::Stage::pixel)};}
void validate(const Programs& p){sizes(p);(void)shader::hlsl(shader::optimize(shader::decode(p.compute)));(void)stage::emit(stage::Stage::vertex,p.vertex);(void)stage::emit(stage::Stage::pixel,p.pixel);}
Programs color_fixture(){auto p=fixture();p.pixel.insert(p.pixel.begin()+4,{0xf0900f08u,0x00400800u,0xbf8c3f70u});p.pixel[8]=0x0b0a0908u;return p;} // second ordered sample; RGBA export
// v_mul_f32 v0, 0.5, v0; transforms clip-space x before POS0 export.
Programs narrow_fixture(){auto p=fixture();p.vertex.insert(p.vertex.begin(),{0x100000ffu,0x3f000000u});return p;}
// Scale x by 0.5, then select it over x+1 and x-1. The min/max
// path must preserve the scaled geometry for the independent framebuffer oracle.
Programs minmax_fixture(){auto p=fixture();p.vertex.insert(p.vertex.begin(),{0x100000f0u,0x060e00f2u,0x1e000f00u,0x060e00f3u,0x20000f00u});return p;}
void require_custom(const Programs& p){validate(p);auto f=fixture();if(p.compute!=f.compute)throw std::runtime_error("RSP1 custom requires fixed compute stage");}
void require_scene_fixture(const Programs& p){validate(p);if(p!=fixture())throw std::runtime_error("RSP1: GPU scene accepts only the known-answer fixture programs");}
std::vector<uint8_t> encode(const Programs& p){validate(p);std::vector<uint8_t> payload;for(auto* s:{&p.compute,&p.vertex,&p.pixel})for(auto w:*s)put(payload,w);std::vector<uint8_t> b;put(b,0x31505352u);put(b,1);put(b,1);put(b,uint32_t(p.compute.size()));put(b,uint32_t(p.vertex.size()));put(b,uint32_t(p.pixel.size()));put(b,0); // reserved
 // Protect the metadata as well as the payload. CRC field is not included.
 auto covered=b;covered.insert(covered.end(),payload.begin(),payload.end());put(b,crc(covered));b.insert(b.end(),payload.begin(),payload.end());return b;}
Programs decode(std::span<const uint8_t> b){if(b.size()<header||b.size()>max_bytes||get(b,0)!=0x31505352u||get(b,4)!=1||get(b,8)!=1||get(b,24)!=0)throw std::runtime_error("RSP1: invalid header or unsupported version/ABI");std::array<uint32_t,3> counts{get(b,12),get(b,16),get(b,20)};size_t total=header;for(auto n:counts){if(!n||n>max_words)throw std::runtime_error("RSP1: invalid stage length");total+=size_t(n)*4;}if(total!=b.size())throw std::runtime_error("RSP1: length mismatch");std::vector<uint8_t> covered(b.begin(),b.begin()+28);covered.insert(covered.end(),b.begin()+header,b.end());if(crc(covered)!=get(b,28))throw std::runtime_error("RSP1: checksum mismatch");Programs p;size_t pos=header,i=0;for(auto* s:{&p.compute,&p.vertex,&p.pixel}){for(uint32_t n=0;n<counts[i];++n,pos+=4)s->push_back(get(b,pos));++i;}validate(p);return p;}
Programs read(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("RSP1: cannot open package");auto n=f.tellg();if(n<0||n>std::streamoff(max_bytes))throw std::runtime_error("RSP1: invalid file size");std::vector<uint8_t> b(static_cast<size_t>(n));f.seekg(0);if(!f.read(reinterpret_cast<char*>(b.data()),n))throw std::runtime_error("RSP1: read failed");return decode(b);}
void write(const std::filesystem::path& path,const Programs& p){auto b=encode(p);if(std::filesystem::exists(path))throw std::runtime_error("RSP1: output exists; choose a new file");std::ofstream f(path,std::ios::binary);if(!f.write(reinterpret_cast<const char*>(b.data()),std::streamsize(b.size())))throw std::runtime_error("RSP1: write failed");f.close();if(!f)throw std::runtime_error("RSP1: close failed");}
std::string describe(const Programs& p){validate(p);std::ostringstream o;o<<"RSP1 version 1, independent ABI 1; GFX10.3 subset\nCompute words: "<<p.compute.size()<<"; vertex words: "<<p.vertex.size()<<"; pixel words: "<<p.pixel.size()<<"\nKnown scene fixture: "<<(p==fixture()?"yes":"no")<<"\n";return o.str();}
}
