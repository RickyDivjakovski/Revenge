#include "revenge/exports.hpp"
#include <iostream>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>
using namespace revenge;
void check(bool v){if(!v)throw std::runtime_error("chain fixture mismatch");}
int main(){try{
 ExportTable a{"a.dll",{{1,100,{"Direct"},{}},{2,200,{"Chain"},"B.Middle"},{3,300,{"Loop","Alias"},"a.#3"},{4,400,{"Absent"},"missing.Foo"},{5,500,{"NoSymbol"},"b.NoSuch"},{6,600,{"Bad"},"invalid"},{7,700,{"Deep"},"d0.Next"},{8,800,{"BadOrdinal"},"b.#abc"},{9,900,{"BadPath"},"../b.Foo"}}};
 ExportTable b{"b.dll",{{1,100,{"Middle"},"C.dll.#5"}}};ExportTable c{"c.dll",{{5,100,{"End"},{}}}};
 Image im{};im.imports={{"A.DLL",{"Direct","Chain","Alias","Absent","NoSymbol","Bad","Deep","BadOrdinal","BadPath"},{false,false,false,false,false,false,false,false,false}}};
 std::vector<std::pair<std::string,ExportTable>> files{{"a.dll",a},{"b.dll",b},{"c.dll",c}};
 for(unsigned i=0;i<18;++i){auto name="d"+std::to_string(i)+".dll";files.emplace_back(name,ExportTable{name,{{1,100,{"Next"},i==17?std::string{}:"d"+std::to_string(i+1)+".Next"}}});}
 auto r=dependency_chain_report(im,files);check(r.find("A.DLL!Chain -> b.dll!Middle -> c.dll!#5 | terminal export present")!=std::string::npos);
 check(r.find("Direct terminals: 1; resolved forwarders: 1; missing modules: 1; missing symbols: 1; cycles: 1; malformed: 3; depth limited: 1; bound: 0")!=std::string::npos);
 std::cout<<"Offline chains: named and ordinal multi-hop terminal, alias cycle, missing modules/symbols, malformed paths/ordinals and depth cap passed.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
