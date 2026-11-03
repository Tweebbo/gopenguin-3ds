#pragma once
#include <stdint.h>
typedef struct {uint32_t samples[256],count,max;uint64_t total;} FrameMetrics;
typedef struct {unsigned count;double fps,mean_ms,p50_ms,p95_ms,p99_ms,max_ms;} FrameSummary;
void frame_metrics_add(FrameMetrics *metrics,uint32_t microseconds);
FrameSummary frame_metrics_summary(FrameMetrics *metrics);
