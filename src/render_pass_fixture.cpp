#include "revenge/render_pass_fixture.hpp"
#include <stdexcept>
#include <cmath>
namespace revenge::passes {
void validate(unsigned w,unsigned h,std::span<const Draw> commands){
    if(!w||!h||w>128||h>128||commands.empty()||commands.size()>64)throw std::out_of_range("render pass limits");
    for(const auto& d:commands){
        const auto& r=d.scissor;
        if(r.left>=r.right||r.top>=r.bottom||r.right>w||r.bottom>h||d.mode>2||!std::isfinite(d.depth)||d.depth<0||d.depth>1||std::floor(d.depth*256)!=d.depth*256)throw std::invalid_argument("invalid render pass draw");
    }
}
Images reference(unsigned w,unsigned h,std::span<const Draw> commands){
    validate(w,h,commands);
    Images images; for(auto& image:images) image.resize(w*h);
    std::vector<float> depth(w*h,1.0f);
    std::vector<unsigned> stencil(w*h);
    for(const auto& draw:commands){
        for(unsigned y=draw.scissor.top;y<draw.scissor.bottom;++y)
        for(unsigned x=draw.scissor.left;x<draw.scissor.right;++x){
            const auto i=y*w+x;
            if(draw.mode!=0 && stencil[i]!=1) continue;
            if(draw.depth>=depth[i]) continue;
            depth[i]=draw.depth;
            if(draw.mode==0){stencil[i]=1;images[0][i]={255,0,0,255};images[1][i]={0,255,0,255};}
            else if(draw.mode==1){images[0][i]={255,255,0,255};images[1][i]={255,0,0,255};}
            else {auto& p=images[0][i];p={std::uint8_t((unsigned(p[0])+1)/2),std::uint8_t((unsigned(p[1])+1)/2),std::uint8_t((unsigned(p[2])+256)/2),255};}
        }
    }
    for(unsigned i=0;i<w*h;++i) images[2][i]={images[0][i][0],images[1][i][1],images[0][i][2],255};
    return images;
}
Images reference(){return reference(width,height,draws);}
}
