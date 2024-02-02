#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "wasm-rt.h"
#include "uvwasi-rt.h"
#include "uv-wasi-setup.inc.c"

#ifndef WASM_MODULE_NAME
#error "Expected WASM_MODULE_NAME to be defined"
#endif

#define APPEND2_HELPER(a, b) a##b
#define APPEND3_HELPER(a, b, c) a##b##c

#define APPEND2(a, b) APPEND2_HELPER(a, b)
#define APPEND3(a, b, c) APPEND3_HELPER(a, b, c)

#define MODULE_TYPE APPEND2(w2c_, WASM_MODULE_NAME)
#define MODULE_INSTANTIATE APPEND3(wasm2c_, WASM_MODULE_NAME, _instantiate)
#define MODULE_START APPEND3(w2c_, WASM_MODULE_NAME, _0x5Fstart)
#define MODULE_FREE APPEND3(wasm2c_, WASM_MODULE_NAME, _free)

#define MODULE_MEMORY_GETTER APPEND3(w2c_, WASM_MODULE_NAME, _memory)


int main(int argc, char const* argv[]) {

  uvwasi_t uvwasi;
  MODULE_TYPE mod;

  w2c_wasi__snapshot__preview1 wasi;
  wasi.uvwasi = &uvwasi;
  wasi.instance_memory = MODULE_MEMORY_GETTER(&mod);

  init_uvwasi_local(wasi.uvwasi, true /* mapRootSubdirs */, argc, argv);

  MODULE_INSTANTIATE(&mod, &wasi);

  MODULE_START(&mod);

  MODULE_FREE(&mod);

  wasm_rt_free();

  fflush(stdout);
  fflush(stderr);
  return 0;
}
