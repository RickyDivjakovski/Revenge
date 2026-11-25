#include <iostream>
#include <string_view>
int main(int argc,char** argv){
    if(argc!=2)return 2;
    std::string_view mode=argv[1];
    if(mode=="--lifecycle"){std::cout<<"verified marker\n";std::cerr<<"closed cache\n";return 0;}
    if(mode=="--silent")return 0;
    if(mode=="--stderr"){std::cerr<<"verified marker\n";return 0;}
    if(mode=="--ok"||mode=="--failure"){std::cout<<"verified marker\n";return mode=="--ok"?0:7;}
    return 2;
}
