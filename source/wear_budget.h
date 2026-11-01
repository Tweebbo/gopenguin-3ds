#pragma once
#include <stddef.h>
#include <stdint.h>

#define WEAR_BUDGET_MAX (16u*1024u*1024u)
#define WEAR_LINEAR_RESERVE (8u*1024u*1024u)

static inline size_t wear_budget_limit(size_t resident,size_t freeBytes){
 size_t total=freeBytes>SIZE_MAX-resident?SIZE_MAX:resident+freeBytes;
 if(total<=WEAR_LINEAR_RESERVE)return 0;
 size_t available=total-WEAR_LINEAR_RESERVE;
 return available<WEAR_BUDGET_MAX?available:WEAR_BUDGET_MAX;
}
