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
/* Retained independent asynchronous operations (not GDK XAsync ABI).
   IDs are context- and logical-queue-scoped, aliases accepted. Max 64 retained
   operations per queue. Work runs on the work dispatcher; completion is called
   once on the dispatcher/cancellation/stop caller, outside the queue lock.
   Pending work alone can be canceled. Result is valid only on success. Wait
   observes terminal state, not notification completion; release requires both.
   A completion callback may query its operation but cannot release it yet.
   Keep user data alive through notification_done. Stop/join/release before last
   close. No callback may destroy the context or throw across this C interface. */
#define RQ_OPERATION_LIMIT 64u
typedef uint64_t rq_operation;
enum rq_operation_state { RQ_OP_QUEUED=1, RQ_OP_RUNNING=2, RQ_OP_CANCELING=3, RQ_OP_SUCCEEDED=4, RQ_OP_FAILED=5, RQ_OP_CANCELED=6 };
typedef struct rq_operation_snapshot { uint32_t size,state,work_status,notification_done,notification_status,reserved; uint64_t result; } rq_operation_snapshot;
typedef uint32_t (RQ_CALL *rq_operation_work)(void*,uint64_t* result);
typedef void (RQ_CALL *rq_operation_completion)(void*,rq_operation,const rq_operation_snapshot*);
RQ_API uint32_t RQ_CALL rq_operation_submit(rq_context*,rq_handle,uint32_t delay_ms,rq_operation_work,rq_operation_completion,void*,rq_operation* out);
RQ_API uint32_t RQ_CALL rq_operation_query(rq_context*,rq_handle,rq_operation,rq_operation_snapshot* out);
RQ_API uint32_t RQ_CALL rq_operation_wait(rq_context*,rq_handle,rq_operation,uint32_t timeout_ms,rq_operation_snapshot* out);
RQ_API uint32_t RQ_CALL rq_operation_cancel(rq_context*,rq_handle,rq_operation);
RQ_API uint32_t RQ_CALL rq_operation_release(rq_context*,rq_handle,rq_operation);
RQ_API uint32_t RQ_CALL rq_operation_count(rq_context*,rq_handle,uint32_t* out);
#ifdef __cplusplus
}
#endif
#endif
