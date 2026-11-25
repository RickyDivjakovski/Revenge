#include "revenge/shader_memory.hpp"
#include <array>
#include <bitset>
#include <sstream>
#include <stdexcept>
#include <utility>
namespace revenge::memory_shader {
namespace {
[[noreturn]] void fail(size_t at,const char* why){throw std::runtime_error("buffer fixture word "+std::to_string(at)+": "+why);}
constexpr uint32_t load_word=0xe0302000u,store_word=0xe0702000u,wait_word=0xbf8c3f70u,end_word=0xbf810000u;
}
Program decode(std::span<const uint32_t> w){
 if(w.empty()||w.size()>256)fail(0,"expected 1..256 dwords");
 std::vector<Instruction> code;std::bitset<256> defined,pending;defined.set(0);bool stored=false,ended=false;
 for(size_t i=0;i<w.size();){const auto at=i;uint32_t x=w[i++];Instruction a{};a.word=at;
  auto read=[&](unsigned r){if(!defined.test(r)||pending.test(r))fail(at,"undefined register or load not waited on");};
  if(x==end_word){if(i!=w.size()||!stored||pending.any())fail(at,"expected one final store, no pending loads or trailing words");a.op=Op::end;ended=true;}
  else if(stored)fail(at,"only endpgm allowed after store");
  else if((x&0xfffff000u)==load_word||(x&0xfffff000u)==store_word){
   if(i==w.size())fail(at,"truncated MUBUF");
   const auto y=w[i++];const bool store=(x&0xfffff000u)==store_word;
   // idxen only, v0 index, inline zero scalar offset, fixed descriptor groups.
   if((y&0xffff00ffu)!=(store?0x80010000u:0x80000000u))fail(at,"unsupported descriptor, address, flags or scalar offset");
   a.dst=(y>>8)&255u;a.offset=x&4095u;if(a.offset%4|| (store&&a.offset))fail(at,"unsupported byte offset");
   if(store){read(a.dst);if(pending.any())fail(at,"outstanding loads at store");a.op=Op::store;stored=true;}
   else{if(!a.dst||pending.test(a.dst))fail(at,"immutable index or pending destination");a.op=Op::load;pending.set(a.dst);defined.reset(a.dst);}
  }else if(x==wait_word){if(!pending.any())fail(at,"wait without outstanding load");a.op=Op::wait;defined|=pending;pending.reset();}
  else if((x&0xfe000000u)==0x4a000000u){a.op=Op::add;a.dst=(x>>17)&255u;a.b=(x>>9)&255u;auto s=x&511u;
   if(!a.dst||pending.test(a.dst))fail(at,"immutable index or pending destination");
   read(a.b);
   if(s>=256){a.a=s-256;read(a.a);}else if(s>=128&&s<=192){a.immediate=true;a.a=s-128;}else if(s>=193&&s<=208){a.immediate=true;a.a=0u-(s-192);}else if(s==255){if(i==w.size())fail(at,"missing literal");a.immediate=true;a.a=w[i++];}else fail(at,"unsupported add source");defined.set(a.dst);
  }else fail(at,"unsupported instruction or modifier");
  code.push_back(a);
 }
 if(!ended)fail(w.size(),"missing endpgm");
 return Program(std::move(code));
}
std::string describe(const Program& p){std::ostringstream s;for(auto a:p.code()){s<<"word "<<a.word<<": ";switch(a.op){case Op::load:s<<"buffer_load_dword v"<<a.dst<<", v0, s[0:3], 0 idxen offset:"<<a.offset;break;case Op::wait:s<<"s_waitcnt vmcnt(0)";break;case Op::add:s<<"v_add_nc_u32 v"<<a.dst<<", "<<(a.immediate?"":"v")<<a.a<<", v"<<a.b;break;case Op::store:s<<"buffer_store_dword v"<<a.dst<<", v0, s[4:7], 0 idxen";break;case Op::end:s<<"s_endpgm";break;}s<<'\n';}return s.str();}
std::string emit(const Program& p){std::ostringstream s;s<<"// Restricted buffer fixture ABI; not a console descriptor decoder.\nStructuredBuffer<uint> sourceData : register(t0);\nRWStructuredBuffer<uint> outputData : register(u0);\n[numthreads(64,1,1)]\nvoid main(uint3 id : SV_DispatchThreadID) {\n  if (id.x >= 4096u) return;\n  uint v0 = id.x;\n";std::bitset<256> declared;declared.set(0);
 for(auto a:p.code()){switch(a.op){case Op::load:case Op::add:{if(!declared.test(a.dst)){s<<"  uint v"<<a.dst<<";\n";declared.set(a.dst);}if(a.op==Op::load){s<<"  v"<<a.dst<<" = 0u;\n  if (v0 + "<<a.offset/4<<"u < 4096u) v"<<a.dst<<" = sourceData[v0 + "<<a.offset/4<<"u];\n";}else{s<<"  v"<<a.dst<<" = ";if(a.immediate)s<<a.a<<"u";else s<<"v"<<a.a;s<<" + v"<<a.b<<";\n";}break;}case Op::wait:s<<"  // Load dependencies already explicit in HLSL.\n";break;case Op::store:s<<"  outputData[v0] = v"<<a.dst<<";\n";break;case Op::end:break;}}
 s<<"}\n";return s.str();}
std::vector<uint32_t> evaluate(const Program& p,std::span<const uint32_t> in){if(in.size()!=count)throw std::invalid_argument("expected 4096 input elements");std::vector<uint32_t> out(count);for(uint32_t id=0;id<count;++id){std::array<uint32_t,256> v{};v[0]=id;for(auto a:p.code()){switch(a.op){case Op::load:{auto index=id+a.offset/4;v[a.dst]=index<count?in[index]:0;break;}case Op::add:v[a.dst]=(a.immediate?a.a:v[a.a])+v[a.b];break;case Op::store:out[id]=v[a.dst];break;case Op::wait:case Op::end:break;}}}return out;}
std::vector<uint32_t> fixture_words(){return {load_word,0x80000100u,load_word|4u,0x80000200u,wait_word,0x4a020501u,store_word,0x80010100u,end_word};}
std::vector<uint32_t> fixture_input(){std::vector<uint32_t> v(count);for(uint32_t i=0;i<count;++i)v[i]=(i*0x9e3779b9u)^0xa5a5a5a5u;return v;}
std::vector<uint32_t> fixture_expected(std::span<const uint32_t> in){if(in.size()!=count)throw std::invalid_argument("expected 4096 input elements");std::vector<uint32_t> out(count);for(uint32_t i=0;i<count;++i)out[i]=in[i]+(i+1<count?in[i+1]:0u);return out;}
}
