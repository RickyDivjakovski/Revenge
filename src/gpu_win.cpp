#include "revenge/shader_package.hpp"
#include "revenge/shader_stage.hpp"
#include "revenge/graphics.hpp"
#include "revenge/scene.hpp"
#include "revenge/resource_registry.hpp"
#include <thread>
#include <mutex>
#include <memory>
#include "revenge/texture.hpp"
#include "revenge/texture_gpu_fixture.hpp"
#include <cmath>
#include "revenge/pipeline_cache.hpp"
#include "revenge/shader_ir.hpp"
#include "revenge/shader_flow.hpp"
#include "revenge/shader_memory.hpp"
#include "revenge/core.hpp"
#include "revenge/system.hpp"
#include <algorithm>
#include <climits>
#include <array>
#include <windows.h>
#include <Xinput.h>
#include "revenge/multimedia.hpp"
#include <d3d12.h>
#include <d3dcompiler.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <chrono>
#include <iostream>
#include <stdexcept>
#include <cstring>
#include <vector>
#include <sstream>
#include <string_view>
#include <limits>
#include <fstream>
using Microsoft::WRL::ComPtr;
namespace revenge {namespace {
void check(HRESULT hr,const char* label){if(FAILED(hr)){std::ostringstream s;s<<label<<" failed (0x"<<std::hex<<static_cast<unsigned>(hr)<<")";throw std::runtime_error(s.str());}}
struct Handle {HANDLE h=CreateEventW(nullptr,FALSE,FALSE,nullptr);Handle(){if(!h)throw std::runtime_error("CreateEvent failed");}~Handle(){CloseHandle(h);}Handle(const Handle&)=delete;};
const char* vendor(UINT id){switch(id){case 0x10de:return "NVIDIA";case 0x1002:return "AMD";case 0x8086:return "Intel";default:return "Other";}}
struct Gpu {ComPtr<IDXGIAdapter1> adapter;ComPtr<ID3D12Device> device;DXGI_ADAPTER_DESC1 desc{};};
Gpu select(){ComPtr<IDXGIFactory6> factory;check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)),"CreateDXGIFactory2");for(UINT i=0;;++i){Gpu g;HRESULT h=factory->EnumAdapterByGpuPreference(i,DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,IID_PPV_ARGS(&g.adapter));if(h==DXGI_ERROR_NOT_FOUND)break;check(h,"EnumAdapterByGpuPreference");check(g.adapter->GetDesc1(&g.desc),"GetDesc1");if(g.desc.Flags&DXGI_ADAPTER_FLAG_SOFTWARE)continue;if(SUCCEEDED(D3D12CreateDevice(g.adapter.Get(),D3D_FEATURE_LEVEL_12_0,IID_PPV_ARGS(&g.device))))return g;}throw std::runtime_error("No hardware D3D12 FL12_0 adapter");}
void print(const Gpu& g){char adapter[512]{};WideCharToMultiByte(CP_UTF8,0,g.desc.Description,-1,adapter,sizeof(adapter),nullptr,nullptr);std::cout<<"Adapter: "<<adapter<<"\n";std::cout<<"Vendor: "<<vendor(g.desc.VendorId)<<"; dedicated VRAM MiB: "<<(g.desc.DedicatedVideoMemory/1048576)<<"\n";D3D12_FEATURE_DATA_ARCHITECTURE1 a{};if(SUCCEEDED(g.device->CheckFeatureSupport(D3D12_FEATURE_ARCHITECTURE1,&a,sizeof(a))))std::cout<<"UMA: "<<a.UMA<<"; cache-coherent UMA: "<<a.CacheCoherentUMA<<"\n";D3D12_FEATURE_DATA_D3D12_OPTIONS o{};if(SUCCEEDED(g.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS,&o,sizeof(o))))std::cout<<"Resource binding tier: "<<o.ResourceBindingTier<<"\n";D3D12_FEATURE_DATA_D3D12_OPTIONS5 ray{};if(SUCCEEDED(g.device->CheckFeatureSupport(D3D12_FEATURE_D3D12_OPTIONS5,&ray,sizeof(ray))))std::cout<<"DXR tier: "<<ray.RaytracingTier<<"\n";for(unsigned model: {0x66u,0x65u,0x64u,0x63u,0x62u,0x61u,0x60u,0x51u}){D3D12_FEATURE_DATA_SHADER_MODEL sm{static_cast<D3D_SHADER_MODEL>(model)};if(SUCCEEDED(g.device->CheckFeatureSupport(D3D12_FEATURE_SHADER_MODEL,&sm,sizeof(sm)))){std::cout<<"Shader model (queried up to 6.6): 0x"<<std::hex<<sm.HighestShaderModel<<std::dec<<"\n";break;}}ComPtr<IDXGIAdapter3> ad;if(SUCCEEDED(g.adapter.As(&ad))){DXGI_QUERY_VIDEO_MEMORY_INFO m{};if(SUCCEEDED(ad->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&m)))std::cout<<"Local memory budget MiB: "<<m.Budget/1048576<<"\n";}}
ComPtr<ID3D12Resource> buffer(ID3D12Device* d,uint64_t size,D3D12_HEAP_TYPE type,D3D12_RESOURCE_STATES state){D3D12_HEAP_PROPERTIES h{};h.Type=type;h.CreationNodeMask=h.VisibleNodeMask=1;D3D12_RESOURCE_DESC r{};r.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;r.Width=size;r.Height=1;r.DepthOrArraySize=1;r.MipLevels=1;r.SampleDesc.Count=1;r.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;ComPtr<ID3D12Resource> p;check(d->CreateCommittedResource(&h,D3D12_HEAP_FLAG_NONE,&r,state,nullptr,IID_PPV_ARGS(&p)),"CreateCommittedResource");return p;}
void wait(ID3D12Fence* f,uint64_t value,HANDLE event){auto completed=f->GetCompletedValue();if(completed==UINT64_MAX)throw std::runtime_error("GPU device removed");if(completed>=value)return;check(f->SetEventOnCompletion(value,event),"SetEventOnCompletion");if(WaitForSingleObject(event,30000)!=WAIT_OBJECT_0)throw std::runtime_error("GPU fence wait failed or timed out");completed=f->GetCompletedValue();if(completed==UINT64_MAX||completed<value)throw std::runtime_error("GPU did not complete fence");}
}
int gpu_info(){auto g=select();print(g);return 0;}
static double bench(Gpu& g,unsigned frames,unsigned slots,unsigned mib){auto d=g.device.Get();uint64_t size=uint64_t(mib)*1048576;D3D12_COMMAND_QUEUE_DESC qd{};qd.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;ComPtr<ID3D12CommandQueue> q;check(d->CreateCommandQueue(&qd,IID_PPV_ARGS(&q)),"CreateCommandQueue");ComPtr<ID3D12Fence> f;check(d->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&f)),"CreateFence");Handle event;struct Slot{ComPtr<ID3D12CommandAllocator> a;ComPtr<ID3D12GraphicsCommandList> list;ComPtr<ID3D12Resource> dst;uint64_t fence=0;};std::vector<Slot> ring(slots);auto src=buffer(d,size,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);auto read=buffer(d,size,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);void* data=nullptr;D3D12_RANGE empty{0,0};check(src->Map(0,&empty,&data),"Map upload");std::memset(data,0xa5,static_cast<size_t>(size));src->Unmap(0,nullptr);for(auto& s:ring){s.dst=buffer(d,size,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COPY_DEST);check(d->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&s.a)),"CreateCommandAllocator");check(d->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,s.a.Get(),nullptr,IID_PPV_ARGS(&s.list)),"CreateCommandList");check(s.list->Close(),"Close initial list");}auto start=std::chrono::steady_clock::now();uint64_t seq=0;for(unsigned i=0;i<frames;++i){auto& s=ring[i%slots];if(s.fence)wait(f.Get(),s.fence,event.h);check(s.a->Reset(),"Reset allocator");check(s.list->Reset(s.a.Get(),nullptr),"Reset list");s.list->CopyBufferRegion(s.dst.Get(),0,src.Get(),0,size);check(s.list->Close(),"Close list");ID3D12CommandList* lists[]={s.list.Get()};q->ExecuteCommandLists(1,lists);s.fence=++seq;check(q->Signal(f.Get(),seq),"Signal");}wait(f.Get(),seq,event.h);double sec=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();auto& last=ring[(frames-1)%slots];check(last.a->Reset(),"Reset readback allocator");check(last.list->Reset(last.a.Get(),nullptr),"Reset readback list");D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition.pResource=last.dst.Get();b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;b.Transition.StateBefore=D3D12_RESOURCE_STATE_COPY_DEST;b.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;last.list->ResourceBarrier(1,&b);last.list->CopyBufferRegion(read.Get(),0,last.dst.Get(),0,size);check(last.list->Close(),"Close readback list");ID3D12CommandList* lists[]={last.list.Get()};q->ExecuteCommandLists(1,lists);check(q->Signal(f.Get(),++seq),"Signal readback");wait(f.Get(),seq,event.h);D3D12_RANGE range{0,static_cast<SIZE_T>(size)};check(read->Map(0,&range,&data),"Map readback");auto p=static_cast<const unsigned char*>(data);bool good=true;for(uint64_t i=0;i<size;++i)if(p[i]!=0xa5){good=false;break;}read->Unmap(0,&empty);if(!good)throw std::runtime_error("GPU copy verification failed");std::cout<<"Verified final copy. Synthetic transfer throughput: "<<(frames*double(mib)/sec)<<" MiB/s; mean submission/completion wall time "<<sec*1000/frames<<" ms; slots="<<slots<<". Not game FPS.\n";return frames*double(mib)/sec;}
int gpu_bench(unsigned frames,unsigned slots,unsigned mib){auto g=select();print(g);bench(g,frames,slots,mib);return 0;}
int gpu_tune(unsigned frames,unsigned mib){auto g=select();print(g);ComPtr<IDXGIAdapter3> ad;check(g.adapter.As(&ad),"IDXGIAdapter3");DXGI_QUERY_VIDEO_MEMORY_INFO m{};check(ad->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&m),"QueryVideoMemoryInfo");const auto size=uint64_t(mib)*1048576;auto free=m.Budget>m.CurrentUsage?m.Budget-m.CurrentUsage:0;unsigned max=slot_budget(free,size);if(!max)throw std::runtime_error("insufficient budget for tuning; reduce transfer size");std::vector<TuneSample> results;for(unsigned slots:{1u,2u,3u,4u,6u,8u}){check(ad->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&m),"QueryVideoMemoryInfo");free=m.Budget>m.CurrentUsage?m.Budget-m.CurrentUsage:0;if(slots>max||slots>slot_budget(free,size))continue;std::array<double,3> times{};for(auto& t:times)t=bench(g,frames,slots,mib);std::sort(times.begin(),times.end());results.push_back({slots,times[1]});std::cout<<"Median for "<<slots<<" slots: "<<times[1]<<" MiB/s\n";}std::cout<<"Suggested slots: "<<choose_slots(results)<<" (smallest within 95% of peak median throughput). Synthetic workload only; no game setting changed.\n";return 0;}
}
namespace revenge {
// Exact bytecode and root-signature identity; driver cache validation is authoritative.
static std::vector<uint8_t> compute_cache_key(const Gpu& g,ID3DBlob* sig,const void* code,size_t code_size){
 std::vector<uint8_t> key;auto number=[&](uint64_t n){for(unsigned i=0;i<8;++i)key.push_back(uint8_t(n>>(i*8)));};
 auto bytes=[&](const void* p,size_t n){number(n);auto begin=static_cast<const uint8_t*>(p);key.insert(key.end(),begin,begin+n);};
 number(1);number(g.desc.VendorId);number(g.desc.DeviceId);number(g.desc.SubSysId);number(g.desc.Revision);number(uint32_t(g.desc.AdapterLuid.HighPart));number(g.desc.AdapterLuid.LowPart);
 LARGE_INTEGER driver{};auto hr=g.adapter->CheckInterfaceSupport(__uuidof(IDXGIDevice),&driver);number(SUCCEEDED(hr)?1:0);number(SUCCEEDED(hr)?uint64_t(driver.QuadPart):0);
 bytes(sig->GetBufferPointer(),sig->GetBufferSize());bytes(code,code_size);return key;
}
static std::vector<uint8_t> read_cache(const std::filesystem::path& path,std::span<const uint8_t> key){
 std::ifstream in(path,std::ios::binary|std::ios::ate);if(!in)return {};auto size=in.tellg();if(size<=0||size>std::streamoff(32+pipeline_cache::max_key+pipeline_cache::max_blob))return {};std::vector<uint8_t> data(static_cast<size_t>(size));in.seekg(0);if(!in.read(reinterpret_cast<char*>(data.data()),size))return {};return pipeline_cache::unpack(key,data);
}
static void save_cache(const std::filesystem::path& path,std::span<const uint8_t> key,ID3D12PipelineState* pso,const char* label="Compute"){
 // Cache is optional. A failed write must never prevent rendering.
 std::filesystem::path temporary;
 try{ComPtr<ID3DBlob> blob;if(FAILED(pso->GetCachedBlob(&blob))||!blob||!blob->GetBufferSize()||blob->GetBufferSize()>pipeline_cache::max_blob){std::cout<<label<<" PSO cache unavailable from driver.\n";return;}
 auto data=pipeline_cache::pack(key,{static_cast<const uint8_t*>(blob->GetBufferPointer()),blob->GetBufferSize()});
 if(!path.parent_path().empty())std::filesystem::create_directories(path.parent_path());temporary=path;temporary+=L".tmp-"+std::to_wstring(GetCurrentProcessId());
 std::ofstream out(temporary,std::ios::binary|std::ios::trunc);if(!out||!out.write(reinterpret_cast<const char*>(data.data()),std::streamsize(data.size())))throw std::runtime_error("cache write failed");out.close();if(!out)throw std::runtime_error("cache close failed");
 if(!MoveFileExW(temporary.c_str(),path.c_str(),MOVEFILE_REPLACE_EXISTING|MOVEFILE_WRITE_THROUGH))throw std::runtime_error("cache replacement failed");std::cout<<label<<" PSO cache saved.\n";
 }catch(const std::exception& e){if(!temporary.empty()){std::error_code ec;std::filesystem::remove(temporary,ec);}std::cout<<label<<" PSO cache not saved: "<<e.what()<<'\n';}
}
static int compute_known_answer(const void* code,size_t code_size,std::span<const uint32_t> expected,std::span<const uint32_t> input={},const std::filesystem::path& cache={}){
 if(!input.empty()&&input.size()!=4096)throw std::runtime_error("expected 4096 inputs");
 if(expected.size()!=4096)throw std::runtime_error("expected 4096 fixture outputs"); auto g=select();print(g);auto d=g.device.Get();constexpr UINT count=4096;constexpr UINT bytes=count*4;D3D12_ROOT_PARAMETER param{};param.ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;param.Descriptor.ShaderRegister=0;param.ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;D3D12_ROOT_PARAMETER params[2]={param,{}};params[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_SRV;params[1].Descriptor.ShaderRegister=0;params[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;D3D12_ROOT_SIGNATURE_DESC desc{};desc.NumParameters=input.empty()?1:2;desc.pParameters=params;ComPtr<ID3DBlob> sig,error;check(D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,&sig,&error),"Serialize root signature");ComPtr<ID3D12RootSignature> root;check(d->CreateRootSignature(0,sig->GetBufferPointer(),sig->GetBufferSize(),IID_PPV_ARGS(&root)),"CreateRootSignature");D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root.Get();pd.CS={code,code_size};ComPtr<ID3D12PipelineState> pso;std::vector<uint8_t> key,cached;if(!cache.empty()){key=compute_cache_key(g,sig.Get(),code,code_size);cached=read_cache(cache,key);if(cached.empty())std::cout<<"Compute PSO cache miss.\n";else pd.CachedPSO={cached.data(),cached.size()};}
 auto pstart=std::chrono::steady_clock::now();auto hr=d->CreateComputePipelineState(&pd,IID_PPV_ARGS(&pso));bool hit=!cached.empty()&&SUCCEEDED(hr);if(!cached.empty()){if(hit)std::cout<<"Compute PSO cache hit (driver accepted).\n";else{std::cout<<"Compute PSO cache rejected; retrying without it.\n";pd.CachedPSO={};pso.Reset();hr=d->CreateComputePipelineState(&pd,IID_PPV_ARGS(&pso));}}check(hr,"CreateComputePipelineState");std::cout<<"Compute PSO creation ms: "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-pstart).count()<<"\n";

 D3D12_HEAP_PROPERTIES h{};h.Type=D3D12_HEAP_TYPE_DEFAULT;h.CreationNodeMask=h.VisibleNodeMask=1;D3D12_RESOURCE_DESC r{};r.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;r.Width=bytes;r.Height=1;r.DepthOrArraySize=1;r.MipLevels=1;r.SampleDesc.Count=1;r.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;r.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;ComPtr<ID3D12Resource> output;check(d->CreateCommittedResource(&h,D3D12_HEAP_FLAG_NONE,&r,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&output)),"Create UAV");auto read=buffer(d,bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
 D3D12_COMMAND_QUEUE_DESC qd{};qd.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;ComPtr<ID3D12CommandQueue> q;check(d->CreateCommandQueue(&qd,IID_PPV_ARGS(&q)),"Create queue");ComPtr<ID3D12CommandAllocator> a;check(d->CreateCommandAllocator(qd.Type,IID_PPV_ARGS(&a)),"Create allocator");ComPtr<ID3D12GraphicsCommandList> list;check(d->CreateCommandList(0,qd.Type,a.Get(),pso.Get(),IID_PPV_ARGS(&list)),"Create list");list->SetComputeRootSignature(root.Get());list->SetComputeRootUnorderedAccessView(0,output->GetGPUVirtualAddress());ComPtr<ID3D12Resource> input_buffer;if(!input.empty()){constexpr UINT padded_bytes=bytes+4092;input_buffer=buffer(d,padded_bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);D3D12_RANGE none{0,0};void* mapped=nullptr;check(input_buffer->Map(0,&none,&mapped),"Map fixture input");std::memset(mapped,0,padded_bytes);std::memcpy(mapped,input.data(),bytes);D3D12_RANGE written{0,padded_bytes};input_buffer->Unmap(0,&written);list->SetComputeRootShaderResourceView(1,input_buffer->GetGPUVirtualAddress());}list->Dispatch(count/64,1,1);D3D12_RESOURCE_BARRIER barrier{};barrier.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;barrier.Transition.pResource=output.Get();barrier.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;barrier.Transition.StateBefore=D3D12_RESOURCE_STATE_UNORDERED_ACCESS;barrier.Transition.StateAfter=D3D12_RESOURCE_STATE_COPY_SOURCE;list->ResourceBarrier(1,&barrier);list->CopyBufferRegion(read.Get(),0,output.Get(),0,bytes);check(list->Close(),"Close list");ComPtr<ID3D12Fence> fence;check(d->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"Create fence");Handle event;ID3D12CommandList* lists[]={list.Get()};q->ExecuteCommandLists(1,lists);check(q->Signal(fence.Get(),1),"Signal");wait(fence.Get(),1,event.h);D3D12_RANGE range{0,bytes},empty{0,0};void* data=nullptr;check(read->Map(0,&range,&data),"Map result");bool valid=true;auto v=static_cast<const uint32_t*>(data);for(UINT i=0;i<count;++i)if(v[i]!=expected[i]){valid=false;break;}read->Unmap(0,&empty);if(!valid)throw std::runtime_error("Compute known-answer test failed");if(!cache.empty()&&!hit)save_cache(cache,key,pso.Get());std::cout<<"Verified all "<<count<<" compute results.\n";return 0;
}
int gpu_compute(const std::filesystem::path& shader){
 // This is a known-answer test for samples/test.hlsl, not a general shader runner.
 std::ifstream in(shader,std::ios::binary|std::ios::ate); if(!in)throw std::runtime_error("cannot open sample shader");auto size=in.tellg();if(size<=0||size>std::streamoff(16*1024*1024))throw std::runtime_error("invalid sample shader size");std::vector<char> code(static_cast<size_t>(size));in.seekg(0);if(!in.read(code.data(),size))throw std::runtime_error("shader read failed");
 std::vector<uint32_t> expected(4096);for(uint32_t i=0;i<expected.size();++i)expected[i]=i*3+1;return compute_known_answer(code.data(),code.size(),expected);
}
namespace shader {
int gpu_shader_fixture(){
 auto p=decode(fixture_words());auto opt=optimize(p);auto source=hlsl(opt);std::vector<uint32_t> expected(4096);for(uint32_t i=0;i<expected.size();++i){expected[i]=evaluate(p,i);if(expected[i]!=i*3+1||evaluate(opt,i)!=expected[i])throw std::runtime_error("CPU fixture oracle mismatch");}
 ComPtr<ID3DBlob> blob,errors;auto hr=D3DCompile(source.data(),source.size(),"revenge-alu-fixture",nullptr,nullptr,"main","cs_5_1",D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&errors);if(FAILED(hr)&&errors)std::cerr<<std::string(static_cast<const char*>(errors->GetBufferPointer()),errors->GetBufferSize())<<'\n';check(hr,"Compile generated fixture HLSL");
 std::cout<<"Decoded and optimized GFX10.3 ALU fixture, compiled generated HLSL (cs_5_1).\n";return compute_known_answer(blob->GetBufferPointer(),blob->GetBufferSize(),expected);
}
}
namespace flow {
int gpu_test(){
 auto p=decode(fixture_words()); auto source=emit(p);std::vector<uint32_t> expected(4096);
 for(uint32_t i=0;i<expected.size();++i){expected[i]=fixture_expected(i);if(evaluate(p,i)!=expected[i])throw std::runtime_error("flow CPU oracle mismatch");}
 ComPtr<ID3DBlob> blob,errors;auto hr=D3DCompile(source.data(),source.size(),"revenge-flow-fixture",nullptr,nullptr,"main","cs_5_1",D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_IEEE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&errors);
 if(FAILED(hr)&&errors)std::cerr<<std::string(static_cast<const char*>(errors->GetBufferPointer()),errors->GetBufferSize())<<'\n';check(hr,"Compile generated flow HLSL");
 std::cout<<"Decoded forward branches and finite normal FP32 fixture (cs_5_1).\n";return compute_known_answer(blob->GetBufferPointer(),blob->GetBufferSize(),expected);
}
}
}

namespace revenge { namespace {
// Self-authored desktop D3D12 test only. No console shader or ABI is used.
constexpr char render_shader[]=R"(
Texture2D<float4> fixtureTexture : register(t0);
float4 vertex(uint id : SV_VertexID) : SV_Position {
    float2 p[3] = {float2(-0.8,-0.8), float2(0,0.8), float2(0.8,-0.8)};
    return float4(p[id],0,1);
}
float4 pixel(float4 p : SV_Position) : SV_Target {
    return fixtureTexture.Load(int3((uint2(p.xy)/32u)&1u,0));
}
)";
ComPtr<ID3DBlob> compile_fixture(const char* entry,const char* target){
 ComPtr<ID3DBlob> blob,errors;auto hr=D3DCompile(render_shader,sizeof(render_shader)-1,"revenge-fixture",nullptr,nullptr,entry,target,D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&errors);
 if(FAILED(hr)&&errors)std::cerr<<std::string(static_cast<const char*>(errors->GetBufferPointer()),errors->GetBufferSize())<<'\n';check(hr,"D3DCompile fixture");return blob;
}
D3D12_RESOURCE_DESC texture_desc(unsigned w,unsigned h,D3D12_RESOURCE_FLAGS flags){
 D3D12_RESOURCE_DESC r{};r.Dimension=D3D12_RESOURCE_DIMENSION_TEXTURE2D;r.Width=w;r.Height=h;r.DepthOrArraySize=1;r.MipLevels=1;r.Format=DXGI_FORMAT_R8G8B8A8_UNORM;r.SampleDesc.Count=1;r.Layout=D3D12_TEXTURE_LAYOUT_UNKNOWN;r.Flags=flags;return r;
}
ComPtr<ID3D12Resource> texture(ID3D12Device* d,const D3D12_RESOURCE_DESC& r,D3D12_RESOURCE_STATES state,const D3D12_CLEAR_VALUE* clear=nullptr){
 D3D12_HEAP_PROPERTIES h{};h.Type=D3D12_HEAP_TYPE_DEFAULT;h.CreationNodeMask=h.VisibleNodeMask=1;ComPtr<ID3D12Resource> p;check(d->CreateCommittedResource(&h,D3D12_HEAP_FLAG_NONE,&r,state,clear,IID_PPV_ARGS(&p)),"Create texture");return p;
}
void transition(ID3D12GraphicsCommandList* l,ID3D12Resource* r,D3D12_RESOURCE_STATES before,D3D12_RESOURCE_STATES after){
 D3D12_RESOURCE_BARRIER b{};b.Type=D3D12_RESOURCE_BARRIER_TYPE_TRANSITION;b.Transition.pResource=r;b.Transition.Subresource=D3D12_RESOURCE_BARRIER_ALL_SUBRESOURCES;b.Transition.StateBefore=before;b.Transition.StateAfter=after;l->ResourceBarrier(1,&b);
}
D3D12_TEXTURE_COPY_LOCATION location(ID3D12Resource* r){D3D12_TEXTURE_COPY_LOCATION loc{};loc.pResource=r;loc.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;return loc;}
D3D12_TEXTURE_COPY_LOCATION location(ID3D12Resource* r,const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& fp){D3D12_TEXTURE_COPY_LOCATION loc{};loc.pResource=r;loc.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;loc.PlacedFootprint=fp;return loc;}
ComPtr<ID3D12DescriptorHeap> descriptors(ID3D12Device* d,D3D12_DESCRIPTOR_HEAP_TYPE type,UINT count,bool visible){D3D12_DESCRIPTOR_HEAP_DESC desc{};desc.Type=type;desc.NumDescriptors=count;desc.Flags=visible?D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE:D3D12_DESCRIPTOR_HEAP_FLAG_NONE;ComPtr<ID3D12DescriptorHeap> heap;check(d->CreateDescriptorHeap(&desc,IID_PPV_ARGS(&heap)),"Create descriptor heap");return heap;}
struct RenderSlot {ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;ComPtr<ID3D12Resource> target;D3D12_CPU_DESCRIPTOR_HANDLE rtv{};};
// Best-effort drain during exceptions. Normal operation uses the checked wait.
struct DrainQueue {ID3D12CommandQueue* queue;ID3D12Fence* fence;HANDLE event;uint64_t last=0;~DrainQueue(){if(last&&SUCCEEDED(queue->Signal(fence,last+1))&&SUCCEEDED(fence->SetEventOnCompletion(last+1,event)))WaitForSingleObject(event,30000);}};
}
int gpu_render(unsigned frames,unsigned slots,const std::filesystem::path& output,const std::filesystem::path& cache){
 if(!cache.empty()&&!output.empty()&&std::filesystem::absolute(cache).lexically_normal()==std::filesystem::absolute(output).lexically_normal())throw std::invalid_argument("cache and BMP paths must differ");
 if(!frames||frames>2000)throw std::invalid_argument("render frames must be 1..2000");FrameRing ring(slots);auto g=select();print(g);auto d=g.device.Get();constexpr unsigned w=256,h=256;
 auto vs=compile_fixture("vertex","vs_5_1"),ps=compile_fixture("pixel","ps_5_1");
 D3D12_DESCRIPTOR_RANGE range{};range.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV;range.NumDescriptors=1;range.OffsetInDescriptorsFromTableStart=0;
 D3D12_ROOT_PARAMETER param{};param.ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;param.DescriptorTable.NumDescriptorRanges=1;param.DescriptorTable.pDescriptorRanges=&range;param.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
 D3D12_ROOT_SIGNATURE_DESC rs{};rs.NumParameters=1;rs.pParameters=&param;ComPtr<ID3DBlob> sig,err;check(D3D12SerializeRootSignature(&rs,D3D_ROOT_SIGNATURE_VERSION_1,&sig,&err),"Serialize render root signature");ComPtr<ID3D12RootSignature> root;check(d->CreateRootSignature(0,sig->GetBufferPointer(),sig->GetBufferSize(),IID_PPV_ARGS(&root)),"Create render root signature");
 D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root.Get();pd.VS={vs->GetBufferPointer(),vs->GetBufferSize()};pd.PS={ps->GetBufferPointer(),ps->GetBufferSize()};pd.SampleMask=UINT_MAX;pd.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;pd.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;pd.RasterizerState.DepthClipEnable=TRUE;
 auto& blend=pd.BlendState.RenderTarget[0];blend.SrcBlend=D3D12_BLEND_ONE;blend.DestBlend=D3D12_BLEND_ZERO;blend.BlendOp=D3D12_BLEND_OP_ADD;blend.SrcBlendAlpha=D3D12_BLEND_ONE;blend.DestBlendAlpha=D3D12_BLEND_ZERO;blend.BlendOpAlpha=D3D12_BLEND_OP_ADD;blend.LogicOp=D3D12_LOGIC_OP_NOOP;blend.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
 pd.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ZERO;pd.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_ALWAYS;pd.DepthStencilState.FrontFace={D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_COMPARISON_FUNC_ALWAYS};pd.DepthStencilState.BackFace=pd.DepthStencilState.FrontFace;pd.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;pd.NumRenderTargets=1;pd.RTVFormats[0]=DXGI_FORMAT_R8G8B8A8_UNORM;pd.SampleDesc.Count=1;
 ComPtr<ID3D12PipelineState> pso;std::vector<uint8_t> key,cached;
 // Fixed fixture state schema; bump identifier if ANY non-shader PSO state changes.
 if(!cache.empty()){
  key=compute_cache_key(g,sig.Get(),vs->GetBufferPointer(),vs->GetBufferSize());
  const std::string tag="revenge-graphics-fixture-pso-state-v1";key.insert(key.end(),tag.begin(),tag.end());
  auto n=ps->GetBufferSize();for(unsigned i=0;i<8;++i)key.push_back(uint8_t(uint64_t(n)>>(8*i)));
  auto code=static_cast<const uint8_t*>(ps->GetBufferPointer());key.insert(key.end(),code,code+n);
  cached=read_cache(cache,key);if(cached.empty())std::cout<<"Graphics PSO cache miss.\n";else pd.CachedPSO={cached.data(),cached.size()};
 }
 auto pstart=std::chrono::steady_clock::now();auto hr=d->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&pso));bool hit=!cached.empty()&&SUCCEEDED(hr);
 if(!cached.empty()){if(hit)std::cout<<"Graphics PSO cache hit (driver accepted).\n";else{std::cout<<"Graphics PSO cache rejected; retrying without it.\n";pd.CachedPSO={};pso.Reset();hr=d->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&pso));}}
 check(hr,"Create graphics PSO");std::cout<<"Graphics PSO creation ms: "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-pstart).count()<<'\n';
 auto rtv=descriptors(d,D3D12_DESCRIPTOR_HEAP_TYPE_RTV,slots,false);auto srv=descriptors(d,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,1,true);
 auto texdesc=texture_desc(2,2,D3D12_RESOURCE_FLAG_NONE);auto tex=texture(d,texdesc,D3D12_RESOURCE_STATE_COPY_DEST);D3D12_SHADER_RESOURCE_VIEW_DESC sv{};sv.Format=texdesc.Format;sv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;sv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;sv.Texture2D.MipLevels=1;d->CreateShaderResourceView(tex.Get(),&sv,srv->GetCPUDescriptorHandleForHeapStart());
 D3D12_PLACED_SUBRESOURCE_FOOTPRINT tf{};UINT64 upload_bytes=0;d->GetCopyableFootprints(&texdesc,0,1,0,&tf,nullptr,nullptr,&upload_bytes);auto upload=buffer(d,upload_bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
 const std::array<uint8_t,16> colors{255,64,32,255,32,255,64,255,64,32,255,255,255,255,32,255};D3D12_RANGE empty{0,0};void* mapped=nullptr;check(upload->Map(0,&empty,&mapped),"Map texture upload");for(unsigned y=0;y<2;++y)std::memcpy(static_cast<uint8_t*>(mapped)+tf.Offset+y*tf.Footprint.RowPitch,colors.data()+y*8,8);upload->Unmap(0,nullptr);
 auto rt_desc=texture_desc(w,h,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);D3D12_CLEAR_VALUE clear{};clear.Format=rt_desc.Format;clear.Color[3]=1;D3D12_PLACED_SUBRESOURCE_FOOTPRINT rf{};UINT64 read_bytes=0;d->GetCopyableFootprints(&rt_desc,0,1,0,&rf,nullptr,nullptr,&read_bytes);auto read=buffer(d,read_bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
 D3D12_COMMAND_QUEUE_DESC qd{};qd.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;ComPtr<ID3D12CommandQueue> q;check(d->CreateCommandQueue(&qd,IID_PPV_ARGS(&q)),"Create render queue");UINT64 frequency=0;check(q->GetTimestampFrequency(&frequency),"GetTimestampFrequency");if(!frequency)throw std::runtime_error("zero GPU timestamp frequency");
 D3D12_QUERY_HEAP_DESC qh{};qh.Type=D3D12_QUERY_HEAP_TYPE_TIMESTAMP;qh.Count=frames*2;ComPtr<ID3D12QueryHeap> queries;check(d->CreateQueryHeap(&qh,IID_PPV_ARGS(&queries)),"Create timestamp heap");auto timestamps=buffer(d,UINT64(frames)*16,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
 std::vector<RenderSlot> resources(slots);auto handle=rtv->GetCPUDescriptorHandleForHeapStart();auto step=d->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);for(auto& s:resources){s.target=texture(d,rt_desc,D3D12_RESOURCE_STATE_RENDER_TARGET,&clear);s.rtv=handle;d->CreateRenderTargetView(s.target.Get(),nullptr,s.rtv);handle.ptr+=step;check(d->CreateCommandAllocator(qd.Type,IID_PPV_ARGS(&s.allocator)),"Create frame allocator");check(d->CreateCommandList(0,qd.Type,s.allocator.Get(),pso.Get(),IID_PPV_ARGS(&s.list)),"Create frame list");check(s.list->Close(),"Close initial frame list");}
 ComPtr<ID3D12Fence> fence;check(d->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"Create render fence");Handle event;DrainQueue drain{q.Get(),fence.Get(),event.h};D3D12_VIEWPORT vp{0,0,float(w),float(h),0,1};D3D12_RECT rect{0,0,LONG(w),LONG(h)};
 auto start=std::chrono::steady_clock::now();for(unsigned i=0;i<frames;++i){if(auto needed=ring.required_fence())wait(fence.Get(),needed,event.h);ring.acquire(fence->GetCompletedValue());auto& s=resources[ring.slot()];auto l=s.list.Get();check(s.allocator->Reset(),"Reset frame allocator");check(l->Reset(s.allocator.Get(),pso.Get()),"Reset frame list");if(i==0){auto dst=location(tex.Get()),src=location(upload.Get(),tf);l->CopyTextureRegion(&dst,0,0,0,&src,nullptr);transition(l,tex.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);}
 l->EndQuery(queries.Get(),D3D12_QUERY_TYPE_TIMESTAMP,2*i);l->SetGraphicsRootSignature(root.Get());ID3D12DescriptorHeap* heaps[]{srv.Get()};l->SetDescriptorHeaps(1,heaps);l->SetGraphicsRootDescriptorTable(0,srv->GetGPUDescriptorHandleForHeapStart());l->RSSetViewports(1,&vp);l->RSSetScissorRects(1,&rect);l->OMSetRenderTargets(1,&s.rtv,FALSE,nullptr);l->ClearRenderTargetView(s.rtv,clear.Color,0,nullptr);l->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);l->DrawInstanced(3,1,0,0);l->EndQuery(queries.Get(),D3D12_QUERY_TYPE_TIMESTAMP,2*i+1);l->ResolveQueryData(queries.Get(),D3D12_QUERY_TYPE_TIMESTAMP,2*i,2,timestamps.Get(),UINT64(i)*16);
 if(i==frames-1){transition(l,s.target.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE);auto dst=location(read.Get(),rf),src=location(s.target.Get());l->CopyTextureRegion(&dst,0,0,0,&src,nullptr);}check(l->Close(),"Close frame list");auto seq=ring.submit();ID3D12CommandList* lists[]{l};q->ExecuteCommandLists(1,lists);drain.last=seq;check(q->Signal(fence.Get(),seq),"Signal frame fence");}
 wait(fence.Get(),ring.last_fence(),event.h);drain.last=0;double wall=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
 D3D12_RANGE tr{0,SIZE_T(frames)*16};check(timestamps->Map(0,&tr,&mapped),"Map timestamps");std::vector<uint64_t> ticks(frames*2);std::memcpy(ticks.data(),mapped,tr.End);timestamps->Unmap(0,&empty);std::vector<double> durations;durations.reserve(frames);for(unsigned i=0;i<frames;++i){if(ticks[2*i+1]<ticks[2*i])throw std::runtime_error("GPU timestamp order invalid");durations.push_back(double(ticks[2*i+1]-ticks[2*i])*1000.0/double(frequency));}auto stats=frame_stats(durations);
 D3D12_RANGE rr{0,static_cast<SIZE_T>(read_bytes)};check(read->Map(0,&rr,&mapped),"Map render output");std::vector<uint8_t> image(size_t(rf.Footprint.RowPitch)*h);for(unsigned y=0;y<h;++y)std::memcpy(image.data()+size_t(y)*rf.Footprint.RowPitch,static_cast<const uint8_t*>(mapped)+rf.Offset+size_t(y)*rf.Footprint.RowPitch,w*4);read->Unmap(0,&empty);
 auto pixel=[&](unsigned x,unsigned y,const uint8_t* expected){auto p=image.data()+size_t(y)*rf.Footprint.RowPitch+x*4;return std::equal(p,p+4,expected);};constexpr uint8_t black[]{0,0,0,255};if(!pixel(0,0,black)||!pixel(128,128,colors.data())||!pixel(160,128,colors.data()+4)||!pixel(128,160,colors.data()+8)||!pixel(160,160,colors.data()+12))throw std::runtime_error("render pixel verification failed");
 if(!cache.empty()&&!hit)save_cache(cache,key,pso.Get(),"Graphics");
 std::cout<<"Verified 5 fixture pixels (background and four texture colors).\n"<<"GPU draw interval ms: mean="<<stats.mean_ms<<" median="<<stats.median_ms<<" p95="<<stats.p95_ms<<"\n"<<"CPU submission plus drain ms/frame: "<<wall/frames<<"; frames="<<frames<<"; slots="<<slots<<". Offscreen synthetic test, not game FPS.\n";if(!output.empty()){write_frame_bmp(output,w,h,rf.Footprint.RowPitch,image);std::cout<<"Saved fixture frame: "<<output.string()<<'\n';}return 0;
}
}

namespace revenge::memory_shader {
int gpu_test(){auto p=decode(fixture_words());auto input=fixture_input();auto expected=fixture_expected(input);if(evaluate(p,input)!=expected)throw std::runtime_error("buffer CPU oracle mismatch");auto source=emit(p);ComPtr<ID3DBlob> blob,errors;auto hr=D3DCompile(source.data(),source.size(),"revenge-buffer-fixture",nullptr,nullptr,"main","cs_5_1",D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&errors);if(FAILED(hr)&&errors)std::cerr<<std::string(static_cast<const char*>(errors->GetBufferPointer()),errors->GetBufferSize())<<'\n';check(hr,"Compile buffer fixture HLSL");std::cout<<"Decoded GFX10 buffer load/store fixture; read-only t0, writable u0 (cs_5_1).\n";return compute_known_answer(blob->GetBufferPointer(),blob->GetBufferSize(),expected,input);}
}

namespace revenge::flow {
int unified_gpu_test(const std::filesystem::path& cache){
 auto p=decode_unified(unified_words());auto input=memory_shader::fixture_input();std::vector<uint32_t> expected(4096);for(uint32_t i=0;i<expected.size();++i){expected[i]=unified_expected(i,input);if(evaluate_unified(p,i,input)!=expected[i])throw std::runtime_error("mixed fixture CPU oracle mismatch");}
 auto source=emit(p);ComPtr<ID3DBlob> blob,errors;auto hr=D3DCompile(source.data(),source.size(),"revenge-unified-fixture",nullptr,nullptr,"main","cs_5_1",D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_IEEE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&errors);if(FAILED(hr)&&errors)std::cerr<<std::string(static_cast<const char*>(errors->GetBufferPointer()),errors->GetBufferSize())<<'\n';check(hr,"Compile mixed fixture HLSL");std::cout<<"Decoded mixed arithmetic, branch and buffer fixture (cs_5_1).\n";return compute_known_answer(blob->GetBufferPointer(),blob->GetBufferSize(),expected,input,cache);
}
}

namespace revenge::textures {
int sampling_gpu_test(){
 auto f=sampling_fixture();auto g=select();print(g);auto d=g.device.Get();
 D3D12_DESCRIPTOR_RANGE range{};range.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV;range.NumDescriptors=UINT(f.images.size());
 D3D12_ROOT_PARAMETER params[2]{};params[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;params[0].DescriptorTable={1,&range};params[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;
 auto address=[](Address a){switch(a){case Address::wrap:return D3D12_TEXTURE_ADDRESS_MODE_WRAP;case Address::mirror:return D3D12_TEXTURE_ADDRESS_MODE_MIRROR;case Address::border:return D3D12_TEXTURE_ADDRESS_MODE_BORDER;default:return D3D12_TEXTURE_ADDRESS_MODE_CLAMP;}};
 std::vector<D3D12_STATIC_SAMPLER_DESC> samplers(f.samplers.size());
 for(UINT i=0;i<samplers.size();++i){auto& s=samplers[i];auto sm=f.samplers[i];s.Filter=sm.spatial==Filter::point?D3D12_FILTER_MIN_MAG_MIP_POINT:(sm.mip==Filter::point?D3D12_FILTER_MIN_MAG_LINEAR_MIP_POINT:D3D12_FILTER_MIN_MAG_MIP_LINEAR);s.AddressU=address(sm.u);s.AddressV=address(sm.v);s.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;s.MaxAnisotropy=1;s.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;s.BorderColor=D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;s.MaxLOD=D3D12_FLOAT32_MAX;s.ShaderRegister=i;}
 D3D12_ROOT_SIGNATURE_DESC rd{};rd.NumParameters=2;rd.pParameters=params;rd.NumStaticSamplers=UINT(samplers.size());rd.pStaticSamplers=samplers.data();ComPtr<ID3DBlob> sig,err,shader;check(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&sig,&err),"Sampling root signature");ComPtr<ID3D12RootSignature> root;check(d->CreateRootSignature(0,sig->GetBufferPointer(),sig->GetBufferSize(),IID_PPV_ARGS(&root)),"Sampling root");
 err.Reset();auto hr=D3DCompile(f.hlsl.data(),f.hlsl.size(),"revenge-sampling",nullptr,nullptr,"main","cs_5_1",D3DCOMPILE_ENABLE_STRICTNESS,0,&shader,&err);if(FAILED(hr)&&err)std::cerr<<std::string(static_cast<const char*>(err->GetBufferPointer()),err->GetBufferSize())<<'\n';check(hr,"Sampling shader compile");D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root.Get();pd.CS={shader->GetBufferPointer(),shader->GetBufferSize()};ComPtr<ID3D12PipelineState> pso;check(d->CreateComputePipelineState(&pd,IID_PPV_ARGS(&pso)),"Sampling PSO");
 D3D12_DESCRIPTOR_HEAP_DESC hd{};hd.Type=D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;hd.NumDescriptors=UINT(f.images.size());hd.Flags=D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;ComPtr<ID3D12DescriptorHeap> heap;check(d->CreateDescriptorHeap(&hd,IID_PPV_ARGS(&heap)),"Sampling descriptor heap");
 D3D12_COMMAND_QUEUE_DESC qd{};qd.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;ComPtr<ID3D12CommandQueue> q;check(d->CreateCommandQueue(&qd,IID_PPV_ARGS(&q)),"Sampling queue");ComPtr<ID3D12CommandAllocator> alloc;check(d->CreateCommandAllocator(qd.Type,IID_PPV_ARGS(&alloc)),"Sampling allocator");ComPtr<ID3D12GraphicsCommandList> list;check(d->CreateCommandList(0,qd.Type,alloc.Get(),pso.Get(),IID_PPV_ARGS(&list)),"Sampling list");
 auto format=[](Format x){switch(x){case Format::bc1:return DXGI_FORMAT_BC1_UNORM;case Format::bc3:return DXGI_FORMAT_BC3_UNORM;case Format::bc4:return DXGI_FORMAT_BC4_UNORM;case Format::bc5:return DXGI_FORMAT_BC5_UNORM;default:return DXGI_FORMAT_R8G8B8A8_UNORM;}};
 std::vector<ComPtr<ID3D12Resource>> images,uploads;auto cpu=heap->GetCPUDescriptorHandleForHeapStart();
 for(auto& image:f.images){auto desc=image.descriptor;auto td=texture_desc(desc.width,desc.height,D3D12_RESOURCE_FLAG_NONE);td.Format=format(desc.format);td.MipLevels=UINT16(desc.mips);td.DepthOrArraySize=UINT16(desc.layers);D3D12_FEATURE_DATA_FORMAT_SUPPORT support{td.Format};check(d->CheckFeatureSupport(D3D12_FEATURE_FORMAT_SUPPORT,&support,sizeof(support)),"Sampling format support");if(!(support.Support1&D3D12_FORMAT_SUPPORT1_SHADER_SAMPLE))throw std::runtime_error("Sampling format unsupported");auto tex=texture(d,td,D3D12_RESOURCE_STATE_COPY_DEST);
  auto p=plan(desc);UINT count=UINT(p.subresources.size());std::vector<D3D12_PLACED_SUBRESOURCE_FOOTPRINT> fp(count);std::vector<UINT> rows(count);std::vector<UINT64> rowbytes(count);UINT64 total=0;d->GetCopyableFootprints(&td,0,count,0,fp.data(),rows.data(),rowbytes.data(),&total);if(!total||total>1024*1024)throw std::runtime_error("Sampling footprint invalid");auto upload=buffer(d,total,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);void* mapped=nullptr;D3D12_RANGE empty{0,0};check(upload->Map(0,&empty,&mapped),"Sampling upload map");std::memset(mapped,0,size_t(total));
  for(UINT i=0;i<count;++i){const auto& s=p.subresources[i];if(rows[i]!=s.blocks_y||rowbytes[i]!=s.row_bytes||fp[i].Footprint.RowPitch<s.row_bytes){upload->Unmap(0,&empty);throw std::runtime_error("Sampling footprint mismatch");}for(UINT y=0;y<rows[i];++y)std::memcpy(static_cast<uint8_t*>(mapped)+fp[i].Offset+size_t(y)*fp[i].Footprint.RowPitch,image.upload.data()+s.upload_offset+size_t(y)*s.row_pitch,s.row_bytes);D3D12_TEXTURE_COPY_LOCATION dst{},src{};dst.pResource=tex.Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;dst.SubresourceIndex=i;src.pResource=upload.Get();src.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;src.PlacedFootprint=fp[i];list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);}
  upload->Unmap(0,nullptr);transition(list.Get(),tex.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_NON_PIXEL_SHADER_RESOURCE);D3D12_SHADER_RESOURCE_VIEW_DESC sv{};sv.Format=td.Format;sv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2DARRAY;sv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;sv.Texture2DArray.MipLevels=desc.mips;sv.Texture2DArray.ArraySize=desc.layers;d->CreateShaderResourceView(tex.Get(),&sv,cpu);cpu.ptr+=d->GetDescriptorHandleIncrementSize(hd.Type);images.push_back(tex);uploads.push_back(upload);
 }
 const UINT64 bytes=f.expected.size()*sizeof(std::array<float,4>);D3D12_HEAP_PROPERTIES hp{};hp.Type=D3D12_HEAP_TYPE_DEFAULT;hp.CreationNodeMask=hp.VisibleNodeMask=1;D3D12_RESOURCE_DESC bd{};bd.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;bd.Width=bytes;bd.Height=1;bd.DepthOrArraySize=1;bd.MipLevels=1;bd.SampleDesc.Count=1;bd.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;bd.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;ComPtr<ID3D12Resource> output;check(d->CreateCommittedResource(&hp,D3D12_HEAP_FLAG_NONE,&bd,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&output)),"Sampling output");auto read=buffer(d,bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);ID3D12DescriptorHeap* heaps[]={heap.Get()};list->SetDescriptorHeaps(1,heaps);list->SetComputeRootSignature(root.Get());list->SetComputeRootDescriptorTable(0,heap->GetGPUDescriptorHandleForHeapStart());list->SetComputeRootUnorderedAccessView(1,output->GetGPUVirtualAddress());list->Dispatch(1,1,1);transition(list.Get(),output.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COPY_SOURCE);list->CopyBufferRegion(read.Get(),0,output.Get(),0,bytes);check(list->Close(),"Sampling close");ComPtr<ID3D12Fence> fence;check(d->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"Sampling fence");Handle event;ID3D12CommandList* lists[]={list.Get()};q->ExecuteCommandLists(1,lists);check(q->Signal(fence.Get(),1),"Sampling signal");wait(fence.Get(),1,event.h);
 void* mapped=nullptr;D3D12_RANGE readRange{0,SIZE_T(bytes)},empty{0,0};check(read->Map(0,&readRange,&mapped),"Sampling readback");auto values=static_cast<const float*>(mapped);float worst=0;size_t bad=f.expected.size();for(size_t i=0;i<f.expected.size();++i)for(unsigned c=0;c<4;++c){float diff=std::abs(values[i*4+c]-f.expected[i][c]);if(!std::isfinite(diff)||diff>2.0f/255.0f){if(bad==f.expected.size())bad=i;}if(std::isfinite(diff))worst=std::max(worst,diff);}read->Unmap(0,&empty);if(bad!=f.expected.size())throw std::runtime_error("GPU sampling mismatch at query "+std::to_string(bad));std::cout<<"GPU texture sampling: "<<f.expected.size()<<" samples, five SRVs, 20 subresources, four samplers; maximum absolute error "<<worst<<" (tolerance 2/255). Desktop D3D12 only.\n";return 0;
}

int gpu_test(const std::filesystem::path& cache){
 auto input=fixture_input(),expected=fixture_expected();auto source=fixture_hlsl();
 ComPtr<ID3DBlob> blob,errors;auto hr=D3DCompile(source.data(),source.size(),"revenge-texture-layout-fixture",nullptr,nullptr,"main","cs_5_1",D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&errors);
 if(FAILED(hr)&&errors)std::cerr<<std::string(static_cast<const char*>(errors->GetBufferPointer()),errors->GetBufferSize())<<'\n';check(hr,"Compile texture layout fixture");
 std::cout<<"64x64 RGBA8 bit-pattern fixture: Morton8 tiles to linear rows (not Xbox hardware layout).\n";
 return compute_known_answer(blob->GetBufferPointer(),blob->GetBufferSize(),expected,input,cache);
}
}

#include "revenge/transfers.hpp"
#include "revenge/virtual_memory.hpp"
namespace revenge::transfer {
int gpu_test(unsigned rounds,unsigned slots,unsigned mib){
 if(!rounds||rounds>1024||!slots||slots>8||rounds<slots||!mib||mib>32)throw std::invalid_argument("transfer test: rounds 1..1024 and >= slots, slots 1..8, MiB 1..32");
 const uint64_t bytes=uint64_t(mib)<<20;
 auto g=select();print(g);auto d=g.device.Get();
 ComPtr<IDXGIAdapter3> ad;check(g.adapter.As(&ad),"transfer IDXGIAdapter3");
 DXGI_QUERY_VIDEO_MEMORY_INFO info{};check(ad->QueryVideoMemoryInfo(0,DXGI_MEMORY_SEGMENT_GROUP_LOCAL,&info),"transfer local budget");
 auto free=info.Budget>info.CurrentUsage?info.Budget-info.CurrentUsage:0;
 // Conservative logical payload budget, not a residency guarantee; actual allocations can fail.
 Pool pool(slots,bytes,std::min<uint64_t>(free/2,768ull<<20));
 vm::Arena arena(bytes*slots,bytes*slots);
 struct NativeSlot {ComPtr<ID3D12Resource> up,dev,down;ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;vm::Handle cpu;};
 std::vector<NativeSlot> resources(slots);
 D3D12_COMMAND_QUEUE_DESC qd{};qd.Type=D3D12_COMMAND_LIST_TYPE_COPY;
 ComPtr<ID3D12CommandQueue> queue;check(d->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue)),"Create copy queue");
 ComPtr<ID3D12Fence> fence;check(d->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"Create copy fence");
 Handle event;
 for(auto& s:resources){
  s.cpu=arena.reserve(bytes);arena.commit(s.cpu,0,bytes);
  s.up=buffer(d,bytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
  s.dev=buffer(d,bytes,D3D12_HEAP_TYPE_DEFAULT,D3D12_RESOURCE_STATE_COMMON);
  s.down=buffer(d,bytes,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
  check(d->CreateCommandAllocator(qd.Type,IID_PPV_ARGS(&s.allocator)),"Create copy allocator");
  check(d->CreateCommandList(0,qd.Type,s.allocator.Get(),nullptr,IID_PPV_ARGS(&s.list)),"Create copy list");check(s.list->Close(),"Close initial copy list");
 }
 DrainQueue drain{queue.Get(),fence.Get(),event.h};
 std::vector<uint8_t> scratch(static_cast<size_t>(bytes));
 D3D12_RANGE empty{0,0},range{0,static_cast<SIZE_T>(bytes)};
 uint64_t verified=0,retired=0;unsigned peak_pending=0,pending=0;
 auto consume=[&](unsigned i){
  const auto seq=pool.slot(i).fence;auto& s=resources[i];
  wait(fence.Get(),seq,event.h);pool.complete(fence->GetCompletedValue());
  void* mapped=nullptr;check(s.down->Map(0,&range,&mapped),"Map transfer readback");
  // Return through the same checked host arena (staged copies, not shared GPU virtual addresses).
  try{arena.write(s.cpu,0,{static_cast<const uint8_t*>(mapped),scratch.size()});}catch(...){s.down->Unmap(0,&empty);throw;}
  s.down->Unmap(0,&empty);arena.read(s.cpu,0,scratch);
  if(!matches(scratch,seq))throw std::runtime_error("transfer readback mismatch at submission "+std::to_string(seq));
  verified+=bytes;--pending;
  if(pool.consume(i)){arena.release(s.cpu);s.list.Reset();s.allocator.Reset();s.up.Reset();s.dev.Reset();s.down.Reset();++retired;}
 };
 auto start=std::chrono::steady_clock::now();
 for(unsigned j=0;j<rounds;++j){
  unsigned i=j%slots;auto& s=resources[i];if(pool.slot(i).phase==Phase::submitted)consume(i);
  pool.begin(i);pattern(scratch,uint64_t(j)+1);arena.write(s.cpu,0,scratch);
  void* mapped=nullptr;check(s.up->Map(0,&empty,&mapped),"Map transfer upload");
  try{arena.read(s.cpu,0,{static_cast<uint8_t*>(mapped),scratch.size()});}catch(...){s.up->Unmap(0,&empty);throw;}s.up->Unmap(0,&range);
  check(s.allocator->Reset(),"Reset copy allocator");check(s.list->Reset(s.allocator.Get(),nullptr),"Reset copy list");
  // Buffer state decays to COMMON after each separate ExecuteCommandLists on this copy queue.
  transition(s.list.Get(),s.dev.Get(),D3D12_RESOURCE_STATE_COMMON,D3D12_RESOURCE_STATE_COPY_DEST);
  pool.upload(i);s.list->CopyBufferRegion(s.dev.Get(),0,s.up.Get(),0,bytes);
  transition(s.list.Get(),s.dev.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_COPY_SOURCE);
  pool.download(i);s.list->CopyBufferRegion(s.down.Get(),0,s.dev.Get(),0,bytes);
  check(s.list->Close(),"Close copy list");auto seq=pool.submit(i,j+slots>=rounds);
  ID3D12CommandList* lists[]{s.list.Get()};queue->ExecuteCommandLists(1,lists);drain.last=seq;check(queue->Signal(fence.Get(),seq),"Signal copy fence");
  peak_pending=std::max(peak_pending,++pending);
 }
 for(unsigned i=0;i<slots;++i)consume(i);
 drain.last=0;
 auto seconds=std::chrono::duration<double>(std::chrono::steady_clock::now()-start).count();auto stats=arena.stats();
 if(pool.live_bytes()||stats.reserved||stats.committed||retired!=slots||pending||verified!=bytes*rounds)throw std::runtime_error("transfer lifecycle accounting mismatch");
 std::cout<<"Dedicated D3D12 copy queue; verified "<<verified<<" round-trip payload bytes across "<<rounds<<" submissions.\n"
 <<"Peak outstanding (submitted, not yet consumed): "<<peak_pending<<"; slots="<<slots<<"\n"
 <<"Retired "<<retired<<" buffer triplets after GPU fence and readback verification.\n"
 <<"Logical GPU buffer payload bytes remaining: "<<pool.live_bytes()<<"; host committed bytes remaining: "<<stats.committed<<"\n"
 <<"End-to-end verified payload MiB/s: "<<double(verified)/1048576.0/seconds<<"; elapsed ms: "<<seconds*1000<<"\n"
 <<"Includes pattern generation, host staging, upload, readback and verification. Not raw bandwidth, zero-copy or Xbox ABI.\n";
 return 0;
}
}

#include "revenge/presentation.hpp"
#include <iomanip>
namespace revenge::presentation { namespace {
struct FrameTimer {
 HANDLE timer=nullptr;bool high=false;
 explicit FrameTimer(bool enabled){if(!enabled)return;timer=CreateWaitableTimerExW(nullptr,nullptr,0x2u,TIMER_MODIFY_STATE|SYNCHRONIZE);high=timer!=nullptr;if(!timer)timer=CreateWaitableTimerExW(nullptr,nullptr,0,TIMER_MODIFY_STATE|SYNCHRONIZE);}
 ~FrameTimer(){if(timer)CloseHandle(timer);}
 void pace(std::chrono::steady_clock::time_point deadline){using Clock=std::chrono::steady_clock;using namespace std::chrono;auto remaining=deadline-Clock::now();if(remaining<=Clock::duration::zero())return;auto coarse=duration_cast<nanoseconds>(remaining)-microseconds(250);if(coarse.count()>0){if(timer){LARGE_INTEGER due{};due.QuadPart=-std::max<int64_t>(1,coarse.count()/100);if(!SetWaitableTimer(timer,&due,0,nullptr,nullptr,FALSE)||WaitForSingleObject(timer,2000)!=WAIT_OBJECT_0)std::this_thread::sleep_until(deadline);}else std::this_thread::sleep_until(deadline);}while(Clock::now()<deadline)std::this_thread::yield();}
};
constexpr wchar_t class_name[]=L"RevengePresentFixture017";
struct Window {
 HWND hwnd=nullptr; HINSTANCE instance=GetModuleHandleW(nullptr); bool stop=false; const char* reason="window close"; bool registered=false;
 static LRESULT CALLBACK proc(HWND hwnd,UINT msg,WPARAM wp,LPARAM lp){
  if(msg==WM_NCCREATE){auto cs=reinterpret_cast<CREATESTRUCTW*>(lp);SetWindowLongPtrW(hwnd,GWLP_USERDATA,reinterpret_cast<LONG_PTR>(cs->lpCreateParams));}
  auto self=reinterpret_cast<Window*>(GetWindowLongPtrW(hwnd,GWLP_USERDATA));
  if(self&&(msg==WM_CLOSE||(msg==WM_KEYDOWN&&wp==VK_ESCAPE))){self->stop=true;self->reason=(msg==WM_CLOSE?"window close":"Escape key");return 0;}
  if(msg==WM_ERASEBKGND)return 1;
  return DefWindowProcW(hwnd,msg,wp,lp);
 }
 Window(unsigned w,unsigned h){
  WNDCLASSW wc{};wc.lpfnWndProc=proc;wc.hInstance=instance;wc.lpszClassName=class_name;wc.hCursor=LoadCursorW(nullptr,MAKEINTRESOURCEW(32512));
  if(!RegisterClassW(&wc))throw std::runtime_error("RegisterClassW failed");registered=true;
  RECT r{0,0,LONG(w),LONG(h)};if(!AdjustWindowRect(&r,WS_OVERLAPPEDWINDOW,FALSE)){UnregisterClassW(class_name,instance);throw std::runtime_error("AdjustWindowRect failed");}
  hwnd=CreateWindowExW(0,class_name,L"Revenge | Initialising desktop fixture",WS_OVERLAPPEDWINDOW,CW_USEDEFAULT,CW_USEDEFAULT,r.right-r.left,r.bottom-r.top,nullptr,nullptr,instance,this);
  if(!hwnd){UnregisterClassW(class_name,instance);throw std::runtime_error("CreateWindowExW failed");}
 }
 ~Window(){if(hwnd)DestroyWindow(hwnd);if(registered)UnregisterClassW(class_name,instance);}
 void pump(){MSG m{};while(PeekMessageW(&m,nullptr,0,0,PM_REMOVE)){if(m.message==WM_QUIT){stop=true;reason="WM_QUIT";break;}TranslateMessage(&m);DispatchMessageW(&m);}}
};
constexpr char present_shader[]=R"(
cbuffer Motion : register(b0) { float4 motion; };
struct Vertex { float4 position : SV_Position; float3 color : COLOR0; };
Vertex vertex(uint id : SV_VertexID) {
    float2 p[3] = {float2(0,0.68),float2(-0.59,-0.34),float2(0.59,-0.34)};
    float3 c[3] = {float3(0.10,0.65,1),float3(0.22,0.93,0.72),float3(0.78,0.28,1)};
    float2 v=p[id]; Vertex o;
    o.position=float4(float2(v.x*motion.x-v.y*motion.y,v.x*motion.y+v.y*motion.x)*motion.zw,0,1);
    o.color=c[id];return o;
}
float4 pixel(Vertex i) : SV_Target { return float4(i.color,1); }
)";
ComPtr<ID3DBlob> compile_present(const char* entry,const char* target){ComPtr<ID3DBlob> blob,err;auto hr=D3DCompile(present_shader,sizeof(present_shader)-1,"revenge-present-fixture",nullptr,nullptr,entry,target,D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&err);if(FAILED(hr)&&err)std::cerr<<std::string(static_cast<const char*>(err->GetBufferPointer()),err->GetBufferSize())<<'\n';check(hr,"Compile presentation shader");return blob;}
}
int run(unsigned seconds,unsigned width,unsigned height,bool vsync,bool fixed,unsigned fps_cap){
 if(seconds<1||seconds>120||width<320||width>3840||height<240||height>2160||fps_cap>1000)throw std::invalid_argument("invalid presentation options");
 auto g=select();print(g);auto d=g.device.Get();Window window(width,height);
 ComPtr<IDXGIFactory6> factory;check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)),"Create presentation factory");
 BOOL tearing=FALSE;ComPtr<IDXGIFactory5> f5;if(SUCCEEDED(factory.As(&f5)))if(FAILED(f5->CheckFeatureSupport(DXGI_FEATURE_PRESENT_ALLOW_TEARING,&tearing,sizeof(tearing))))tearing=FALSE;
 const UINT flags=(!vsync&&tearing)?DXGI_SWAP_CHAIN_FLAG_ALLOW_TEARING:0;
 D3D12_COMMAND_QUEUE_DESC qd{};qd.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;ComPtr<ID3D12CommandQueue> queue;check(d->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue)),"Create present queue");
 constexpr unsigned count=3;DXGI_SWAP_CHAIN_DESC1 sd{};sd.Width=width;sd.Height=height;sd.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.SampleDesc.Count=1;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.BufferCount=count;sd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;sd.Scaling=DXGI_SCALING_STRETCH;sd.Flags=flags;
 ComPtr<IDXGISwapChain1> base;check(factory->CreateSwapChainForHwnd(queue.Get(),window.hwnd,&sd,nullptr,nullptr,&base),"CreateSwapChainForHwnd");check(factory->MakeWindowAssociation(window.hwnd,DXGI_MWA_NO_ALT_ENTER),"MakeWindowAssociation");ComPtr<IDXGISwapChain3> swap;check(base.As(&swap),"IDXGISwapChain3");
 auto vs=compile_present("vertex","vs_5_1"),ps=compile_present("pixel","ps_5_1");D3D12_ROOT_PARAMETER param{};param.ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;param.Constants.Num32BitValues=4;param.ShaderVisibility=D3D12_SHADER_VISIBILITY_VERTEX;
 D3D12_ROOT_SIGNATURE_DESC rs{};rs.NumParameters=1;rs.pParameters=&param;ComPtr<ID3DBlob> sig,err;check(D3D12SerializeRootSignature(&rs,D3D_ROOT_SIGNATURE_VERSION_1,&sig,&err),"Serialize present root signature");ComPtr<ID3D12RootSignature> root;check(d->CreateRootSignature(0,sig->GetBufferPointer(),sig->GetBufferSize(),IID_PPV_ARGS(&root)),"Create present root signature");
 D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root.Get();pd.VS={vs->GetBufferPointer(),vs->GetBufferSize()};pd.PS={ps->GetBufferPointer(),ps->GetBufferSize()};pd.SampleMask=UINT_MAX;pd.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;pd.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;pd.RasterizerState.DepthClipEnable=TRUE;
 auto& blend=pd.BlendState.RenderTarget[0];blend.SrcBlend=D3D12_BLEND_ONE;blend.DestBlend=D3D12_BLEND_ZERO;blend.BlendOp=D3D12_BLEND_OP_ADD;blend.SrcBlendAlpha=D3D12_BLEND_ONE;blend.DestBlendAlpha=D3D12_BLEND_ZERO;blend.BlendOpAlpha=D3D12_BLEND_OP_ADD;blend.LogicOp=D3D12_LOGIC_OP_NOOP;blend.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
 pd.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ZERO;pd.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_ALWAYS;pd.DepthStencilState.FrontFace={D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_COMPARISON_FUNC_ALWAYS};pd.DepthStencilState.BackFace=pd.DepthStencilState.FrontFace;pd.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;pd.NumRenderTargets=1;pd.RTVFormats[0]=sd.Format;pd.SampleDesc.Count=1;
 ComPtr<ID3D12PipelineState> pso;check(d->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&pso)),"Create present PSO");auto heap=descriptors(d,D3D12_DESCRIPTOR_HEAP_TYPE_RTV,count,false);auto step=d->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);
 std::array<RenderSlot,count> slots;for(auto& s:slots){check(d->CreateCommandAllocator(qd.Type,IID_PPV_ARGS(&s.allocator)),"Create present allocator");check(d->CreateCommandList(0,qd.Type,s.allocator.Get(),pso.Get(),IID_PPV_ARGS(&s.list)),"Create present list");check(s.list->Close(),"Close initial present list");}
 auto targets=[&]{for(unsigned i=0;i<count;++i){auto& s=slots[i];check(swap->GetBuffer(i,IID_PPV_ARGS(&s.target)),"Get swap buffer");s.rtv=heap->GetCPUDescriptorHandleForHeapStart();s.rtv.ptr+=SIZE_T(i)*step;d->CreateRenderTargetView(s.target.Get(),nullptr,s.rtv);}};targets();
 ComPtr<ID3D12Fence> fence;check(d->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"Create present fence");Handle event;BackBuffers states(count);
 UINT64 frequency=0;check(queue->GetTimestampFrequency(&frequency),"Present timestamp frequency");if(!frequency)throw std::runtime_error("zero timestamp frequency");D3D12_QUERY_HEAP_DESC qh{};qh.Type=D3D12_QUERY_HEAP_TYPE_TIMESTAMP;qh.Count=count*2;ComPtr<ID3D12QueryHeap> queries;check(d->CreateQueryHeap(&qh,IID_PPV_ARGS(&queries)),"Create present queries");auto read=buffer(d,count*16,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
 DrainQueue drain{queue.Get(),fence.Get(),event.h};std::array<bool,count> pending{};std::vector<double> gpu_ms,cpu_ms;gpu_ms.reserve(10000);cpu_ms.reserve(10000);
 auto collect=[&](unsigned i){if(!pending[i])return;void* p=nullptr;D3D12_RANGE rr{SIZE_T(i)*16,SIZE_T(i+1)*16},empty{0,0};check(read->Map(0,&rr,&p),"Map present timestamps");uint64_t ticks[2];std::memcpy(ticks,static_cast<const char*>(p)+i*16,16);read->Unmap(0,&empty);if(ticks[1]<ticks[0])throw std::runtime_error("invalid present GPU timestamps");gpu_ms.push_back(double(ticks[1]-ticks[0])*1000/double(frequency));pending[i]=false;};
 auto flush=[&]{if(states.last())wait(fence.Get(),states.last(),event.h);for(unsigned i=0;i<count;++i)collect(i);};
 FrameTimer pacing(fps_cap!=0);if(fps_cap)std::cout<<"Frame pacing: "<<(pacing.high?"high-resolution waitable timer":pacing.timer?"standard waitable timer fallback":"sleep fallback")<<"; short yield tail.\n";
 ShowWindow(window.hwnd,SW_SHOW);using Clock=std::chrono::steady_clock;auto begin=Clock::now(),last_title=begin;unsigned presented=0,resizes=0,title_frames=0;uint64_t skipped=0;
 while(!window.stop&&std::chrono::duration<double>(Clock::now()-begin).count()<seconds&&states.last()<1000000){
  window.pump();if(window.stop)break;RECT client{};if(!GetClientRect(window.hwnd,&client))throw std::runtime_error("GetClientRect failed");unsigned w=unsigned(client.right-client.left),h=unsigned(client.bottom-client.top);
  if(IsIconic(window.hwnd)||!w||!h||w>16384||h>16384){Sleep(25);continue;}
  if(!fixed&&(w!=width||h!=height)){flush();states.resized(states.last());for(auto& s:slots){check(s.allocator->Reset(),"Reset resize allocator");check(s.list->Reset(s.allocator.Get(),nullptr),"Reset resize list");check(s.list->Close(),"Close resize list");s.target.Reset();}check(swap->ResizeBuffers(count,w,h,sd.Format,flags),"ResizeBuffers");width=w;height=h;targets();++resizes;}
  auto frame_begin=Clock::now();auto index=swap->GetCurrentBackBufferIndex();if(index>=count)throw std::runtime_error("invalid swap-chain index");
  if(states.last()>=count-1)wait(fence.Get(),states.last()-(count-2),event.h);if(auto needed=states.required(index))wait(fence.Get(),needed,event.h);collect(index);states.acquire(index,fence->GetCompletedValue());
  auto& s=slots[index];check(s.allocator->Reset(),"Reset present allocator");check(s.list->Reset(s.allocator.Get(),pso.Get()),"Reset present list");auto l=s.list.Get();
  auto motion=scene(std::chrono::duration<double>(Clock::now()-begin).count(),width,height);D3D12_VIEWPORT vp{0,0,float(width),float(height),0,1};D3D12_RECT sc{0,0,LONG(width),LONG(height)};const float clear[]{0.018f,0.025f,0.045f,1};
  l->EndQuery(queries.Get(),D3D12_QUERY_TYPE_TIMESTAMP,index*2);transition(l,s.target.Get(),D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_RENDER_TARGET);l->SetGraphicsRootSignature(root.Get());l->SetGraphicsRoot32BitConstants(0,4,motion.data(),0);l->RSSetViewports(1,&vp);l->RSSetScissorRects(1,&sc);l->OMSetRenderTargets(1,&s.rtv,FALSE,nullptr);l->ClearRenderTargetView(s.rtv,clear,0,nullptr);l->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);l->DrawInstanced(3,1,0,0);transition(l,s.target.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PRESENT);l->EndQuery(queries.Get(),D3D12_QUERY_TYPE_TIMESTAMP,index*2+1);l->ResolveQueryData(queries.Get(),D3D12_QUERY_TYPE_TIMESTAMP,index*2,2,read.Get(),UINT64(index)*16);check(l->Close(),"Close present list");
  auto seq=states.submit();drain.last=seq;ID3D12CommandList* lists[]{l};queue->ExecuteCommandLists(1,lists);auto hr=swap->Present(vsync?1:0,flags?DXGI_PRESENT_ALLOW_TEARING:0);check(queue->Signal(fence.Get(),seq),"Signal present fence");pending[index]=true;check(hr,"Present");if(hr==S_OK){++presented;++title_frames;}else{++skipped;Sleep(25);}if(fps_cap){auto next=frame_begin+std::chrono::duration_cast<Clock::duration>(std::chrono::duration<double>(1.0/fps_cap));pacing.pace(next);}auto now=Clock::now();cpu_ms.push_back(std::chrono::duration<double,std::milli>(now-frame_begin).count());double interval=std::chrono::duration<double>(now-last_title).count();if(interval>=0.5){std::wostringstream title;title<<L"Revenge | Desktop fixture | "<<width<<L"x"<<height<<L" | "<<std::fixed<<std::setprecision(1)<<title_frames/interval<<L" Present/s | VSync "<<(vsync?L"on":L"off")<<L" | Esc to close";SetWindowTextW(window.hwnd,title.str().c_str());last_title=now;title_frames=0;}
 }
 auto elapsed=std::chrono::duration<double>(Clock::now()-begin).count();flush();drain.last=0;
 std::cout<<"DXGI desktop fixture; successful Present calls: "<<presented<<"; other successful status: "<<skipped<<"; submitted: "<<states.last()<<"; resizes: "<<resizes<<"\n";
 RECT final_client{};GetClientRect(window.hwnd,&final_client);
 std::cout<<"Final client extent: "<<final_client.right-final_client.left<<'x'<<final_client.bottom-final_client.top<<"; render extent: "<<width<<'x'<<height<<"; fixed: "<<fixed<<"; VSync: "<<vsync<<"; allow tearing: "<<(flags!=0)<<"; FPS cap: "<<fps_cap<<"\n";
 std::cout<<"Requested seconds: "<<seconds<<"; exit reason: "<<(window.stop?window.reason:(states.last()>=1000000?"submission safety limit":"duration reached"))<<"\n";
 std::cout<<"Wall time seconds: "<<elapsed<<"; successful Present calls/s: "<<(elapsed>0?presented/elapsed:0)<<"\n";
 if(!cpu_ms.empty()){auto s=frame_stats(cpu_ms);std::cout<<"CPU frame loop ms (wait, record, Present, optional cap): mean="<<s.mean_ms<<" median="<<s.median_ms<<" p95="<<s.p95_ms<<'\n';}
 if(!gpu_ms.empty()){auto s=frame_stats(gpu_ms);std::cout<<"GPU draw interval ms: mean="<<s.mean_ms<<" median="<<s.median_ms<<" p95="<<s.p95_ms<<"; samples="<<gpu_ms.size()<<'\n';}
 std::cout<<"Queue drained; synthetic desktop scene, no pixel oracle, not scanout FPS or Xbox gameplay.\n";return presented?0:2;
}
}

namespace revenge::scene3d { namespace {
constexpr char scene_shader[]=R"(
Texture2D<float4> tex : register(t0);
SamplerState pointSampler : register(s0);
struct V {float4 p:SV_Position; float2 uv:TEXCOORD0;};
V vertex(float3 p:POSITION,float2 uv:TEXCOORD0) {
 V o; o.p=float4(p.xy,(4.0*p.z-2.0)/3.5,p.z);o.uv=uv;return o;
}
float4 pixel(V i):SV_Target {return tex.SampleLevel(pointSampler,i.uv,0);}
)";
ComPtr<ID3DBlob> compile_scene(const char* entry,const char* profile,bool translated,const package::Programs* programs){
 auto kind=std::string_view(entry)=="vertex"?stage::Stage::vertex:stage::Stage::pixel;
 std::string source=translated?stage::emit(kind,programs?(kind==stage::Stage::vertex?programs->vertex:programs->pixel):stage::fixture(kind)):scene_shader;
 ComPtr<ID3DBlob> code,err;auto hr=D3DCompile(source.data(),source.size(),"revenge-scene",nullptr,nullptr,entry,profile,D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&err);
 if(FAILED(hr)&&err)std::cerr<<std::string(static_cast<const char*>(err->GetBufferPointer()),err->GetBufferSize())<<'\n';check(hr,"Compile scene shader");return code;
}
struct GeneratedTexture {
 ComPtr<ID3D12Resource> data;ComPtr<ID3D12RootSignature> root;ComPtr<ID3D12PipelineState> pso;
 ComPtr<ID3D12CommandQueue> queue;ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;
 ComPtr<ID3D12Fence> fence;Handle event;bool submitted=false;
 ~GeneratedTexture(){if(submitted&&queue&&fence&&SUCCEEDED(queue->Signal(fence.Get(),2))&&SUCCEEDED(fence->SetEventOnCompletion(2,event.h)))WaitForSingleObject(event.h,30000);}
 void prepare(ID3D12Device* d,const D3D12_PLACED_SUBRESOURCE_FOOTPRINT& fp,UINT64 bytes,std::span<const uint32_t> words){
  if(fp.Offset||fp.Footprint.Width!=2||fp.Footprint.Height!=2||fp.Footprint.Format!=DXGI_FORMAT_R8G8B8A8_UNORM)throw std::runtime_error("unexpected generated texture footprint");
  auto source=resources::texture_compute_hlsl(fp.Footprint.RowPitch,words);ComPtr<ID3DBlob> code,err;
  auto hr=D3DCompile(source.data(),source.size(),"revenge-generated-texture",nullptr,nullptr,"main","cs_5_1",D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&code,&err);
  if(FAILED(hr)&&err)std::cerr<<std::string(static_cast<const char*>(err->GetBufferPointer()),err->GetBufferSize())<<'\n';check(hr,"Compile translated texture compute shader");
  D3D12_ROOT_PARAMETER param{};param.ParameterType=D3D12_ROOT_PARAMETER_TYPE_UAV;param.ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
  D3D12_ROOT_SIGNATURE_DESC desc{};desc.NumParameters=1;desc.pParameters=&param;ComPtr<ID3DBlob> sig;
  check(D3D12SerializeRootSignature(&desc,D3D_ROOT_SIGNATURE_VERSION_1,&sig,&err),"Serialize texture compute signature");check(d->CreateRootSignature(0,sig->GetBufferPointer(),sig->GetBufferSize(),IID_PPV_ARGS(&root)),"Create texture compute signature");
  D3D12_COMPUTE_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root.Get();pd.CS={code->GetBufferPointer(),code->GetBufferSize()};check(d->CreateComputePipelineState(&pd,IID_PPV_ARGS(&pso)),"Create translated texture PSO");
  D3D12_HEAP_PROPERTIES heap{};heap.Type=D3D12_HEAP_TYPE_DEFAULT;heap.CreationNodeMask=heap.VisibleNodeMask=1;
  D3D12_RESOURCE_DESC rd{};rd.Dimension=D3D12_RESOURCE_DIMENSION_BUFFER;rd.Width=std::max<UINT64>(bytes,UINT64(fp.Footprint.RowPitch)*2);rd.Height=1;rd.DepthOrArraySize=1;rd.MipLevels=1;rd.SampleDesc.Count=1;rd.Layout=D3D12_TEXTURE_LAYOUT_ROW_MAJOR;rd.Flags=D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;
  check(d->CreateCommittedResource(&heap,D3D12_HEAP_FLAG_NONE,&rd,D3D12_RESOURCE_STATE_UNORDERED_ACCESS,nullptr,IID_PPV_ARGS(&data)),"Create generated texture buffer");
  D3D12_COMMAND_QUEUE_DESC qd{};qd.Type=D3D12_COMMAND_LIST_TYPE_COMPUTE;check(d->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue)),"Create texture compute queue");check(d->CreateCommandAllocator(qd.Type,IID_PPV_ARGS(&allocator)),"Create texture compute allocator");check(d->CreateCommandList(0,qd.Type,allocator.Get(),pso.Get(),IID_PPV_ARGS(&list)),"Create texture compute list");check(d->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"Create texture compute fence");
  list->SetComputeRootSignature(root.Get());list->SetComputeRootUnorderedAccessView(0,data->GetGPUVirtualAddress());list->Dispatch(1,1,1);
  transition(list.Get(),data.Get(),D3D12_RESOURCE_STATE_UNORDERED_ACCESS,D3D12_RESOURCE_STATE_COMMON);check(list->Close(),"Close texture compute list");
  ID3D12CommandList* lists[]{list.Get()};queue->ExecuteCommandLists(1,lists);submitted=true;check(queue->Signal(fence.Get(),1),"Signal texture producer fence");
 }
};

}
struct CustomSlot {
 ComPtr<ID3D12Resource> vb,ib,upload,tex,color,depth,read;
 ComPtr<ID3D12DescriptorHeap> rtv,dsv,srv;
 ComPtr<ID3D12CommandAllocator> allocator;ComPtr<ID3D12GraphicsCommandList> list;
 unsigned w=0,h=0,tw=0,th=0;uint64_t vbbytes=0,ibbytes=0,seq=0,rb=0;
 D3D12_PLACED_SUBRESOURCE_FOOTPRINT rf{};bool initialized=false;
};
struct CustomDevice {
 Gpu gpu;ComPtr<ID3D12RootSignature> root;ComPtr<ID3D12PipelineState> pso;ComPtr<ID3D12CommandQueue> queue;
 ComPtr<ID3D12Fence> fence;Handle event;std::array<CustomSlot,2> slots;uint64_t last=0,allocated=0,reused=0,collected=0;unsigned peak_pending=0;
 ~CustomDevice(){
  // Drain before releasing any slot-owned resource on an error path. Never reuse this context after failure.
  if(queue&&fence&&last&&collected!=last){auto end=last+1;if(SUCCEEDED(queue->Signal(fence.Get(),end))){try{Handle drain_event;wait(fence.Get(),end,drain_event.h);}catch(...){}}}
  if(last)std::cout<<"GPU slot summary: submitted "<<last<<"; collected "<<collected<<"; allocation sets "<<allocated<<"; reuses "<<reused<<"; peak submitted-uncollected "<<peak_pending<<".\n";
 }
};
static void submit_custom(CustomDevice& dev,unsigned index,unsigned w,unsigned h,const Geometry& geo,std::span<const Command> stream,const CustomScene& custom){
 if(w>512||h>512)throw std::invalid_argument("custom slot extent");
 auto d=dev.gpu.device.Get();auto& s=dev.slots.at(index);if(s.seq)throw std::logic_error("slot not collected");
 auto tw=custom.texture_width,th=custom.texture_height;uint64_t vbbytes=geo.vertices.size()*sizeof(Vertex),ibbytes=geo.indices.size()*sizeof(uint16_t);
 if(vbbytes>1048576||ibbytes>1048576)throw std::invalid_argument("custom slot buffer budget");
 if(!dev.queue){D3D12_COMMAND_QUEUE_DESC qd{};qd.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;check(d->CreateCommandQueue(&qd,IID_PPV_ARGS(&dev.queue)),"Create slot queue");}
 if(!dev.fence)check(d->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&dev.fence)),"Create slot fence");
 // Each slot is independently backed and is changed only after its previous fence and readback.
 auto td=texture_desc(tw,th,D3D12_RESOURCE_FLAG_NONE);auto cd=texture_desc(w,h,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);
 auto dd=texture_desc(w,h,D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);dd.Format=DXGI_FORMAT_D32_FLOAT;
 D3D12_CLEAR_VALUE clear{};clear.Format=cd.Format;clear.Color[3]=1;D3D12_CLEAR_VALUE dc{};dc.Format=dd.Format;dc.DepthStencil.Depth=1;
 D3D12_PLACED_SUBRESOURCE_FOOTPRINT tf{};UINT64 ub=0;d->GetCopyableFootprints(&td,0,1,0,&tf,nullptr,nullptr,&ub);
 const bool reuse=s.initialized&&s.w==w&&s.h==h&&s.tw==tw&&s.th==th&&s.vbbytes==vbbytes&&s.ibbytes==ibbytes;
 if(!reuse){
  s=CustomSlot{};s.w=w;s.h=h;s.tw=tw;s.th=th;s.vbbytes=vbbytes;s.ibbytes=ibbytes;
  s.vb=buffer(d,vbbytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);s.ib=buffer(d,ibbytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
  s.upload=buffer(d,ub,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);s.tex=texture(d,td,D3D12_RESOURCE_STATE_COPY_DEST);
  s.color=texture(d,cd,D3D12_RESOURCE_STATE_RENDER_TARGET,&clear);s.depth=texture(d,dd,D3D12_RESOURCE_STATE_DEPTH_WRITE,&dc);
  UINT64 rb=0;d->GetCopyableFootprints(&cd,0,1,0,&s.rf,nullptr,nullptr,&rb);s.rb=rb;s.read=buffer(d,rb,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
  s.rtv=descriptors(d,D3D12_DESCRIPTOR_HEAP_TYPE_RTV,1,false);s.dsv=descriptors(d,D3D12_DESCRIPTOR_HEAP_TYPE_DSV,1,false);s.srv=descriptors(d,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,1,true);
  d->CreateRenderTargetView(s.color.Get(),nullptr,s.rtv->GetCPUDescriptorHandleForHeapStart());D3D12_DEPTH_STENCIL_VIEW_DESC dv{};dv.Format=dd.Format;dv.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2D;d->CreateDepthStencilView(s.depth.Get(),&dv,s.dsv->GetCPUDescriptorHandleForHeapStart());
  D3D12_SHADER_RESOURCE_VIEW_DESC sv{};sv.Format=td.Format;sv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;sv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;sv.Texture2D.MipLevels=1;d->CreateShaderResourceView(s.tex.Get(),&sv,s.srv->GetCPUDescriptorHandleForHeapStart());
  check(d->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&s.allocator)),"Create slot allocator");check(d->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,s.allocator.Get(),dev.pso.Get(),IID_PPV_ARGS(&s.list)),"Create slot list");
 }else{check(s.allocator->Reset(),"Reset retired slot allocator");check(s.list->Reset(s.allocator.Get(),dev.pso.Get()),"Reset retired slot list");}
 D3D12_RANGE empty{0,0};void* ptr=nullptr;
 check(s.vb->Map(0,&empty,&ptr),"Map slot vertices");std::memcpy(ptr,geo.vertices.data(),size_t(vbbytes));s.vb->Unmap(0,nullptr);
 check(s.ib->Map(0,&empty,&ptr),"Map slot indices");std::memcpy(ptr,geo.indices.data(),size_t(ibbytes));s.ib->Unmap(0,nullptr);
 check(s.upload->Map(0,&empty,&ptr),"Map slot texture");for(unsigned y=0;y<th;++y)std::memcpy(static_cast<uint8_t*>(ptr)+tf.Offset+size_t(y)*tf.Footprint.RowPitch,custom.rgba.data()+size_t(y)*tw*4,size_t(tw)*4);s.upload->Unmap(0,nullptr);
 auto l=s.list.Get();if(reuse){transition(l,s.tex.Get(),D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE,D3D12_RESOURCE_STATE_COPY_DEST);transition(l,s.color.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);}
 auto dst=location(s.tex.Get()),src=location(s.upload.Get(),tf);l->CopyTextureRegion(&dst,0,0,0,&src,nullptr);transition(l,s.tex.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
 D3D12_VIEWPORT vp{0,0,float(w),float(h),0,1};D3D12_RECT sc{0,0,LONG(w),LONG(h)};
 D3D12_VERTEX_BUFFER_VIEW vv{s.vb->GetGPUVirtualAddress(),UINT(vbbytes),UINT(sizeof(Vertex))};D3D12_INDEX_BUFFER_VIEW iv{s.ib->GetGPUVirtualAddress(),UINT(ibbytes),DXGI_FORMAT_R16_UINT};
 auto rh=s.rtv->GetCPUDescriptorHandleForHeapStart(),dh=s.dsv->GetCPUDescriptorHandleForHeapStart();
 for(auto c:stream)switch(c.op){
 case Op::begin:l->SetGraphicsRootSignature(dev.root.Get());l->RSSetViewports(1,&vp);l->RSSetScissorRects(1,&sc);l->OMSetRenderTargets(1,&rh,FALSE,&dh);l->ClearRenderTargetView(rh,clear.Color,0,nullptr);l->ClearDepthStencilView(dh,D3D12_CLEAR_FLAG_DEPTH,1,0,0,nullptr);break;
 case Op::bind_mesh:l->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);l->IASetVertexBuffers(0,1,&vv);l->IASetIndexBuffer(&iv);break;
 case Op::bind_texture:{ID3D12DescriptorHeap* heaps[]{s.srv.Get()};l->SetDescriptorHeaps(1,heaps);l->SetGraphicsRootDescriptorTable(0,s.srv->GetGPUDescriptorHandleForHeapStart());break;}
 case Op::draw_indexed:l->DrawIndexedInstanced(c.count,1,c.first,0,0);break;
 case Op::end:{transition(l,s.color.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE);auto dest=location(s.read.Get(),s.rf),source=location(s.color.Get());l->CopyTextureRegion(&dest,0,0,0,&source,nullptr);break;}
 }
 check(l->Close(),"Close slot list");if(dev.last>=UINT64_MAX-2)throw std::overflow_error("slot fence exhausted");s.seq=++dev.last;ID3D12CommandList* lists[]{l};dev.queue->ExecuteCommandLists(1,lists);check(dev.queue->Signal(dev.fence.Get(),s.seq),"Signal slot fence");s.initialized=true;if(reuse)++dev.reused;else ++dev.allocated;
 unsigned pending=0;for(const auto& slot:dev.slots)if(slot.seq)++pending;dev.peak_pending=std::max(dev.peak_pending,pending);
 if(!custom.quiet)std::cout<<"GPU slot "<<index<<": "<<(reuse?"reused allocations and allocator":"allocated resources")<<"; submitted fence "<<s.seq<<".\n";
}
void collect_custom(CustomDevice& dev,unsigned index,CustomScene& custom){
 auto& s=dev.slots.at(index);if(!s.seq)throw std::logic_error("slot not submitted");wait(dev.fence.Get(),s.seq,dev.event.h);
 custom.framebuffer.resize(size_t(s.w)*s.h*4);D3D12_RANGE rr{0,SIZE_T(s.rb)},empty{0,0};void* ptr=nullptr;check(s.read->Map(0,&rr,&ptr),"Map retired slot readback");
 for(unsigned y=0;y<s.h;++y)std::memcpy(custom.framebuffer.data()+size_t(y)*s.w*4,static_cast<const uint8_t*>(ptr)+s.rf.Offset+size_t(y)*s.rf.Footprint.RowPitch,size_t(s.w)*4);s.read->Unmap(0,&empty);
 if(!custom.quiet)std::cout<<"GPU slot "<<index<<": collected fence "<<s.seq<<".\n";s.seq=0;++dev.collected;
}
std::shared_ptr<CustomDevice> create_custom_device(){auto c=std::make_shared<CustomDevice>();c->gpu=select();print(c->gpu);return c;}
int gpu_test(unsigned w,unsigned h,const std::filesystem::path& output,bool integrated,bool translated,const package::Programs* programs,const std::filesystem::path& cache,unsigned present_seconds,size_t* verified,Continuous* continuous,CustomScene* custom,CustomDevice* device,unsigned async_slot){
 if(verified)*verified=0;
 if(continuous&&(!integrated||!translated||present_seconds||continuous->seconds<1||continuous->seconds>30||continuous->fps_cap>240))throw std::invalid_argument("invalid continuous scene options");
 if(present_seconds>30)throw std::invalid_argument("invalid scene presentation duration");
 if(custom&&(continuous||integrated||!translated||present_seconds||!programs))throw std::invalid_argument("invalid custom scene options");
 if(custom&&(!custom->texture_width||!custom->texture_height||custom->texture_width>512||custom->texture_height>512||custom->rgba.size()!=size_t(custom->texture_width)*custom->texture_height*4))throw std::invalid_argument("invalid custom texture");
 if(programs){if(custom)package::require_custom(*programs);else package::require_scene_fixture(*programs);if((!integrated&&!custom)||!translated)throw std::invalid_argument("package requires all three translated stages");}
 if(w<320||h<240||w>3840||h>2160)throw std::invalid_argument("invalid scene extent");
 auto geo=custom?custom->mesh:geometry();auto stream=custom?custom->stream:commands();validate(geo,stream);if(device&&(!custom||!cache.empty()))throw std::invalid_argument("persistent device requires custom scene without disk cache");
 auto g=device?device->gpu:select();if(!device)print(g);auto d=g.device.Get();
 ComPtr<ID3D12RootSignature> root;ComPtr<ID3D12PipelineState> pso;std::vector<uint8_t> key,cached;bool hit=false;
 if(device&&device->pso){root=device->root;pso=device->pso;if(!custom||!custom->quiet)std::cout<<"Async scene: reused translated pipeline.\n";}else{
 auto vs=compile_scene("vertex","vs_5_1",translated,programs),ps=compile_scene("pixel","ps_5_1",translated,programs);if(translated)std::cout<<"Compiled translated RDNA2 vertex and pixel HLSL (vs_5_1, ps_5_1).\n";
 D3D12_DESCRIPTOR_RANGE range{};range.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV;range.NumDescriptors=1;range.OffsetInDescriptorsFromTableStart=0;
 D3D12_ROOT_PARAMETER param{};param.ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;param.DescriptorTable.NumDescriptorRanges=1;param.DescriptorTable.pDescriptorRanges=&range;param.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
 D3D12_STATIC_SAMPLER_DESC sampler{};sampler.Filter=D3D12_FILTER_MIN_MAG_MIP_POINT;sampler.AddressU=sampler.AddressV=sampler.AddressW=D3D12_TEXTURE_ADDRESS_MODE_CLAMP;sampler.MaxAnisotropy=1;sampler.ComparisonFunc=D3D12_COMPARISON_FUNC_ALWAYS;sampler.BorderColor=D3D12_STATIC_BORDER_COLOR_TRANSPARENT_BLACK;sampler.MaxLOD=D3D12_FLOAT32_MAX;sampler.ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
 D3D12_ROOT_SIGNATURE_DESC rs{};rs.NumParameters=1;rs.pParameters=&param;rs.NumStaticSamplers=1;rs.pStaticSamplers=&sampler;rs.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
 ComPtr<ID3DBlob> sig,err;check(D3D12SerializeRootSignature(&rs,D3D_ROOT_SIGNATURE_VERSION_1,&sig,&err),"Serialize scene root signature");check(d->CreateRootSignature(0,sig->GetBufferPointer(),sig->GetBufferSize(),IID_PPV_ARGS(&root)),"Create scene root signature");
 const D3D12_INPUT_ELEMENT_DESC elems[]={{"POSITION",0,DXGI_FORMAT_R32G32B32_FLOAT,0,0,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0},{"TEXCOORD",0,DXGI_FORMAT_R32G32_FLOAT,0,12,D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA,0}};
 static_assert(sizeof(Vertex)==20);
 D3D12_GRAPHICS_PIPELINE_STATE_DESC pd{};pd.pRootSignature=root.Get();pd.VS={vs->GetBufferPointer(),vs->GetBufferSize()};pd.PS={ps->GetBufferPointer(),ps->GetBufferSize()};pd.InputLayout={elems,2};pd.SampleMask=UINT_MAX;pd.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;pd.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;pd.RasterizerState.DepthClipEnable=TRUE;
 auto& blend=pd.BlendState.RenderTarget[0];blend.SrcBlend=D3D12_BLEND_ONE;blend.DestBlend=D3D12_BLEND_ZERO;blend.BlendOp=D3D12_BLEND_OP_ADD;blend.SrcBlendAlpha=D3D12_BLEND_ONE;blend.DestBlendAlpha=D3D12_BLEND_ZERO;blend.BlendOpAlpha=D3D12_BLEND_OP_ADD;blend.LogicOp=D3D12_LOGIC_OP_NOOP;blend.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;
 pd.DepthStencilState.DepthEnable=TRUE;pd.DepthStencilState.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ALL;pd.DepthStencilState.DepthFunc=D3D12_COMPARISON_FUNC_LESS;pd.DepthStencilState.FrontFace={D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_COMPARISON_FUNC_ALWAYS};pd.DepthStencilState.BackFace=pd.DepthStencilState.FrontFace;pd.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;pd.NumRenderTargets=1;pd.RTVFormats[0]=DXGI_FORMAT_R8G8B8A8_UNORM;pd.DSVFormat=DXGI_FORMAT_D32_FLOAT;pd.SampleDesc.Count=1;

 // Fixed scene PSO schema; bump this tag if non-shader state changes.
 if(!cache.empty()){
  key=compute_cache_key(g,sig.Get(),vs->GetBufferPointer(),vs->GetBufferSize());
  const std::string tag="revenge-indexed-depth-scene-pso-v1";key.insert(key.end(),tag.begin(),tag.end());
  auto n=ps->GetBufferSize();for(unsigned i=0;i<8;++i)key.push_back(uint8_t(uint64_t(n)>>(8*i)));
  auto code=static_cast<const uint8_t*>(ps->GetBufferPointer());key.insert(key.end(),code,code+n);
  cached=read_cache(cache,key);if(cached.empty())std::cout<<"Scene PSO cache miss.\n";else pd.CachedPSO={cached.data(),cached.size()};
 }
 auto pstart=std::chrono::steady_clock::now();auto hr=d->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&pso));hit=!cached.empty()&&SUCCEEDED(hr);
 if(!cached.empty()){if(hit)std::cout<<"Scene PSO cache hit (driver accepted).\n";else{std::cout<<"Scene PSO cache rejected; retrying without it.\n";pd.CachedPSO={};pso.Reset();hr=d->CreateGraphicsPipelineState(&pd,IID_PPV_ARGS(&pso));}}
 check(hr,"Create scene PSO");std::cout<<"Scene PSO creation ms: "<<std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-pstart).count()<<'\n';
 if(device){device->root=root;device->pso=pso;}
 }
 if(device&&async_slot<2){submit_custom(*device,async_slot,w,h,geo,stream,*custom);return 0;}
 auto rtv=descriptors(d,D3D12_DESCRIPTOR_HEAP_TYPE_RTV,1,false),dsv=descriptors(d,D3D12_DESCRIPTOR_HEAP_TYPE_DSV,1,false),srv=descriptors(d,D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,4,true);
 resources::Registry registry(4);auto reserved=registry.allocate(resources::Kind::buffer);auto tex_handle=registry.allocate(resources::Kind::texture);auto slot=registry.descriptor(tex_handle,resources::Kind::texture);auto step=d->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);auto cpu_srv=srv->GetCPUDescriptorHandleForHeapStart();cpu_srv.ptr+=SIZE_T(slot)*step;auto gpu_srv=srv->GetGPUDescriptorHandleForHeapStart();gpu_srv.ptr+=UINT64(slot)*step;
 auto color_desc=texture_desc(w,h,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);D3D12_CLEAR_VALUE clear{};clear.Format=color_desc.Format;clear.Color[3]=1;auto color=texture(d,color_desc,D3D12_RESOURCE_STATE_RENDER_TARGET,&clear);
 auto depth_desc=texture_desc(w,h,D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);depth_desc.Format=DXGI_FORMAT_D32_FLOAT;D3D12_CLEAR_VALUE dc{};dc.Format=depth_desc.Format;dc.DepthStencil.Depth=1;auto depth=texture(d,depth_desc,D3D12_RESOURCE_STATE_DEPTH_WRITE,&dc);
 auto rh=rtv->GetCPUDescriptorHandleForHeapStart(),dh=dsv->GetCPUDescriptorHandleForHeapStart();d->CreateRenderTargetView(color.Get(),nullptr,rh);D3D12_DEPTH_STENCIL_VIEW_DESC dv{};dv.Format=depth_desc.Format;dv.ViewDimension=D3D12_DSV_DIMENSION_TEXTURE2D;d->CreateDepthStencilView(depth.Get(),&dv,dh);
 auto tw=custom?custom->texture_width:2u,th=custom?custom->texture_height:2u;auto td=texture_desc(tw,th,D3D12_RESOURCE_FLAG_NONE);auto tex=texture(d,td,D3D12_RESOURCE_STATE_COPY_DEST);D3D12_SHADER_RESOURCE_VIEW_DESC sv{};sv.Format=td.Format;sv.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;sv.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;sv.Texture2D.MipLevels=1;d->CreateShaderResourceView(tex.Get(),&sv,cpu_srv);
 D3D12_PLACED_SUBRESOURCE_FOOTPRINT tf{},rf{};UINT64 ub=0,rb=0;d->GetCopyableFootprints(&td,0,1,0,&tf,nullptr,nullptr,&ub);d->GetCopyableFootprints(&color_desc,0,1,0,&rf,nullptr,nullptr,&rb);
 auto read=buffer(d,rb,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);ComPtr<ID3D12Resource> upload;GeneratedTexture generated;
 D3D12_RANGE empty{0,0};void* mapped=nullptr;
 if(integrated){generated.prepare(d,tf,ub,programs?std::span<const uint32_t>(programs->compute):std::span<const uint32_t>{});std::cout<<"Translated RDNA2 ALU fixture dispatched on compute queue.\n";}
 else {upload=buffer(d,ub,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);check(upload->Map(0,&empty,&mapped),"Map scene texture upload");for(unsigned y=0;y<th;++y)std::memcpy(static_cast<uint8_t*>(mapped)+tf.Offset+y*tf.Footprint.RowPitch,custom?custom->rgba.data()+size_t(y)*tw*4:reinterpret_cast<const uint8_t*>(palette.data()+2*y),size_t(tw)*4);upload->Unmap(0,nullptr);}
 auto vbbytes=geo.vertices.size()*sizeof(Vertex),ibbytes=geo.indices.size()*sizeof(uint16_t);auto vb=buffer(d,vbbytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ),ib=buffer(d,ibbytes,D3D12_HEAP_TYPE_UPLOAD,D3D12_RESOURCE_STATE_GENERIC_READ);
 check(vb->Map(0,&empty,&mapped),"Map vertex buffer");std::memcpy(mapped,geo.vertices.data(),vbbytes);vb->Unmap(0,nullptr);check(ib->Map(0,&empty,&mapped),"Map index buffer");std::memcpy(mapped,geo.indices.data(),ibbytes);ib->Unmap(0,nullptr);
 D3D12_VERTEX_BUFFER_VIEW vv{vb->GetGPUVirtualAddress(),UINT(vbbytes),UINT(sizeof(Vertex))};D3D12_INDEX_BUFFER_VIEW iv{ib->GetGPUVirtualAddress(),UINT(ibbytes),DXGI_FORMAT_R16_UINT};
 D3D12_COMMAND_QUEUE_DESC qd{};qd.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;ComPtr<ID3D12CommandQueue> q;if(device&&device->queue)q=device->queue;else{check(d->CreateCommandQueue(&qd,IID_PPV_ARGS(&q)),"Create scene queue");if(device)device->queue=q;}ComPtr<ID3D12CommandAllocator> a;check(d->CreateCommandAllocator(qd.Type,IID_PPV_ARGS(&a)),"Create scene allocator");ComPtr<ID3D12GraphicsCommandList> l;check(d->CreateCommandList(0,qd.Type,a.Get(),pso.Get(),IID_PPV_ARGS(&l)),"Create scene command list");
 ComPtr<ID3D12Fence> fence;check(d->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"Create scene fence");Handle event;DrainQueue drain{q.Get(),fence.Get(),event.h};
 if(integrated){check(q->Wait(generated.fence.Get(),1),"Graphics wait for translated compute texture");}
 auto dst=location(tex.Get()),src=location(integrated?generated.data.Get():upload.Get(),tf);l->CopyTextureRegion(&dst,0,0,0,&src,nullptr);transition(l.Get(),tex.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
 D3D12_VIEWPORT vp{0,0,float(w),float(h),0,1};D3D12_RECT sc{0,0,LONG(w),LONG(h)};
 for(auto c:stream){switch(c.op){
 case Op::begin:l->SetGraphicsRootSignature(root.Get());l->RSSetViewports(1,&vp);l->RSSetScissorRects(1,&sc);l->OMSetRenderTargets(1,&rh,FALSE,&dh);l->ClearRenderTargetView(rh,clear.Color,0,nullptr);l->ClearDepthStencilView(dh,D3D12_CLEAR_FLAG_DEPTH,1,0,0,nullptr);break;
 case Op::bind_mesh:l->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);l->IASetVertexBuffers(0,1,&vv);l->IASetIndexBuffer(&iv);break;
 case Op::bind_texture:{ID3D12DescriptorHeap* heaps[]{srv.Get()};l->SetDescriptorHeaps(1,heaps);registry.descriptor(tex_handle,resources::Kind::texture);l->SetGraphicsRootDescriptorTable(0,gpu_srv);break;}
 case Op::draw_indexed:l->DrawIndexedInstanced(c.count,1,c.first,0,0);break;
 case Op::end:{transition(l.Get(),color.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE);auto dest=location(read.Get(),rf),source=location(color.Get());l->CopyTextureRegion(&dest,0,0,0,&source,nullptr);break;}
 }}
 registry.submitted(tex_handle,resources::Kind::texture,1);if(!continuous){registry.retire(tex_handle,resources::Kind::texture);registry.retire(reserved,resources::Kind::buffer);}check(l->Close(),"Close scene list");ID3D12CommandList* lists[]{l.Get()};q->ExecuteCommandLists(1,lists);drain.last=1;check(q->Signal(fence.Get(),1),"Signal scene fence");wait(fence.Get(),1,event.h);drain.last=0;if(integrated)generated.submitted=false;auto retired=registry.collect(fence->GetCompletedValue());if(!continuous&&(retired!=2||registry.occupied()))throw std::runtime_error("scene descriptor retirement failure");if(integrated&&!continuous)std::cout<<"Compute-to-graphics fence completed; descriptor slot "<<slot<<" retired; registry empty.\n";
 D3D12_RANGE rr{0,SIZE_T(rb)};check(read->Map(0,&rr,&mapped),"Map scene readback");std::vector<uint8_t> image(size_t(rf.Footprint.RowPitch)*h);for(unsigned y=0;y<h;++y)std::memcpy(image.data()+size_t(y)*rf.Footprint.RowPitch,static_cast<const uint8_t*>(mapped)+rf.Offset+size_t(y)*rf.Footprint.RowPitch,w*4);read->Unmap(0,&empty);
 if(custom){custom->framebuffer.resize(size_t(w)*h*4);for(unsigned y=0;y<h;++y)std::memcpy(custom->framebuffer.data()+size_t(y)*w*4,image.data()+size_t(y)*rf.Footprint.RowPitch,size_t(w)*4);std::cout<<"Application-defined indexed scene: "<<w<<"x"<<h<<", caller framebuffer readback, queue drained.\n";return 0;}
 auto n=verify(image,w,h,rf.Footprint.RowPitch,translated);
 if(continuous){
  // Reuse the original device, PSO, translated compute texture, descriptors and mesh.
  // Serial fence completion keeps one shared color/depth pair and allocator safe.
  using Clock=std::chrono::steady_clock;using namespace std::chrono;
  auto& stats=*continuous;stats.first_pixels=n;
  presentation::Window window(std::min(w,1280u),std::min(h,720u));
  SetWindowTextW(window.hwnd,L"Revenge | Continuous translated scene | Esc to close");
  ComPtr<IDXGIFactory6> factory;check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)),"Create continuous factory");
  DXGI_SWAP_CHAIN_DESC1 sd{};sd.Width=w;sd.Height=h;sd.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.SampleDesc.Count=1;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.BufferCount=2;sd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;sd.Scaling=DXGI_SCALING_STRETCH;
  ComPtr<IDXGISwapChain1> base;check(factory->CreateSwapChainForHwnd(q.Get(),window.hwnd,&sd,nullptr,nullptr,&base),"Create continuous swap chain");check(factory->MakeWindowAssociation(window.hwnd,DXGI_MWA_NO_ALT_ENTER),"Continuous window association");ComPtr<IDXGISwapChain3> swap;check(base.As(&swap),"Continuous swap chain interface");
  std::array<ComPtr<ID3D12Resource>,2> backs;for(unsigned i=0;i<backs.size();++i)check(swap->GetBuffer(i,IID_PPV_ARGS(&backs[i])),"Continuous swap buffer");
  UINT64 frequency=0;check(q->GetTimestampFrequency(&frequency),"Continuous timestamp frequency");if(!frequency)throw std::runtime_error("zero timestamp frequency");
  D3D12_QUERY_HEAP_DESC qh{};qh.Type=D3D12_QUERY_HEAP_TYPE_TIMESTAMP;qh.Count=2;ComPtr<ID3D12QueryHeap> queries;check(d->CreateQueryHeap(&qh,IID_PPV_ARGS(&queries)),"Continuous timestamp heap");auto times=buffer(d,16,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
  DrainQueue loop_drain{q.Get(),fence.Get(),event.h};presentation::FrameTimer pacing(stats.fps_cap!=0);
  ShowWindow(window.hwnd,SW_SHOW);auto begin=Clock::now(),title_at=begin;uint64_t seq=1,title_count=0;
  while(!window.stop&&Clock::now()-begin<seconds(stats.seconds)&&stats.submitted<30000){
   window.pump();if(window.stop)break;if(IsIconic(window.hwnd)){Sleep(20);continue;}
   const auto start=Clock::now();int shift=motion_offset(w,stats.submitted);
   if(GetForegroundWindow()==window.hwnd){
    for(DWORD slot=0;slot<XUSER_MAX_COUNT;++slot){XINPUT_STATE state{};if(XInputGetState(slot,&state)!=ERROR_SUCCESS)continue;auto stick=media::normalize_stick(state.Gamepad.sThumbLX,state.Gamepad.sThumbLY,XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);if(stick.x!=0)shift=int(stick.x*float(w/8));break;}
    if(GetAsyncKeyState(VK_LEFT)&0x8000)shift=-int(w/8);else if(GetAsyncKeyState(VK_RIGHT)&0x8000)shift=int(w/8);
   }
   auto* back=backs.at(swap->GetCurrentBackBufferIndex()).Get();
   check(a->Reset(),"Reset continuous allocator");check(l->Reset(a.Get(),pso.Get()),"Reset continuous list");
   l->EndQuery(queries.Get(),D3D12_QUERY_TYPE_TIMESTAMP,0);
   transition(l.Get(),color.Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);
   D3D12_VIEWPORT moved{float(shift),0,float(w),float(h),0,1};
   l->SetGraphicsRootSignature(root.Get());l->RSSetViewports(1,&moved);l->RSSetScissorRects(1,&sc);l->OMSetRenderTargets(1,&rh,FALSE,&dh);l->ClearRenderTargetView(rh,clear.Color,0,nullptr);l->ClearDepthStencilView(dh,D3D12_CLEAR_FLAG_DEPTH,1,0,0,nullptr);
   l->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);l->IASetVertexBuffers(0,1,&vv);l->IASetIndexBuffer(&iv);ID3D12DescriptorHeap* heaps[]{srv.Get()};l->SetDescriptorHeaps(1,heaps);l->SetGraphicsRootDescriptorTable(0,gpu_srv);
   for(auto c:stream)if(c.op==Op::draw_indexed)l->DrawIndexedInstanced(c.count,1,c.first,0,0);
   transition(l.Get(),color.Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE);
   transition(l.Get(),back,D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_COPY_DEST);l->CopyResource(back,color.Get());transition(l.Get(),back,D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PRESENT);
   l->EndQuery(queries.Get(),D3D12_QUERY_TYPE_TIMESTAMP,1);l->ResolveQueryData(queries.Get(),D3D12_QUERY_TYPE_TIMESTAMP,0,2,times.Get(),0);check(l->Close(),"Close continuous list");
   ++seq;registry.submitted(tex_handle,resources::Kind::texture,seq);loop_drain.last=seq;q->ExecuteCommandLists(1,lists);auto hr=swap->Present(0,0);check(q->Signal(fence.Get(),seq),"Signal continuous fence");wait(fence.Get(),seq,event.h);loop_drain.last=0;check(hr,"Present continuous scene");++stats.submitted;stats.last_shift=shift;
   if(hr==S_OK){++stats.presented;++title_count;}else Sleep(20);
   D3D12_RANGE tr{0,16};void* ptr=nullptr;check(times->Map(0,&tr,&ptr),"Map continuous timestamps");uint64_t ticks[2];std::memcpy(ticks,ptr,16);times->Unmap(0,&empty);if(ticks[1]<ticks[0])throw std::runtime_error("invalid continuous timestamps");stats.gpu_ns+=static_cast<uint64_t>(double(ticks[1]-ticks[0])*1e9/double(frequency));
   if(stats.fps_cap)pacing.pace(start+duration_cast<Clock::duration>(duration<double>(1.0/stats.fps_cap)));
   auto now=Clock::now();auto interval=duration<double>(now-title_at).count();if(interval>=0.5){std::wostringstream title;title<<L"Revenge | Translated scene | "<<w<<L"x"<<h<<L" | "<<std::fixed<<std::setprecision(1)<<title_count/interval<<L" Present/s | Stick / Arrows / Esc";SetWindowTextW(window.hwnd,title.str().c_str());title_count=0;title_at=now;}
  }
  stats.elapsed_ns=duration_cast<nanoseconds>(Clock::now()-begin).count();if(!stats.submitted||!stats.presented)throw std::runtime_error("no continuous frame presented");
  // Check the actual last rendered frame with the independently shifted CPU oracle.
  check(a->Reset(),"Reset final oracle allocator");check(l->Reset(a.Get(),nullptr),"Reset final oracle list");auto dest=location(read.Get(),rf),source=location(color.Get());l->CopyTextureRegion(&dest,0,0,0,&source,nullptr);check(l->Close(),"Close final oracle list");loop_drain.last=++seq;q->ExecuteCommandLists(1,lists);check(q->Signal(fence.Get(),seq),"Signal final oracle");wait(fence.Get(),seq,event.h);loop_drain.last=0;
  check(read->Map(0,&rr,&mapped),"Map final scene readback");for(unsigned y=0;y<h;++y)std::memcpy(image.data()+size_t(y)*rf.Footprint.RowPitch,static_cast<const uint8_t*>(mapped)+rf.Offset+size_t(y)*rf.Footprint.RowPitch,w*4);read->Unmap(0,&empty);
  stats.last_pixels=verify(image,w,h,rf.Footprint.RowPitch,translated,stats.last_shift);
  registry.retire(tex_handle,resources::Kind::texture);registry.retire(reserved,resources::Kind::buffer);if(registry.collect(seq-1)!=2||registry.occupied())throw std::runtime_error("continuous descriptor retirement failure");
  std::cout<<"Continuous scene: first and final pixel oracles passed; texture and pipeline reused; queue drained; registry empty.\n";
 }
 if(present_seconds){
  // Present the verified GPU render target directly; no CPU upload or substitute shader.
  presentation::Window window(std::min(w,1280u),std::min(h,720u));
  SetWindowTextW(window.hwnd,L"Revenge | Translated RDNA2 scene (verified frame)");
  ComPtr<IDXGIFactory6> factory;check(CreateDXGIFactory2(0,IID_PPV_ARGS(&factory)),"Create scene present factory");
  DXGI_SWAP_CHAIN_DESC1 sd{};sd.Width=w;sd.Height=h;sd.Format=DXGI_FORMAT_R8G8B8A8_UNORM;sd.SampleDesc.Count=1;sd.BufferUsage=DXGI_USAGE_RENDER_TARGET_OUTPUT;sd.BufferCount=2;sd.SwapEffect=DXGI_SWAP_EFFECT_FLIP_DISCARD;sd.Scaling=DXGI_SCALING_STRETCH;
  ComPtr<IDXGISwapChain1> base;check(factory->CreateSwapChainForHwnd(q.Get(),window.hwnd,&sd,nullptr,nullptr,&base),"Create scene swap chain");check(factory->MakeWindowAssociation(window.hwnd,DXGI_MWA_NO_ALT_ENTER),"Scene window association");ComPtr<IDXGISwapChain3> swap;check(base.As(&swap),"Scene swap chain interface");
  ComPtr<ID3D12Resource> back;check(swap->GetBuffer(swap->GetCurrentBackBufferIndex(),IID_PPV_ARGS(&back)),"Scene swap buffer");
  check(a->Reset(),"Reset scene present allocator");check(l->Reset(a.Get(),nullptr),"Reset scene present list");
  transition(l.Get(),back.Get(),D3D12_RESOURCE_STATE_PRESENT,D3D12_RESOURCE_STATE_COPY_DEST);l->CopyResource(back.Get(),color.Get());transition(l.Get(),back.Get(),D3D12_RESOURCE_STATE_COPY_DEST,D3D12_RESOURCE_STATE_PRESENT);check(l->Close(),"Close scene present list");
  DrainQueue present_drain{q.Get(),fence.Get(),event.h};q->ExecuteCommandLists(1,lists);present_drain.last=2;
  ShowWindow(window.hwnd,SW_SHOW);check(swap->Present(1,0),"Present translated scene");check(q->Signal(fence.Get(),2),"Signal scene present");wait(fence.Get(),2,event.h);present_drain.last=0;
  std::cout<<"Translated GPU frame submitted to native DXGI swap chain; fence completed. Static frame, not gameplay FPS.\n";
  const auto until=std::chrono::steady_clock::now()+std::chrono::seconds(present_seconds);
  while(!window.stop&&std::chrono::steady_clock::now()<until){window.pump();std::this_thread::sleep_for(std::chrono::milliseconds(10));}
 }
 if(verified)*verified=n;
if(!cache.empty()&&!hit)save_cache(cache,key,pso.Get(),"Scene");std::cout<<"Verified "<<n<<" interior pixels at exact "<<w<<'x'<<h<<" render extent; two indexed draws, point texture sampling and depth occlusion.\n";if(!output.empty()){write_frame_bmp(output,w,h,rf.Footprint.RowPitch,image);std::cout<<"Saved scene frame: "<<output.string()<<'\n';}std::cout<<(translated?"Translated compute, vertex and pixel stages; fixed independent fixture ABI, no Xbox gameplay.\n":integrated?"Integrated translated compute texture + indexed graphics fixture; no Xbox ABI or gameplay.\n":"Independent offscreen perspective fixture; no Xbox ABI or gameplay.\n");return 0;
}
}

namespace revenge::flow {
static int execute_shader22(const std::string& source,const std::vector<uint32_t>& expected){
 ComPtr<ID3DBlob> blob,errors;auto hr=D3DCompile(source.data(),source.size(),"revenge-shader22-fixture",nullptr,nullptr,"main","cs_5_1",D3DCOMPILE_ENABLE_STRICTNESS|D3DCOMPILE_OPTIMIZATION_LEVEL3,0,&blob,&errors);
 if(FAILED(hr)&&errors)std::cerr<<std::string(static_cast<const char*>(errors->GetBufferPointer()),errors->GetBufferSize())<<'\n';check(hr,"Compile shader 22 fixture HLSL");
 return compute_known_answer(blob->GetBufferPointer(),blob->GetBufferSize(),expected);
}
int loop_gpu_test(){auto p=decode_loop(loop_words());std::vector<uint32_t> expected(4096);for(uint32_t i=0;i<expected.size();++i){expected[i]=loop_expected(i);if(evaluate(p,i)!=expected[i])throw std::runtime_error("loop CPU oracle mismatch");}std::cout<<"Decoded signed backward branch and scalar carry fixture; 1..64 loop iterations; 1024-instruction limit.\n";return execute_shader22(emit(p),expected);}
int wave_gpu_test(unsigned width){auto expected=wave_fixture_expected(width);std::cout<<"Logical Wave"<<width<<" shared-memory mask fixture; not native wave opcode translation.\n";return execute_shader22(wave_fixture_hlsl(width),expected);}
}

#include "revenge/render_pass_fixture.hpp"
#include <bit>
namespace revenge::passes {
namespace {
struct PassContext {
    Gpu g;
    ComPtr<ID3D12RootSignature> root,root2;
    ComPtr<ID3D12PipelineState> init,reject,mix,finish;
    ComPtr<ID3D12CommandQueue> queue;
    ComPtr<ID3D12Fence> fence;
    Handle event;
    uint64_t seq=0;
    bool poisoned=false,in_flight=false;
    explicit PassContext(bool trace):g(select()) {
    if(trace)std::cerr<<"[pass] hardware device created"<<std::endl;auto d=g.device.Get();
    const char shader[]=R"(
cbuffer DrawConstants : register(b0) { float z; uint mode; };
float4 vs(uint id:SV_VertexID):SV_Position {
    float2 p=float2((id==2)?3.0:-1.0,(id==1)?-3.0:1.0);
    return float4(p,z,1);
}
struct Targets {float4 a:SV_Target0;float4 b:SV_Target1;};
Targets first(){Targets o;
    if(mode==0){o.a=float4(1,0,0,1);o.b=float4(0,1,0,1);}
    else if(mode==1){o.a=float4(1,1,0,1);o.b=float4(1,0,0,1);}
    else {o.a=float4(0,0,1,0.5);o.b=float4(0,0,0,0);}
    return o;
}
Texture2D<float4> a:register(t0);Texture2D<float4> b:register(t1);
float4 compose(float4 p:SV_Position):SV_Target0 {
    int3 loc=int3(int2(p.xy),0);float4 x=a.Load(loc);float4 y=b.Load(loc);
    return float4(x.r,y.g,x.b,1);
}
)";
    auto compile=[&](const char* entry,const char* profile){ComPtr<ID3DBlob> blob,err;auto hr=D3DCompile(shader,sizeof(shader)-1,"revenge-pass",nullptr,nullptr,entry,profile,D3DCOMPILE_ENABLE_STRICTNESS,0,&blob,&err);if(FAILED(hr)&&err)std::cerr<<std::string(static_cast<const char*>(err->GetBufferPointer()),err->GetBufferSize())<<'\n';check(hr,"Pass shader");return blob;};
    auto vs=compile("vs","vs_5_1"),ps=compile("first","ps_5_1"),composite=compile("compose","ps_5_1");
    D3D12_DESCRIPTOR_RANGE range{};range.RangeType=D3D12_DESCRIPTOR_RANGE_TYPE_SRV;range.NumDescriptors=2;
    D3D12_ROOT_PARAMETER rp[2]{};rp[0].ParameterType=D3D12_ROOT_PARAMETER_TYPE_32BIT_CONSTANTS;rp[0].Constants.Num32BitValues=4;rp[0].ShaderVisibility=D3D12_SHADER_VISIBILITY_ALL;
    rp[1].ParameterType=D3D12_ROOT_PARAMETER_TYPE_DESCRIPTOR_TABLE;rp[1].DescriptorTable={1,&range};rp[1].ShaderVisibility=D3D12_SHADER_VISIBILITY_PIXEL;
    // The draw pipelines need no SRV table. Use a separate signature for composition.
    D3D12_ROOT_SIGNATURE_DESC rd{};rd.NumParameters=1;rd.pParameters=rp;rd.Flags=D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT;
    auto root_make=[&](){ComPtr<ID3DBlob> sig,err;check(D3D12SerializeRootSignature(&rd,D3D_ROOT_SIGNATURE_VERSION_1,&sig,&err),"Pass root signature");ComPtr<ID3D12RootSignature> r;check(d->CreateRootSignature(0,sig->GetBufferPointer(),sig->GetBufferSize(),IID_PPV_ARGS(&r)),"Pass root");return r;};
    root=root_make();rd.NumParameters=2;root2=root_make();
    D3D12_GRAPHICS_PIPELINE_STATE_DESC p{};p.pRootSignature=root.Get();p.VS={vs->GetBufferPointer(),vs->GetBufferSize()};p.PS={ps->GetBufferPointer(),ps->GetBufferSize()};p.SampleMask=UINT_MAX;p.RasterizerState.FillMode=D3D12_FILL_MODE_SOLID;p.RasterizerState.CullMode=D3D12_CULL_MODE_NONE;p.RasterizerState.DepthClipEnable=TRUE;p.PrimitiveTopologyType=D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;p.SampleDesc.Count=1;p.NumRenderTargets=2;p.RTVFormats[0]=p.RTVFormats[1]=DXGI_FORMAT_R8G8B8A8_UNORM;p.DSVFormat=DXGI_FORMAT_D24_UNORM_S8_UINT;
    p.BlendState.IndependentBlendEnable=TRUE;for(auto& t:p.BlendState.RenderTarget){t.SrcBlend=D3D12_BLEND_ONE;t.DestBlend=D3D12_BLEND_ZERO;t.BlendOp=D3D12_BLEND_OP_ADD;t.SrcBlendAlpha=D3D12_BLEND_ONE;t.DestBlendAlpha=D3D12_BLEND_ZERO;t.BlendOpAlpha=D3D12_BLEND_OP_ADD;t.LogicOp=D3D12_LOGIC_OP_NOOP;t.RenderTargetWriteMask=D3D12_COLOR_WRITE_ENABLE_ALL;}
    auto& ds=p.DepthStencilState;ds.DepthEnable=TRUE;ds.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ALL;ds.DepthFunc=D3D12_COMPARISON_FUNC_LESS;ds.StencilEnable=TRUE;ds.StencilReadMask=ds.StencilWriteMask=0xff;ds.FrontFace={D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_KEEP,D3D12_STENCIL_OP_REPLACE,D3D12_COMPARISON_FUNC_ALWAYS};ds.BackFace=ds.FrontFace;
    auto make=[&](){ComPtr<ID3D12PipelineState> state;check(d->CreateGraphicsPipelineState(&p,IID_PPV_ARGS(&state)),"Pass graphics PSO");return state;};
    init=make();ds.FrontFace.StencilPassOp=D3D12_STENCIL_OP_KEEP;ds.FrontFace.StencilFunc=D3D12_COMPARISON_FUNC_EQUAL;ds.BackFace=ds.FrontFace;reject=make();
    auto& blend=p.BlendState.RenderTarget[0];blend.BlendEnable=TRUE;blend.SrcBlend=D3D12_BLEND_SRC_ALPHA;blend.DestBlend=D3D12_BLEND_INV_SRC_ALPHA;blend.DestBlendAlpha=D3D12_BLEND_INV_SRC_ALPHA;p.BlendState.RenderTarget[1].RenderTargetWriteMask=0;mix=make();
    blend.BlendEnable=FALSE;ds.DepthEnable=FALSE;ds.DepthWriteMask=D3D12_DEPTH_WRITE_MASK_ZERO;ds.StencilEnable=FALSE;p.NumRenderTargets=1;p.RTVFormats[1]=DXGI_FORMAT_UNKNOWN;p.DSVFormat=DXGI_FORMAT_UNKNOWN;p.pRootSignature=root2.Get();p.PS={composite->GetBufferPointer(),composite->GetBufferSize()};finish=make();
    D3D12_COMMAND_QUEUE_DESC qd{};qd.Type=D3D12_COMMAND_LIST_TYPE_DIRECT;
    check(d->CreateCommandQueue(&qd,IID_PPV_ARGS(&queue)),"Pass queue");
    check(d->CreateFence(0,D3D12_FENCE_FLAG_NONE,IID_PPV_ARGS(&fence)),"Pass fence");
    }
};
struct PassResources {
    unsigned w=0,h=0;
    ComPtr<ID3D12DescriptorHeap> rtv,dsv,srv;
    std::array<ComPtr<ID3D12Resource>,3> colors,reads;
    ComPtr<ID3D12Resource> depth;
    std::array<D3D12_CPU_DESCRIPTOR_HANDLE,3> rt{};
    D3D12_CPU_DESCRIPTOR_HANDLE dh{};
    D3D12_PLACED_SUBRESOURCE_FOOTPRINT fp{};
    UINT64 size=0;
    ComPtr<ID3D12CommandAllocator> alloc;
    ComPtr<ID3D12GraphicsCommandList> list;
};
}
struct PassCache {
    std::mutex mutex;
    std::unique_ptr<PassContext> context;
    PassResources resources;
    uint64_t calls=0,allocations=0,reuses=0;
    bool trace;
    explicit PassCache(bool diagnostics):trace(diagnostics){}
    ~PassCache() noexcept {
        if(!context)return;
        bool clean=!context->poisoned;
        // A failed fence wait may leave a submission outstanding. Attempt a bounded
        // drain outside DLL detach; a removed device or another timeout is not success.
        if(context->in_flight){
            try{auto value=++context->seq;check(context->queue->Signal(context->fence.Get(),value),"Pass cleanup signal");wait(context->fence.Get(),value,context->event.h);context->in_flight=false;}
            catch(const std::exception& e){std::cerr<<"[pass] cleanup drain failed: "<<e.what()<<std::endl;}
            catch(...){std::cerr<<"[pass] cleanup drain failed"<<std::endl;}
        }
        resources=PassResources{};context.reset();
        if(trace){std::cerr<<(clean?"[pass] GPU cache closed: calls=":"[pass] failed GPU cache closed: calls=")<<calls<<" allocations="<<allocations<<" reuses="<<reuses<<std::endl;}
    }
};
std::shared_ptr<PassCache> create_pass_cache(){return std::make_shared<PassCache>(true);}
Images render_gpu(PassCache& cache,unsigned w,unsigned h,std::span<const Draw> commands){
    validate(w,h,commands);
    std::lock_guard lock(cache.mutex);
    if(!cache.context){if(cache.trace)std::cerr<<"[pass] selecting hardware and creating pipelines"<<std::endl;cache.context=std::make_unique<PassContext>(cache.trace);if(cache.trace)std::cerr<<"[pass] pipelines and queue ready"<<std::endl;else print(cache.context->g);}
    auto& context=*cache.context;
    if(context.poisoned||context.seq==UINT64_MAX-1)throw std::runtime_error("Pass context unavailable after GPU failure or sequence exhaustion");
    auto d=context.g.device.Get();auto& root=context.root;auto& root2=context.root2;
    auto& init=context.init;auto& reject=context.reject;auto& mix=context.mix;auto& finish=context.finish;
    auto& queue=context.queue;auto& fence=context.fence;auto& event=context.event;
    auto& r=cache.resources;
    // A single size-keyed slot. Previous submissions have completed before this lock is released.
    // New resources are built transactionally, then replace the old completed slot.
    if(r.w!=w||r.h!=h){if(cache.trace)std::cerr<<"[pass] allocating slot "<<w<<"x"<<h<<std::endl;PassResources next;next.w=w;next.h=h;
    auto heap=[&](D3D12_DESCRIPTOR_HEAP_TYPE type,UINT count,bool visible){D3D12_DESCRIPTOR_HEAP_DESC h{};h.Type=type;h.NumDescriptors=count;h.Flags=visible?D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE:D3D12_DESCRIPTOR_HEAP_FLAG_NONE;ComPtr<ID3D12DescriptorHeap> result;check(d->CreateDescriptorHeap(&h,IID_PPV_ARGS(&result)),"Pass descriptor heap");return result;};
    next.rtv=heap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV,3,false);next.dsv=heap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV,1,false);next.srv=heap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV,2,true);auto& rtv=next.rtv;auto& dsv=next.dsv;auto& srv=next.srv;
    auto td=texture_desc(w,h,D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET);D3D12_CLEAR_VALUE clear{};clear.Format=td.Format;
    auto& colors=next.colors;auto& rt=next.rt;auto sh=srv->GetCPUDescriptorHandleForHeapStart();
    for(unsigned i=0;i<3;++i){colors[i]=texture(d,td,D3D12_RESOURCE_STATE_RENDER_TARGET,&clear);rt[i]=rtv->GetCPUDescriptorHandleForHeapStart();rt[i].ptr+=SIZE_T(i)*d->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);d->CreateRenderTargetView(colors[i].Get(),nullptr,rt[i]);if(i<2){D3D12_SHADER_RESOURCE_VIEW_DESC v{};v.Format=td.Format;v.ViewDimension=D3D12_SRV_DIMENSION_TEXTURE2D;v.Shader4ComponentMapping=D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;v.Texture2D.MipLevels=1;d->CreateShaderResourceView(colors[i].Get(),&v,sh);sh.ptr+=d->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);}}
    auto dd=texture_desc(w,h,D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);dd.Format=DXGI_FORMAT_D24_UNORM_S8_UINT;D3D12_CLEAR_VALUE dc{};dc.Format=dd.Format;dc.DepthStencil.Depth=1;next.depth=texture(d,dd,D3D12_RESOURCE_STATE_DEPTH_WRITE,&dc);next.dh=dsv->GetCPUDescriptorHandleForHeapStart();d->CreateDepthStencilView(next.depth.Get(),nullptr,next.dh);
    check(d->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT,IID_PPV_ARGS(&next.alloc)),"Pass allocator");
    check(d->CreateCommandList(0,D3D12_COMMAND_LIST_TYPE_DIRECT,next.alloc.Get(),nullptr,IID_PPV_ARGS(&next.list)),"Pass list");check(next.list->Close(),"Close initial pass list");
    UINT rows{};UINT64 rowbytes{};d->GetCopyableFootprints(&td,0,1,0,&next.fp,&rows,&rowbytes,&next.size);
    if(rows!=h||rowbytes!=w*4||next.fp.Footprint.RowPitch<rowbytes)throw std::runtime_error("Pass footprint mismatch");
    for(auto& read:next.reads)read=buffer(d,next.size,D3D12_HEAP_TYPE_READBACK,D3D12_RESOURCE_STATE_COPY_DEST);
    r=std::move(next);++cache.allocations;
    }else ++cache.reuses;
    auto& rt=r.rt;auto dh=r.dh;auto& srv=r.srv;auto& colors=r.colors;auto& reads=r.reads;auto& list=r.list;
    auto& fp=r.fp;auto size=r.size;const float clear_color[4]={};
    // Fail closed on recording/submission errors; retain resources until explicit cache teardown.
    context.poisoned=true;
    check(r.alloc->Reset(),"Reset pass allocator");check(list->Reset(r.alloc.Get(),nullptr),"Reset pass list");
    for(auto h:rt)list->ClearRenderTargetView(h,clear_color,0,nullptr);list->ClearDepthStencilView(dh,D3D12_CLEAR_FLAG_DEPTH|D3D12_CLEAR_FLAG_STENCIL,1,0,0,nullptr);
    D3D12_VIEWPORT vp{0,0,float(w),float(h),0,1};list->RSSetViewports(1,&vp);list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);list->SetGraphicsRootSignature(root.Get());list->OMSetRenderTargets(2,rt.data(),FALSE,&dh);list->OMSetStencilRef(1);
    ID3D12PipelineState* states[]={init.Get(),reject.Get(),mix.Get()};for(const auto& draw:commands){D3D12_RECT sc{LONG(draw.scissor.left),LONG(draw.scissor.top),LONG(draw.scissor.right),LONG(draw.scissor.bottom)};list->RSSetScissorRects(1,&sc);list->SetPipelineState(states[draw.mode]);const UINT c[]={std::bit_cast<UINT>(draw.depth),draw.mode,0,0};list->SetGraphicsRoot32BitConstants(0,4,c,0);list->DrawInstanced(3,1,0,0);}
    D3D12_RECT full_scissor{0,0,LONG(w),LONG(h)};list->RSSetScissorRects(1,&full_scissor);
    list->OMSetRenderTargets(1,&rt[2],FALSE,nullptr);for(unsigned i=0;i<2;++i)transition(list.Get(),colors[i].Get(),D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE);
    ID3D12DescriptorHeap* heaps[]={srv.Get()};list->SetDescriptorHeaps(1,heaps);list->SetGraphicsRootSignature(root2.Get());const UINT compose_constants[]={std::bit_cast<UINT>(0.125f),0,0,0};list->SetGraphicsRoot32BitConstants(0,4,compose_constants,0);list->SetGraphicsRootDescriptorTable(1,srv->GetGPUDescriptorHandleForHeapStart());list->SetPipelineState(finish.Get());list->DrawInstanced(3,1,0,0);
    list->OMSetRenderTargets(0,nullptr,FALSE,nullptr);for(unsigned i=0;i<3;++i){transition(list.Get(),colors[i].Get(),i<2?D3D12_RESOURCE_STATE_PIXEL_SHADER_RESOURCE:D3D12_RESOURCE_STATE_RENDER_TARGET,D3D12_RESOURCE_STATE_COPY_SOURCE);D3D12_TEXTURE_COPY_LOCATION dst{},src{};dst.pResource=reads[i].Get();dst.Type=D3D12_TEXTURE_COPY_TYPE_PLACED_FOOTPRINT;dst.PlacedFootprint=fp;src.pResource=colors[i].Get();src.Type=D3D12_TEXTURE_COPY_TYPE_SUBRESOURCE_INDEX;list->CopyTextureRegion(&dst,0,0,0,&src,nullptr);transition(list.Get(),colors[i].Get(),D3D12_RESOURCE_STATE_COPY_SOURCE,D3D12_RESOURCE_STATE_RENDER_TARGET);}
    check(list->Close(),"Close pass list");ID3D12CommandList* lists[]={list.Get()};if(cache.trace)std::cerr<<"[pass] submitting "<<w<<"x"<<h<<std::endl;context.in_flight=true;queue->ExecuteCommandLists(1,lists);const auto value=++context.seq;check(queue->Signal(fence.Get(),value),"Pass signal");wait(fence.Get(),value,event.h);context.in_flight=false;if(cache.trace)std::cerr<<"[pass] fence complete "<<value<<std::endl;
    Images actual;for(auto& image:actual)image.resize(size_t(w)*h);
    for(unsigned i=0;i<3;++i){void* mapped{};D3D12_RANGE rr{0,SIZE_T(size)},empty{0,0};check(reads[i]->Map(0,&rr,&mapped),"Map pass readback");auto bytes=static_cast<const uint8_t*>(mapped);for(unsigned y=0;y<h;++y)std::memcpy(actual[i].data()+size_t(y)*w,bytes+fp.Offset+uint64_t(y)*fp.Footprint.RowPitch,size_t(w)*4);reads[i]->Unmap(0,&empty);}
    context.poisoned=false;++cache.calls;if(cache.trace)std::cerr<<"[pass] readback complete"<<std::endl;
    return actual;
}
int gpu_test(){
    PassCache cache(false);auto actual=render_gpu(cache,width,height,draws),expected=reference();unsigned worst=0;
    for(unsigned i=0;i<3;++i)for(size_t j=0;j<actual[i].size();++j)for(unsigned c=0;c<4;++c)worst=std::max(worst,unsigned(std::abs(int(actual[i][j][c])-int(expected[i][j][c]))));
    if(worst>1)throw std::runtime_error("Render pass GPU oracle mismatch: max byte error "+std::to_string(worst));
    // Fixed-size repeats followed by a size change and another repeat. Compare every output.
    uint64_t start_alloc=0,start_reuse=0;{std::lock_guard lock(cache.mutex);start_alloc=cache.allocations;start_reuse=cache.reuses;}
    auto start=std::chrono::steady_clock::now();unsigned checked=0;
    for(unsigned i=0;i<24;++i){unsigned w=i<16?width:24,h=height;std::array<Draw,3> work=draws;work[0].scissor.right=8+i%6;work[1].scissor.right=work[2].scissor.right=w;
        auto got=render_gpu(cache,w,h,work),want=reference(w,h,work);for(unsigned k=0;k<3;++k)for(size_t j=0;j<got[k].size();++j)for(unsigned c=0;c<4;++c)if(std::abs(int(got[k][j][c])-int(want[k][j][c]))>1)throw std::runtime_error("Persistent pass oracle mismatch");checked+=3*w*h;}
    auto ms=std::chrono::duration<double,std::milli>(std::chrono::steady_clock::now()-start).count();
    {std::lock_guard lock(cache.mutex);auto allocations=cache.allocations-start_alloc,reuses=cache.reuses-start_reuse;if(allocations!=1||reuses!=23)throw std::runtime_error("Persistent pass reuse count mismatch");std::cout<<"Persistent pass: 24 workloads, "<<checked<<" pixels; one size change, "<<reuses<<" slot reuses, "<<ms<<" ms (CPU oracle included), one device/queue/fence and four fixed PSOs. Not game FPS.\n";}
std::cout<<"Desktop render passes: 768 pixels verified, two MRTs, depth/stencil, scissor, independent blending, render-to-texture, two SRVs, root constants and four PSOs; max byte error "<<worst<<". Desktop D3D12 only.\n";return 0;
}
}
