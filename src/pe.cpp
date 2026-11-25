#include "revenge/core.hpp"
#include <algorithm>
#include <fstream>
#include <limits>
#include <stdexcept>
namespace revenge { namespace {
struct Reader {
 std::span<const uint8_t> b;
 void check(uint64_t p,uint64_t n) const {if(p>b.size() || n>b.size()-p) throw std::runtime_error("truncated or invalid PE range");}
 uint64_t get(uint64_t p,unsigned n) const {check(p,n); uint64_t v=0; for(unsigned i=0;i<n;++i)v|=uint64_t(b[p+i])<<(8*i); return v;}
 std::string str(uint64_t p,uint64_t end) const {check(p,1); end=std::min<uint64_t>(end,b.size()); std::string s; while(p<end && s.size()<4096){char c=char(b[p++]); if(!c)return s; if(static_cast<unsigned char>(c)<32 || static_cast<unsigned char>(c)>126)throw std::runtime_error("non-ASCII import name"); s+=c;}throw std::runtime_error("unterminated import name");}
};
struct Mapping {uint64_t offset,end;};
}
Image inspect(std::span<const uint8_t> bytes){
 Reader r{bytes}; if(r.get(0,2)!=0x5a4d)throw std::runtime_error("not MZ"); uint64_t pe=r.get(0x3c,4); if(r.get(pe,4)!=0x4550)throw std::runtime_error("not PE");
 uint64_t coff=pe+4,opt=coff+20; Image out{}; out.machine=uint16_t(r.get(coff,2)); auto n=r.get(coff+2,2), sz=r.get(coff+16,2); r.check(opt,sz);
 if(out.machine!=0x8664 || r.get(opt,2)!=0x20b || sz<112)throw std::runtime_error("expected AMD64 PE32+");
 if(n==0 || n>96)throw std::runtime_error("invalid section count");
 out.entry=uint32_t(r.get(opt+16,4)); out.base=r.get(opt+24,8); out.image_size=uint32_t(r.get(opt+56,4)); out.subsystem=uint16_t(r.get(opt+68,2)); auto hdr=r.get(opt+60,4), dirs=r.get(opt+108,4);
 if(hdr>bytes.size() || hdr>out.image_size || opt+sz+n*40>hdr || out.entry>=out.image_size)throw std::runtime_error("invalid header/image bounds");
 auto dir=[&](unsigned i){if(i>=dirs || 112+8ull*(i+1)>sz)return std::pair<uint64_t,uint64_t>{0,0};return std::pair{r.get(opt+112+8ull*i,4),r.get(opt+116+8ull*i,4)};};
 out.tls=dir(9).first!=0;out.unwind=dir(3).first!=0;out.reloc=dir(5).first!=0;
 for(uint64_t i=0;i<n;++i){auto p=opt+sz+i*40;r.check(p,40); Section s{};for(unsigned j=0;j<8 && bytes[p+j];++j)s.name+=char(bytes[p+j]);s.virtual_size=uint32_t(r.get(p+8,4));s.rva=uint32_t(r.get(p+12,4));s.raw_size=uint32_t(r.get(p+16,4));s.raw=uint32_t(r.get(p+20,4));s.characteristics=uint32_t(r.get(p+36,4));if(s.raw_size)r.check(s.raw,s.raw_size);if(uint64_t(s.rva)+std::max(s.virtual_size,s.raw_size)>out.image_size)throw std::runtime_error("section outside image");out.sections.push_back(s);}
 auto map=[&](uint64_t a,uint64_t count)->Mapping{if(a<hdr && count<=hdr-a)return {a,hdr}; for(auto& s:out.sections)if(a>=s.rva && a-s.rva<s.raw_size && count<=s.raw_size-(a-s.rva))return {uint64_t(s.raw)+a-s.rva,uint64_t(s.raw)+s.raw_size}; throw std::runtime_error("RVA not backed by file");};
 auto name=[&](uint64_t a){auto m=map(a,1);return r.str(m.offset,m.end);};
 auto [imp,size]=dir(1); if(imp){if(size<20 || size>out.image_size || imp+size>out.image_size)throw std::runtime_error("invalid import directory"); bool ended=false; uint64_t total=0;
 for(uint64_t i=0;i+20<=size && i/20<4096;i+=20){auto p=map(imp+i,20).offset; auto oft=r.get(p,4), dll=r.get(p+12,4),ft=r.get(p+16,4);if(!(oft|dll|ft|r.get(p+4,4)|r.get(p+8,4))){ended=true;break;}if(!dll||!ft)throw std::runtime_error("invalid import descriptor");Import dep{name(dll),{}, {}};auto thunk=oft?oft:ft;bool done=false;for(uint64_t j=0;j<65536 && total<1000000;++j){auto v=r.get(map(thunk+j*8,8).offset,8);if(!v){done=true;break;}++total;dep.by_ordinal.push_back((v>>63)!=0);if(v>>63)dep.symbols.push_back("#"+std::to_string(v&0xffff));else{if(v>UINT32_MAX-2)throw std::runtime_error("invalid import name RVA");dep.symbols.push_back(name(v+2));}}if(!done)throw std::runtime_error("unterminated or excessive imports");out.imports.push_back(std::move(dep));}if(!ended)throw std::runtime_error("unterminated import directory");}
 return out;
}
Image inspect_file(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("cannot open executable"); auto n=f.tellg();if(n<0 || n>std::streamoff(512ull*1024*1024))throw std::runtime_error("file too large (512 MiB limit)");std::vector<uint8_t> b(static_cast<size_t>(n));f.seekg(0);if(!f.read(reinterpret_cast<char*>(b.data()),n))throw std::runtime_error("read failed");return inspect(b);}
}
