#include "revenge/dependency_graph.hpp"
#include "revenge/runtime.hpp"
#include <map>
#include <set>
#include <sstream>
#include <stdexcept>
namespace revenge { namespace {
std::string key(std::string name){for(auto& c:name)if(c>='A'&&c<='Z')c=char(c-'A'+'a');if(name.empty()||name=="."||name==".."||name.find_first_not_of("abcdefghijklmnopqrstuvwxyz0123456789._-+")!=std::string::npos)throw std::runtime_error("dependency graph: invalid inventory filename");return name;}
}
std::string dependency_graph_report(const std::string& root,const Image& image,const std::vector<std::pair<std::string,Image>>& supplied){
 if(supplied.size()>64)throw std::runtime_error("dependency graph: inventory limit");
 auto start=key(root);std::map<std::string,const Image*> files{{start,&image}};
 for(const auto& [name,im]:supplied)if(!files.emplace(key(name),&im).second)throw std::runtime_error("dependency graph: duplicate filename");
 std::vector<std::string> pending{start};std::set<std::string> visited{start};std::map<std::string,uint64_t> missing;uint64_t edges=0,symbols=0,known=0,invalid=0;std::ostringstream o;o<<"Offline direct-import graph (explicit files only).\n";
 for(size_t i=0;i<pending.size();++i){auto name=pending[i];std::map<std::string,uint64_t> deps;for(const auto& d:files.at(name)->imports){std::string target;try{target=key(d.dll);}catch(const std::runtime_error&){++invalid;continue;}deps[target]+=d.symbols.size();}
  for(const auto& [target,n]:deps){++edges;symbols+=n;bool present=files.contains(target);o<<name<<" -> "<<target<<" | "<<n<<" symbols | "<<(present?"supplied":dependency_label(classify_dependency(target)))<<'\n';if(present){++known;if(visited.insert(target).second)pending.push_back(target);}else missing[target]+=n;}
 }
 for(const auto& [name,_]:files)if(!visited.contains(name))o<<"Not reached by direct imports: "<<name<<'\n';
 for(const auto& [name,n]:missing)o<<"Unprovided: "<<name<<" | "<<n<<" symbol references\n";
 o<<"Reachable supplied files: "<<visited.size()<<" / "<<files.size()<<"; module edges: "<<edges<<"; supplied edges: "<<known<<"; symbol references: "<<symbols<<"; distinct unprovided modules: "<<missing.size()<<"; invalid descriptors: "<<invalid<<".\nNo delay/forwarder/dynamic edge discovery, API-set mapping, signature validation or code execution.\n";return o.str();
}
}
