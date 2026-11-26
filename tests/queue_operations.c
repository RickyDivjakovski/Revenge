#include "revenge/queue_abi.h"
#include <stdio.h>
#include <stdlib.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"operation C fixture line %d\n",__LINE__);return 1;}}while(0)
struct data {rq_context* ctx;rq_handle h;unsigned work,done,bad;uint32_t status;};
static uint32_t RQ_CALL work(void* p,uint64_t* result){struct data* d=p;++d->work;*result=12345;return d->status;}
static void RQ_CALL done(void* p,rq_operation id,const rq_operation_snapshot* notice){struct data* d=p;rq_operation_snapshot s={sizeof(s)};++d->done;if(rq_operation_query(d->ctx,d->h,id,&s)!=RQ_OK||s.state!=notice->state||s.notification_done||rq_operation_release(d->ctx,d->h,id)!=RQ_STATE)++d->bad;}
int main(void){rq_context* c=0;rq_handle h=0,a=0,f=0;rq_operation id=0,fail=0,cancel=0;uint32_t count=0;rq_operation_snapshot s={sizeof(s)};
 CHECK(rq_open(RQ_VERSION,&c)==RQ_OK&&rq_create(c,&h)==RQ_OK&&rq_duplicate(c,h,&a)==RQ_OK&&rq_create(c,&f)==RQ_OK);
 struct data d={c,a,0,0,0,RQ_OK};
 CHECK(rq_operation_submit(c,h,0,work,done,&d,&id)==RQ_OK&&id);
 CHECK(rq_operation_query(c,a,id,&s)==RQ_OK&&s.state==RQ_OP_QUEUED);
 CHECK(rq_operation_wait(c,h,id,0,&s)==RQ_TIMEOUT&&s.state==RQ_OP_QUEUED);
 CHECK(rq_operation_query(c,f,id,&s)==RQ_INVALID&&s.state==RQ_OP_QUEUED);
 CHECK(rq_operation_release(c,h,id)==RQ_STATE&&rq_dispatch(c,a,RQ_WORK,0)==RQ_OK);
 CHECK(rq_operation_wait(c,h,id,0,&s)==RQ_OK&&s.state==RQ_OP_SUCCEEDED&&s.result==12345&&s.notification_done&&s.work_status==RQ_OK);
 d.status=RQ_FAILURE;CHECK(rq_operation_submit(c,h,0,work,done,&d,&fail)==RQ_OK&&rq_dispatch(c,h,RQ_WORK,0)==RQ_OK);
 CHECK(rq_operation_query(c,h,fail,&s)==RQ_OK&&s.state==RQ_OP_FAILED&&s.work_status==RQ_FAILURE&&s.result==0&&s.notification_done);
 CHECK(rq_operation_submit(c,h,60000,work,done,&d,&cancel)==RQ_OK&&rq_operation_cancel(c,a,cancel)==RQ_OK);
 CHECK(rq_operation_query(c,h,cancel,&s)==RQ_OK&&s.state==RQ_OP_CANCELED&&s.result==0&&s.notification_done);
 CHECK(rq_operation_cancel(c,h,cancel)==RQ_STATE&&d.work==2&&d.done==3&&!d.bad);
 CHECK(rq_operation_count(c,a,&count)==RQ_OK&&count==3&&rq_close(c,h)==RQ_OK&&rq_close(c,a)==RQ_STATE);
 CHECK(rq_operation_release(c,a,id)==RQ_OK&&rq_operation_query(c,a,id,&s)==RQ_INVALID&&rq_operation_release(c,a,fail)==RQ_OK&&rq_operation_release(c,a,cancel)==RQ_OK);
 rq_operation ids[RQ_OPERATION_LIMIT];for(unsigned i=0;i<RQ_OPERATION_LIMIT;++i)CHECK(rq_operation_submit(c,a,60000,work,done,&d,&ids[i])==RQ_OK);
 CHECK(rq_operation_submit(c,a,0,work,done,&d,&id)==RQ_LIMIT&&!id&&rq_stop(c,a)==RQ_OK);
 CHECK(d.work==2&&d.done==3+RQ_OPERATION_LIMIT&&!d.bad);
 for(unsigned i=0;i<RQ_OPERATION_LIMIT;++i){CHECK(rq_operation_query(c,a,ids[i],&s)==RQ_OK&&s.state==RQ_OP_CANCELED&&s.notification_done);CHECK(rq_operation_release(c,a,ids[i])==RQ_OK);}
 CHECK(rq_operation_count(c,a,&count)==RQ_OK&&!count&&rq_operation_submit(c,a,0,work,done,&d,&id)==RQ_STATE&&rq_close(c,a)==RQ_OK&&rq_close(c,f)==RQ_OK&&rq_destroy(c)==RQ_OK);
 puts("Retained C operations: result, failure, timeout, alias, cancellation, notification, capacity and stop passed.");return 0;}
