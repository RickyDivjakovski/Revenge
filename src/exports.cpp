#include "revenge/exports.hpp"
#include <algorithm>
#include <charconv>
#include <fstream>
#include <map>
#include <sstream>
#include <set>
#include <string_view>
#include <stdexcept>
namespace revenge { namespace {
void need(bool b){if(!b)throw std::runtime_error("exports: malformed or unsupported PE export table");}
uint64_t get(std::span<const uint8_t> b,uint64_t p,unsigned n){need(p<=b.size()&&n<=b.size()-p);uint64_t v=0;for(unsigned i=0;i<n;++i)v|=uint64_t(b[p+i])<<(8*i);return v;}
std::string lower(std::string s){for(auto& c:s)if(c>='A'&&c<='Z')c=char(c-'A'+'a');return s;}
}
ExportTable inspect_exports(std::span<const uint8_t> b){
 auto image=inspect(b);auto pe=get(b,0x3c,4),o=pe+24,sz=get(b,pe+20,2),hdr=get(b,o+60,4);ExportTable out;
 if(sz<120||!get(b,o+108,4))return out;
 auto rva=get(b,o+112,4),size=get(b,o+116,4);if(!rva&&!size)return out;need(rva&&size>=40&&rva+size<=image.image_size);
 auto map=[&](uint64_t a,uint64_t n){std::pair<uint64_t,uint64_t> m{};unsigned found=0;if(a<hdr&&n<=hdr-a){m={a,hdr};++found;}for(auto& s:image.sections)if(a>=s.rva&&a-s.rva<s.raw_size&&n<=s.raw_size-(a-s.rva)){m={s.raw+a-s.rva,uint64_t(s.raw)+s.raw_size};++found;}need(found==1&&m.second<=b.size());return m;};
 auto str=[&](uint64_t a,uint64_t limit){auto [p,e]=map(a,1);e=std::min(e,p+limit);std::string s;for(;p<e&&s.size()<4096;++p){auto c=b[p];if(!c){need(!s.empty());return s;}need(c>=32&&c<=126);s+=char(c);}throw std::runtime_error("exports: unterminated string");};
 auto p=map(rva,40).first;auto base=get(b,p+16,4),nf=get(b,p+20,4),nn=get(b,p+24,4);need(nf<=65536&&nn<=65536&&(!nf||base+nf-1<=UINT32_MAX));
 out.module=str(get(b,p+12,4),4096);auto f=get(b,p+28,4),n=get(b,p+32,4),ord=get(b,p+36,4);need((!nf||f)&&(!nn||(n&&ord&&nf)));
 if(nf)map(f,nf*4);
 if(nn){map(n,nn*4);map(ord,nn*2);}
 out.symbols.resize(static_cast<size_t>(nf));
 for(uint64_t i=0;i<nf;++i){auto& e=out.symbols[i];e.ordinal=uint32_t(base+i);e.rva=uint32_t(get(b,map(f+i*4,4).first,4));if(!e.rva)continue;need(e.rva<image.image_size);if(e.rva>=rva&&e.rva<rva+size)e.forwarder=str(e.rva,rva+size-e.rva);else{bool occupied=false;for(auto& s:image.sections)if(e.rva>=s.rva&&uint64_t(e.rva)-s.rva<std::max(s.raw_size,s.virtual_size))occupied=true;need(occupied);}}
 std::map<std::string,bool> seen;for(uint64_t i=0;i<nn;++i){auto index=get(b,map(ord+i*2,2).first,2);need(index<nf&&out.symbols[index].rva);auto name=str(get(b,map(n+i*4,4).first,4),4096);need(seen.emplace(name,true).second);out.symbols[index].names.push_back(std::move(name));}return out;
}
ExportTable inspect_exports_file(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary|std::ios::ate);if(!f)throw std::runtime_error("cannot open dependency file");auto n=f.tellg();if(n<0||n>std::streamoff(512ull*1024*1024))throw std::runtime_error("dependency exceeds 512 MiB");std::vector<uint8_t>b(static_cast<size_t>(n));f.seekg(0);if(!f.read(reinterpret_cast<char*>(b.data()),n))throw std::runtime_error("dependency read failed");return inspect_exports(b);}
std::string export_report(const ExportTable& t){std::ostringstream o;o<<"Offline exports: "<<t.module<<"\n";size_t live=0,names=0,forward=0;for(auto& e:t.symbols){if(!e.rva)continue;++live;names+=e.names.size();forward+=!e.forwarder.empty();o<<"#"<<e.ordinal<<" RVA 0x"<<std::hex<<e.rva<<std::dec;for(auto& n:e.names)o<<" | "<<n;if(!e.forwarder.empty())o<<" | forwarder: "<<e.forwarder;o<<'\n';}o<<"Export slots: "<<t.symbols.size()<<"; populated: "<<live<<"; names: "<<names<<"; forwarded: "<<forward<<"\nNo modules loaded or addresses bound.\n";return o.str();}
std::string dependency_report(const Image& image,const std::vector<std::pair<std::string,ExportTable>>& files){
 struct Index{std::map<std::string,const ExportSymbol*> names;std::map<uint32_t,const ExportSymbol*> ords;};std::map<std::string,Index> modules;
 for(auto& [name,t]:files){auto key=lower(name);if(key.empty()||!modules.emplace(key,Index{}).second)throw std::runtime_error("duplicate dependency filename");auto& idx=modules.at(key);for(auto& e:t.symbols)if(e.rva){idx.ords.emplace(e.ordinal,&e);for(auto& n:e.names)idx.names.emplace(n,&e);}}
 size_t direct=0,forward=0,missing=0,unprovided=0;std::ostringstream o;o<<"Offline dependency inventory (direct imports only).\n";
 for(auto& d:image.imports){auto mod=modules.find(lower(d.dll));for(size_t si=0;si<d.symbols.size();++si){auto& sym=d.symbols[si];o<<d.dll<<'!'<<sym<<" | ";if(mod==modules.end()){++unprovided;o<<"module not supplied\n";continue;}const ExportSymbol* e=nullptr;auto& idx=mod->second; // Preserve the parser explicit ordinal flag.
 if(si<d.by_ordinal.size()&&d.by_ordinal[si]&&sym.size()>1&&sym[0]=='#'){uint32_t ord=0;auto [p,ec]=std::from_chars(sym.data()+1,sym.data()+sym.size(),ord);if(ec==std::errc{}&&p==sym.data()+sym.size()){auto it=idx.ords.find(ord);if(it!=idx.ords.end())e=it->second;}}else{auto it=idx.names.find(sym);if(it!=idx.names.end())e=it->second;}
 if(!e){++missing;o<<"symbol absent\n";}else if(!e->forwarder.empty()){++forward;o<<"forwarder declared (not followed): "<<e->forwarder<<'\n';}else{++direct;o<<"export present (signature unverified)\n";}}}
 o<<"Direct exports present: "<<direct<<"; forwarded: "<<forward<<"; missing symbols: "<<missing<<"; module not supplied: "<<unprovided<<"; bound: 0\nNo API-set mapping, transitive loading or execution.\n";return o.str();}
std::string dependency_chain_report(const Image& image,const std::vector<std::pair<std::string,ExportTable>>& files){
 struct Index{std::map<std::string,const ExportSymbol*> names;std::map<uint32_t,const ExportSymbol*> ords;};
 std::map<std::string,Index> modules;
 for(const auto& [name,t]:files){auto key=lower(name);if(key.empty()||!modules.emplace(key,Index{}).second)throw std::runtime_error("duplicate dependency filename");auto& idx=modules.at(key);for(const auto& e:t.symbols)if(e.rva){idx.ords.emplace(e.ordinal,&e);for(const auto& n:e.names)idx.names.emplace(n,&e);}}
 std::ostringstream o;o<<"Offline forwarder chains (direct import roots; explicit files only).\n";
 size_t direct=0,resolved=0,missing_module=0,missing_symbol=0,cycle=0,malformed=0,limit=0;
 for(const auto& d:image.imports)for(size_t i=0;i<d.symbols.size();++i){
  auto mod=lower(d.dll),sym=d.symbols[i];bool ordinal=i<d.by_ordinal.size()&&d.by_ordinal[i];unsigned hops=0;
  std::set<std::pair<std::string,uint32_t>> seen;o<<d.dll<<'!'<<sym;
  for(;;){
   auto m=modules.find(mod);if(m==modules.end()){++missing_module;o<<" | module not supplied\n";break;}
   const ExportSymbol* e=nullptr;
   if(ordinal){uint32_t n=0;if(sym.size()<2||sym[0]!='#'){++malformed;o<<" | malformed ordinal\n";break;}
    auto [p,ec]=std::from_chars(sym.data()+1,sym.data()+sym.size(),n);if(ec!=std::errc{}||p!=sym.data()+sym.size()){++malformed;o<<" | malformed ordinal\n";break;}auto it=m->second.ords.find(n);if(it!=m->second.ords.end())e=it->second;
   }else{auto it=m->second.names.find(sym);if(it!=m->second.names.end())e=it->second;}
   if(!e){++missing_symbol;o<<" | symbol absent\n";break;}
   if(!seen.emplace(mod,e->ordinal).second){++cycle;o<<" | forwarder cycle\n";break;}
   if(e->forwarder.empty()){if(hops)++resolved;else ++direct;o<<" | terminal export present (signature unverified)\n";break;}
   if(hops==16){++limit;o<<" | forwarder depth limit\n";break;}
   const auto& f=e->forwarder;auto dot=f.rfind('.');
   if(dot==std::string::npos||dot==0||dot+1==f.size()){++malformed;o<<" | malformed forwarder\n";break;}
   mod=lower(f.substr(0,dot));sym=f.substr(dot+1);ordinal=sym[0]=='#';
   if(mod.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789._-")!=std::string::npos||mod=="."||mod==".."){++malformed;o<<" | malformed forwarder module\n";break;}
   if(!mod.ends_with(".dll")&&!mod.ends_with(".exe"))mod+=".dll";
   ++hops;o<<" -> "<<mod<<'!'<<sym;
  }
 }
 o<<"Direct terminals: "<<direct<<"; resolved forwarders: "<<resolved<<"; missing modules: "<<missing_module<<"; missing symbols: "<<missing_symbol<<"; cycles: "<<cycle<<"; malformed: "<<malformed<<"; depth limited: "<<limit<<"; bound: 0\nNo API-set mapping, transitive module loading or execution.\n";return o.str();
}
}
