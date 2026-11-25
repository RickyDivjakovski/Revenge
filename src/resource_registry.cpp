#include "revenge/resource_registry.hpp"
#include "revenge/shader_ir.hpp"
#include "revenge/scene.hpp"
#include <atomic>
#include <stdexcept>
#include <sstream>
namespace revenge::resources {
namespace {std::atomic<uint64_t> next_owner{1};}
Registry::Registry(unsigned capacity):owner_(next_owner.fetch_add(1)){
 if(!capacity||capacity>65536||!owner_)throw std::invalid_argument("invalid descriptor capacity/identity");
 entries_.resize(capacity);
}
Registry::Entry& Registry::resolve(Handle h,Kind k){
 if(h.owner!=owner_||h.slot>=entries_.size())throw std::invalid_argument("foreign resource handle");
 auto& e=entries_[h.slot];
 if(e.state!=Entry::live||e.generation!=h.generation||e.kind!=k)throw std::invalid_argument("stale, retired or wrong-kind resource handle");
 return e;
}
Handle Registry::allocate(Kind k){
 if(k!=Kind::texture&&k!=Kind::buffer)throw std::invalid_argument("invalid resource kind");
 for(unsigned i=0;i<entries_.size();++i){auto& e=entries_[i];if(e.state==Entry::free&&e.generation!=UINT64_MAX){++e.generation;e.kind=k;e.state=Entry::live;e.last_use=0;return {owner_,e.generation,i};}}
 throw std::runtime_error("descriptor registry exhausted");
}
unsigned Registry::descriptor(Handle h,Kind k){resolve(h,k);return h.slot;}
void Registry::submitted(Handle h,Kind k,uint64_t fence){auto& e=resolve(h,k);if(!fence||fence==UINT64_MAX||fence<=completed_||fence<submitted_)throw std::invalid_argument("invalid submission fence");e.last_use=fence;submitted_=fence;}
void Registry::retire(Handle h,Kind k){auto& e=resolve(h,k);e.state=Entry::retired;}
unsigned Registry::collect(uint64_t completed){if(completed<completed_||completed>submitted_)throw std::invalid_argument("invalid completed fence");completed_=completed;unsigned count=0;for(auto& e:entries_)if(e.state==Entry::retired&&e.last_use<=completed_){e.state=Entry::free;++count;}return count;}
unsigned Registry::occupied() const noexcept{unsigned n=0;for(auto& e:entries_)if(e.state!=Entry::free)++n;return n;}
uint32_t texture_word(unsigned index){if(index>=4)throw std::invalid_argument("invalid fixture texel");auto p=scene3d::palette[index];return uint32_t(p[0])|(uint32_t(p[1])<<8)|(uint32_t(p[2])<<16)|(uint32_t(p[3])<<24);}
std::string texture_compute_hlsl(unsigned pitch,std::span<const uint32_t> words){
 if(pitch<256||pitch>65536||pitch%256)throw std::invalid_argument("invalid texture pitch");
 auto p=shader::optimize(words.empty()?shader::decode(shader::fixture_words()):shader::decode(words));
 for(unsigned i=0;i<4;++i)if(shader::evaluate(p,i)!=3*i+1)throw std::runtime_error("translated texture oracle mismatch");
 std::ostringstream o;o<<shader::hlsl_function(p)<<"\nRWByteAddressBuffer target : register(u0);\n[numthreads(4,1,1)]\nvoid main(uint3 id:SV_DispatchThreadID) {\n uint v=translated_alu(id.x);\n uint pixel=0xffff00ffu;\n switch(v) {\n";
 for(unsigned i=0;i<4;++i)o<<" case "<<3*i+1<<"u: pixel="<<texture_word(i)<<"u; break;\n";
 o<<" }\n target.Store((id.x/2u)*"<<pitch<<"u+(id.x%2u)*4u,pixel);\n}\n";return o.str();
}
}
