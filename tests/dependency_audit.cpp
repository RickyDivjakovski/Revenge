#include "revenge/dependency_audit.hpp"
#include <iostream>
#include <stdexcept>
using namespace revenge;
void check(bool b){if(!b)throw std::runtime_error("dependency audit mismatch");}
int main(){try{
 DependencyFile root{"root.exe",{},{}},a{"a.dll",{},{}},b{"b.dll",{},{}},c{"c.dll",{},{}},unused{"unused.dll",{}, {}};
 root.image.imports={{"A.DLL",{"Go","#2","Loop","Bad","Absent","#bad"},{false,true,false,false,false,true}},{"missing.dll",{"x"},{}}};
 a.exports.symbols={{1,100,{"Go"},"b.Middle"},{2,200,{"#2"},{}},{3,300,{"Loop"},"a.#3"},{4,400,{"Bad"},"../b.Foo"}};
 a.image.imports={{"C.dll",{"End"},{}}};b.exports.symbols={{7,100,{"Middle"},"c.#8"}};c.exports.symbols={{8,100,{"End"},{}}};c.image.imports={{"a.dll",{"#2"},{true}}};unused.image.imports={{"never.dll",{"x"},{}}};
 auto r=audit_dependencies(root,{a,b,c,unused});check(r.reachable==4&&r.supplied==5&&r.resolved==4&&r.missing_modules==1&&r.missing_symbols==1&&r.malformed==2&&r.forwarder_cycles==1&&r.depth_limits==0&&r.module_cycles>=1);check(r.report.find("never.dll")==std::string::npos);check(r.report.find("Unreachable inventory: unused.dll")!=std::string::npos);
 root.image.imports={{"a.dll",{"#2"},{false}}};check(audit_dependencies(root,{a,b,c}).resolved==3); // literal export name, plus transitive imports
 a.exports.symbols[1].names.clear();check(audit_dependencies(root,{a,b,c}).missing_symbols==1);
 bool rejected=false;try{audit_dependencies(root,{a,a});}catch(const std::runtime_error&){rejected=true;}check(rejected);
 rejected=false;a.exports.symbols.push_back(a.exports.symbols.front());try{audit_dependencies(root,{a});}catch(const std::runtime_error&){rejected=true;}check(rejected);
 std::cout<<"Transitive dependency audit: named/ordinal identity, forwarders, missing dependencies, cycles, malformed records, duplicate inventory and unreachable isolation passed. No binding.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
