#include "revenge/multimedia.hpp"
#include "revenge/application.hpp"
#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <ostream>
#include <random>
#include <stdexcept>
#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#endif
namespace revenge::media {
Stick normalize_stick(int16_t x,int16_t y,unsigned d){
 if(d>=32767)throw std::invalid_argument("invalid stick deadzone");
 const double fx=x,fy=y,m=std::hypot(fx,fy);if(m<=d)return {};
 const double s=(std::min(m,32767.0)-d)/(32767.0-d)/m;
 return {static_cast<float>(fx*s),static_cast<float>(fy*s)};
}
float normalize_trigger(uint8_t v,unsigned d){if(d>=255)throw std::invalid_argument("invalid trigger threshold");return v<=d?0.f:float(v-d)/float(255-d);}
std::vector<int16_t> mix(std::span<const int16_t>a,std::span<const int16_t>b,unsigned ga,unsigned gb){
 if(a.size()!=b.size()||a.size()%2||a.size()>48000u*2*10||ga>32768||gb>32768)throw std::invalid_argument("invalid stereo PCM mix");
 std::vector<int16_t> result(a.size());for(size_t i=0;i<a.size();++i){auto n=(int64_t(a[i])*ga+int64_t(b[i])*gb)/32768;result[i]=static_cast<int16_t>(std::clamp<int64_t>(n,-32768,32767));}return result;
}
std::vector<int16_t> tone_fixture(unsigned ms){
 if(ms<20||ms>10000)throw std::invalid_argument("invalid tone duration");
 const unsigned frames=ms*48;
 std::vector<int16_t>a(frames*2),b(frames*2);for(unsigned i=0;i<frames;++i){
  const double fade=std::min({1.0,double(i)/480.0,double(frames-1-i)/480.0});
  auto tone=[&](double hz){return static_cast<int16_t>(std::lround(4000.0*fade*std::sin(6.283185307179586*hz*i/48000.0)));};
  a[i*2]=a[i*2+1]=tone(440);b[i*2]=b[i*2+1]=tone(660);
 }return mix(a,b,16384,8192);
}
namespace {
constexpr size_t limit=1u<<20;constexpr std::array<uint8_t,8> magic={'R','V','S','A','V','E',1,0};
uint32_t crc(std::span<const uint8_t>b){uint32_t c=0xffffffffu;for(auto v:b){c^=v;for(int k=0;k<8;++k)c=(c>>1)^((0u-(c&1u))&0xedb88320u);}return ~c;}
void put(Bytes& b,uint32_t v){for(int i=0;i<4;++i)b.push_back(static_cast<uint8_t>(v>>(i*8)));}
uint32_t get(const Bytes& b,size_t p){uint32_t v=0;for(int i=0;i<4;++i)v|=uint32_t(b.at(p+i))<<(i*8);return v;}
namespace fs=std::filesystem;
void regular(const fs::path& p){if(!fs::is_regular_file(fs::symlink_status(p)))throw std::runtime_error("snapshot must be a regular non-symlink file");}
}
Bytes load(const std::filesystem::path& p){
 regular(p);auto n=fs::file_size(p);if(n<16||n>limit+16)throw std::runtime_error("invalid snapshot size");
 Bytes b(static_cast<size_t>(n));std::ifstream in(p,std::ios::binary);if(!in.read(reinterpret_cast<char*>(b.data()),static_cast<std::streamsize>(b.size()))||in.peek()!=std::char_traits<char>::eof())throw std::runtime_error("snapshot read failed");
 if(!std::equal(magic.begin(),magic.end(),b.begin())||get(b,8)!=b.size()-16)throw std::runtime_error("invalid snapshot header");
 Bytes data(b.begin()+16,b.end());if(crc(data)!=get(b,12))throw std::runtime_error("snapshot checksum mismatch");return data;
}
void save(const std::filesystem::path& p,std::span<const uint8_t> data){
 if(data.size()>limit||p.filename().empty())throw std::invalid_argument("invalid snapshot path or payload size");
 // Do not replace unrelated files, even when the caller chooses their name.
 if(fs::symlink_status(p).type()!=fs::file_type::not_found) (void)load(p);
 auto parent=p.parent_path();if(parent.empty())parent=".";if(!fs::is_directory(parent))throw std::runtime_error("snapshot parent must exist");
 Bytes b(magic.begin(),magic.end());put(b,static_cast<uint32_t>(data.size()));put(b,crc(data));b.insert(b.end(),data.begin(),data.end());
 // Private temporary directory in the same filesystem; atomic namespace replacement on local filesystems.
 std::random_device rng;fs::path temp;bool created=false;for(int i=0;i<16&&!created;++i){temp=parent/(".revenge-save-"+std::to_string(rng())+"-"+std::to_string(rng()));created=fs::create_directory(temp);}if(!created)throw std::runtime_error("cannot create snapshot staging directory");
 struct Cleanup{fs::path p;~Cleanup(){std::error_code ec;fs::remove_all(p,ec);}} cleanup{temp};auto file=temp/"payload";
 {std::ofstream out(file,std::ios::binary|std::ios::trunc);out.write(reinterpret_cast<const char*>(b.data()),static_cast<std::streamsize>(b.size()));out.close();if(!out)throw std::runtime_error("snapshot write failed");}
#ifdef _WIN32
 if(!MoveFileExW(file.c_str(),p.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("snapshot replacement failed: "+std::to_string(GetLastError()));
#else
 fs::rename(file,p);
#endif
}
int storage_fixture(std::ostream& out,const std::filesystem::path& p,bool write){
 Bytes data(16384);for(size_t i=0;i<data.size();++i)data[i]=static_cast<uint8_t>((i*37u+(i>>8))&255);
 if(write){app::Runtime runtime({},p);runtime.dispatch(app::Start{});runtime.dispatch(app::Store{"save",data});runtime.dispatch(app::Persist{"save"});runtime.dispatch(app::Stop{});}
 app::Runtime runtime({},p);runtime.dispatch(app::Start{});runtime.dispatch(app::Restore{"save"});if(std::get<Bytes>(runtime.dispatch(app::Load{"save"}))!=data)throw std::runtime_error("persistent fixture mismatch");runtime.dispatch(app::Stop{});if(runtime.stats().files)throw std::runtime_error("storage cleanup failed");
 out<<"Persistent snapshot "<<(write?"written and reopened":"reopened without writing")<<"; verified "<<data.size()<<" bytes. CRC32, version and length checked.\n";return 0;
}
#ifndef _WIN32
int audio_test(std::ostream& out){out<<"XAudio2 playback requires Windows.\n";return 1;}
int input_test(std::ostream& out,unsigned,bool){out<<"XInput polling requires Windows.\n";return 1;}
#endif
}
