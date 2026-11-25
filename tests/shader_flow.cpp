#include "revenge/shader_flow.hpp"
#include <array>
#include <bit>
#include <iostream>
#include <stdexcept>
using namespace revenge::flow;
void require(bool b){if(!b)throw std::runtime_error("flow assertion failed");}
void reject(std::initializer_list<uint32_t> w){bool failed=false;try{decode(w);}catch(const std::runtime_error&){failed=true;}require(failed);}
int main(){try{
 auto p=decode(fixture_words());require(p.code.size()==7);for(uint32_t i=0;i<4096;++i)require(evaluate(p,i)==fixture_expected(i));
 auto h=emit(p);require(h.find("!scc ? 5u : 3u")!=std::string::npos);require(h.find("precise float fp")!=std::string::npos);
 constexpr uint32_t end=0xbf810000u;
 // SCC1 branch, inequality, two destinations, merge.
 auto q=decode(std::array<uint32_t,6>{0xbf078000u,0xbf850002u,0x7e000281u,0xbf820001u,0x7e000282u,end});
 require(evaluate(q,0)==1);require(evaluate(q,64)==2);
 // Both paths must define v1 before join; only first path writes it here.
 reject({0xbf068000u,0xbf840001u,0x7e020281u,0x7e000301u,end});
 reject({0xbf840000u,end}); // SCC undefined
 reject({0xbf82ffffu,end}); // backward
 reject({0xbf820001u,0x7e0002ffu,0x12345678u,end}); // literal target
 reject({0xbf820005u,end}); // out of range
 reject({0xbf860000u,end}); // VCC unsupported
 reject({0xbf810000u,end}); // trailing data
 reject({0x7e0002ffu}); // truncated
 for(unsigned op:{3u,4u,5u,8u}){auto code=decode(std::array<uint32_t,5>{0x7e0002ffu,0x40800000u,(op<<25)|255u,0x40000000u,end});float expect=op==3?6.f:op==4?-2.f:op==5?2.f:8.f;require(evaluate(code,0)==std::bit_cast<uint32_t>(expect));}
 auto badfp=decode(std::array<uint32_t,4>{0x7e000280u,0x060000ffu,0x3f800000u,end});bool failed=false;try{evaluate(badfp,0);}catch(const std::runtime_error&){failed=true;}require(failed);
 for(unsigned n:{32u,64u}){WaveMask m(n);std::vector<uint32_t> a(n),b(n),dst(n,99);for(unsigned i=0;i<n;++i){a[i]=i;b[i]=i%3==0?i:0xffffffffu;}uint64_t mask=0xaaaaaaaaaaaaaaaaull;auto full=m.set_exec(mask);require(full==(n==64?UINT64_MAX:0xffffffffull));m.compare_equal(a,b);uint64_t want=0;for(unsigned i=0;i<n;++i)if(i%2&&i%3==0)want|=uint64_t(1)<<i;require(m.vcc()==want);m.masked_write(dst,a);for(unsigned i=0;i<n;++i)require(dst[i]==(i%2?i:99));m.set_exec(0);m.compare_equal(a,b);require(m.vcc()==0);m.set_exec(full);require(m.exec()==full);}
 std::cout<<"4096 forward-branch FP32 oracle cases; SCC1, validation and CPU wave mask tests passed.\n";return 0;
}catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
