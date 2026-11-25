#include "revenge/gdk_inventory.hpp"
#include <sstream>
#include <iostream>
#include <set>
int main(){using namespace revenge;using namespace revenge::gaming;
    std::set<std::string_view> names;for(auto e:catalog())if(e.area.empty()||e.state.empty()||!names.insert(e.symbol).second)return 1;
    Image img{};img.imports.push_back({"example.dll",{"XTaskQueueCreate","XGameRuntimeInitialize","xtaskqueuecreate","XUserAddAsync","GameInputCreate"},{false,false,false,true,false}});
    std::ostringstream out;report(img,out);auto s=out.str();
    if(names.size()!=20||s.find("Named imports: 4; ordinal imports: 1; catalog name matches: 3; bound: 0.")==std::string::npos||s.find("example.dll!XUserAddAsync")!=std::string::npos)return 1;
    std::cout<<"20 public GDK API catalog entries; exact named matching and ordinal exclusion passed. No addresses bound.\n";return 0;
}
