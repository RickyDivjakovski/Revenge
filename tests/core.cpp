#include "revenge/core.hpp"
#include "revenge/launch.hpp"
#include "revenge/loader.hpp"
#include "revenge/runtime.hpp"
#include <fstream>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <random>
using namespace revenge;
static void require(bool v){if(!v)throw std::runtime_error("test failed");}
static void put(std::vector<uint8_t>& b,size_t p,uint64_t v,unsigned n){for(unsigned i=0;i<n;++i)b.at(p+i)=uint8_t(v>>(8*i));}
int main(){require(classify_dependency("D3D12_XS.DLL")==DependencyKind::console);require(classify_dependency("api-ms-win-core-file-l1-1-0.dll")==DependencyKind::api_contract);require(classify_dependency("KERNEL32.DLL")==DependencyKind::system_candidate);require(classify_dependency("custom.dll")==DependencyKind::other);require(classify_dependency("../evil.dll")==DependencyKind::invalid);require(classify_dependency("C:\\evil.dll")==DependencyKind::invalid);Image candidate{};candidate.machine=0x8664;candidate.subsystem=3;candidate.entry=0x1000;require(windows_candidate(candidate,"test.exe"));candidate.imports.push_back({"xmem.dll",{}});require(!windows_candidate(candidate,"test.exe"));candidate.imports[0].dll="../evil.dll";require(!windows_candidate(candidate,"test.exe"));candidate.imports.clear();require(!windows_candidate(candidate,"scarlett.exe"));DirtyPages p(4096*130+1);p.mark(4095,2);p.mark(4096*63,4096*2);p.mark(4096*130,1);require(p.ranges()==std::vector<Range>{{0,8192},{4096*63,8192},{4096*130,1}});p.clear();require(p.ranges().empty());p.mark(0,4096*130+1);require(p.ranges()==std::vector<Range>{{0,4096*130+1}});bool caught=false;try{p.mark(4096*130+1,1);}catch(...){caught=true;}require(caught);
std::mt19937 rng(42);for(unsigned t=0;t<100;++t){DirtyPages d(257,4);std::vector<bool> expected(65);for(int j=0;j<100;++j){auto a=rng()%257,n=1+rng()%(257-a);d.mark(a,n);for(auto k=a/4;k<=(a+n-1)/4;++k)expected[k]=true;}std::vector<bool> got(65);for(auto r:d.ranges()){require(r.offset%4==0 && r.offset+r.size<=257);for(auto k=r.offset/4;k<=(r.offset+r.size-1)/4;++k)got[k]=true;}require(got==expected);}
std::vector<uint8_t>b(1024);put(b,0,0x5a4d,2);put(b,0x3c,0x80,4);put(b,0x80,0x4550,4);put(b,0x84,0x8664,2);put(b,0x86,1,2);put(b,0x94,240,2);size_t o=0x98;put(b,o,0x20b,2);put(b,o+16,0x1000,4);put(b,o+24,0x140000000,8);put(b,o+56,0x2000,4);put(b,o+60,512,4);put(b,o+108,16,4);size_t s=o+240;b[s]='x';put(b,s+8,512,4);put(b,s+12,0x1000,4);put(b,s+16,512,4);put(b,s+20,512,4);put(b,o+120,0x1000,4);put(b,o+124,40,4);put(b,512,0x1050,4);put(b,524,0x1080,4);put(b,528,0x1060,4);put(b,592,0x1090,8);put(b,600,0x8000000000000007ull,8);b[640]='d';b[641]='l';b[642]='l';b[658]='f';auto img=inspect(b);require(img.imports.size()==1&&img.imports[0].symbols==std::vector<std::string>{"f","#7"});// Add a DIR64 relocation block to the synthetic PE.
put(b,o+112+5*8,0x10a0,4);put(b,o+116+5*8,12,4);
put(b,512+0xa0,0x1000,4);put(b,512+0xa4,12,4);put(b,512+0xa8,0xa0c0,2);
put(b,512+0xc0,0x1400010d0ull,8);
auto original=prepare_image(b);auto rebased=prepare_image(b,0x180000000ull);
require(original.relocations==1&&rebased.relocations==1);
require(rebased.slots.size()==2&&rebased.slots[0].rva==0x1060&&rebased.slots[1].rva==0x1068);
uint64_t relocated=0;for(unsigned i=0;i<8;++i)relocated|=uint64_t(rebased.mapped[0x10c0+i])<<(8*i);
require(relocated==0x1800010d0ull);require(rebased.mapped[0x1800]==0);
put(b,512+0xa8,0x30c0,2);caught=false;try{prepare_image(b,0x180000000ull);}catch(...){caught=true;}require(caught);
put(b,512+0xa8,0xa0c0,2);put(b,o+112+5*8,0,4);caught=false;try{prepare_image(b,0x180000000ull);}catch(...){caught=true;}require(caught);
b.resize(513);caught=false;try{inspect(b);}catch(...){caught=true;}require(caught);require(!has_console_markers(img,"sample.exe"));require(has_console_markers(img,"game_scarlett_master.exe"));img.imports.push_back({"D3D12_XS.DLL",{}});require(has_console_markers(img,"sample.exe"));img.imports.pop_back();img.sections.push_back({".xbld",0,0,0,0});require(has_console_markers(img,"sample.exe"));
auto dir=std::filesystem::temp_directory_path()/("revenge-launch-test-"+std::to_string(std::chrono::steady_clock::now().time_since_epoch().count()));std::filesystem::create_directory(dir);try{std::ofstream(dir/"Game.EXE").put('x');std::ofstream(dir/"notes.txt").put('x');std::filesystem::create_directory(dir/"nested");std::ofstream(dir/"nested"/"other.exe").put('x');auto files=game_candidates(dir);require(files.size()==1&&files[0].filename()=="Game.EXE");}catch(...){std::filesystem::remove_all(dir);throw;}std::filesystem::remove_all(dir);std::cout<<"PE, relocation, import slot, dirty-page and launch discovery tests passed\n";}
