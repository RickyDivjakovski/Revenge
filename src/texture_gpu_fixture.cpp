#include "revenge/texture_gpu_fixture.hpp"
#include <sstream>
#include <locale>
#include <iomanip>
namespace revenge::textures {
SamplingFixture sampling_fixture(){
 SamplingFixture f;
 f.samplers={{Address::clamp,Address::clamp,Filter::point,Filter::point},{Address::wrap,Address::mirror,Filter::point,Filter::point},{Address::border,Address::clamp,Filter::linear,Filter::point},{Address::mirror,Address::wrap,Filter::linear,Filter::linear}};
 for(auto format:{Format::rgba8,Format::bc1,Format::bc3,Format::bc4,Format::bc5}){
  Descriptor d{8,8,2,2,format,Layout::linear};auto p=plan(d);std::vector<uint8_t> bytes(p.source_bytes);
  for(auto s:p.subresources)for(unsigned y=0;y<s.blocks_y;++y)for(unsigned x=0;x<s.blocks_x;++x){
   auto b=bytes.data()+s.source_offset+y*s.row_bytes+x*s.block_bytes;
   unsigned k=(x+y+s.layer+2*s.mip)%3;
   uint16_t rgb=k==0?0xf800:k==1?0x07e0:0x001f;
   auto colour=[&](unsigned at){b[at]=uint8_t(rgb);b[at+1]=uint8_t(rgb>>8);b[at+2]=b[at];b[at+3]=b[at+1];};
   auto channel=[&](unsigned at,unsigned value){b[at]=b[at+1]=uint8_t(value);};
   switch(format){case Format::rgba8:b[k]=255;b[3]=255;break;case Format::bc1:colour(0);break;case Format::bc3:channel(0,64+64*k);colour(8);break;case Format::bc4:channel(0,64+64*k);break;case Format::bc5:channel(0,64+64*k);channel(8,192-64*k);break;default:break;}
  }
  f.images.push_back({d,convert(d,bytes)});
 }
 // Exact binary-fraction coordinates avoid ambiguous point-filter boundaries.
 const std::array<std::array<float,3>,8> coords{{{.0625f,.0625f,0},{.6875f,.1875f,0},{.5f,.5f,0},{.125f,.375f,1},{-.1875f,1.1875f,0},{0,.1875f,0},{.375f,.625f,.25f},{.625f,.375f,.75f}}};
 std::ostringstream h;h.imbue(std::locale::classic());h<<std::fixed<<std::setprecision(6);
 for(unsigned i=0;i<f.images.size();++i)h<<"Texture2DArray<float4> t"<<i<<" : register(t"<<i<<");\n";
 for(unsigned i=0;i<f.samplers.size();++i)h<<"SamplerState s"<<i<<" : register(s"<<i<<");\n";
 h<<"RWStructuredBuffer<float4> result : register(u0);\n[numthreads(1,1,1)] void main() {\n";
 for(unsigned t=0;t<f.images.size();++t)for(unsigned s=0;s<f.samplers.size();++s)for(unsigned l=0;l<2;++l)for(auto c:coords){
  auto& image=f.images[t];
  // For single/dual-channel textures avoid border colour swizzle ambiguity on missing channels.
  if(s==2&&(image.descriptor.format==Format::bc4||image.descriptor.format==Format::bc5)&&c[0]<.125f)c[0]=.1875f;
  h<<"result["<<f.queries.size()<<"] = t"<<t<<".SampleLevel(s"<<s<<",float3("<<c[0]<<","<<c[1]<<","<<l<<"),"<<c[2]<<");\n";
  f.queries.push_back({t,s,l,c[0],c[1],c[2]});f.expected.push_back(sample_lod(image.descriptor,image.upload,l,c[0],c[1],c[2],f.samplers[s]));
 }
 h<<"}\n";f.hlsl=h.str();return f;
}
}
