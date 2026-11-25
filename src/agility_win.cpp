// App-local opt-in per Microsoft Agility SDK public documentation.
#include <windows.h>
extern "C" {
__declspec(dllexport) extern const UINT D3D12SDKVersion = REVENGE_AGILITY_VERSION;
__declspec(dllexport) const char* D3D12SDKPath = ".\\D3D12\\";
}
