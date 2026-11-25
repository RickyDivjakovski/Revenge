#include "revenge/system.hpp"
#include <array>
#include <iostream>
#include <stdexcept>
#include <limits>
using namespace revenge;
static void require(bool ok){if(!ok)throw std::runtime_error("system test failed");}
template<class F> void rejects(F f){bool caught=false;try{f();}catch(const std::exception&){caught=true;}require(caught);}
int main(){
 MemoryArena a(8,16,256);auto pages=a.allocate(3);require(a.used_pages()==3);a.clear_dirty();
 std::array<uint32_t,2> scattered{pages[2],pages[0]};a.map(1,scattered);a.map(5,std::span<const uint32_t>(pages).first(1));
 std::array<uint8_t,4> data{11,22,33,44},got{};a.write(510,data);a.read(510,got);require(got==data);
 std::array<uint8_t,2> alias{};a.read(5*256,alias);require(alias[0]==33&&alias[1]==44);
 require(a.dirty_ranges()==std::vector<Range>{{0,256},{512,256}});
 rejects([&]{a.release(pages);});rejects([&]{a.map(2,pages);});rejects([&]{a.write(3*256-1,data);});
 a.read(510,got);require(got==data);a.unmap(1,2);a.unmap(5,1);a.release(pages);require(a.used_pages()==0);
 rejects([&]{a.release(pages);});auto again=a.allocate(3);a.map(0,again);a.read(0,got);require(got==std::array<uint8_t,4>{});
 rejects([&]{a.allocate(6);});rejects([&]{a.read(16*256,got);});rejects([]{MemoryArena b(0,1);});
 require(auditable_contract("API-MS-WIN-CORE-FILE-L1-1-0.DLL"));require(!auditable_contract("api-ms-win-core-fake.dll"));require(!auditable_contract("../api-ms-win-core-file-l1-1-0.dll"));require(!api_registry().empty());
 const auto root=std::filesystem::absolute("fixture-root");require(translate_path(root,"G:\\test\\file.bin")==root/"content"/"test"/"file.bin");require(translate_path(root,"u:/save")==root/"user"/"save");rejects([&]{translate_path(root,"G:\\..\\escape");});rejects([&]{translate_path(root,"G:\\NUL.txt");});rejects([&]{translate_path(root,"G:\\test:stream");});rejects([&]{translate_path(root,"Z:\\test");});rejects([&]{translate_path(root,"G:relative");});rejects([&]{translate_path(root,"G:\\\\test");});
 const std::array<TuneSample,3> samples{{{1,50523.9},{3,83817.2},{6,104614}}};require(choose_slots(samples)==6);const std::array<TuneSample,3> plateau{{{3,97},{6,100},{8,99}}};require(choose_slots(plateau)==3);rejects([]{choose_slots({});});require(slot_budget(16ull*1024*1024*1024,4*1024*1024)==8);require(slot_budget(128ull*1024*1024,4*1024*1024)==0);
 std::cout<<"Physical-page model, aliasing, dirty tracking, path mapping, API registry and tuning tests passed\n";
}
