#include "revenge/core.hpp"
#include "revenge/graphics.hpp"
#include <iostream>
namespace revenge {int gpu_info(){std::cerr<<"D3D12 backend requires Windows.\n";return 2;}int gpu_tune(unsigned,unsigned){return gpu_info();}int gpu_bench(unsigned,unsigned,unsigned){return gpu_info();}int gpu_compute(const std::filesystem::path&){return gpu_info();}}

namespace revenge {int gpu_render(unsigned,unsigned,const std::filesystem::path&,const std::filesystem::path&){return gpu_info();}}

#include "revenge/shader_ir.hpp"
namespace revenge::shader {int gpu_shader_fixture(){return revenge::gpu_info();}}

#include "revenge/shader_flow.hpp"
namespace revenge::flow {int gpu_test(){return revenge::gpu_info();}}

#include "revenge/shader_memory.hpp"
namespace revenge::memory_shader {int gpu_test(){return revenge::gpu_info();}}

namespace revenge::flow {int unified_gpu_test(const std::filesystem::path&){return revenge::gpu_info();}}

#include "revenge/texture.hpp"
namespace revenge::textures {int gpu_test(const std::filesystem::path&){return revenge::gpu_info();}}

#include "revenge/transfers.hpp"
namespace revenge::transfer {int gpu_test(unsigned,unsigned,unsigned){return revenge::gpu_info();}}

#include "revenge/presentation.hpp"
namespace revenge::presentation {int run(unsigned,unsigned,unsigned,bool,bool,unsigned){return revenge::gpu_info();}}

#include "revenge/scene.hpp"
namespace revenge::scene3d {int gpu_test(unsigned,unsigned,const std::filesystem::path&,bool,bool,const package::Programs*,const std::filesystem::path&,unsigned,size_t*,Continuous*,CustomScene*,CustomDevice*,unsigned){return revenge::gpu_info();}}
namespace revenge::flow {int loop_gpu_test(){return revenge::gpu_info();}int wave_gpu_test(unsigned){return revenge::gpu_info();}}

namespace revenge::textures {int sampling_gpu_test(){return revenge::gpu_info();}}

namespace revenge::passes {int gpu_test(){return revenge::gpu_info();}}
