#include "revenge/target_plan.hpp"
#include "revenge/runtime.hpp"
#include "revenge/launch.hpp"
#include "revenge/gdk_inventory.hpp"
#include <algorithm>
#include <fstream>
#include <map>
#include <sstream>
#include <stdexcept>
namespace revenge {
std::string compatibility_report(const LoadPlan& p,const std::filesystem::path& path){
 std::ostringstream o;auto spans=page_layout(p);bool entry=false,raw=false;uint64_t wx=0,gaps=0;
 for(const auto& s:spans){if((s.flags&6)==6)wx+=s.bytes;if(!(s.flags&8))gaps+=s.bytes;}
 for(const auto& s:p.image.sections)if(p.image.entry&&p.image.entry>=s.rva&&uint64_t(p.image.entry)-s.rva<std::max(s.raw_size,s.virtual_size)){entry=(s.characteristics&0x20000000u)!=0;raw=uint64_t(p.image.entry)-s.rva<s.raw_size;}
 o<<"Revenge compatibility report v1 (offline only)\nFormat: AMD64 PE32+; console markers (heuristic): "<<has_console_markers(p.image,path)<<"\nEntry RVA: 0x"<<std::hex<<p.image.entry<<std::dec<<"; executable section: "<<entry<<"; file-backed: "<<raw<<"\nImage bytes: "<<p.mapped.size()<<"; WX bytes: "<<wx<<"; gap bytes: "<<gaps<<'\n';
 std::map<std::string,uint64_t> modules;uint64_t console=0,external=0,invalid=0,ordinals=0;
 for(const auto& d:p.image.imports){modules[d.dll]+=d.symbols.size();auto k=classify_dependency(d.dll);if(k==DependencyKind::console)console+=d.symbols.size();if(k==DependencyKind::other)external+=d.symbols.size();if(k==DependencyKind::invalid)invalid+=d.symbols.size();ordinals+=std::count(d.by_ordinal.begin(),d.by_ordinal.end(),true);}
 o<<"Direct modules: "<<modules.size()<<"; console slots: "<<console<<"; external slots: "<<external<<"; invalid-name slots: "<<invalid<<"; ordinal imports: "<<ordinals<<'\n';
 for(const auto& [name,count]:modules)o<<name<<" | "<<dependency_label(classify_dependency(name))<<" | "<<count<<'\n';
 o<<"Launch verdict: NOT VERIFIED (no target execution)\n";
 if(!entry||!raw)o<<"Blocker: entry is not in a file-backed executable section.\n";
 if(console)o<<"Blocker: console-specific direct imports have no verified binding.\n";
 if(external||invalid)o<<"Blocker: external or invalid-name dependencies require review.\n";
 if(p.delay_imports)o<<"Review: delay imports require separate binding analysis.\n";
 if(p.image.tls)o<<"Review: target TLS initialization is not performed.\n";
 if(p.unwind_entries)o<<"Review: target unwind registration is not performed.\n";
 if(p.load_config)o<<"Review: target load configuration requires compatibility analysis.\n";
 o<<'\n'<<target_plan_report(p,path)<<'\n'<<layout_report(p)<<'\n'<<init_metadata_report(p)<<'\n'<<tls_template_report(p)<<'\n'<<import_plan_report(p)<<'\n';gaming::report(p.image,o);return o.str();
}
std::string compatibility_file(const std::filesystem::path& path){std::ifstream f(path,std::ios::binary);if(!f)throw std::runtime_error("cannot open target");char b[4]{};f.read(b,4);if(f.gcount()!=4)throw std::runtime_error("target signature truncated");if(b[0]!='M'||b[1]!='Z')throw std::runtime_error("unsupported target format: expected accessible PE executable; no extraction or decryption performed");return compatibility_report(prepare_file(path),path);}
}
