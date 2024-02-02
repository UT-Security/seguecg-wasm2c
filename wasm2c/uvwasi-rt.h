#pragma once

#include "uvwasi.h"
#include "wasm-rt.h"

typedef struct w2c_wasi__snapshot__preview1
{
  uvwasi_t* uvwasi;
  wasm_rt_memory_t* instance_memory;
} w2c_wasi__snapshot__preview1;
