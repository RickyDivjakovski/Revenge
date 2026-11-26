#include "revenge/dependency_audit.hpp"
#include <map>
#include <set>
#include <sstream>
#include <charconv>
#include <stdexcept>
#include <functional>
namespace revenge { namespace {
std::string module_key(std::string s){for(auto& c:s)if(c>='A'&&c<='Z')c=char(c-'A'+'a');if(s.empty()||s.size()>255||s=="."||s==".."||s.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789._-+")!=std::string::npos)return {};return s;}
struct Indexed {const DependencyFile* file;std::map<std::string,const ExportSymbol*> names;std::map<uint32_t,const ExportSymbol*> ords;};
bool ordinal_value(const std::string& s,uint32_t& n){if(s.size()<2||s[0]!='#')return false;auto [p,e]=std::from_chars(s.data()+1,s.data()+s.size(),n);return e==std::errc{}&&p==s.data()+s.size();}
}
DependencyAudit audit_dependencies(const DependencyFile& root,const std::vector<DependencyFile>& files){
 if(files.size()>64)throw std::runtime_error("dependency audit: inventory limit");
 DependencyAudit out;std::map<std::string,Indexed> inventory;
 auto add=[&](const DependencyFile& f){auto k=module_key(f.name);if(k.empty()||!inventory.emplace(k,Indexed{&f,{},{}}).second)throw std::runtime_error("dependency audit: invalid or duplicate filename");auto& ix=inventory.at(k);if(f.exports.symbols.size()>65536)throw std::runtime_error("dependency audit: export limit");for(auto& s:f.exports.symbols){if(!s.rva)continue;if(!ix.ords.emplace(s.ordinal,&s).second)throw std::runtime_error("dependency audit: duplicate ordinal");for(auto& n:s.names)if(n.empty()||n.size()>4096||!ix.names.emplace(n,&s).second)throw std::runtime_error("dependency audit: invalid or duplicate export name");}};
 add(root);for(auto& f:files)add(f);out.supplied=inventory.size();
 std::vector<std::string> pending{module_key(root.name)};std::set<std::string> reached{pending.front()};std::map<std::string,std::set<std::string>> edges;std::ostringstream log;log<<"Offline transitive dependency audit (explicit inventory only).\n";
 auto edge=[&](const std::string& from,const std::string& to){if(inventory.contains(to)){edges[from].insert(to);if(reached.insert(to).second)pending.push_back(to);}};
 size_t references=0;
 for(size_t j=0;j<pending.size();++j){const auto owner=pending[j];for(const auto& d:inventory.at(owner).file->image.imports){for(size_t i=0;i<d.symbols.size();++i){if(++references>1000000)throw std::runtime_error("dependency audit: reference limit");auto mod=module_key(d.dll),sym=d.symbols[i];bool ord=i<d.by_ordinal.size()&&d.by_ordinal[i];log<<owner<<" -> "<<d.dll<<'!'<<sym;std::set<std::pair<std::string,uint32_t>> seen;unsigned hops=0;edge(owner,mod);
 for(;;){uint32_t ordinal=0;if(mod.empty()||sym.empty()||sym.size()>4096||(ord&&!ordinal_value(sym,ordinal))){++out.malformed;log<<" | malformed\n";break;}auto m=inventory.find(mod);if(m==inventory.end()){++out.missing_modules;log<<" | missing module\n";break;}const ExportSymbol* e=nullptr;if(ord){auto it=m->second.ords.find(ordinal);if(it!=m->second.ords.end())e=it->second;}else{auto it=m->second.names.find(sym);if(it!=m->second.names.end())e=it->second;}if(!e){++out.missing_symbols;log<<" | missing symbol\n";break;}if(!seen.emplace(mod,e->ordinal).second){++out.forwarder_cycles;log<<" | forwarder cycle\n";break;}if(e->forwarder.empty()){++out.resolved;log<<" | terminal #"<<e->ordinal<<" RVA "<<e->rva<<" (signature unverified)\n";break;}if(hops++==16){++out.depth_limits;log<<" | depth limit\n";break;}auto dot=e->forwarder.rfind('.');if(dot==std::string::npos||dot==0||dot+1==e->forwarder.size()){++out.malformed;log<<" | malformed forwarder\n";break;}auto next=module_key(e->forwarder.substr(0,dot));if(!next.empty()&&!next.ends_with(".dll")&&!next.ends_with(".exe"))next=module_key(next+".dll");edge(mod,next);mod=next;sym=e->forwarder.substr(dot+1);ord=sym[0]=='#';log<<" -> "<<mod<<'!'<<sym;}
 }}}
 // Count DFS back edges, not distinct elementary cycles. Deterministic lexical traversal.
 std::map<std::string,int> color;std::function<void(const std::string&)> visit=[&](const std::string& n){color[n]=1;for(auto& t:edges[n]){if(color[t]==1){++out.module_cycles;log<<"Module cycle back edge: "<<n<<" -> "<<t<<'\n';}else if(!color[t])visit(t);}color[n]=2;};visit(pending.front());
 out.reachable=reached.size();for(auto& [n,_]:inventory)if(!reached.contains(n))log<<"Unreachable inventory: "<<n<<'\n';
 log<<"Reachable: "<<out.reachable<<" / "<<out.supplied<<"; terminal exports: "<<out.resolved<<"; missing modules: "<<out.missing_modules<<"; missing symbols: "<<out.missing_symbols<<"; malformed: "<<out.malformed<<"; forwarder cycles: "<<out.forwarder_cycles<<"; depth limits: "<<out.depth_limits<<"; module cycle back edges: "<<out.module_cycles<<".\nNo delay or dynamic imports, API-set resolution, signature verification, binding or execution.\n";out.report=log.str();return out;
}
}
