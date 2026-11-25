#include "native_reference.h"
#include <atomic>
#include <stdexcept>
namespace {
std::atomic<uint32_t> retired{0};
struct Local { uint32_t value=17; ~Local(){retired.fetch_add(1,std::memory_order_relaxed);} };
thread_local Local local;
struct Cleanup { uint32_t& n; ~Cleanup(){++n;} };
#if defined(_MSC_VER)
__declspec(noinline)
#else
__attribute__((noinline))
#endif
void throw_nested(uint32_t& n){Cleanup guard{n};throw std::runtime_error("reference exception");}
}
uint32_t ref_step(uint32_t increment,uint32_t* value){if(!value||increment>1000)return 1;local.value+=increment;*value=local.value;return 0;}
uint32_t ref_unwind(uint32_t value,uint32_t* result){if(!result)return 1;uint32_t n=0;try{Cleanup outer{n};throw_nested(n);}catch(const std::runtime_error&){*result=value+n;return n==2?0:2;}catch(...){return 3;}return 4;}
uint32_t ref_retired(void){return retired.load(std::memory_order_relaxed);}
