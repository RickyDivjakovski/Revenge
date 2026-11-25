/* Compiled as C, not C++; validates the public C ABI across the shared library. */
#include "revenge/native_abi.h"
#include <stddef.h>
#include <stdio.h>
#include <string.h>
typedef char request_layout[(sizeof(rv_request)==56 && offsetof(rv_request,handle)==16)?1:-1];
typedef char stats_layout[(sizeof(rv_stats)==48)?1:-1];
#define CHECK(condition) do {if(!(condition)){fprintf(stderr,"C ABI check failed at line %d\n",__LINE__);goto fail;}}while(0)
int main(void){
 rv_session* s=NULL;rv_request r={0};uint64_t h=0,out=0;uint8_t source[8]={1,2,3,4,5,6,7,8},dest[8]={0};rv_stats stats={0};
 CHECK(rv_abi_version()==RV_ABI_VERSION);CHECK(rv_open(RV_ABI_VERSION,&s)==RV_OK&&s);
 r.size=sizeof(r);r.version=RV_ABI_VERSION;r.op=RV_ALLOC;r.count=32;
 CHECK(rv_dispatch(s,&r,NULL,0,&h)==RV_OK&&h);
 r.op=RV_WRITE;r.handle=h;r.offset=7;r.count=sizeof(source);
 CHECK(rv_dispatch(s,&r,source,sizeof(source),&out)==RV_OK&&out==sizeof(source));
 r.op=RV_READ;CHECK(rv_dispatch(s,&r,dest,sizeof(dest),&out)==RV_OK&&out==sizeof(dest)&&memcmp(source,dest,sizeof(dest))==0);
 CHECK(rv_dispatch(s,&r,dest,4,&out)==RV_RANGE&&out==0);
 CHECK(rv_dispatch(s,&r,NULL,8,&out)==RV_INVALID&&out==0);
 r.op=RV_STOP;r.handle=0;r.offset=0;r.count=0;CHECK(rv_dispatch(s,&r,NULL,0,&out)==RV_OK);
 r.op=RV_STATS;CHECK(rv_dispatch(s,&r,&stats,sizeof(stats),&out)==RV_OK&&out==sizeof(stats)&&!stats.running&&!stats.committed_bytes&&!stats.memory_handles);
 rv_close(s);puts("C client: structure layout, 5-argument calls, nonzero-offset buffer round trip and shutdown passed.");return 0;
 fail:rv_close(s);return 1;
}
