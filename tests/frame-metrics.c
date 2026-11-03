#include "frame_metrics.h"
#include <assert.h>
#include <math.h>
#include <stdio.h>
int main(void){FrameMetrics m={0};assert(frame_metrics_summary(&m).count==0);for(unsigned n=0;n<99;n++)frame_metrics_add(&m,16667);frame_metrics_add(&m,100000);FrameSummary s=frame_metrics_summary(&m);assert(s.count==100&&fabs(s.p50_ms-16.667)<.001&&fabs(s.p95_ms-16.667)<.001&&fabs(s.p99_ms-16.667)<.001&&s.max_ms==100&&s.fps<60);puts("PASS: frame statistics retain stalls and distinguish average, percentiles and worst frame.");}
