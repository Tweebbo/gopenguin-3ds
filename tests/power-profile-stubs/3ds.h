#pragma once
#include <stdbool.h>
#include <stdint.h>
typedef uint8_t u8;
typedef int32_t Result;
#define R_SUCCEEDED(value) ((value)>=0)
void osSetSpeedupEnable(bool enabled);
Result ptmSysmInit(void);
Result PTMSYSM_ConfigureNew3DSCPU(u8 flags);
void ptmSysmExit(void);
