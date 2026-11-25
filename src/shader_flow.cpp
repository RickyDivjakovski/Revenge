#include "revenge/shader_flow.hpp"
#include <array>
#include <utility>
#include <bit>
#include <bitset>
#include <cmath>
#include <iomanip>
#include <limits>
#include <sstream>
#include <stdexcept>
namespace revenge::flow { namespace {
constexpr unsigned regs=362,scc_reg=regs,max_words=256;
static_assert(sizeof(float)==4 && std::numeric_limits<float>::is_iec559);
[[noreturn]] void fail(size_t word,const char* message){throw std::runtime_error("flow fixture word "+std::to_string(word)+": "+message);}
Operand reg(unsigned r){return {false,r};}
Operand imm(uint32_t n){return {true,n};}
Operand source(unsigned s,bool vector,std::span<const uint32_t> w,size_t& i,size_t at){
 if(vector&&s>=256&&s<512)return reg(s-256);
 if(s<=105)return reg(256+s);
 if(s>=128&&s<=192)return imm(s-128);
 if(s>=193&&s<=208)return imm(0u-(s-192));
 if(s==255){if(i>=w.size())fail(at,"missing literal");return imm(w[i++]);}
 fail(at,"unsupported source selector");
}
bool compare(Op o){return o>=Op::eq&&o<=Op::le;}
bool branch(Op o){return o>=Op::jump&&o<=Op::scc1;}
bool arithmetic(Op o){return (o>=Op::mov&&o<=Op::mul_f)||o==Op::scalar_add;}
const char* opname(Op o){switch(o){case Op::scalar_add:return "s_add_u32";case Op::nop:return "nop";case Op::end:return "endpgm";case Op::mov:return "mov";case Op::add:return "add_u32";case Op::or_b:return "or_b32";case Op::add_f:return "add_f32";case Op::sub_f:return "sub_f32";case Op::subrev_f:return "subrev_f32";case Op::mul_f:return "mul_f32";case Op::eq:return "cmp_eq_u32";case Op::ne:return "cmp_lg_u32";case Op::gt:return "cmp_gt_u32";case Op::ge:return "cmp_ge_u32";case Op::lt:return "cmp_lt_u32";case Op::le:return "cmp_le_u32";case Op::jump:return "branch";case Op::scc0:return "cbranch_scc0";case Op::scc1:return "cbranch_scc1";case Op::load:return "buffer_load_dword";case Op::store:return "buffer_store_dword";case Op::wait:return "s_waitcnt vmcnt(0)";}throw std::runtime_error("invalid flow opcode");}
// Forward edges use definite-assignment intersections. In opt-in non-buffer
// loop mode, backward edges must preserve every fact established at the header.
void validate(const Program& p){
 if(p.loops&&p.buffers)fail(0,"buffer loops unsupported");
 const size_t n=p.code.size();if(!n||n>max_words||p.code.back().op!=Op::end)fail(0,"invalid program bounds");
 using Bits=std::bitset<regs+1>;
 std::vector<Bits> defs(n);std::vector<bool> reachable(n),may_store(n),must_store(n);
 reachable[0]=true;defs[0].set(0);defs[0].set(256);Bits pending;
 auto merge=[&](size_t i,const Bits& b,bool may,bool must){if(reachable[i]){defs[i]&=b;may_store[i]=may_store[i]||may;must_store[i]=must_store[i]&&must;}else{reachable[i]=true;defs[i]=b;may_store[i]=may;must_store[i]=must;}};
 for(size_t i=0;i<n;++i){const auto& x=p.code[i];(void)opname(x.op);if(!reachable[i])fail(x.word,"unreachable instruction");auto d=defs[i];bool may=may_store[i],must=must_store[i];
  auto reserved=[&](unsigned r){return p.buffers&&r>=260&&r<=267;};
  auto use=[&](Operand v,bool scalar=false){if(!v.immediate&&(v.value>=regs||(scalar&&v.value<256)||reserved(v.value)||!d.test(v.value)||pending.test(v.value)))fail(x.word,"undefined or invalid source register");};
  if(arithmetic(x.op)){if(x.dst>=regs||(x.op!=Op::mov&&x.op!=Op::scalar_add&&x.dst>=256)||(x.op==Op::scalar_add&&(!p.loops||x.dst<256))||reserved(x.dst)||(p.buffers&&x.dst==0)||pending.test(x.dst))fail(x.word,"invalid destination");use(x.a,x.dst>=256);if(x.op!=Op::mov)use(x.b,x.op==Op::scalar_add);d.set(x.dst);if(x.op==Op::scalar_add)d.set(scc_reg);}
  else if(compare(x.op)){use(x.a,true);use(x.b,true);d.set(scc_reg);}
  else if(x.op==Op::load){if(!p.buffers||x.dst==0||x.dst>=256||pending.test(x.dst)||x.a.value>4092||x.a.value%4)fail(x.word,"invalid buffer load");pending.set(x.dst);d.reset(x.dst);}
  else if(x.op==Op::wait){if(!p.buffers||!pending.any())fail(x.word,"wait without pending load");d|=pending;pending.reset();}
  else if(x.op==Op::store){if(!p.buffers||may||pending.any()||x.dst>=256)fail(x.word,"duplicate store or pending loads");use(reg(x.dst));may=must=true;}
  else if(branch(x.op)){if(pending.any())fail(x.word,"loads must be waited before branches");if(x.target>=n||(!p.loops&&x.target<=i))fail(x.word,"invalid branch target");if(x.op!=Op::jump&&!d.test(scc_reg))fail(x.word,"SCC not defined on every incoming path");if(x.target<=i){if(!reachable[x.target]||(defs[x.target]&d)!=defs[x.target])fail(x.word,"backedge loses definite assignments");}else merge(x.target,d,may,must);if(x.op==Op::jump)continue;}
  else if(x.op==Op::end){if(i+1!=n||!d.test(0)||pending.any()||(p.buffers&&!must))fail(x.word,"invalid exit or missing store on a path");continue;}
  if(i+1>=n)fail(x.word,"missing exit");
  if(pending.any()&&reachable[i+1])fail(x.word,"pending load at control-flow join");
  merge(i+1,d,may,must);
 }
}
std::string rname(unsigned n){return std::string(n<256?"v":"s")+std::to_string(n<256?n:n-256);}
std::string val(Operand a){if(!a.immediate)return rname(a.value);std::ostringstream s;s<<"0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<a.value<<"u";return s.str();}
const char* relation(Op o){switch(o){case Op::eq:return "==";case Op::ne:return "!=";case Op::gt:return ">";case Op::ge:return ">=";case Op::lt:return "<";case Op::le:return "<=";default:throw std::logic_error("not comparison");}}
bool cmp(Op o,uint32_t a,uint32_t b){switch(o){case Op::eq:return a==b;case Op::ne:return a!=b;case Op::gt:return a>b;case Op::ge:return a>=b;case Op::lt:return a<b;case Op::le:return a<=b;default:throw std::logic_error("not comparison");}}
const char* fp_symbol(Op o){return o==Op::add_f?"+":o==Op::mul_f?"*":"-";}
uint32_t alu(Op o,uint32_t a,uint32_t b,size_t word){switch(o){case Op::mov:return a;case Op::add:return a+b;case Op::or_b:return a|b;default:break;}
 float fa=std::bit_cast<float>(a),fb=std::bit_cast<float>(b);if(!std::isnormal(fa)||!std::isnormal(fb))fail(word,"floating-point operand outside finite normal fixture domain");
 float r=0;switch(o){case Op::add_f:r=fa+fb;break;case Op::sub_f:r=fa-fb;break;case Op::subrev_f:r=fb-fa;break;case Op::mul_f:r=fa*fb;break;default:fail(word,"invalid arithmetic opcode");}
 if(!std::isnormal(r))fail(word,"floating-point result outside finite normal fixture domain");
 return std::bit_cast<uint32_t>(r);
}
}
static Program decode_impl(std::span<const uint32_t> w,bool buffers,bool loops=false){if(w.empty()||w.size()>max_words)fail(0,"expected 1..256 dwords");Program p;p.buffers=buffers;p.loops=loops;std::array<size_t,max_words> starts;starts.fill(max_words);size_t i=0;
 while(i<w.size()){size_t at=i;starts[at]=p.code.size();uint32_t x=w[i++];Instruction a{};a.word=at;
  if(x==0xbf810000u){a.op=Op::end;if(i!=w.size())fail(at,"trailing data after endpgm");}
  else if(buffers&&x==0xbf8c3f70u)a.op=Op::wait;
  else if(buffers&&((x&0xfffff000u)==0xe0302000u||(x&0xfffff000u)==0xe0702000u)){
   if(i>=w.size())fail(at,"truncated MUBUF");
   auto y=w[i++];bool store=(x&0xfffff000u)==0xe0702000u;
   if((y&0xffff00ffu)!=(store?0x80020000u:0x80010000u))fail(at,"unsupported fixture descriptor or address mode");
   a.dst=(y>>8)&255u;a.a=imm(x&4095u);if(a.a.value%4||(store&&a.a.value))fail(at,"invalid fixture offset");a.op=store?Op::store:Op::load;
  }
  else if((x&0xfffffff0u)==0xbf800000u)a.op=Op::nop;
  else if((x&0xff800000u)==0xbf800000u){unsigned op=(x>>16)&127;if(op!=2&&op!=4&&op!=5)fail(at,"unsupported SOPP instruction");a.op=op==2?Op::jump:op==4?Op::scc0:Op::scc1;int offset=int(x&0xffffu);if(offset>=32768)offset-=65536;auto target=static_cast<int>(i)+offset;if((!loops&&offset<0)||target<0||target>=static_cast<int>(w.size()))fail(at,"backward or out-of-range branch");a.target=static_cast<size_t>(target);}
  else if((x&0xff800000u)==0xbf000000u){unsigned op=(x>>16)&127;switch(op){case 0:case 6:a.op=Op::eq;break;case 1:case 7:a.op=Op::ne;break;case 8:a.op=Op::gt;break;case 9:a.op=Op::ge;break;case 10:a.op=Op::lt;break;case 11:a.op=Op::le;break;default:fail(at,"unsupported SOPC comparison");}unsigned s0=x&255,s1=(x>>8)&255;if(s0==255&&s1==255){a.a=source(s0,false,w,i,at);a.b=a.a;}else{a.a=source(s0,false,w,i,at);a.b=source(s1,false,w,i,at);}}
  else if((x&0xff800000u)==0xbe800000u){unsigned dst=(x>>16)&127;if(((x>>8)&255)!=3||dst>105)fail(at,"unsupported SOP1");a.op=Op::mov;a.dst=256+dst;a.a=source(x&255,false,w,i,at);}
  else if(loops&&(x&0xff800000u)==0x80000000u){unsigned dst=(x>>16)&127;if(dst>105)fail(at,"unsupported scalar destination");a.op=Op::scalar_add;a.dst=256+dst;unsigned a0=x&255u,b0=(x>>8)&255u;a.a=source(a0,false,w,i,at);a.b=(a0==255&&b0==255)?a.a:source(b0,false,w,i,at);}
  else if((x&0xfe000000u)==0x7e000000u){if(((x>>9)&255)!=1)fail(at,"unsupported VOP1");a.op=Op::mov;a.dst=(x>>17)&255;a.a=source(x&511,true,w,i,at);}
  else if(!(x&0x80000000u)){switch((x>>25)&63){case 3:a.op=Op::add_f;break;case 4:a.op=Op::sub_f;break;case 5:a.op=Op::subrev_f;break;case 8:a.op=Op::mul_f;break;case 28:a.op=Op::or_b;break;case 37:a.op=Op::add;break;default:fail(at,"unsupported VOP2");}a.dst=(x>>17)&255;a.a=source(x&511,true,w,i,at);a.b=reg((x>>9)&255);}
  else fail(at,"unsupported instruction encoding");
  p.code.push_back(a);
 }
 for(auto& a:p.code)if(branch(a.op)){if(starts[a.target]==max_words)fail(a.word,"branch into literal data");a.target=starts[a.target];}
 validate(p);return p;
}
Program decode(std::span<const uint32_t> w){return decode_impl(w,false);}
Program decode_loop(std::span<const uint32_t> w){return decode_impl(w,false,true);}
Program decode_unified(std::span<const uint32_t> w){return decode_impl(w,true);}
static uint32_t evaluate_impl(const Program& p,uint32_t id,std::span<const uint32_t> input){validate(p);if(id>=4096||(p.buffers&&input.size()!=4096))throw std::invalid_argument("fixture dispatch/input range");std::array<uint32_t,regs> r{};r[0]=id;r[256]=id/64;bool scc=false;uint32_t out=0;auto read=[&](Operand a){return a.immediate?a.value:r[a.value];};unsigned steps=0;for(size_t pc=0;pc<p.code.size();){if(++steps>1024)fail(p.code[pc].word,"instruction budget exhausted");const auto& x=p.code[pc];if(x.op==Op::end)return p.buffers?out:r[0];if(x.op==Op::scalar_add){auto a=read(x.a),b=read(x.b);r[x.dst]=a+b;scc=r[x.dst]<a;}else if(arithmetic(x.op))r[x.dst]=alu(x.op,read(x.a),x.op==Op::mov?0:read(x.b),x.word);else if(x.op==Op::load){auto index=id+x.a.value/4;r[x.dst]=index<4096?input[index]:0;}else if(x.op==Op::store)out=r[x.dst];else if(compare(x.op))scc=cmp(x.op,read(x.a),read(x.b));else if(branch(x.op)&&(x.op==Op::jump||(x.op==Op::scc0&&!scc)||(x.op==Op::scc1&&scc))){pc=x.target;continue;}++pc;}throw std::logic_error("missing end");}
uint32_t evaluate(const Program& p,uint32_t id){if(p.buffers)throw std::invalid_argument("buffer input required");return evaluate_impl(p,id,{});}
uint32_t evaluate_unified(const Program& p,uint32_t id,std::span<const uint32_t> input){if(!p.buffers)throw std::invalid_argument("unified fixture required");return evaluate_impl(p,id,input);}
std::string emit(const Program& p){validate(p);std::bitset<regs> used;used.set(0);used.set(256);for(auto x:p.code){if(arithmetic(x.op)||x.op==Op::load||x.op==Op::store)used.set(x.dst);if(arithmetic(x.op)||compare(x.op)){if(!x.a.immediate)used.set(x.a.value);if(x.op!=Op::mov&&!x.b.immediate)used.set(x.b.value);}}
 std::ostringstream s;s<<(p.loops?"// Bounded loop fixture ABI (1024 instruction steps).\n":"// Forward-only fixture ABI; normal finite FP32 only.\n");if(p.buffers)s<<"StructuredBuffer<uint> sourceData : register(t0);\n";s<<"RWStructuredBuffer<uint> output : register(u0);\n[numthreads(64,1,1)]\nvoid main(uint3 id : SV_DispatchThreadID) {\n";
 if(p.buffers||p.loops)s<<"  if (id.x >= 4096u) return;\n";
 for(unsigned r=0;r<regs;++r)if(used.test(r))s<<"  uint "<<rname(r)<<" = "<<(r==0?"id.x":r==256?"id.x / 64u":"0u")<<";\n";
 s<<"  bool scc = false;\n  uint pc = 0u;\n";
 if(p.loops)s<<"  [loop] for (uint step=0u; step<1024u; ++step) {\n";
 for(size_t i=0;i<p.code.size();++i){auto x=p.code[i];auto a=val(x.a),b=val(x.b);s<<"  if (pc == "<<i<<"u) {\n";
  if(x.op==Op::end){if(!p.buffers)s<<"    output[id.x] = v0;\n";if(p.loops)s<<"    return;\n";s<<"  }\n";continue;}
  if(arithmetic(x.op)){auto d=rname(x.dst);if(x.op==Op::scalar_add)s<<"    uint lhs = "<<a<<"; uint rhs = "<<b<<";\n    "<<d<<" = lhs + rhs; scc = "<<d<<" < lhs;\n";else if(x.op==Op::mov)s<<"    "<<d<<" = "<<a<<";\n";else if(x.op==Op::add||x.op==Op::or_b)s<<"    "<<d<<" = "<<a<<(x.op==Op::add?" + ":" | ")<<b<<";\n";else{if(x.op==Op::subrev_f)std::swap(a,b);s<<"    precise float fp = asfloat("<<a<<") "<<fp_symbol(x.op)<<" asfloat("<<b<<");\n    "<<d<<" = asuint(fp);\n";}}
  else if(x.op==Op::load){auto d=rname(x.dst);s<<"    "<<d<<" = 0u;\n    if (id.x + "<<x.a.value/4<<"u < 4096u) "<<d<<" = sourceData[id.x + "<<x.a.value/4<<"u];\n";}
  else if(x.op==Op::store)s<<"    output[id.x] = "<<rname(x.dst)<<";\n";
  else if(compare(x.op))s<<"    scc = "<<a<<" "<<relation(x.op)<<" "<<b<<";\n";
  s<<"    pc = ";if(x.op==Op::jump)s<<x.target<<"u";else if(x.op==Op::scc0||x.op==Op::scc1)s<<(x.op==Op::scc0?"!scc":"scc")<<" ? "<<x.target<<"u : "<<i+1<<"u";else s<<i+1<<"u";s<<";\n";if(p.loops)s<<"    continue;\n";s<<"  }\n";
 }if(p.loops)s<<"  }\n  output[id.x] = 0xffffffffu; // exhaustion marker for known-answer fixture\n";s<<"}\n";return s.str();}
std::string describe(const Program& p){validate(p);std::ostringstream s;for(auto x:p.code){s<<"word "<<x.word<<": "<<opname(x.op);if(branch(x.op))s<<" -> word "<<p.code[x.target].word;else if(x.op==Op::load||x.op==Op::store){s<<" "<<rname(x.dst)<<", v0, fixture resource offset:"<<x.a.value;}else if(arithmetic(x.op)||compare(x.op)){if(arithmetic(x.op))s<<" "<<rname(x.dst)<<",";s<<" "<<val(x.a);if(x.op!=Op::mov)s<<", "<<val(x.b);}s<<'\n';}return s.str();}
// Branch based on group id; input id occupies low mantissa bits of 1.x.
// Groups <32 add 1.0; the others multiply by 2.0. Both produce identical
// high exponents but distinct mantissas for nonzero inputs.
std::vector<uint32_t> fixture_words(){return {
 0x380000ffu,0x3f800000u, // v_or_b32 v0, literal, v0
 0xbf0aa000u,             // s_cmp_lt_u32 s0, 32
 0xbf840003u,             // s_cbranch_scc0 to multiply
 0x060000ffu,0x3f800000u, // v_add_f32 v0, 1.0, v0
 0xbf820002u,             // s_branch to end
 0x100000ffu,0x40000000u, // v_mul_f32 v0, 2.0, v0
 0xbf810000u};}
uint32_t fixture_expected(uint32_t id){if(id>=4096)throw std::invalid_argument("fixture dispatch range");return 0x40000000u+(id<2048?id/2+(id%4==3?1u:0u):id);}
// Read two values; branch by group ID; add or OR; write exactly once.
std::vector<uint32_t> unified_words(){return {0xe0302000u,0x80010100u,0xe0302004u,0x80010200u,0xbf8c3f70u,0xbf0aa000u,0xbf840002u,0x4a020501u,0xbf820001u,0x38020501u,0xe0702000u,0x80020100u,0xbf810000u};}
uint32_t unified_expected(uint32_t id,std::span<const uint32_t> in){if(id>=4096||in.size()!=4096)throw std::invalid_argument("unified fixture range");auto a=in[id],b=id+1<4096?in[id+1]:0;return id<2048?a+b:a|b;}
std::vector<uint32_t> loop_words(){return {0xbe810380u,0x4a000081u,0x80018101u,0xbf0b0001u,0xbf85fffcu,0xbf810000u};}
uint32_t loop_expected(uint32_t id){if(id>=4096)throw std::invalid_argument("loop fixture range");return id+id/64+1;}
WaveMask::WaveMask(unsigned w):width_(w),exec_(0){if(w!=32&&w!=64)throw std::invalid_argument("wave width must be 32 or 64");exec_=w==64?UINT64_MAX:0xffffffffull;}
uint64_t WaveMask::set_exec(uint64_t m) noexcept {auto old=exec_;exec_=m&(width_==64?UINT64_MAX:0xffffffffull);return old;}
void WaveMask::compare_equal(std::span<const uint32_t> a,std::span<const uint32_t> b){if(a.size()!=width_||b.size()!=width_)throw std::invalid_argument("wave size mismatch");vcc_=0;for(unsigned i=0;i<width_;++i)if((exec_&(uint64_t(1)<<i))&&a[i]==b[i])vcc_|=uint64_t(1)<<i;}
void WaveMask::masked_write(std::span<uint32_t> dst,std::span<const uint32_t> src) const {if(dst.size()!=width_||src.size()!=width_)throw std::invalid_argument("wave size mismatch");std::array<uint32_t,64> tmp{};for(unsigned i=0;i<width_;++i)tmp[i]=src[i];for(unsigned i=0;i<width_;++i)if(exec_&(uint64_t(1)<<i))dst[i]=tmp[i];}
}

namespace revenge::flow {
std::vector<uint32_t> wave_fixture_expected(unsigned width){
 if(width!=32&&width!=64)throw std::invalid_argument("wave width must be 32 or 64");
 std::vector<uint32_t> out(4096);
 for(unsigned group=0;group<64;++group)for(unsigned base=0;base<64;base+=width){
  WaveMask mask(width);uint64_t active=0;std::vector<uint32_t> a(width),b(width),values(width),dst(width,0x12345678u);
  for(unsigned lane=0;lane<width;++lane){if((lane+group)%5!=0)active|=uint64_t(1)<<lane;a[lane]=lane;b[lane]=(lane*3+group)%7<3?lane:lane+1;values[lane]=(group*64+base+lane)*3u+1u;}
  mask.set_exec(active);mask.compare_equal(a,b);auto vcc=mask.vcc();mask.set_exec(vcc);mask.masked_write(dst,values);mask.set_exec(active);
  for(unsigned lane=0;lane<width;++lane){unsigned bit=lane%32,shift=lane/32*32;out[group*64+base+lane]=bit==0?uint32_t(mask.exec()>>shift):bit==1?uint32_t(vcc>>shift):dst[lane];}
 }
 return out;
}
std::string wave_fixture_hlsl(unsigned width){
 if(width!=32&&width!=64)throw std::invalid_argument("wave width must be 32 or 64");
 return std::string("// Logical wave masks, not native RDNA2 wave opcodes.\n#define WIDTH ")+std::to_string(width)+R"(
RWStructuredBuffer<uint> output : register(u0);
groupshared uint execWords[2];
groupshared uint vccWords[2];
[numthreads(64,1,1)]
void main(uint3 id:SV_DispatchThreadID, uint3 group:SV_GroupID, uint index:SV_GroupIndex) {
    uint word=index/32u, lane=index%WIDTH, bit=index%32u;
    if (index<2u) { execWords[index]=0u; vccWords[index]=0u; }
    GroupMemoryBarrierWithGroupSync();
    uint old;
    if ((lane+group.x)%5u!=0u) InterlockedOr(execWords[word],1u<<bit,old);
    GroupMemoryBarrierWithGroupSync();
    if ((execWords[word] & (1u<<bit))!=0u && (lane*3u+group.x)%7u<3u)
        InterlockedOr(vccWords[word],1u<<bit,old);
    GroupMemoryBarrierWithGroupSync();
    uint value=0x12345678u;
    if ((vccWords[word] & (1u<<bit))!=0u) value=id.x*3u+1u;
    output[id.x]=bit==0u?execWords[word]:bit==1u?vccWords[word]:value;
}
)";
}
}
