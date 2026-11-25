// Source-only opt-in sample for an authorised Xbox GDKX title project.
// Compile in the official project, with its normal platform libraries and lifecycle.
// This is intentionally NOT linked to Revenge's Windows ABI or built by CMake.
#include "probe_trace.hpp"
#include <windows.h>
#include <XSystem.h>
// Call after the template's real XGameRuntimeInitialize succeeds. Do not reinitialise it.
// Return value is for the caller to handle; this does not replace the template entry point.
extern "C" int RunRevengePlatformProbe() noexcept {
 try {
  const auto device=XSystemGetDeviceType();
  char metadata[100]{};
  std::snprintf(metadata,sizeof(metadata),"REVENGE-GDK-PROBE device-type=%u\n",static_cast<unsigned>(device));
  OutputDebugStringA(metadata);
  const auto trace=revenge_arena::scripted_trace();
  if(trace!=revenge_arena::expected_trace)return 2;
  char line[160]{};auto n=revenge_probe::format_trace(line,sizeof(line)-2,trace);
  if(n<=0||size_t(n)>=sizeof(line)-2)return 3;
  line[n]='\n';line[n+1]='\0';OutputDebugStringA(line);return 0;
 }catch(...){OutputDebugStringA("REVENGE-GDK-PROBE failed\n");return 1;}
}
