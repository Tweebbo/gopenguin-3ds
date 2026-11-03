#include "frame_metrics.h"
#include <stdlib.h>
static int compare(const void *a,const void *b){uint32_t x=*(const uint32_t *)a,y=*(const uint32_t *)b;return (x>y)-(x<y);}
void frame_metrics_add(FrameMetrics *m,uint32_t us){if(m->count<256)m->samples[m->count]=us;m->count++;m->total+=us;if(us>m->max)m->max=us;}
FrameSummary frame_metrics_summary(FrameMetrics *m){
 FrameSummary s={.count=m->count};if(!m->count)return s;
 unsigned n=m->count<256?m->count:256;qsort(m->samples,n,sizeof *m->samples,compare);
 s.fps=m->total?(double)m->count*1000000/m->total:0;s.mean_ms=(double)m->total/m->count/1000;s.max_ms=m->max/1000.0;
 s.p50_ms=m->samples[(n*50+99)/100-1]/1000.0;s.p95_ms=m->samples[(n*95+99)/100-1]/1000.0;s.p99_ms=m->samples[(n*99+99)/100-1]/1000.0;return s;
}
