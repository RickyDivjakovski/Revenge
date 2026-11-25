#include "revenge/shader_stage.hpp"
#include "revenge/scene.hpp"
#include <algorithm>
#include <bit>
#include <bitset>
#include <cmath>
#include <sstream>
#include <stdexcept>
namespace revenge::stage { namespace {
enum class Op { mov, add, sub, subrev, mul, minimum, maximum, sample, wait, pos, param, color, end };
struct Inst { Op op{}; unsigned dst=0,a=0,b=0; bool imm=false; uint32_t bits=0; std::array<unsigned,4> src{}; size_t word=0; };
[[noreturn]] void fail(size_t n,const char* why){throw std::invalid_argument("graphics shader word "+std::to_string(n)+": "+why);}
const char* name(Op o){switch(o){case Op::mov:return "v_mov_b32";case Op::add:return "v_add_f32";case Op::sub:return "v_sub_f32";case Op::subrev:return "v_subrev_f32";case Op::mul:return "v_mul_f32";case Op::minimum:return "v_min_f32";case Op::maximum:return "v_max_f32";case Op::sample:return "image_sample_l";case Op::wait:return "s_waitcnt vmcnt(0)";case Op::pos:return "exp pos0";case Op::param:return "exp param0";case Op::color:return "exp mrt0";case Op::end:return "s_endpgm";}throw std::logic_error("stage opcode");}
std::string reg(unsigned n){return "v"+std::to_string(n);}
std::vector<Inst> decode(Stage stage,std::span<const uint32_t> words){
 if(words.empty()||words.size()>256)fail(0,"invalid size");
 bool vertex=stage==Stage::vertex; std::bitset<256> defined,pending,zero; for(unsigned r=0;r<(vertex?5u:2u);++r)defined.set(r);
 bool pos=false,param=false,color=false,done=false,ended=false;unsigned sampled=0;std::vector<Inst> code;
 for(size_t i=0;i<words.size();){size_t at=i;auto x=words[i++];Inst a{};a.word=at;
  auto next=[&]{if(i>=words.size())fail(at,"truncated instruction");return words[i++];};
  auto use=[&](unsigned r){if(!defined.test(r)||pending.test(r))fail(at,"undefined or pending VGPR");};
  auto write=[&](unsigned r){if(pending.test(r))fail(at,"overwrite pending VGPR");defined.set(r);zero.reset(r);};
  auto source=[&](unsigned r){if(r>=256){a.a=r-256;use(a.a);}else if(r>=128&&r<=192){a.imm=true;a.bits=r-128;}else if(r>=193&&r<=208){a.imm=true;a.bits=uint32_t(0)-uint32_t(r-192);}else if(r>=240&&r<=247){constexpr std::array<float,8> constants{.5f,-.5f,1.f,-1.f,2.f,-2.f,4.f,-4.f};a.imm=true;a.bits=std::bit_cast<uint32_t>(constants[r-240]);}else if(r==255){a.imm=true;a.bits=next();}else fail(at,"unsupported source selector");};
  if(ended)fail(at,"instruction after end");
  if(x==0xbf810000u){if(!done||pending.any()||(vertex?!(pos&&param):!color))fail(at,"incomplete stage exports");a.op=Op::end;ended=true;}
  else if(done)fail(at,"instruction after final export");
  else if((x&0xfc000000u)==0xf8000000u){
   if((x&0xffffe00fu)!=0xf800000fu||(x&0x400u))fail(at,"unsupported export encoding or mask");
   unsigned target=(x>>4)&63;bool last=(x&0x800u)!=0,vm=(x&0x1000u)!=0;auto y=next();for(unsigned j=0;j<4;++j){a.src[j]=(y>>(8*j))&255;use(a.src[j]);}
   if(vertex&&target==32&&!param&&!last&&!vm){a.op=Op::param;param=true;}
   else if(vertex&&target==12&&!pos&&param&&last&&!vm){a.op=Op::pos;pos=true;done=true;}
   else if(!vertex&&target==0&&!color&&last&&vm&&sampled){a.op=Op::color;color=true;done=true;}
   else fail(at,"unsupported stage export order or flags");
  }else if(x==0xf0900f08u){ // MIMG SAMPLE_L, 2D, dmask=15, contiguous 32-bit VGPRs
   if(vertex||sampled>=8||pending.any())fail(at,"unsupported sample sequence");
   auto y=next();if((y&0xffff0000u)!=0x00400000u)fail(at,"unsupported image/sampler descriptors");a.op=Op::sample;a.a=y&255;a.dst=(y>>8)&255;if(a.a>253||a.dst>252)fail(at,"sample register range");for(unsigned j=0;j<3;++j)use(a.a+j);if(!zero.test(a.a+2))fail(at,"fixture requires statically zero LOD");for(unsigned j=0;j<4;++j){write(a.dst+j);pending.set(a.dst+j);}++sampled;
  }else if(x==0xbf8c3f70u){if(!pending.any())fail(at,"unexpected wait");pending.reset();a.op=Op::wait;}
  else if((x&0xfe000000u)==0x7e000000u){if(((x>>9)&255)!=1)fail(at,"unsupported VOP1");a.op=Op::mov;a.dst=(x>>17)&255;source(x&511);bool z=a.imm?(a.bits&0x7fffffffu)==0:zero.test(a.a);write(a.dst);zero.set(a.dst,z);}
  else if(!(x&0x80000000u)){switch((x>>25)&63){case 3:a.op=Op::add;break;case 4:a.op=Op::sub;break;case 5:a.op=Op::subrev;break;case 8:a.op=Op::mul;break;case 15:a.op=Op::minimum;break;case 16:a.op=Op::maximum;break;default:fail(at,"unsupported VOP2");}a.dst=(x>>17)&255;a.b=(x>>9)&255;source(x&511);use(a.b);write(a.dst);}
  else fail(at,"unsupported instruction encoding");
  code.push_back(a);
 }
 if(!ended)fail(words.size(),"missing endpgm");
 return code;
}
std::string operand(const Inst& i){return i.imm?"asfloat("+std::to_string(i.bits)+"u)":reg(i.a);}
std::string symbol(Op o){return o==Op::mul?" * ":o==Op::add?" + ":" - ";}
std::string vec(const Inst& i){return "float4("+reg(i.src[0])+", "+reg(i.src[1])+", "+reg(i.src[2])+", "+reg(i.src[3])+")";}
}
std::vector<uint32_t> fixture(Stage s){
 if(s==Stage::vertex)return {0x100c04ffu,0x40800000u,0x0a0c0cffu,0x40000000u,0x100c0cffu,0x3e924925u,0x7e0a0280u,0xf800020fu,0x05050403u,0xf80008cfu,0x02060100u,0xbf810000u};
 return {0x7e040280u,0xf0900f08u,0x00400400u,0xbf8c3f70u,0xf800180fu,0x07040506u,0xbf810000u}; // export B,G,R,A
}
std::string describe(Stage s,std::span<const uint32_t> w){std::ostringstream o;for(auto i:decode(s,w)){o<<"word "<<i.word<<": "<<name(i.op);if(i.op==Op::sample)o<<" v["<<i.dst<<':'<<i.dst+3<<"], v["<<i.a<<':'<<i.a+2<<"], s[0:7], s[8:11] dmask:0xf dim:2D";else if(i.op==Op::pos||i.op==Op::param||i.op==Op::color){for(auto r:i.src)o<<' '<<reg(r);}else if(i.op!=Op::wait&&i.op!=Op::end){o<<' '<<reg(i.dst)<<", "<<operand(i);if(i.op!=Op::mov)o<<", "<<reg(i.b);}o<<'\n';}return o.str();}
std::string emit(Stage s,std::span<const uint32_t> w){auto code=decode(s,w);bool vertex=s==Stage::vertex;std::ostringstream o;
 o<<"struct V { float4 p : SV_Position; float4 attr : TEXCOORD0; };\n";
 if(vertex)o<<"V vertex(float3 p : POSITION, float2 uv : TEXCOORD0) {\n V outv;\n float v0=p.x,v1=p.y,v2=p.z,v3=uv.x,v4=uv.y;\n";
 else o<<"Texture2D<float4> tex : register(t0);\nSamplerState pointSampler : register(s0);\nfloat4 pixel(V i) : SV_Target {\n float v0=i.attr.x,v1=i.attr.y;\n float4 color;\n";
 std::bitset<256> declared;for(unsigned r=0;r<(vertex?5u:2u);++r)declared.set(r);auto dest=[&](unsigned r){if(!declared.test(r)){o<<" precise float ";declared.set(r);}else o<<' ';o<<reg(r)<<" = ";};
 for(auto i:code){switch(i.op){case Op::mov:dest(i.dst);o<<operand(i)<<";\n";break;case Op::add:case Op::sub:case Op::subrev:case Op::mul:{auto a=operand(i),b=reg(i.b);if(i.op==Op::subrev)std::swap(a,b);dest(i.dst);o<<a<<symbol(i.op)<<b<<";\n";break;}
 case Op::minimum:case Op::maximum:dest(i.dst);o<<(i.op==Op::minimum?"min(":"max(")<<operand(i)<<", "<<reg(i.b)<<");\n";break;
 case Op::sample:o<<" float4 sampled_"<<i.word<<" = tex.SampleLevel(pointSampler,float2("<<reg(i.a)<<','<<reg(i.a+1)<<"),"<<reg(i.a+2)<<");\n";for(unsigned j=0;j<4;++j){dest(i.dst+j);o<<"sampled_"<<i.word<<"["<<j<<"];\n";}break;
 case Op::wait:break;case Op::pos:o<<" outv.p = "<<vec(i)<<";\n";break;case Op::param:o<<" outv.attr = "<<vec(i)<<";\n";break;case Op::color:o<<" color = "<<vec(i)<<";\n";break;case Op::end:o<<(vertex?" return outv;\n":" return color;\n");break;}}
 o<<"}\n";return o.str();}
Result evaluate(Stage s,std::span<const uint32_t> w,std::span<const float> input){auto code=decode(s,w);if(input.size()!=(s==Stage::vertex?5u:2u))throw std::invalid_argument("stage input size");std::array<float,256> r{};for(float v:input)if(!std::isfinite(v))throw std::invalid_argument("nonfinite stage input");std::copy(input.begin(),input.end(),r.begin());Result result;
 for(auto i:code){auto a=i.imm?std::bit_cast<float>(i.bits):r[i.a],b=r[i.b];switch(i.op){case Op::mov:r[i.dst]=a;break;case Op::add:r[i.dst]=a+b;break;case Op::sub:r[i.dst]=a-b;break;case Op::subrev:r[i.dst]=b-a;break;case Op::mul:r[i.dst]=a*b;break;case Op::minimum:case Op::maximum:if(!std::isfinite(a)||!std::isfinite(b))throw std::invalid_argument("min/max requires finite operands in reference evaluator");r[i.dst]=i.op==Op::minimum?std::min(a,b):std::max(a,b);break;case Op::sample:{if(r[i.a+2]!=0||!std::isfinite(r[i.a])||!std::isfinite(r[i.a+1]))throw std::invalid_argument("only finite UV and LOD zero fixture");unsigned idx=(r[i.a]>=.5f?1u:0u)+(r[i.a+1]>=.5f?2u:0u);for(unsigned j=0;j<4;++j)r[i.dst+j]=scene3d::palette[idx][j]/255.f;break;}case Op::pos:case Op::param:case Op::color:{auto& out=i.op==Op::pos?result.position:i.op==Op::param?result.parameter:result.color;for(unsigned j=0;j<4;++j)out[j]=r[i.src[j]];break;}default:break;}}
 return result;}
}
