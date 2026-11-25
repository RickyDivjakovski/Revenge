#include "revenge/multimedia.hpp"
#include <windows.h>
#include <xaudio2.h>
#include <xinput.h>
#include <wrl/client.h>
#include <chrono>
#include <atomic>
#include <string>
#include <ostream>
#include <stdexcept>
#include <thread>
namespace revenge::media {
namespace {
void check(HRESULT hr,const char* msg){if(FAILED(hr))throw std::runtime_error(std::string(msg)+" HRESULT="+std::to_string(static_cast<unsigned long>(hr)));}
struct Com {Com(){check(CoInitializeEx(nullptr,COINIT_MULTITHREADED),"COM initialization");}~Com(){CoUninitialize();}};
template<class T>struct Voice{T* p{};~Voice(){if(p)p->DestroyVoice();}};
struct PlaybackCallback final : IXAudio2VoiceCallback {
 std::atomic<bool> ended{false};
 std::atomic<HRESULT> error{S_OK};
 void STDMETHODCALLTYPE OnVoiceProcessingPassStart(UINT32) override {}
 void STDMETHODCALLTYPE OnVoiceProcessingPassEnd() override {}
 void STDMETHODCALLTYPE OnStreamEnd() override { ended.store(true); }
 void STDMETHODCALLTYPE OnBufferStart(void*) override {}
 void STDMETHODCALLTYPE OnBufferEnd(void*) override {}
 void STDMETHODCALLTYPE OnLoopEnd(void*) override {}
 void STDMETHODCALLTYPE OnVoiceError(void*,HRESULT hr) override { error.store(hr); }
};
struct RumbleReset{DWORD index;bool active=false;~RumbleReset(){if(active){XINPUT_VIBRATION zero{};XInputSetState(index,&zero);}}};
}
static uint64_t playback(std::span<const int16_t> pcm){
 Com com;Microsoft::WRL::ComPtr<IXAudio2> engine;check(XAudio2Create(engine.GetAddressOf(),0,XAUDIO2_DEFAULT_PROCESSOR),"XAudio2Create");
 Voice<IXAudio2MasteringVoice> master;check(engine->CreateMasteringVoice(&master.p),"CreateMasteringVoice");
 WAVEFORMATEX fmt{};fmt.wFormatTag=WAVE_FORMAT_PCM;fmt.nChannels=2;fmt.nSamplesPerSec=48000;fmt.wBitsPerSample=16;fmt.nBlockAlign=4;fmt.nAvgBytesPerSec=192000;
 PlaybackCallback callback; // Must outlive source voice (destroyed first).
 Voice<IXAudio2SourceVoice> source;check(engine->CreateSourceVoice(&source.p,&fmt,0,XAUDIO2_DEFAULT_FREQ_RATIO,&callback),"CreateSourceVoice");
 XAUDIO2_BUFFER buffer{};buffer.Flags=XAUDIO2_END_OF_STREAM;buffer.AudioBytes=static_cast<UINT32>(pcm.size()*sizeof(int16_t));buffer.pAudioData=reinterpret_cast<const BYTE*>(pcm.data());
 check(source.p->SubmitSourceBuffer(&buffer),"SubmitSourceBuffer");check(source.p->Start(),"Start audio");
 auto deadline=std::chrono::steady_clock::now()+std::chrono::seconds(6);XAUDIO2_VOICE_STATE state{};
 do{
  check(callback.error.load(),"XAudio2 voice error");
  source.p->GetState(&state);
  if(callback.ended.load() && !state.BuffersQueued)break;
  if(std::chrono::steady_clock::now()>=deadline)throw std::runtime_error("audio playback timeout: stream completion not confirmed; queued="+std::to_string(state.BuffersQueued));
  std::this_thread::sleep_for(std::chrono::milliseconds(5));
 }while(true);
 check(source.p->Stop(),"Stop audio");check(callback.error.load(),"XAudio2 voice error");
 // SamplesPlayed is reset at an END_OF_STREAM boundary, so it is diagnostic only.
 return state.SamplesPlayed;
}
void play_pcm(std::span<const int16_t> pcm){if(pcm.size()%2||pcm.size()<1920||pcm.size()>192000)throw std::invalid_argument("PCM size");(void)playback(pcm);}
int audio_test(std::ostream& out){auto pcm=tone_fixture();auto played=playback(pcm);out<<"XAudio2: stereo PCM16, 48000 Hz; stream completed (OnStreamEnd); submitted "<<pcm.size()/2<<" sample frames; buffers queued: 0.\nPost-stream SamplesPlayed (diagnostic, may reset): "<<played<<".\nAudibility requires human confirmation.\n";return 0;}
Pad poll_pad(unsigned index){if(index>=XUSER_MAX_COUNT)throw std::invalid_argument("controller index");XINPUT_STATE state{};auto status=XInputGetState(index,&state);if(status==ERROR_DEVICE_NOT_CONNECTED)return {};if(status!=ERROR_SUCCESS)throw std::runtime_error("XInput poll failed");auto& g=state.Gamepad;auto l=normalize_stick(g.sThumbLX,g.sThumbLY,XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE),r=normalize_stick(g.sThumbRX,g.sThumbRY,XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);return {1,g.wButtons,state.dwPacketNumber,l.x,l.y,r.x,r.y,normalize_trigger(g.bLeftTrigger),normalize_trigger(g.bRightTrigger)};}
int input_test(std::ostream& out,unsigned seconds,bool rumble){
 if(seconds<1||seconds>30)throw std::invalid_argument("input duration must be 1..30 seconds");
 DWORD index=0;XINPUT_STATE state{};for(;index<XUSER_MAX_COUNT;++index){auto code=XInputGetState(index,&state);if(code==ERROR_SUCCESS)break;if(code!=ERROR_DEVICE_NOT_CONNECTED)throw std::runtime_error("XInput poll failed: "+std::to_string(code));}
 if(index==XUSER_MAX_COUNT){out<<"No XInput controller connected; hardware test not performed.\n";return 2;}
 RumbleReset reset{index};if(rumble){XINPUT_VIBRATION v{8000,8000};if(XInputSetState(index,&v)!=ERROR_SUCCESS)throw std::runtime_error("rumble failed");reset.active=true;std::this_thread::sleep_for(std::chrono::milliseconds(150));XINPUT_VIBRATION zero{};if(XInputSetState(index,&zero)!=ERROR_SUCCESS)throw std::runtime_error("rumble stop failed");reset.active=false;}
 out<<"Controller slot "<<index<<" connected. Move sticks and press buttons during polling.\n";
 auto end=std::chrono::steady_clock::now()+std::chrono::seconds(seconds);unsigned polls=0,changes=0;bool first=true;DWORD last=0;
 while(std::chrono::steady_clock::now()<end){auto status=XInputGetState(index,&state);if(status!=ERROR_SUCCESS)throw std::runtime_error("controller disconnected or polling failed");++polls;if(first||last!=state.dwPacketNumber){if(!first)++changes;first=false;last=state.dwPacketNumber;auto& g=state.Gamepad;auto l=normalize_stick(g.sThumbLX,g.sThumbLY,XINPUT_GAMEPAD_LEFT_THUMB_DEADZONE);auto r=normalize_stick(g.sThumbRX,g.sThumbRY,XINPUT_GAMEPAD_RIGHT_THUMB_DEADZONE);out<<"buttons="<<g.wButtons<<" L="<<l.x<<','<<l.y<<" R="<<r.x<<','<<r.y<<" triggers="<<normalize_trigger(g.bLeftTrigger)<<','<<normalize_trigger(g.bRightTrigger)<<'\n';}std::this_thread::sleep_for(std::chrono::milliseconds(16));}
 out<<"XInput polls: "<<polls<<"; changed packets: "<<changes<<"; rumble "<<(rumble?"requested and stopped":"disabled")<<".\n";return 0;
}
}
