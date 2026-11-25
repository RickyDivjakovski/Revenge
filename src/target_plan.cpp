#include "revenge/target_plan.hpp"
#include "revenge/runtime.hpp"
#include "revenge/launch.hpp"
#include <array>
#include <sstream>
#include <stdexcept>
#include <utility>
namespace revenge { namespace {
struct Read {
 std::span<const uint8_t> b;
 void range(uint64_t p,uint64_t n)const{if(p>b.size()||n>b.size()-p)throw std::runtime_error("target plan: invalid range");}
 uint64_t get(uint64_t p,unsigned n)const{range(p,n);uint64_t v=0;for(unsigned i=0;i<n;++i)v|=uint64_t(b[p+i])<<(8*i);return v;}
 std::string name(uint64_t p)const{if(!p)throw std::runtime_error("target plan: null name");std::string s;for(unsigned i=0;i<4096;++i){auto c=get(p+i,1);if(!c){if(s.empty())throw std::runtime_error("target plan: empty name");return s;}if(c<32||c>126)throw std::runtime_error("target plan: non-ASCII name");s+=static_cast<char>(c);}throw std::runtime_error("target plan: unterminated name");}
};
}
std::string target_plan_report(const LoadPlan& p,const std::filesystem::path& path){
 Read r{p.mapped};auto nt=r.get(0x3c,4),opt=nt+24,os=r.get(nt+20,2);r.range(opt,os);if(os<112)throw std::runtime_error("target plan: optional header truncated");auto dirs=r.get(opt+108,4);
 auto dir=[&](unsigned i){if(i>=dirs||112ull+8ull*(i+1)>os)return std::pair<uint64_t,uint64_t>{0,0};auto a=r.get(opt+112+8ull*i,4),n=r.get(opt+116+8ull*i,4);if(bool(a)!=bool(n))throw std::runtime_error("target plan: inconsistent directory");if(a)r.range(a,n);return std::pair{a,n};};
 std::ostringstream o;o<<"Offline target readiness inventory (no executable loading).\n";
 auto coff=r.get(nt+22,2),dll=r.get(opt+70,2);o<<"Machine: AMD64 PE32+; subsystem: "<<p.image.subsystem<<"; COFF flags: 0x"<<std::hex<<coff<<"; DLL characteristics: 0x"<<dll<<std::dec<<".\n";
 o<<"Section alignment: "<<r.get(opt+32,4)<<"; file alignment: "<<r.get(opt+36,4)<<"; stack reserve/commit: "<<r.get(opt+72,8)<<"/"<<r.get(opt+80,8)<<"; heap reserve/commit: "<<r.get(opt+88,8)<<"/"<<r.get(opt+96,8)<<".\n";
 o<<"Console markers (heuristic): "<<has_console_markers(p.image,path)<<"; ordinary Windows candidate (direct imports only): "<<windows_candidate(p.image,path)<<".\n";
 std::array<uint64_t,5> direct{},delayed{};auto add=[&](auto& counts,const std::string& name,uint64_t count){counts.at(static_cast<size_t>(classify_dependency(name)))+=count;};
 for(const auto& d:p.image.imports)add(direct,d.dll,d.symbols.size());
 auto [a,n]=dir(13);uint64_t descriptors=0,total=0;bool end=!a;
 if(a){if(n<32||n%32)throw std::runtime_error("target plan: malformed delay directory size");for(uint64_t d=0;d+32<=n&&descriptors<4096;d+=32){std::array<uint64_t,8> f{};bool empty=true;for(unsigned i=0;i<8;++i){f[i]=r.get(a+d+4ull*i,4);empty&=f[i]==0;}if(empty){end=true;break;}if(f[0]!=1||!f[1]||!f[3]||!f[4])throw std::runtime_error("target plan: unsupported or malformed delay descriptor (RVA format required)");
  ++descriptors;auto name=r.name(f[1]);if(f[2])r.range(f[2],8);uint64_t count=0;bool terminated=false;for(uint64_t j=0;j<65536&&total<100000;++j){auto t=r.get(f[4]+8*j,8);r.range(f[3]+8*j,8);if(!t){terminated=true;break;}++count;++total;if(t>>63){if(t&UINT64_C(0x7fffffffffff0000))throw std::runtime_error("target plan: invalid delay ordinal");o<<"Delay "<<name<<"!#"<<(t&0xffff)<<'\n';}else{if(t>UINT32_MAX-2)throw std::runtime_error("target plan: invalid delay name RVA");r.range(t,2);o<<"Delay "<<name<<"!"<<r.name(t+2)<<'\n';}if(static_cast<std::streamoff>(o.tellp())>std::streamoff(8*1024*1024))throw std::runtime_error("target plan: report exceeds limit");}
  if(!terminated)throw std::runtime_error("target plan: unterminated delay thunk table");
  if(f[5])r.range(f[5],8*(count+1));
  if(f[6])r.range(f[6],8*(count+1));
  add(delayed,name,count);
 }}if(!end)throw std::runtime_error("target plan: unterminated delay descriptors");
 o<<"Delay descriptors: "<<descriptors<<"; delay symbols: "<<total<<".\n";
 for(size_t i=0;i<direct.size();++i)o<<dependency_label(static_cast<DependencyKind>(i))<<": direct="<<direct[i]<<" delay="<<delayed[i]<<'\n';
 auto [cfg,cs]=dir(10);o<<"Load config present: "<<bool(cfg);if(cfg){if(cs<4)throw std::runtime_error("target plan: truncated load configuration");auto declared=r.get(cfg,4);if(declared<4||declared>cs)throw std::runtime_error("target plan: invalid load configuration size");o<<"; declared bytes: "<<declared;if(declared>=148)o<<"; guard flags: 0x"<<std::hex<<r.get(cfg+144,4)<<std::dec;else o<<"; guard flags not present";}o<<".\n";
 o<<"CLR directory present: "<<bool(dir(14).first)<<"; TLS callbacks: "<<p.tls_callbacks<<"; unwind entries: "<<p.unwind_entries<<"; relocations: "<<p.relocations<<".\n";
 o<<"No API signatures inferred; dynamic/transitive dependencies not enumerated; no imports bound, security checks changed or target code executed.\n";return o.str();
}
}
