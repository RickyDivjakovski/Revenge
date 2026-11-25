#include "revenge/render_pass_fixture.hpp"
namespace revenge::passes {
Images reference(){
    Images images; for(auto& image:images) image.resize(width*height);
    std::array<float,width*height> depth; depth.fill(1.0f);
    std::array<unsigned,width*height> stencil{};
    for(const auto& draw:draws){
        for(unsigned y=draw.scissor.top;y<draw.scissor.bottom;++y)
        for(unsigned x=draw.scissor.left;x<draw.scissor.right;++x){
            const auto i=y*width+x;
            if(draw.mode!=0 && stencil[i]!=1) continue;
            if(draw.depth>=depth[i]) continue;
            depth[i]=draw.depth;
            if(draw.mode==0){stencil[i]=1;images[0][i]={255,0,0,255};images[1][i]={0,255,0,255};}
            else if(draw.mode==1){images[0][i]={255,255,0,255};images[1][i]={255,0,0,255};}
            else {auto& p=images[0][i];p={std::uint8_t((unsigned(p[0])+1)/2),std::uint8_t((unsigned(p[1])+1)/2),std::uint8_t((unsigned(p[2])+256)/2),255};}
        }
    }
    for(unsigned i=0;i<width*height;++i) images[2][i]={images[0][i][0],images[1][i][1],images[0][i][2],255};
    return images;
}
}
