#ifndef REVENGE_NATIVE_ABI_H
#define REVENGE_NATIVE_ABI_H
#include <stdint.h>
#if defined(_WIN32)
# if defined(REVENGE_ABI_BUILD)
#  define RV_API __declspec(dllexport)
# else
#  define RV_API __declspec(dllimport)
# endif
# define RV_CALL __cdecl
#else
# define RV_API __attribute__((visibility("default")))
# define RV_CALL
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define RV_ABI_VERSION 1u
#define RV_MAX_TRANSFER 1048576u
typedef struct rv_session rv_session;
enum rv_status { RV_OK=0, RV_INVALID=1, RV_VERSION=2, RV_UNSUPPORTED=3, RV_RANGE=4, RV_STATE=5, RV_FAILURE=6 };
enum rv_op { RV_ALLOC=1, RV_FREE=2, RV_READ=3, RV_WRITE=4, RV_EVENT_CREATE=5, RV_EVENT_SET=6, RV_EVENT_RESET=7, RV_EVENT_WAIT=8, RV_EVENT_CLOSE=9, RV_CLOCK=10, RV_RENDER_VALIDATE=11, RV_STATS=12, RV_STOP=13, RV_GFX_CAPS=14, RV_GFX_LOAD=15, RV_GFX_RELEASE=16, RV_GFX_DRAW=17, RV_GFX_COUNT=18, RV_GFX_RUN=19, RV_RES_CREATE=20, RV_RES_RELEASE=21, RV_CMD_VALIDATE=22, RV_CMD_SUBMIT=23, RV_RES_COUNT=24, RV_RES_UPDATE=25, RV_JOB_SUBMIT=26, RV_JOB_POLL=27, RV_JOB_WAIT=28, RV_JOB_READ=29, RV_JOB_RELEASE=30, RV_JOB_COUNT=31, RV_MUTEX_CREATE=32, RV_MUTEX_LOCK=33, RV_MUTEX_UNLOCK=34, RV_MUTEX_CLOSE=35, RV_SEM_CREATE=36, RV_SEM_WAIT=37, RV_SEM_RELEASE=38, RV_SEM_CLOSE=39, RV_STORE_PUT=40, RV_STORE_GET=41, RV_STORE_SIZE=42, RV_STORE_ERASE=43, RV_SYNC_COUNT=44, RV_TLS_CREATE=45, RV_TLS_SET=46, RV_TLS_GET=47, RV_TLS_RELEASE=48, RV_TLS_DETACH=49, RV_TLS_COUNT=50, RV_THREAD_CREATE=51, RV_THREAD_POLL=52, RV_THREAD_WAIT=53, RV_THREAD_EXIT=54, RV_THREAD_RELEASE=55, RV_THREAD_COUNT=56, RV_MEDIA_CAPS=57, RV_AUDIO_PLAY=58, RV_PAD_POLL=59, RV_SNAPSHOT_SAVE=60, RV_SNAPSHOT_LOAD=61, RV_GFX_LOAD_CUSTOM=62, RV_RES_PATCH=63 };
/* MEDIA_CAPS: bits 1 PCM playback, 2 XInput, 4 snapshots. Compiled capabilities only.
   AUDIO_PLAY: count=bytes, buffer=48 kHz stereo PCM16, 20..2000 ms, synchronous.
   PAD_POLL: arg0=controller index 0..3; buffer=rv_pad_state; disconnected is success.
   SNAPSHOT_SAVE: arg0=UTF-8 path bytes (1..1024), count=payload bytes; exact capacity=path+payload.
   SNAPSHOT_LOAD: arg0=path bytes, capacity>=path and payload; overwrites buffer start on success.
   Trusted caller-selected local path; single writer; all other fields zero.
   GFX_LOAD_CUSTOM: same as GFX_LOAD but permits validated vertex/pixel-stage variants with fixed CS;
   only CMD_SUBMIT/JOB_SUBMIT/VALIDATE, not known-answer GFX_DRAW/GFX_RUN.
   RES_PATCH: handle=vertex/index/texture; offset and count=byte range, exact buffer capacity;
   aligned to vertex/uint16/RGBA8 unit; validates changed vertices; no resize. */
typedef struct rv_pad_state { uint32_t connected, buttons, packet, reserved; float lx,ly,rx,ry,lt,rt; } rv_pad_state;
#define RV_MEDIA_AUDIO 1u
#define RV_MEDIA_PAD 2u
#define RV_MEDIA_STORAGE 4u
/* Trusted host callbacks only. CREATE buffer: rv_thread_start, copied before return.
   WAIT arg0=0..10000 ms; POLL/WAIT return rv_thread_state. EXIT returns callback
   code when complete, RV_FAILURE if callback threw. RELEASE joins terminal thread.
   Up to 16 retained threads; STOP requires all released. No force termination.
   Callback must return, must not close the session, and must keep code alive until joined. */
typedef uint32_t (RV_CALL *rv_thread_proc)(rv_session*, uint64_t);
typedef struct rv_thread_start { rv_thread_proc proc; uint64_t argument; } rv_thread_start;
enum rv_thread_state { RV_THREAD_RUNNING=1, RV_THREAD_COMPLETE=2, RV_THREAD_FAILED=3 };
/* Native thread-local scalar slots, not PE TLS. SET: handle=slot, arg0=value (zero erases).
   GET: unset returns zero. RELEASE destroys slot; DETACH clears calling thread values.
   COUNT returns live slots. Up to 64 slots and 128 nonzero thread values per slot.
   Call DETACH before exiting a worker thread; STOP clears all slots. All other fields zero. */
/* Mutex lock and semaphore wait: handle, arg0=timeout ms; result=1 acquired/0 timeout.
   Mutex is nonrecursive and must be unlocked by its acquiring thread; close requires unlocked.
   SEM_CREATE: arg0=initial, arg1=maximum (1..65535). SEM_RELEASE: handle, arg0=release count;
   result=previous count. SYNC_COUNT: live mutex+semaphore handles (running session only).
   STORE_*: arg0=ASCII key length (1..64), buffer starts with key (no NUL).
   PUT: count=value length; exact capacity=key+value. GET: capacity >= key and value;
   overwrites buffer start with value on success. SIZE/ERASE: exact key capacity. SIZE returns bytes.
   Storage is session-local RAM, not a filesystem or persistence API. All other fields zero. */
/* Graphics capabilities indicate compiled backend, not hardware availability. */
#define RV_GFX_GPU 1u
#define RV_GFX_PRESENT 2u
#define RV_GFX_CONTINUOUS 4u
#define RV_GFX_CUSTOM 8u
#define RV_GFX_ASYNC 16u
/* UPDATE replaces all bytes of a vertex/index/texture resource, same length and dimensions.
   JOB_SUBMIT: handle=commands, arg0/arg1=width/height, offset=mode (0 GPU, 1 validate-only, 2 GPU quiet per-frame logging).
   Returns a job handle; at most four jobs (including completed) per session.
   POLL/WAIT: handle=job; WAIT arg0=timeout ms (0..10000), result=rv_job_state.
   READ: completed GPU job, exact RGBA buffer size; result=bytes.
   RELEASE: terminal job only; COUNT: retained job count. STOP drains then clears jobs. */
enum rv_job_state { RV_JOB_QUEUED=1, RV_JOB_RUNNING=2, RV_JOB_COMPLETE=3, RV_JOB_FAILED=4 };
/* RES_CREATE: arg0=kind, count=byte length, arg1=texture width, offset=texture height (zero otherwise).
   COMMANDS payload: rv_draw_header followed by draw_count rv_indexed_draw records.
   CMD_VALIDATE: handle=command resource, result=draw count.
   CMD_SUBMIT: handle=command resource, arg0=width, arg1=height, output packed RGBA8;
   320..512 width, 240..512 height; result=bytes written. Synchronous.
   RES_RELEASE: handle=resource; RES_COUNT: result=resource count (excludes packages). */
enum rv_resource_kind { RV_VERTEX=1, RV_INDEX=2, RV_TEXTURE=3, RV_COMMANDS=4 };
typedef struct rv_vertex { float x,y,z,u,v; } rv_vertex;
typedef struct rv_draw_header { uint64_t vertex,index,texture,package; uint32_t draw_count,reserved; } rv_draw_header;
typedef struct rv_indexed_draw { uint32_t first,count; } rv_indexed_draw;
/* RUN: handle=package, offset=FPS cap (0 uncapped), count=seconds,
   arg0=width, arg1=height; buffer=rv_frame_stats. Result=bytes written. */
typedef struct rv_frame_stats {
 uint64_t submitted, presented, elapsed_ns, gpu_ns, first_pixels, last_pixels;
 int64_t last_shift;
} rv_frame_stats;
/* All unused fields must be zero. READ/WRITE use a caller-owned byte buffer.
   This is a trusted native caller interface, not a guest pointer sandbox. */
typedef struct rv_request {
 uint32_t size, version, op, reserved;
 uint64_t handle, offset, count, arg0, arg1;
} rv_request;
typedef struct rv_stats {
 uint64_t memory_handles, events, storage_entries, storage_bytes, committed_bytes, running;
} rv_stats;
RV_API uint32_t RV_CALL rv_abi_version(void);
RV_API uint32_t RV_CALL rv_open(uint32_t version, rv_session** out);
/* result is a required writable scalar; buffer must be valid for buffer_size bytes.
   Request, result and buffer must not overlap. Do not close while calls are active. */
RV_API uint32_t RV_CALL rv_dispatch(rv_session* session,const rv_request* request,void* buffer,uint64_t buffer_size,uint64_t* result);
RV_API void RV_CALL rv_close(rv_session* session);
RV_API const char* RV_CALL rv_status_name(uint32_t status);
#ifdef __cplusplus
}
#endif
#endif
