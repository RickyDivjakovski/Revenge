#define WIN32_LEAN_AND_MEAN
#include <windows.h>
// Independent ordinary Windows client; inspected as data by the regression suite.
int main(){return GetSystemMetrics(SM_CXSCREEN)>0?0:1;}
