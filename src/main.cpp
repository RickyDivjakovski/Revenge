#include "revenge/core.hpp"
#include "revenge/launch.hpp"
#include "revenge/loader.hpp"
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <string_view>
#include <charconv>
#include <algorithm>
#include <cctype>
using namespace revenge;
static unsigned number(const char* p,unsigned lo,unsigned hi){unsigned n{};std::string_view s(p);auto [end,e]=std::from_chars(s.data(),s.data()+s.size(),n);if(e!=std::errc{}||end!=s.data()+s.size()||n<lo||n>hi)throw std::runtime_error("numeric argument out of range");return n;}
static int inspect_cmd(const char* p){auto x=inspect_file(std::filesystem::path(p));std::cout<<"AMD64 PE32+; subsystem "<<x.subsystem<<"; base 0x"<<std::hex<<x.base<<"; entry RVA 0x"<<x.entry<<std::dec<<"; image bytes "<<x.image_size<<"\nTLS "<<x.tls<<", unwind "<<x.unwind<<", relocations "<<x.reloc<<"\n";size_t count=0;for(auto& s:x.sections)std::cout<<"section "<<s.name<<" rva=0x"<<std::hex<<s.rva<<std::dec<<" raw="<<s.raw_size<<" virtual="<<s.virtual_size<<"\n";for(auto& d:x.imports){count+=d.symbols.size();std::cout<<"\n"<<d.dll<<" ("<<d.symbols.size()<<")\n";for(auto& n:d.symbols)std::cout<<"  "<<n<<"\n";}std::cout<<"\n"<<x.imports.size()<<" direct DLL descriptors; "<<count<<" direct imports. Delay/dynamic/transitive imports not included.\nRead-only inspection; no title code executed.\n";return 0;}
static int mem_bench(){DirtyPages p(256ull*1024*1024);uint64_t ranges=0,bytes=0;auto begin=std::chrono::steady_clock::now();for(unsigned k=0;k<1000;++k){p.clear();for(unsigned j=0;j<512;++j)p.mark((uint64_t(j)*131072+k%8*4096),16384);for(auto r:p.ranges()){++ranges;bytes+=r.size;}}auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-begin).count();std::cout<<"Synthetic dirty-page batch test: "<<ms<<" ms; 1000 iterations; "<<ranges<<" ranges; "<<bytes<<" marked bytes (not transferred).\n";return 0;}
int main(int argc,char** argv){try{
#ifdef _WIN32
if(argc==1 || (argc==2 && std::string_view(argv[1])=="browse")) return launch_dialog();
#endif
if(argc<2 || std::string_view(argv[1])=="--help"){std::cout<<"Revenge Native 0.6.0 (independent foundation)\nbrowse (Windows) | inspect <exe> | prepare <exe> [--rebase] | gpu-info | gpu-bench [iterations=240] [slots=3] [MiB=4] | mem-bench | gpu-compute <sample.dxil>\nNo Xbox execution or native shader translation in this release.\n";return 0;}std::string_view c=argv[1];if(c=="--version"){std::cout<<"Revenge Native 0.6.0\n";return 0;}if(c=="inspect"&&argc==3)return inspect_cmd(argv[2]);if(c=="prepare"&&(argc==3||(argc==4&&std::string_view(argv[3])=="--rebase"))){auto path=std::filesystem::path(argv[2]);auto plan=prepare_file(path,argc==4?0x180000000ull:0);std::cout<<preparation_report(plan,path);return 0;}if(c=="gpu-info"&&argc==2)return gpu_info();if(c=="gpu-compute"&&argc==3)return gpu_compute(std::filesystem::path(argv[2]));if(c=="mem-bench"&&argc==2)return mem_bench();if(c=="gpu-bench"&&argc<=5)return gpu_bench(argc>2?number(argv[2],1,100000):240,argc>3?number(argv[3],1,8):3,argc>4?number(argv[4],1,64):4);throw std::runtime_error("unsupported command; see --help");}catch(const std::exception& e){std::cerr<<"Revenge: "<<e.what()<<"\n";return 1;}}
