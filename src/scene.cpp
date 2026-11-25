#include "revenge/scene.hpp"
#include <cmath>
#include <algorithm>
#include <stdexcept>
namespace revenge::scene3d {
Geometry geometry(){
 // Perspective projection uses clip=(x,y, (4*z-2)/3.5, z).
 // Front z=1, rear z=2. Front is intentionally submitted first.
 return {{{-.5f,.5f,1,0,0},{.5f,.5f,1,1,0},{-.5f,-.5f,1,0,1},{.5f,-.5f,1,1,1},
          {-1.5f,1.5f,2,.75f,.75f},{1.5f,1.5f,2,.75f,.75f},{-1.5f,-1.5f,2,.75f,.75f},{1.5f,-1.5f,2,.75f,.75f}},
         {0,1,2,2,1,3,4,5,6,6,5,7}};
}
std::vector<Command> commands(){return {{Op::begin},{Op::bind_mesh},{Op::bind_texture},{Op::draw_indexed,0,6},{Op::draw_indexed,6,6},{Op::end}};}
void validate(const Geometry& g,std::span<const Command> commands){
 if(g.vertices.empty()||g.vertices.size()>65536||g.indices.empty()||g.indices.size()>1000000||commands.size()>10000)throw std::invalid_argument("invalid scene geometry or command count");
 for(auto v:g.vertices)if(!std::isfinite(v.x)||!std::isfinite(v.y)||!std::isfinite(v.z)||!std::isfinite(v.u)||!std::isfinite(v.v)||v.z<=0)throw std::invalid_argument("invalid scene vertex");
 for(auto i:g.indices)if(i>=g.vertices.size())throw std::invalid_argument("scene index outside vertex buffer");
 bool active=false,mesh=false,texture=false,ended=false;unsigned draws=0;
 for(auto c:commands){if(ended)throw std::invalid_argument("command after end");if(c.op!=Op::draw_indexed&&(c.first||c.count))throw std::invalid_argument("unexpected command operands");
 switch(c.op){case Op::begin:if(active)throw std::invalid_argument("nested scene");active=true;break;
 case Op::bind_mesh:if(!active)throw std::invalid_argument("bind outside scene");mesh=true;break;
 case Op::bind_texture:if(!active)throw std::invalid_argument("bind outside scene");texture=true;break;
 case Op::draw_indexed:if(!active||!mesh||!texture||!c.count||c.count%3||c.first>g.indices.size()||c.count>g.indices.size()-c.first)throw std::invalid_argument("invalid indexed draw");++draws;break;
 case Op::end:if(!active||!draws)throw std::invalid_argument("empty or missing scene");active=false;ended=true;break;
 default:throw std::invalid_argument("unknown scene command");}}
 if(!ended)throw std::invalid_argument("unterminated scene");
}
bool expected(unsigned x,unsigned y,unsigned w,unsigned h,Pixel& result){
 if(!w||!h||x>=w||y>=h)throw std::invalid_argument("invalid scene pixel");
 double px=(2.0*x+1)/w-1,py=1-(2.0*y+1)/h;
 double ax=std::abs(px),ay=std::abs(py),dx=2.0/w,dy=2.0/h;
 if(std::abs(ax-.75)<=dx||std::abs(ay-.75)<=dy||std::abs(ax-.5)<=dx||std::abs(ay-.5)<=dy)return false;
 if(ax<.5&&ay<.5){if(ax<=dx||ay<=dy)return false;result=palette[(px>0?1:0)+(py<0?2:0)];}
 else if(ax<.75&&ay<.75)result=palette[3];
 else result={0,0,0,255};
 return true;
}
int motion_offset(unsigned width,uint64_t frame){
 if(width<320||width>3840)throw std::invalid_argument("invalid motion extent");
 const auto phase=frame%120;const int step=phase<=60?int(phase):120-int(phase);
 return int(width/8)*(step-30)/30;
}
size_t verify(std::span<const uint8_t> image,unsigned w,unsigned h,size_t pitch,bool swapped,int shift_x){
 if(w<16||h<16||w>3840||h>2160||pitch<size_t(w)*4||pitch>image.size()/h||shift_x < -int(w/8)||shift_x > int(w/8))throw std::invalid_argument("invalid scene image");
 size_t checked=0;for(unsigned y=0;y<h;++y)for(unsigned x=0;x<w;++x){Pixel p;const int source_x=int(x)-shift_x;if(source_x<0||source_x>=int(w))p={0,0,0,255};else if(!expected(unsigned(source_x),y,w,h,p))continue;if(swapped)std::swap(p[0],p[2]);auto q=image.data()+size_t(y)*pitch+size_t(x)*4;if(!std::equal(p.begin(),p.end(),q))throw std::runtime_error("scene pixel mismatch at "+std::to_string(x)+","+std::to_string(y));++checked;}return checked;
}
}
