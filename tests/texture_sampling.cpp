#include "revenge/texture.hpp"
#include <array>
#include <cmath>
#include <iostream>
#include <limits>
#include <stdexcept>
using namespace revenge::textures;
using C=std::array<float,4>;
static unsigned checks=0;
void check(C a,C b){for(unsigned i=0;i<4;++i)if(!std::isfinite(a[i])||std::abs(a[i]-b[i])>1e-6f)throw std::runtime_error("sample mismatch at check "+std::to_string(checks));++checks;}
void alpha(std::vector<uint8_t>& b,size_t off,uint8_t a,uint8_t c){b[off]=a;b[off+1]=c;uint64_t bits=0;for(unsigned t=0;t<16;++t)bits|=uint64_t(t%8)<<(3*t);for(unsigned i=0;i<6;++i)b[off+2+i]=uint8_t(bits>>(8*i));}
void colors(std::vector<uint8_t>& b,size_t off,uint16_t a,uint16_t c){b[off]=uint8_t(a);b[off+1]=uint8_t(a>>8);b[off+2]=uint8_t(c);b[off+3]=uint8_t(c>>8);for(unsigned i=0;i<4;++i)b[off+4+i]=0xe4;}
int main(){try{
 for(auto f:{Format::bc1,Format::bc3,Format::bc4,Format::bc5}){Descriptor d{4,4,1,1,f};std::vector<uint8_t> b(plan(d).source_bytes);if(f==Format::bc1)colors(b,0,0xf800,0x001f);if(f==Format::bc3){alpha(b,0,0,255);colors(b,8,0x001f,0xf800);}if(f==Format::bc4||f==Format::bc5)alpha(b,0,255,0);if(f==Format::bc5)alpha(b,8,0,255);auto u=convert(d,b);constexpr float k=1.f/255;const std::array<unsigned,8> hi{255,0,218,182,145,109,72,36},lo{0,255,51,102,153,204,0,255};
 for(unsigned t=0;t<16;++t){float x=(float(t%4)+.5f)/4,y=(float(t/4)+.5f)/4;C want{};if(f==Format::bc1){std::array<C,4> palette{{{1,0,0,1},{0,0,1,1},{170*k,0,85*k,1},{85*k,0,170*k,1}}};want=palette[t%4];}else if(f==Format::bc3){std::array<C,4> palette{{{0,0,1,1},{1,0,0,1},{85*k,0,170*k,1},{170*k,0,85*k,1}}};want=palette[t%4];want[3]=lo[t%8]*k;}else want={hi[t%8]*k,f==Format::bc5?lo[t%8]*k:0.f,0,1};check(sample(d,u,0,0,x,y,Address::clamp,Filter::point),want);}
 if(f==Format::bc1){colors(b,0,0x001f,0xf800);u=convert(d,b);check(sample(d,u,0,0,.875f,.125f,Address::clamp,Filter::point),{0,0,0,0});check(sample(d,u,0,0,.625f,.125f,Address::clamp,Filter::point),{127*k,0,127*k,1});}
 }
 Descriptor d{5,3,1,1,Format::bc1};std::vector<uint8_t> b(plan(d).source_bytes);colors(b,0,0xf800,0xf800);colors(b,8,0x07e0,0x07e0);b[4]=b[5]=b[6]=b[7]=b[12]=b[13]=b[14]=b[15]=0;auto u=convert(d,b);check(sample(d,u,0,0,1,1,Address::clamp,Filter::point),{0,1,0,1});check(sample(d,u,0,0,.8f,.5f,Address::clamp,Filter::linear),{.5f,.5f,0,1});
 Descriptor m{2,2,2,2,Format::rgba8};std::vector<uint8_t> src(plan(m).source_bytes);auto p=plan(m);const std::array<std::array<uint8_t,4>,4> cols{{{255,0,0,255},{0,0,255,255},{0,255,0,255},{255,255,255,255}}};for(size_t j=0;j<4;++j)for(size_t n=0;n<p.subresources[j].source_size;++n)src[p.subresources[j].source_offset+n]=cols[j][n%4];u=convert(m,src);Sampler sm{Address::clamp,Address::clamp,Filter::linear,Filter::linear};check(sample_lod(m,u,0,.5f,.5f,.25f,sm),{.75f,0,.25f,1});check(sample_lod(m,u,1,.5f,.5f,.5f,sm),{.5f,1,.5f,1});check(sample_lod(m,u,1,0,0,-100,sm),{0,1,0,1});check(sample_lod(m,u,1,0,0,100,sm),{1,1,1,1});sm.mip=Filter::point;check(sample_lod(m,u,0,0,0,.49f,sm),{1,0,0,1});check(sample_lod(m,u,0,0,0,.5f,sm),{0,0,1,1});
 Descriptor a{2,2};std::array<uint8_t,16> px{255,0,0,255,0,255,0,255,0,0,255,255,255,255,255,255};u=convert(a,px);sm={Address::mirror,Address::wrap,Filter::point,Filter::point};check(sample_lod(a,u,0,-.25f,1.75f,0,sm),{0,0,1,1});check(sample_lod(a,u,0,1.25f,-.75f,0,sm),{0,1,0,1});sm={Address::border,Address::clamp,Filter::linear,Filter::point};check(sample_lod(a,u,0,0,.25f,0,sm),{.5f,0,0,.5f});check(sample_lod(a,u,0,-100,.25f,0,sm),{0,0,0,0});sm={Address::mirror,Address::mirror,Filter::linear,Filter::point};check(sample_lod(a,u,0,1,.25f,0,sm),{0,1,0,1});
 for(float bad:{std::numeric_limits<float>::infinity(),std::numeric_limits<float>::quiet_NaN()}){bool rejected=false;try{sample_lod(a,u,0,0,0,bad,sm);}catch(const std::invalid_argument&){rejected=true;}if(!rejected)throw std::runtime_error("bad LOD accepted");}
 std::cout<<"Texture sampling: "<<checks<<" known-answer samples; BC1/3/4/5, partial blocks, per-axis addressing and mip filters passed. CPU reference only.\n";return 0;
 }catch(const std::exception& e){std::cerr<<e.what()<<'\n';return 1;}}
