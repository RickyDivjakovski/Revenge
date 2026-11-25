#include "revenge/texture.hpp"
#include <algorithm>
#include <cmath>
#include <cstring>
#include <stdexcept>
#include <limits>
namespace revenge::textures {
namespace {
constexpr size_t cap=256u*1024u*1024u;
size_t checked(size_t a,size_t b){if(b>cap||a>cap-b)throw std::runtime_error("texture allocation exceeds fixture limit");return a+b;}
size_t mul(size_t a,size_t b){if(a&&b>cap/a)throw std::runtime_error("texture allocation exceeds fixture limit");return a*b;}
size_t align(size_t n,size_t a){return checked(n,(a-n%a)%a);}
struct FormatInfo {uint32_t block,bytes;};
FormatInfo info(Format f){switch(f){case Format::r8:return {1,1};case Format::rgba8:case Format::bgra8:return {1,4};case Format::bc1:case Format::bc4:return {4,8};case Format::bc3:case Format::bc5:return {4,16};}throw std::invalid_argument("unknown texture format");}
size_t tiled_index(uint32_t x,uint32_t y,uint32_t bx){return (size_t(y/8)*((bx+7)/8)+x/8)*64+morton8(x%8,y%8);}
}
uint32_t morton8(uint32_t x,uint32_t y){if(x>=8||y>=8)throw std::invalid_argument("tile coordinate out of range");uint32_t r=0;for(unsigned i=0;i<3;++i){r|=((x>>i)&1u)<<(2*i);r|=((y>>i)&1u)<<(2*i+1);}return r;}
Plan plan(Descriptor d){
 if(!d.width||!d.height||d.width>16384||d.height>16384||!d.layers||d.layers>2048||!d.mips)throw std::invalid_argument("invalid texture dimensions");
 uint32_t max_mips=1;for(auto n=std::max(d.width,d.height);n>1;n>>=1)++max_mips;
 if(d.mips>max_mips||uint64_t(d.mips)*d.layers>4096)throw std::invalid_argument("invalid subresource count");
 if(d.layout!=Layout::linear&&d.layout!=Layout::morton8)throw std::invalid_argument("unknown layout");
 auto f=info(d.format);Plan result;
 for(uint32_t l=0;l<d.layers;++l)for(uint32_t m=0;m<d.mips;++m){
  uint32_t w=std::max(1u,d.width>>m),h=std::max(1u,d.height>>m),bx=(w+f.block-1)/f.block,by=(h+f.block-1)/f.block;
  size_t row=mul(bx,f.bytes),pitch=align(row,256),off=align(result.upload_bytes,512);
  size_t size=d.layout==Layout::linear?mul(row,by):mul(mul(mul((bx+7)/8,(by+7)/8),64),f.bytes);
  result.subresources.push_back({l,m,w,h,bx,by,f.bytes,result.source_bytes,size,off,pitch,row});
  result.source_bytes=checked(result.source_bytes,size);result.upload_bytes=checked(off,mul(pitch,by));
 }
 return result;
}
std::vector<uint8_t> convert(Descriptor d,std::span<const uint8_t> source){auto p=plan(d);if(source.size()!=p.source_bytes)throw std::invalid_argument("source size does not match descriptor");std::vector<uint8_t> out(p.upload_bytes,0);
 for(auto s:p.subresources)for(uint32_t y=0;y<s.blocks_y;++y){auto dst=out.data()+s.upload_offset+y*s.row_pitch;auto src=source.data()+s.source_offset;if(d.layout==Layout::linear)std::memcpy(dst,src+y*s.row_bytes,s.row_bytes);else for(uint32_t x=0;x<s.blocks_x;++x)std::memcpy(dst+x*s.block_bytes,src+tiled_index(x,y,s.blocks_x)*s.block_bytes,s.block_bytes);}
 return out;
}
namespace {
using Color=std::array<float,4>;
constexpr float k=1.f/255.f;
uint32_t le(const uint8_t* b,unsigned n){uint32_t x=0;for(unsigned i=0;i<n;++i)x|=uint32_t(b[i])<<(8*i);return x;}
std::array<unsigned,3> rgb565(unsigned x){unsigned r=(x>>11)&31,g=(x>>5)&63,b=x&31;return {(r<<3)|(r>>2),(g<<2)|(g>>4),(b<<3)|(b>>2)};}
Color bc_color(const uint8_t* b,unsigned texel,bool force_four){auto e0=le(b,2),e1=le(b+2,2);auto a=rgb565(e0),c=rgb565(e1);auto sel=(le(b+4,4)>>(2*texel))&3u;Color out{0,0,0,1};
 if(sel==3&&!force_four&&e0<=e1)return {0,0,0,0};
 for(unsigned i=0;i<3;++i){unsigned v=0;if(sel==0)v=a[i];else if(sel==1)v=c[i];else if(force_four||e0>e1)v=sel==2?(2*a[i]+c[i])/3:(a[i]+2*c[i])/3;else v=(a[i]+c[i])/2;out[i]=float(v)*k;}return out;}
float bc_channel(const uint8_t* b,unsigned texel){uint64_t bits=0;for(unsigned i=0;i<6;++i)bits|=uint64_t(b[2+i])<<(8*i);unsigned sel=unsigned((bits>>(3*texel))&7),a=b[0],c=b[1],v=0;
 if(sel==0)v=a;else if(sel==1)v=c;else if(a>c)v=((8-sel)*a+(sel-1)*c)/7;else if(sel<6)v=((6-sel)*a+(sel-1)*c)/5;else v=sel==6?0:255;return float(v)*k;}
Color texel(Descriptor d,std::span<const uint8_t> upload,const Footprint& s,unsigned x,unsigned y){bool block=d.format==Format::bc1||d.format==Format::bc3||d.format==Format::bc4||d.format==Format::bc5;unsigned px=block?x/4:x,py=block?y/4:y;const auto* b=upload.data()+s.upload_offset+size_t(py)*s.row_pitch+size_t(px)*s.block_bytes;unsigned t=(y%4)*4+x%4;
 switch(d.format){case Format::r8:return {b[0]*k,0,0,1};case Format::rgba8:return {b[0]*k,b[1]*k,b[2]*k,b[3]*k};case Format::bgra8:return {b[2]*k,b[1]*k,b[0]*k,b[3]*k};case Format::bc1:return bc_color(b,t,false);case Format::bc3:{auto out=bc_color(b+8,t,true);out[3]=bc_channel(b,t);return out;}case Format::bc4:return {bc_channel(b,t),0,0,1};case Format::bc5:return {bc_channel(b,t),bc_channel(b+8,t),0,1};}throw std::invalid_argument("unknown format");}
bool valid(Address a){return a==Address::clamp||a==Address::wrap||a==Address::mirror||a==Address::border;}
bool valid(Filter f){return f==Filter::point||f==Filter::linear;}
double norm(float x,Address a){double t=x;if(a==Address::wrap)return t-std::floor(t);if(a==Address::mirror){t=std::fmod(t,2.);if(t<0)t+=2.;return t<=1?t:2-t;}if(a==Address::border)return std::clamp(t,-1.,2.);return std::clamp(t,0.,1.);}
int index(int x,int n,Address a){if(a==Address::wrap)return (x%n+n)%n;if(a==Address::mirror){int t=(x%(2*n)+2*n)%(2*n);return t<n?t:2*n-1-t;}if(a==Address::border&&(x<0||x>=n))return -1;return std::clamp(x,0,n-1);}
Color blend(Color a,const Color& b,float t){for(unsigned i=0;i<4;++i)a[i]=a[i]*(1-t)+b[i]*t;return a;}
Color sample_level(Descriptor d,std::span<const uint8_t> upload,const Footprint& s,float u,float v,Sampler sm){
 auto fetch=[&](int x,int y){x=index(x,int(s.width),sm.u);y=index(y,int(s.height),sm.v);if(x<0||y<0)return Color{};return texel(d,upload,s,unsigned(x),unsigned(y));};
 double x=norm(u,sm.u)*s.width,y=norm(v,sm.v)*s.height;if(sm.spatial==Filter::point)return fetch(int(std::floor(x)),int(std::floor(y)));x-=.5;y-=.5;int x0=int(std::floor(x)),y0=int(std::floor(y));float fx=float(x-x0),fy=float(y-y0);return blend(blend(fetch(x0,y0),fetch(x0+1,y0),fx),blend(fetch(x0,y0+1),fetch(x0+1,y0+1),fx),fy);
}
}
std::array<float,4> sample(Descriptor d,std::span<const uint8_t> upload,uint32_t mip,uint32_t layer,float u,float v,Address address,Filter filter){if(mip>=d.mips)throw std::invalid_argument("invalid texture mip");return sample_lod(d,upload,layer,u,v,float(mip),{address,address,filter,Filter::point});}
std::array<float,4> sample_lod(Descriptor d,std::span<const uint8_t> upload,uint32_t layer,float u,float v,float lod,Sampler sm){auto p=plan(d);if(upload.size()!=p.upload_bytes||layer>=d.layers||!std::isfinite(u)||!std::isfinite(v)||!std::isfinite(lod)||!valid(sm.u)||!valid(sm.v)||!valid(sm.spatial)||!valid(sm.mip))throw std::invalid_argument("invalid texture sample");float level=std::clamp(lod,0.f,float(d.mips-1));auto at=[&](uint32_t m){return sample_level(d,upload,p.subresources[layer*d.mips+m],u,v,sm);};if(sm.mip==Filter::point)return at(uint32_t(std::floor(level+.5f)));auto lo=uint32_t(std::floor(level));return blend(at(lo),at(std::min(lo+1,d.mips-1)),level-float(lo));}
std::vector<uint32_t> fixture_expected(){std::vector<uint32_t> out(4096);for(uint32_t i=0;i<4096;++i)out[i]=(i*2654435761u)^0xa5a5a5a5u;return out;}
std::vector<uint32_t> fixture_input(){auto values=fixture_expected();std::vector<uint32_t> out(4096);for(uint32_t y=0;y<64;++y)for(uint32_t x=0;x<64;++x)out[tiled_index(x,y,64)]=values[y*64+x];return out;}
std::string fixture_hlsl(){return R"(StructuredBuffer<uint> source : register(t0);
RWStructuredBuffer<uint> output : register(u0);
[numthreads(64,1,1)] void main(uint3 id : SV_DispatchThreadID) {
 uint i=id.x; if(i>=4096) return;
 uint x=i%64,y=i/64,tx=x&7,ty=y&7;
 uint m=(tx&1)|((ty&1)<<1)|((tx&2)<<1)|((ty&2)<<2)|((tx&4)<<2)|((ty&4)<<3);
 uint tile=(y/8)*8+x/8;
 output[i]=source[tile*64+m];
}
)";}
}
