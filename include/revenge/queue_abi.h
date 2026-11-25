#ifndef REVENGE_QUEUE_ABI_H
#define REVENGE_QUEUE_ABI_H
#include <stdint.h>
#if defined(_WIN32)
# if defined(REVENGE_QUEUE_BUILD)
#  define RQ_API __declspec(dllexport)
# else
#  define RQ_API __declspec(dllimport)
# endif
# define RQ_CALL __cdecl
#else
# define RQ_API __attribute__((visibility("default")))
# define RQ_CALL
#endif
#ifdef __cplusplus
extern "C" {
#endif
#define RQ_VERSION 1u
typedef struct rq_context rq_context;
typedef uint64_t rq_handle;
typedef uint64_t rq_ticket;
typedef void (RQ_CALL *rq_callback)(void*,uint32_t canceled);
enum rq_status { RQ_OK=0, RQ_INVALID=1, RQ_VERSION_ERROR=2, RQ_LIMIT=3, RQ_STATE=4, RQ_TIMEOUT=5, RQ_FAILURE=6 };
enum rq_port { RQ_WORK=0, RQ_COMPLETION=1 };
/* Independent trusted native C ABI, not GDK ABI. Callbacks run on dispatch/stop
   caller's thread outside locks. Keep code/context alive until callbacks return.
   Destroy requires all handles closed and all calls joined. */
RQ_API uint32_t RQ_CALL rq_open(uint32_t version,rq_context** out);
RQ_API uint32_t RQ_CALL rq_destroy(rq_context* ctx);
RQ_API uint32_t RQ_CALL rq_create(rq_context* ctx,rq_handle* out);
RQ_API uint32_t RQ_CALL rq_duplicate(rq_context* ctx,rq_handle h,rq_handle* out);
RQ_API uint32_t RQ_CALL rq_close(rq_context* ctx,rq_handle h);
RQ_API uint32_t RQ_CALL rq_submit(rq_context* ctx,rq_handle h,uint32_t port,uint32_t delay_ms,rq_callback cb,void* data);
/* Ticket is scoped to its context and logical queue (aliases are accepted).
   Cancel claims only pending tracked work; dispatch/stop/cancel race under the
   queue mutex. The winner invokes the callback exactly once. A nonpending or
   foreign ticket returns RQ_INVALID; this is not a retained operation status. */
RQ_API uint32_t RQ_CALL rq_submit_tracked(rq_context* ctx,rq_handle h,uint32_t port,uint32_t delay_ms,rq_callback cb,void* data,rq_ticket* out);
RQ_API uint32_t RQ_CALL rq_cancel(rq_context* ctx,rq_handle h,rq_ticket ticket);
/* Atomic queue snapshot shared by aliases; caller supplies structure size.
   On failure the caller's snapshot is not modified. */
typedef struct rq_snapshot { uint32_t size, stopped, canceling, aliases, active, pending_work, pending_completion; } rq_snapshot;
RQ_API uint32_t RQ_CALL rq_query(rq_context* ctx,rq_handle h,rq_snapshot* out);
/* Dispatch at most one ready callback. timeout 0 polls; max 10000 ms. */
RQ_API uint32_t RQ_CALL rq_dispatch(rq_context* ctx,rq_handle h,uint32_t port,uint32_t timeout_ms);
/* Stop cancels pending work synchronously; active callbacks finish independently.
   Last close rejects pending/active callbacks. Reentrant stop during cancellation
   returns RQ_STATE. Callbacks must not throw. */
RQ_API uint32_t RQ_CALL rq_stop(rq_context* ctx,rq_handle h);
#ifdef __cplusplus
}
#endif
#endif
