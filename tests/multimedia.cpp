#include "revenge/multimedia.hpp"
#include "revenge/application.hpp"
#include <cmath>
#include <fstream>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
using namespace revenge::media;
namespace fs=std::filesystem;
void require(bool v){if(!v)throw std::runtime_error("multimedia test failure");}
template<class F>void reject(F f){bool bad=false;try{f();}catch(const std::exception&){bad=true;}require(bad);}
int main(){try{
 auto pcm=tone_fixture();require(pcm.size()==96000&&pcm.front()==0&&pcm.back()==0);bool nonzero=false;for(size_t i=0;i<pcm.size();i+=2){require(pcm[i]==pcm[i+1]&&std::abs(int(pcm[i]))<=3000);nonzero|=pcm[i]!=0;}require(nonzero);
 std::vector<int16_t>a{32767,-32768,1000,-1000},b=a;require(mix(a,b,32768,32768)==std::vector<int16_t>({32767,-32768,2000,-2000}));require(mix(a,b,0,0)==std::vector<int16_t>(4));reject([&]{mix(a,b,32769,0);});reject([&]{tone_fixture(0);});
 require(normalize_stick(100,100,7849).x==0);require(normalize_stick(32767,0,7849).x==1);require(normalize_stick(-32768,0,7849).x==-1);require(normalize_trigger(30)==0&&normalize_trigger(255)==1);reject([]{normalize_trigger(0,255);});reject([]{normalize_stick(0,0,32767);});
 std::mt19937 rng(41);std::uniform_int_distribution<int>d(-32768,32767);for(int i=0;i<10000;++i){int16_t x=static_cast<int16_t>(d(rng)),y=static_cast<int16_t>(d(rng));auto s=normalize_stick(x,y,7849);require(std::hypot(s.x,s.y)<=1.000001f);if(std::hypot(double(x),double(y))<=7849)require(s.x==0&&s.y==0);}
 auto dir=fs::temp_directory_path()/("revenge-media-test-"+std::to_string(std::random_device{}())+"-"+std::to_string(std::random_device{}()));require(fs::create_directory(dir));struct Cleanup{fs::path p;~Cleanup(){std::error_code ec;fs::remove_all(p,ec);}}cleanup{dir};auto path=dir/"save.bin";std::ostringstream out;require(storage_fixture(out,path,true)==0);require(storage_fixture(out,path,false)==0);
 Bytes data(1u<<20);for(auto& v:data)v=static_cast<uint8_t>(rng());save(path,data);require(load(path)==data);save(path,{});require(load(path).empty());reject([&]{save(path,Bytes((1u<<20)+1));});require(load(path).empty());save(path,Bytes{1,2,3});
 {std::fstream f(path,std::ios::binary|std::ios::in|std::ios::out);f.seekp(16);f.put(9);require(bool(f));}reject([&]{load(path);});reject([&]{save(path,Bytes{9});});require(fs::file_size(path)==19);
 auto foreign=dir/"foreign.bin";{std::ofstream f(foreign);f<<"foreign";}reject([&]{save(foreign,Bytes{1});});require(fs::file_size(foreign)==7);
 revenge::app::Runtime runtime;runtime.dispatch(revenge::app::Start{});runtime.dispatch(revenge::app::Store{"key",{1}});reject([&]{runtime.dispatch(revenge::app::Persist{"key"});});runtime.dispatch(revenge::app::Stop{});
 std::cout<<"96000 PCM samples, saturation, 10000 stick cases, 16 KiB persistent fixture, 1 MiB snapshot, corruption and overwrite rejection verified\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
