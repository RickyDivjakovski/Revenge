#include "revenge/queue_abi.h"
#include <stdio.h>
#define CHECK(x) do {if(!(x)){fprintf(stderr,"C ABI failure line %d\n",__LINE__);return 1;}}while(0)
struct trace {unsigned calls,canceled,sum;};
struct item {struct trace* t;unsigned value;};
static void RQ_CALL count(void* p,uint32_t canceled){struct item* i=p;if(canceled)++i->t->canceled;else{++i->t->calls;i->t->sum+=i->value;}}
int main(void){rq_context* c=0;rq_handle h=0,a=0;rq_snapshot snapshot={sizeof(rq_snapshot)};struct trace t={0};struct item w={&t,7},d={&t,11},p={&t,13};
 CHECK(rq_open(999,&c)==RQ_VERSION_ERROR&&!c);CHECK(rq_open(RQ_VERSION,&c)==RQ_OK&&c);CHECK(rq_create(c,&h)==RQ_OK);CHECK(rq_duplicate(c,h,&a)==RQ_OK&&a!=h);CHECK(rq_query(c,a,&snapshot)==RQ_OK&&snapshot.aliases==2);CHECK(rq_close(c,h)==RQ_OK);CHECK(rq_dispatch(c,h,RQ_WORK,0)==RQ_INVALID);
 CHECK(rq_submit(c,a,RQ_WORK,20,count,&w)==RQ_OK);CHECK(rq_submit(c,a,RQ_COMPLETION,0,count,&d)==RQ_OK);CHECK(rq_query(c,a,&snapshot)==RQ_OK&&snapshot.pending_work==1&&snapshot.pending_completion==1&&snapshot.aliases==1);CHECK(rq_dispatch(c,a,RQ_COMPLETION,0)==RQ_OK);CHECK(rq_dispatch(c,a,RQ_WORK,2000)==RQ_OK);
 CHECK(rq_submit(c,a,RQ_WORK,60000,count,&p)==RQ_OK);CHECK(rq_close(c,a)==RQ_STATE);CHECK(rq_destroy(c)==RQ_STATE);CHECK(rq_stop(c,a)==RQ_OK);CHECK(rq_stop(c,a)==RQ_OK);CHECK(rq_query(c,a,&snapshot)==RQ_OK&&snapshot.stopped==1&&snapshot.pending_work==0&&snapshot.pending_completion==0);CHECK(rq_close(c,a)==RQ_OK);CHECK(rq_query(c,a,&snapshot)==RQ_INVALID&&snapshot.stopped==1);CHECK(rq_destroy(c)==RQ_OK);
 CHECK(rq_open(RQ_VERSION,&c)==RQ_OK&&rq_create(c,&h)==RQ_OK);{rq_handle handles[64];unsigned i;handles[0]=h;for(i=1;i<64;++i)CHECK(rq_duplicate(c,h,&handles[i])==RQ_OK);CHECK(rq_duplicate(c,h,&a)==RQ_LIMIT&&a==0);CHECK(rq_submit(c,h,99,0,count,&w)==RQ_INVALID);CHECK(rq_submit(c,h,RQ_WORK,60001,count,&w)==RQ_INVALID);CHECK(rq_dispatch(c,h,RQ_WORK,10001)==RQ_INVALID);for(i=0;i<64;++i)CHECK(rq_close(c,handles[i])==RQ_OK);}CHECK(rq_destroy(c)==RQ_OK);
 CHECK(t.calls==2&&t.sum==18&&t.canceled==1);puts("REVENGE-DELAY-V1 work=1 completion=1 sum=18 canceled=1 alias=1");puts("Independent C queue ABI: version, alias lifetime, real-time delay, cancellation, snapshots and teardown passed.");return 0;}
