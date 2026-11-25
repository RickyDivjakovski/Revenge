#include "revenge/shader_package.hpp"
#include "revenge/shader_ir.hpp"
#include "revenge/shader_stage.hpp"
#include "revenge/resource_registry.hpp"
#include "revenge/application.hpp"
#include "revenge/loader.hpp"
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace revenge;
void require(bool v){if(!v)throw std::runtime_error("package assertion failed");}
template<class F>void reject(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}require(bad);}
int main(){try{
 auto p=package::fixture();auto bytes=package::encode(p);auto r=package::decode(bytes);require(p==r);require(package::encode(r)==bytes);package::require_scene_fixture(r);
 for(size_t n=0;n<bytes.size();++n)reject([&]{package::decode(std::span<const uint8_t>(bytes).first(n));});
 for(size_t i=0;i<bytes.size();++i){auto b=bytes;b[i]^=1;reject([&]{package::decode(b);});}
 auto trailing=bytes;trailing.push_back(0);reject([&]{package::decode(trailing);});
 auto bad=p;bad.compute.clear();reject([&]{package::encode(bad);});bad=p;bad.vertex=p.pixel;reject([&]{package::encode(bad);});bad=p;bad.compute.resize(4097);reject([&]{package::encode(bad);});
 auto narrow=package::narrow_fixture();package::require_custom(narrow);require(package::decode(package::encode(narrow))==narrow);reject([&]{package::require_scene_fixture(narrow);});
 for(float x:{-1.f,-.5f,0.f,.5f,1.f}){auto input=std::array<float,5>{x,.25f,1.f,.5f,.5f};auto a=stage::evaluate(stage::Stage::vertex,p.vertex,input),b=stage::evaluate(stage::Stage::vertex,narrow.vertex,input);require(b.position[0]==a.position[0]*.5f&&b.position[1]==a.position[1]&&b.position[2]==a.position[2]&&b.position[3]==a.position[3]&&b.parameter==a.parameter);}
 auto alu=shader::optimize(shader::decode(r.compute));for(unsigned i=0;i<4096;++i)require(shader::evaluate(alu,i)==3*i+1);require(resources::texture_compute_hlsl(256,r.compute)==resources::texture_compute_hlsl(256));
 require(stage::emit(stage::Stage::vertex,r.vertex)==stage::emit(stage::Stage::vertex,p.vertex));require(stage::emit(stage::Stage::pixel,r.pixel)==stage::emit(stage::Stage::pixel,p.pixel));
 std::ostringstream out;require(app::fixture(out,320,240,{},&r)==0);require(out.str().find("package round-trip")!=std::string::npos);
 LoadPlan plan;plan.slots={{"kernel32.dll","GetTickCount",0x1000},{"xmem.dll","unknown",0x1008},{"custom.dll","#7",0x1010}};auto report=import_plan_report(plan);require(report.find("host audit candidates: 1; unsupported/unverified: 2; bound: 0")!=std::string::npos);plan.slots.push_back({"revenge_abi.dll","rv_dispatch",0x1018});plan.slots.push_back({"MSVCP140_ATOMIC_WAIT.dll","__std_atomic_wait_direct",0x1020});plan.slots.push_back({"revenge_abi.dll","unknown",0x1028});report=import_plan_report(plan);require(report.find("host audit candidates: 2; unsupported/unverified: 3; bound: 0")!=std::string::npos&&report.find("Revenge ABI declarations (not export-verified): 1")!=std::string::npos);plan.slots.push_back(plan.slots.front());reject([&]{import_plan_report(plan);});
 std::cout<<"RSP1 roundtrip, "<<bytes.size()<<" truncated lengths, "<<bytes.size()<<" single-bit corruptions, 4096 CPU outputs, application service and import plan verified.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
