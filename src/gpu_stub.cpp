#include "revenge/core.hpp"
#include <iostream>
namespace revenge {int gpu_info(){std::cerr<<"D3D12 backend requires Windows.\n";return 2;}int gpu_bench(unsigned,unsigned,unsigned){return gpu_info();}int gpu_compute(const std::filesystem::path&){return gpu_info();}}
