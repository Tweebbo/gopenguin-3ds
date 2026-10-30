#pragma once
typedef struct {unsigned tag;float depth;} RenderOrderItem;
void render_order_sort(RenderOrderItem *items,unsigned count,RenderOrderItem *scratch);
