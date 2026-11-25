#include "revenge/gdk_inventory.hpp"
#include <algorithm>
#include <ostream>
namespace revenge::gaming {
std::span<const Api> catalog() noexcept {
    static constexpr Api entries[]={
        {"XGameRuntimeInitialize","lifecycle","PC reference only"}, {"XGameRuntimeUninitialize","lifecycle","PC reference only"},
        {"XGameRuntimeInitializeWithOptions","lifecycle","not implemented"}, {"XSystemGetDeviceType","system","PC reference only"},
        {"XTaskQueueCreate","async","manual reference subset"}, {"XTaskQueueSubmitCallback","async","manual reference subset"},
        {"XTaskQueueDispatch","async","manual reference subset"}, {"XTaskQueueTerminate","async","manual reference subset"},
        {"XTaskQueueCloseHandle","async","manual reference subset"}, {"XTaskQueueDuplicateHandle","async","scheduled reference subset"},
        {"XTaskQueueSubmitDelayedCallback","async","scheduled reference subset"}, {"XAsyncGetStatus","async","not implemented"},
        {"XAsyncCancel","async","not implemented"}, {"XUserAddAsync","user","not implemented"},
        {"XUserCloseHandle","user","not implemented"}, {"GameInputCreate","input","not implemented"},
        {"GameInputInitialize","input","not implemented"}, {"XSystemGetRuntimeInfo","system","not implemented"},
        {"XGameSaveInitializeProvider","storage","not implemented"}, {"XGameSaveCloseProvider","storage","not implemented"}
    };return entries;
}
void report(const Image& image,std::ostream& out){
    size_t named=0,ordinal=0,matched=0;
    out<<"Public GDK named direct-import inventory (offline only).\n";
    for(const auto& module:image.imports) for(size_t i=0;i<module.symbols.size();++i){
        if(i<module.by_ordinal.size()&&module.by_ordinal[i]){++ordinal;continue;}
        ++named;const auto& symbol=module.symbols[i];
        auto entries=catalog();auto it=std::find_if(entries.begin(),entries.end(),[&](const Api& a){return a.symbol==symbol;});
        if(it!=entries.end()){++matched;out<<module.dll<<'!'<<symbol<<" | "<<it->area<<" | "<<it->state<<'\n';}
    }
    out<<"Named imports: "<<named<<"; ordinal imports: "<<ordinal<<"; catalog name matches: "<<matched<<"; bound: 0.\n";
    out<<"Names alone do not prove ABI or module identity. Static SDK code, delay imports and dynamic lookup are not covered.\n";
}
}
