#include <windows.h>
// An intentional, self-authored exception for debugger integration tests.
int main(){RaiseException(0xE0420001,EXCEPTION_NONCONTINUABLE,0,nullptr);return 99;}
