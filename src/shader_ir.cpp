#include "revenge/shader_ir.hpp"
#include <algorithm>
#include <array>
#include <charconv>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <stdexcept>
#include <string_view>
namespace revenge::shader { namespace {
constexpr size_t regs=362, max_words=8192;
[[noreturn]] void bad(size_t word,const char* message){throw std::runtime_error("RDNA2 fixture word "+std::to_string(word)+": "+message);}
Value reg(uint32_t index){return {false,index};}
Value imm(uint32_t value){return {true,value};}
Value source(unsigned s,bool vector,std::span<const uint32_t> w,size_t& i,size_t at){
 if(vector&&s>=256&&s<512)return reg(s-256);
 if(s<=105)return reg(256+s);
 if(s>=128&&s<=192)return imm(s-128);
 if(s>=193&&s<=208)return imm(0u-(s-192));
 if(s==255){if(i==w.size())bad(at,"missing literal dword");return imm(w[i++]);}
 bad(at,"unsupported source selector (special register, float inline constant or modifier)");
}
const char* name(Op op){switch(op){case Op::mov:return "mov";case Op::add:return "add_u32";case Op::sub:return "sub_u32";case Op::subrev:return "subrev_u32";case Op::mul24:return "mul_u24";case Op::min_u:return "min_u32";case Op::max_u:return "max_u32";case Op::shr:return "lshrrev_b32";case Op::shl:return "lshlrev_b32";case Op::and_b:return "and_b32";case Op::or_b:return "or_b32";case Op::xor_b:return "xor_b32";case Op::xnor_b:return "xnor_b32";}throw std::runtime_error("invalid IR opcode");}
Op vop2(unsigned op,size_t at){switch(op){case 11:return Op::mul24;case 19:return Op::min_u;case 20:return Op::max_u;case 22:return Op::shr;case 26:return Op::shl;case 27:return Op::and_b;case 28:return Op::or_b;case 29:return Op::xor_b;case 30:return Op::xnor_b;case 37:return Op::add;case 38:return Op::sub;case 39:return Op::subrev;default:bad(at,"unsupported VOP2 opcode");}}
uint32_t calc(Op op,uint32_t a,uint32_t b){switch(op){case Op::mov:return a;case Op::add:return a+b;case Op::sub:return a-b;case Op::subrev:return b-a;case Op::mul24:return (a&0xffffffu)*(b&0xffffffu);case Op::min_u:return std::min(a,b);case Op::max_u:return std::max(a,b);case Op::shr:return b>>(a&31u);case Op::shl:return b<<(a&31u);case Op::and_b:return a&b;case Op::or_b:return a|b;case Op::xor_b:return a^b;case Op::xnor_b:return ~(a^b);}throw std::runtime_error("invalid IR opcode");}
void validate(const Program& p){if(p.instructions.size()>max_words)throw std::runtime_error("IR too large");std::array<bool,regs> defined{};defined[0]=true;for(const auto& x:p.instructions){(void)name(x.op);if(x.dst>=regs)bad(x.word,"invalid destination");auto use=[&](Value v){if(!v.immediate&&(v.value>=regs||!defined[v.value]))bad(x.word,"read of undefined register");};use(x.a);if(x.op!=Op::mov)use(x.b);defined[x.dst]=true;}}
std::string rname(uint32_t r){return std::string(r<256?"v":"s")+std::to_string(r<256?r:r-256);}
std::string literal(uint32_t n){std::ostringstream s;s<<"0x"<<std::hex<<std::setw(8)<<std::setfill('0')<<n<<"u";return s.str();}
std::string expression(Op op,const std::string& a,const std::string& b){switch(op){case Op::mov:return a;case Op::add:return a+" + "+b;case Op::sub:return a+" - "+b;case Op::subrev:return b+" - "+a;case Op::mul24:return "("+a+" & 0xffffffu) * ("+b+" & 0xffffffu)";case Op::min_u:return "min("+a+", "+b+")";case Op::max_u:return "max("+a+", "+b+")";case Op::shr:return b+" >> ("+a+" & 31u)";case Op::shl:return b+" << ("+a+" & 31u)";case Op::and_b:return a+" & "+b;case Op::or_b:return a+" | "+b;case Op::xor_b:return a+" ^ "+b;case Op::xnor_b:return "~("+a+" ^ "+b+")";}throw std::runtime_error("invalid IR opcode");}
}
Program decode(std::span<const uint32_t> w){if(w.empty()||w.size()>max_words)throw std::runtime_error("fixture must contain 1..8192 dwords");Program p;size_t i=0;bool end=false;while(i<w.size()){
 auto at=i;uint32_t x=w[i++];
 if(x==0xbf810000u){end=true;if(i!=w.size())bad(at,"trailing words after endpgm");break;}
 if((x&0xfffffff0u)==0xbf800000u)continue; // timing not observable in this pure ALU fixture
 if((x&0xff800000u)==0xbe800000u){unsigned dst=(x>>16)&127,op=(x>>8)&255;if(op!=3||dst>105)bad(at,"unsupported SOP1 operation or special destination");auto a=source(x&255,false,w,i,at);p.instructions.push_back({Op::mov,256+dst,a,imm(0),at});continue;}
 if((x&0xfe000000u)==0x7e000000u){if(((x>>9)&255)!=1)bad(at,"unsupported VOP1 opcode");auto a=source(x&511,true,w,i,at);p.instructions.push_back({Op::mov,(x>>17)&255,a,imm(0),at});continue;}
 if((x&0x80000000u)==0){auto op=vop2((x>>25)&63,at);auto a=source(x&511,true,w,i,at);p.instructions.push_back({op,(x>>17)&255,a,reg((x>>9)&255),at});continue;}
 bad(at,"unsupported encoding (control flow, memory, export, wave operation or extended instruction)");
 }if(!end)bad(w.size(),"missing s_endpgm");validate(p);return p;}
std::vector<uint32_t> read_words(const std::filesystem::path& path){std::ifstream in(path,std::ios::binary|std::ios::ate);if(!in||in.tellg()<0||in.tellg()>std::streamoff(512*1024))throw std::runtime_error("invalid fixture text size");in.seekg(0);std::vector<uint32_t> words;std::string line;while(std::getline(in,line)){if(auto n=line.find('#');n!=std::string::npos)line.resize(n);std::istringstream ss(line);std::string word;while(ss>>word){std::string_view s(word);if(s.starts_with("0x")||s.starts_with("0X"))s.remove_prefix(2);uint32_t x{};auto [end,err]=std::from_chars(s.data(),s.data()+s.size(),x,16);if(s.empty()||s.size()>8||err!=std::errc{}||end!=s.data()+s.size())throw std::runtime_error("invalid hexadecimal fixture word");if(words.size()==max_words)throw std::runtime_error("fixture too large");words.push_back(x);}}if(in.bad())throw std::runtime_error("fixture read error");return words;}
Program optimize(const Program& p){validate(p);Program out;std::array<bool,regs> known{};std::array<uint32_t,regs> constants{};for(auto x:p.instructions){auto fold=[&](Value v){return !v.immediate&&known[v.value]?imm(constants[v.value]):v;};x.a=fold(x.a);if(x.op!=Op::mov)x.b=fold(x.b);if(x.a.immediate&&(x.op==Op::mov||x.b.immediate)){auto c=calc(x.op,x.a.value,x.b.value);x.op=Op::mov;x.a=imm(c);x.b=imm(0);known[x.dst]=true;constants[x.dst]=c;}else known[x.dst]=false;out.instructions.push_back(x);}
 std::array<bool,regs> live{};live[0]=true;Program result;for(auto it=out.instructions.rbegin();it!=out.instructions.rend();++it){auto x=*it;if(!live[x.dst])continue;live[x.dst]=false;if(!x.a.immediate)live[x.a.value]=true;if(x.op!=Op::mov&&!x.b.immediate)live[x.b.value]=true;result.instructions.push_back(x);}std::reverse(result.instructions.begin(),result.instructions.end());validate(result);return result;}
uint32_t evaluate(const Program& p,uint32_t input){validate(p);std::array<uint32_t,regs> values{};values[0]=input;auto read=[&](Value v){return v.immediate?v.value:values[v.value];};for(auto& x:p.instructions)values[x.dst]=calc(x.op,read(x.a),x.op==Op::mov?0:read(x.b));return values[0];}
std::string hlsl(const Program& p){validate(p);std::array<unsigned,regs> versions{};std::array<std::string,regs> current{};current[0]="v0_input";std::ostringstream out;out<<"// Independent ALU fixture; v0=input/output, not a console shader ABI.\nRWStructuredBuffer<uint> output : register(u0);\n[numthreads(64,1,1)]\nvoid main(uint3 id : SV_DispatchThreadID) {\n    uint v0_input = id.x;\n";auto operand=[&](Value v){return v.immediate?literal(v.value):current[v.value];};for(auto& x:p.instructions){auto rhs=expression(x.op,operand(x.a),x.op==Op::mov?"":operand(x.b));auto dst=rname(x.dst)+"_"+std::to_string(++versions[x.dst]);out<<"    uint "<<dst<<" = "<<rhs<<";\n";current[x.dst]=dst;}out<<"    output[id.x] = "<<current[0]<<";\n}\n";return out.str();}
std::string hlsl_function(const Program& p){validate(p);std::array<unsigned,regs> versions{};std::array<std::string,regs> current{};current[0]="v0_input";std::ostringstream out;out<<"// Independent ALU fixture; v0=input/output, not a console shader ABI.\nuint translated_alu(uint v0_input) {\n";auto operand=[&](Value v){return v.immediate?literal(v.value):current[v.value];};for(auto& x:p.instructions){auto rhs=expression(x.op,operand(x.a),x.op==Op::mov?"":operand(x.b));auto dst=rname(x.dst)+"_"+std::to_string(++versions[x.dst]);out<<"    uint "<<dst<<" = "<<rhs<<";\n";current[x.dst]=dst;}out<<"    return "<<current[0]<<";\n}\n";return out.str();}
std::string describe(const Program& p){validate(p);std::ostringstream out;for(auto& x:p.instructions){auto val=[](Value v){return v.immediate?literal(v.value):rname(v.value);};out<<"word "<<x.word<<": "<<name(x.op)<<" "<<rname(x.dst)<<", "<<val(x.a);if(x.op!=Op::mov)out<<", "<<val(x.b);out<<'\n';}return out.str();}
std::vector<uint32_t> fixture_words(){return {0xbe800381u,0x4a020100u,0x4a000300u,0x4a000000u,0xbf810000u};}
}
