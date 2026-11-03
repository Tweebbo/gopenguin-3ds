#pragma once
#include "resource_file.h"
/* Loader-only stdio adapter: indexed RomFS reads use immutable SD objects.
 * Unindexed paths and writable files retain the normal stdio behavior. */
#define fopen resource_fopen
