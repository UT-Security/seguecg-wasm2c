const char* s_source_declarations = R"w2c_template(
#define TRAP(x) (wasm_rt_trap(WASM_RT_TRAP_##x), 0)
)w2c_template"
R"w2c_template(
#if WASM_RT_USE_STACK_DEPTH_COUNT
)w2c_template"
R"w2c_template(#define FUNC_PROLOGUE                                            \
)w2c_template"
R"w2c_template(  if (++wasm_rt_call_stack_depth > WASM_RT_MAX_CALL_STACK_DEPTH) \
)w2c_template"
R"w2c_template(    TRAP(EXHAUSTION);
)w2c_template"
R"w2c_template(
#define FUNC_EPILOGUE --wasm_rt_call_stack_depth
)w2c_template"
R"w2c_template(#else
)w2c_template"
R"w2c_template(#define FUNC_PROLOGUE
)w2c_template"
R"w2c_template(
#define FUNC_EPILOGUE
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(
#define UNREACHABLE TRAP(UNREACHABLE)
)w2c_template"
R"w2c_template(
#if WASM_RT_MEMCHECK_MASK_PDEP
)w2c_template"
R"w2c_template(#include <immintrin.h>
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(
static inline bool func_types_eq(const wasm_rt_func_type_t a,
)w2c_template"
R"w2c_template(                                 const wasm_rt_func_type_t b) {
)w2c_template"
R"w2c_template(  return (a == b) || LIKELY(a && b && !memcmp(a, b, 32));
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
#define CALL_INDIRECT(table, t, ft, x, ...)              \
)w2c_template"
R"w2c_template(  (LIKELY((x) < table.size && table.data[x].func &&      \
)w2c_template"
R"w2c_template(          func_types_eq(ft, table.data[x].func_type)) || \
)w2c_template"
R"w2c_template(       TRAP(CALL_INDIRECT),                              \
)w2c_template"
R"w2c_template(   ((t)table.data[x].func)(__VA_ARGS__))
)w2c_template"
R"w2c_template(
#ifdef SUPPORT_MEMORY64
)w2c_template"
R"w2c_template(#define RANGE_CHECK(mem, offset, len)              \
)w2c_template"
R"w2c_template(  do {                                             \
)w2c_template"
R"w2c_template(    uint64_t res;                                  \
)w2c_template"
R"w2c_template(    if (__builtin_add_overflow(offset, len, &res)) \
)w2c_template"
R"w2c_template(      TRAP(OOB);                                   \
)w2c_template"
R"w2c_template(    if (UNLIKELY(res > mem->size))                 \
)w2c_template"
R"w2c_template(      TRAP(OOB);                                   \
)w2c_template"
R"w2c_template(  } while (0);
)w2c_template"
R"w2c_template(#else
)w2c_template"
R"w2c_template(#define RANGE_CHECK(mem, offset, len)               \
)w2c_template"
R"w2c_template(  if (UNLIKELY(offset + (uint64_t)len > mem->size)) \
)w2c_template"
R"w2c_template(    TRAP(OOB);
)w2c_template"
R"w2c_template(
#define RANGE_CHECK_MASKED(mem, offset, len)             \
)w2c_template"
R"w2c_template(  do {                                                   \
)w2c_template"
R"w2c_template(    uint8_t is_oob = offset + (uint64_t)len > mem->size; \
)w2c_template"
R"w2c_template(    uint64_t zero = -1;                                  \
)w2c_template"
R"w2c_template(    if (UNLIKELY(is_oob))                                \
)w2c_template"
R"w2c_template(      TRAP(OOB);                                         \
)w2c_template"
R"w2c_template(    asm("test %[is_oob],%[is_oob]\n"                     \
)w2c_template"
R"w2c_template(        "cmovne %[zero], %[offset_var]\n"                \
)w2c_template"
R"w2c_template(        : [offset_var] "+r"(offset)                      \
)w2c_template"
R"w2c_template(        : [is_oob] "r"(is_oob), [zero] "r"(zero)         \
)w2c_template"
R"w2c_template(        : "cc");                                         \
)w2c_template"
R"w2c_template(  } while (0)
)w2c_template"
R"w2c_template(
#if WASM_RT_MEMCHECK_BOUNDS_CHECK_TRAP_SCHEME == 1
)w2c_template"
R"w2c_template(#define RANGE_CHECK_TRAP(mem, offset, len)          \
)w2c_template"
R"w2c_template(  offset = (UNLIKELY(offset + (uint64_t)len > mem->size)) ? mem->size : offset;
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_BOUNDS_CHECK_TRAP_SCHEME == 2
)w2c_template"
R"w2c_template(#define RANGE_CHECK_TRAP(mem, offset, len)          \
)w2c_template"
R"w2c_template(  offset = (UNLIKELY(offset + (uint64_t)len > mem->size)) ? -1 : offset;
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(
#define RANGE_CHECK_ASM(mem, offset, len)                             \
)w2c_template"
R"w2c_template(  do {                                                                \
)w2c_template"
R"w2c_template(    bool is_oob;                                                      \
)w2c_template"
R"w2c_template(    uint64_t offset_plus_len = offset + len;                          \
)w2c_template"
R"w2c_template(    uint64_t size = mem->size;                                        \
)w2c_template"
R"w2c_template(    if (!__builtin_constant_p(offset)) {                              \
)w2c_template"
R"w2c_template(      asm("cmpq %[offset_plus_len], %[size]\n"                        \
)w2c_template"
R"w2c_template(          : "=@ccb"(is_oob)                                           \
)w2c_template"
R"w2c_template(          : [offset_plus_len] "rm"(offset_plus_len), [size] "r"(size) \
)w2c_template"
R"w2c_template(          : "cc");                                                    \
)w2c_template"
R"w2c_template(      if (UNLIKELY(is_oob))                                           \
)w2c_template"
R"w2c_template(        TRAP(OOB);                                                    \
)w2c_template"
R"w2c_template(    }                                                                 \
)w2c_template"
R"w2c_template(  } while (0)
)w2c_template"
R"w2c_template(
#define RANGE_CHECK_ASM_MASKED(mem, offset, len)                       \
)w2c_template"
R"w2c_template(  do {                                                                 \
)w2c_template"
R"w2c_template(    uint64_t zero = 0;                                                 \
)w2c_template"
R"w2c_template(    bool is_oob;                                                       \
)w2c_template"
R"w2c_template(    uint64_t offset_plus_len = offset + len;                           \
)w2c_template"
R"w2c_template(    uint64_t size = mem->size;                                         \
)w2c_template"
R"w2c_template(    if (!__builtin_constant_p(offset)) {                               \
)w2c_template"
R"w2c_template(      asm("cmpq %[offset_plus_len], %[size]\n"                         \
)w2c_template"
R"w2c_template(          "cmovbq %[zero], %0\n"                                       \
)w2c_template"
R"w2c_template(          : "+r"(offset), "=@ccb"(is_oob)                              \
)w2c_template"
R"w2c_template(          : [offset_plus_len] "rm"(offset_plus_len), [size] "r"(size), \
)w2c_template"
R"w2c_template(            [zero] "rm"(zero)                                          \
)w2c_template"
R"w2c_template(          : "cc");                                                     \
)w2c_template"
R"w2c_template(      if (UNLIKELY(is_oob))                                            \
)w2c_template"
R"w2c_template(        TRAP(OOB);                                                     \
)w2c_template"
R"w2c_template(    }                                                                  \
)w2c_template"
R"w2c_template(  } while (0)
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(
#ifdef __GNUC__
)w2c_template"
R"w2c_template(#define FORCE_READ_INT(var) __asm__("" ::"r"(var));
)w2c_template"
R"w2c_template(// Clang on Mips requires "f" constraints on floats
)w2c_template"
R"w2c_template(// See https://github.com/llvm/llvm-project/issues/64241
)w2c_template"
R"w2c_template(#if defined(__clang__) && \
)w2c_template"
R"w2c_template(    (defined(mips) || defined(__mips__) || defined(__mips))
)w2c_template"
R"w2c_template(#define FORCE_READ_FLOAT(var) __asm__("" ::"f"(var));
)w2c_template"
R"w2c_template(#else
)w2c_template"
R"w2c_template(#define FORCE_READ_FLOAT(var) __asm__("" ::"r"(var));
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(#else
)w2c_template"
R"w2c_template(#error "No force read supported"
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(
#define WASM_RT_GS_REF(TYPE, ptr) (*((TYPE __seg_gs*)(uintptr_t)(ptr)))
)w2c_template"
R"w2c_template(
#if WASM_RT_MEMCHECK_GUARD_PAGES
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t)
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_PAGE
)w2c_template"
R"w2c_template(
// Access to the first 64k Wasm page should map to an access of the first 4k
)w2c_template"
R"w2c_template(// shadow page Access to the second 64k Wasm page should map to an access of the
)w2c_template"
R"w2c_template(// second 4k shadow page
)w2c_template"
R"w2c_template(// ... and so on ...
)w2c_template"
R"w2c_template(
#if WASM_RT_USE_SHADOW_SEGUE
)w2c_template"
R"w2c_template(#if WASM_RT_MEMCHECK_SHADOW_PAGE_SCHEME == 1 && !WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) FORCE_READ_INT(WASM_RT_GS_REF(u8, a >> 4))
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_PAGE_SCHEME == 2 && !WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) FORCE_READ_INT(WASM_RT_GS_REF(u8, (a >> 16) << 12))
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_PAGE_SCHEME == 1 && WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) a = (WASM_RT_GS_REF(u8, a >> 4)) ? 0 : a
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_PAGE_SCHEME == 2 && WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) a = (WASM_RT_GS_REF(u8, (a >> 16) << 12)) ? 0 : a
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_PAGE_SCHEME == 3
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) WASM_RT_GS_REF(u8, a >> 4) = 0
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_PAGE_SCHEME == 4
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) WASM_RT_GS_REF(u8, (a >> 16) << 12) = 0
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_PAGE_SCHEME == 5
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) FORCE_READ_INT(WASM_RT_GS_REF(u8, a >> 40))
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(#else
)w2c_template"
R"w2c_template(#if WASM_RT_MEMCHECK_SHADOW_PAGE_SCHEME == 1 && !WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) FORCE_READ_INT(mem->shadow_memory[a >> 4])
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_PAGE_SCHEME == 2 && !WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) FORCE_READ_INT(mem->shadow_memory[(a >> 16) << 12])
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_PAGE_SCHEME == 1 && WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) a = (mem->shadow_memory[a >> 4]) ? 0 : a
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_PAGE_SCHEME == 2 && WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) a = (mem->shadow_memory[(a >> 16) << 12]) ? 0 : a
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_PAGE_SCHEME == 3
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) mem->shadow_memory[a >> 4] = 0
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_PAGE_SCHEME == 4
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) mem->shadow_memory[(a >> 16) << 12] = 0
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_PAGE_SCHEME == 5
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) FORCE_READ_INT(mem->shadow_memory[a >> 40])
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(
#elif WASM_RT_MEMCHECK_PRESHADOW_PAGE
)w2c_template"
R"w2c_template(
#if WASM_RT_USE_SHADOW_SEGUE
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) FORCE_READ_INT(WASM_RT_GS_REF(u8, -(a >> 16) - 1))
)w2c_template"
R"w2c_template(#else
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) FORCE_READ_INT(mem->data[-(a >> 16) - 1])
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(
#elif WASM_RT_MEMCHECK_SHADOW_BYTES_TAG
)w2c_template"
R"w2c_template(  bool floatzone_y_init_done = false;
)w2c_template"
R"w2c_template(  float floatzone_y;
)w2c_template"
R"w2c_template(#if WASM_RT_USE_SHADOW_SEGUE
)w2c_template"
R"w2c_template(
#if WASM_RT_MEMCHECK_SHADOW_BYTES_TAG_SCHEME == 1
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) FORCE_READ_FLOAT(WASM_RT_GS_REF(float, a >> 32) + floatzone_y)
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_BYTES_TAG_SCHEME == 2
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) asm("addss  %%gs:0x0,%%xmm15\n" \
)w2c_template"
R"w2c_template(        :                                                 \
)w2c_template"
R"w2c_template(        :                                                 \
)w2c_template"
R"w2c_template(        : "xmm15");
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(
#else
)w2c_template"
R"w2c_template(
#if WASM_RT_MEMCHECK_SHADOW_BYTES_TAG_SCHEME == 1
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) FORCE_READ_FLOAT(mem->shadow_bytes[(u32)(a >> 32)] + floatzone_y)
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_BYTES_TAG_SCHEME == 2
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t)                                  \
)w2c_template"
R"w2c_template(do {                                                         \
)w2c_template"
R"w2c_template(float* tag_ptr = &mem->shadow_bytes[(u32)(a >> 32)];         \
)w2c_template"
R"w2c_template(asm("addss  %[tag_ptr],%%xmm15\n"                             \
)w2c_template"
R"w2c_template(        :                                                    \
)w2c_template"
R"w2c_template(        :  [tag_ptr] "m"(tag_ptr)                            \
)w2c_template"
R"w2c_template(        : "xmm15");                                          \
)w2c_template"
R"w2c_template(} while(0)
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(
#endif
)w2c_template"
R"w2c_template(
#elif WASM_RT_MEMCHECK_SHADOW_BYTES
)w2c_template"
R"w2c_template(
#if WASM_RT_USE_SHADOW_SEGUE
)w2c_template"
R"w2c_template(#if WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 1 && !WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) FORCE_READ_INT(WASM_RT_GS_REF(u32, (a >> 32) << 2))
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 2 && !WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) FORCE_READ_INT(WASM_RT_GS_REF(u8, a >> 32))
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 3 && !WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) FORCE_READ_INT(WASM_RT_GS_REF(u8, a >> 16))
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 4 && !WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) FORCE_READ_INT(WASM_RT_GS_REF(u8, a >> 24))
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 1 && WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) a = (WASM_RT_GS_REF(u32, (a >> 32) << 2)) ? 0 : a
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 2 && WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) a = (WASM_RT_GS_REF(u8, a >> 32)) ? 0 : a
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 3 && WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) a = (WASM_RT_GS_REF(u8, a >> 16)) ? 0 : a
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 4 && WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) a = (WASM_RT_GS_REF(u8, a >> 24)) ? 0 : a
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 5 && !WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) WASM_RT_GS_REF(u32, (a >> 32) << 2) = 0
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 6 && !WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) WASM_RT_GS_REF(u8, a >> 32) = 0
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 7 && !WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) WASM_RT_GS_REF(u8, a >> 16) = 0
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 8 && !WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) WASM_RT_GS_REF(u8, a >> 24) = 0
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(#else
)w2c_template"
R"w2c_template(#if WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 1 && !WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) FORCE_READ_INT(mem->shadow_bytes[(u32)(a >> 32)])
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 2 && !WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) FORCE_READ_INT(mem->shadow_bytes[(u32)(a >> 32)])
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 3 && !WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) FORCE_READ_INT(mem->shadow_bytes[a >> 16])
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 4 && !WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) FORCE_READ_INT(mem->shadow_bytes[a >> 24])
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 1 && WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) a = (mem->shadow_bytes[(u32)(a >> 32)]) ? 0 : a
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 2 && WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) a = (mem->shadow_bytes[(u32)(a >> 32)]) ? 0 : a
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 3 && WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) a = (mem->shadow_bytes[a >> 16]) ? 0 : a
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 4 && WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) a = (mem->shadow_bytes[a >> 24]) ? 0 : a
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 5 && !WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) mem->shadow_bytes[(u32)(a >> 32)] = 0
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 6 && !WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) mem->shadow_bytes[(u32)(a >> 32)] = 0
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 7 && !WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) mem->shadow_bytes[a >> 16] = 0
)w2c_template"
R"w2c_template(#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 8 && !WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) mem->shadow_bytes[a >> 24] = 0
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(
#elif WASM_RT_MEMCHECK_PRESHADOW_BYTES
)w2c_template"
R"w2c_template(
#if WASM_RT_USE_SHADOW_SEGUE && !WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t)      \
)w2c_template"
R"w2c_template(  FORCE_READ_INT(WASM_RT_GS_REF( \
)w2c_template"
R"w2c_template(      u8, -mem->shadow_bytes_distance_from_heap + (u32)(a >> 32)))
)w2c_template"
R"w2c_template(#elif WASM_RT_USE_SHADOW_SEGUE && WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t)                                                    \
)w2c_template"
R"w2c_template(  a = (WASM_RT_GS_REF(u8,                                                      \
)w2c_template"
R"w2c_template(                      -mem->shadow_bytes_distance_from_heap + (u32)(a >> 32))) \
)w2c_template"
R"w2c_template(          ? 0                                                                  \
)w2c_template"
R"w2c_template(          : a
)w2c_template"
R"w2c_template(#elif !WASM_RT_USE_SHADOW_SEGUE && !WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) \
)w2c_template"
R"w2c_template(  FORCE_READ_INT(           \
)w2c_template"
R"w2c_template(      mem->data[-mem->shadow_bytes_distance_from_heap + (u32)(a >> 32)])
)w2c_template"
R"w2c_template(#elif !WASM_RT_USE_SHADOW_SEGUE && WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t)                                                   \
)w2c_template"
R"w2c_template(  a = (mem->data[-mem->shadow_bytes_distance_from_heap + (u32)(a >> 32)]) ? 0 \
)w2c_template"
R"w2c_template(                                                                          : a
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(
#elif WASM_RT_MEMCHECK_DEBUG_WATCH
)w2c_template"
R"w2c_template(
#if WASM_RT_USE_SHADOW_SEGUE
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) WASM_RT_GS_REF(u64, 0) = a
)w2c_template"
R"w2c_template(#else
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) mem->debug_watch_buffer = a
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(
#elif WASM_RT_MEMCHECK_MPX
)w2c_template"
R"w2c_template(
#define MEMCHECK(mem, a, t)           \
)w2c_template"
R"w2c_template(    asm volatile(                     \
)w2c_template"
R"w2c_template(        "bndcu %[addr_val], %%bnd1\n" \
)w2c_template"
R"w2c_template(        :                             \
)w2c_template"
R"w2c_template(        : [addr_val] "r"(a)           \
)w2c_template"
R"w2c_template(        :)
)w2c_template"
R"w2c_template(
#elif WASM_RT_MEMCHECK_MASK_PDEP
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t)           \
)w2c_template"
R"w2c_template(    a = a | _pdep_u64(a, (uint64_t)0xf00000000fffffff)
)w2c_template"
R"w2c_template(
#elif WASM_RT_MEMCHECK_BOUNDS_CHECK_ASM
)w2c_template"
R"w2c_template(
#if WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) RANGE_CHECK_ASM_MASKED(mem, a, sizeof(t))
)w2c_template"
R"w2c_template(#else
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) RANGE_CHECK_ASM(mem, a, sizeof(t))
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(
#elif WASM_RT_MEMCHECK_BOUNDS_CHECK_TRAP
)w2c_template"
R"w2c_template(
#if WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#error "Not impl"
)w2c_template"
R"w2c_template(#else
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) RANGE_CHECK_TRAP(mem, a, sizeof(t))
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(
#elif WASM_RT_MEMCHECK_BOUNDS_CHECK
)w2c_template"
R"w2c_template(
#if WASM_RT_SPECTREMASK
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) RANGE_CHECK_MASKED(mem, a, sizeof(t))
)w2c_template"
R"w2c_template(#else
)w2c_template"
R"w2c_template(#define MEMCHECK(mem, a, t) RANGE_CHECK(mem, a, sizeof(t))
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(
#endif
)w2c_template"
R"w2c_template(
#ifdef WASM_RT_NOINLINE
)w2c_template"
R"w2c_template(#define MAYBEINLINE __attribute__((noinline))
)w2c_template"
R"w2c_template(#else
)w2c_template"
R"w2c_template(#define MAYBEINLINE inline
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(
#if WASM_RT_USE_SEGUE
)w2c_template"
R"w2c_template(
#define MEMCPY_GS(TYPE)                                                  \
)w2c_template"
R"w2c_template(  static MAYBEINLINE void memcpyfromgs_##TYPE(TYPE* target, u64 index) { \
)w2c_template"
R"w2c_template(    TYPE __seg_gs* source = (TYPE __seg_gs*)(uintptr_t)index;            \
)w2c_template"
R"w2c_template(    *target = *source;                                                   \
)w2c_template"
R"w2c_template(  }                                                                      \
)w2c_template"
R"w2c_template(  static MAYBEINLINE void memcpytogs_##TYPE(u64 index, TYPE* source) {   \
)w2c_template"
R"w2c_template(    TYPE __seg_gs* target = (TYPE __seg_gs*)(uintptr_t)index;            \
)w2c_template"
R"w2c_template(    *target = *source;                                                   \
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(
MEMCPY_GS(u8);
)w2c_template"
R"w2c_template(MEMCPY_GS(s8);
)w2c_template"
R"w2c_template(MEMCPY_GS(u16);
)w2c_template"
R"w2c_template(MEMCPY_GS(s16);
)w2c_template"
R"w2c_template(MEMCPY_GS(u32);
)w2c_template"
R"w2c_template(MEMCPY_GS(s32);
)w2c_template"
R"w2c_template(MEMCPY_GS(u64);
)w2c_template"
R"w2c_template(MEMCPY_GS(s64);
)w2c_template"
R"w2c_template(MEMCPY_GS(f32);
)w2c_template"
R"w2c_template(MEMCPY_GS(f64);
)w2c_template"
R"w2c_template(
#endif
)w2c_template"
R"w2c_template(
static void init_memchk() {
)w2c_template"
R"w2c_template(#if WASM_RT_MEMCHECK_SHADOW_BYTES_TAG
)w2c_template"
R"w2c_template(  if (!floatzone_y_init_done) {
)w2c_template"
R"w2c_template(    uint32_t crash_value_int = 0x8b8b8b8b;
)w2c_template"
R"w2c_template(    memcpy(&floatzone_y, &crash_value_int, sizeof(float));
)w2c_template"
R"w2c_template(    floatzone_y_init_done = true;
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
#if WABT_BIG_ENDIAN
)w2c_template"
R"w2c_template(#error "Big endian not supported"
)w2c_template"
R"w2c_template(#else
)w2c_template"
R"w2c_template(static MAYBEINLINE void load_data(void* dest, const void* src, size_t n) {
)w2c_template"
R"w2c_template(  if (!n) {
)w2c_template"
R"w2c_template(    return;
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  wasm_rt_memcpy(dest, src, n);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(#define LOAD_DATA(m, o, i, s)      \
)w2c_template"
R"w2c_template(  do {                             \
)w2c_template"
R"w2c_template(    RANGE_CHECK((&m), o, s);       \
)w2c_template"
R"w2c_template(    load_data(&(m.data[o]), i, s); \
)w2c_template"
R"w2c_template(  } while (0)
)w2c_template"
R"w2c_template(#define DEFINE_LOAD_REGULAR(name, t1, t2, t3, force_read)       \
)w2c_template"
R"w2c_template(  static MAYBEINLINE t3 name(wasm_rt_memory_t* mem, u64 addr) { \
)w2c_template"
R"w2c_template(    MEMCHECK(mem, addr, t1);                                    \
)w2c_template"
R"w2c_template(    t1 result;                                                  \
)w2c_template"
R"w2c_template(    wasm_rt_memcpy(&result, &mem->data[addr], sizeof(t1));      \
)w2c_template"
R"w2c_template(    force_read(result);                                         \
)w2c_template"
R"w2c_template(    return (t3)(t2)result;                                      \
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(
#define DEFINE_STORE_REGULAR(name, t1, t2)                                  \
)w2c_template"
R"w2c_template(  static MAYBEINLINE void name(wasm_rt_memory_t* mem, u64 addr, t2 value) { \
)w2c_template"
R"w2c_template(    MEMCHECK(mem, addr, t1);                                                \
)w2c_template"
R"w2c_template(    t1 wrapped = (t1)value;                                                 \
)w2c_template"
R"w2c_template(    wasm_rt_memcpy(&mem->data[addr], &wrapped, sizeof(t1));                 \
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(
#define DEFINE_LOAD_GS(name, t1, t2, t3, force_read)            \
)w2c_template"
R"w2c_template(  static MAYBEINLINE t3 name(wasm_rt_memory_t* mem, u64 addr) { \
)w2c_template"
R"w2c_template(    MEMCHECK(mem, addr, t1);                                    \
)w2c_template"
R"w2c_template(    t1 result;                                                  \
)w2c_template"
R"w2c_template(    memcpyfromgs_##t1(&result, addr);                           \
)w2c_template"
R"w2c_template(    force_read(result);                                         \
)w2c_template"
R"w2c_template(    return (t3)(t2)result;                                      \
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(
#define DEFINE_STORE_GS(name, t1, t2)                                       \
)w2c_template"
R"w2c_template(  static MAYBEINLINE void name(wasm_rt_memory_t* mem, u64 addr, t2 value) { \
)w2c_template"
R"w2c_template(    MEMCHECK(mem, addr, t1);                                                \
)w2c_template"
R"w2c_template(    t1 wrapped = (t1)value;                                                 \
)w2c_template"
R"w2c_template(    memcpytogs_##t1(addr, &wrapped);                                        \
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(
#endif
)w2c_template"
R"w2c_template(
#if WASM_RT_USE_SEGUE
)w2c_template"
R"w2c_template(#define DEFINE_LOAD DEFINE_LOAD_GS
)w2c_template"
R"w2c_template(#define DEFINE_STORE DEFINE_STORE_GS
)w2c_template"
R"w2c_template(#else
)w2c_template"
R"w2c_template(#define DEFINE_LOAD DEFINE_LOAD_REGULAR
)w2c_template"
R"w2c_template(#define DEFINE_STORE DEFINE_STORE_REGULAR
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(
DEFINE_LOAD(i32_load, u32, u32, u32, FORCE_READ_INT)
)w2c_template"
R"w2c_template(DEFINE_LOAD(i64_load, u64, u64, u64, FORCE_READ_INT)
)w2c_template"
R"w2c_template(DEFINE_LOAD(f32_load, f32, f32, f32, FORCE_READ_FLOAT)
)w2c_template"
R"w2c_template(DEFINE_LOAD(f64_load, f64, f64, f64, FORCE_READ_FLOAT)
)w2c_template"
R"w2c_template(DEFINE_LOAD(i32_load8_s, s8, s32, u32, FORCE_READ_INT)
)w2c_template"
R"w2c_template(DEFINE_LOAD(i64_load8_s, s8, s64, u64, FORCE_READ_INT)
)w2c_template"
R"w2c_template(DEFINE_LOAD(i32_load8_u, u8, u32, u32, FORCE_READ_INT)
)w2c_template"
R"w2c_template(DEFINE_LOAD(i64_load8_u, u8, u64, u64, FORCE_READ_INT)
)w2c_template"
R"w2c_template(DEFINE_LOAD(i32_load16_s, s16, s32, u32, FORCE_READ_INT)
)w2c_template"
R"w2c_template(DEFINE_LOAD(i64_load16_s, s16, s64, u64, FORCE_READ_INT)
)w2c_template"
R"w2c_template(DEFINE_LOAD(i32_load16_u, u16, u32, u32, FORCE_READ_INT)
)w2c_template"
R"w2c_template(DEFINE_LOAD(i64_load16_u, u16, u64, u64, FORCE_READ_INT)
)w2c_template"
R"w2c_template(DEFINE_LOAD(i64_load32_s, s32, s64, u64, FORCE_READ_INT)
)w2c_template"
R"w2c_template(DEFINE_LOAD(i64_load32_u, u32, u64, u64, FORCE_READ_INT)
)w2c_template"
R"w2c_template(DEFINE_STORE(i32_store, u32, u32)
)w2c_template"
R"w2c_template(DEFINE_STORE(i64_store, u64, u64)
)w2c_template"
R"w2c_template(DEFINE_STORE(f32_store, f32, f32)
)w2c_template"
R"w2c_template(DEFINE_STORE(f64_store, f64, f64)
)w2c_template"
R"w2c_template(DEFINE_STORE(i32_store8, u8, u32)
)w2c_template"
R"w2c_template(DEFINE_STORE(i32_store16, u16, u32)
)w2c_template"
R"w2c_template(DEFINE_STORE(i64_store8, u8, u64)
)w2c_template"
R"w2c_template(DEFINE_STORE(i64_store16, u16, u64)
)w2c_template"
R"w2c_template(DEFINE_STORE(i64_store32, u32, u64)
)w2c_template"
R"w2c_template(
#if defined(_MSC_VER)
)w2c_template"
R"w2c_template(
// Adapted from
)w2c_template"
R"w2c_template(// https://github.com/nemequ/portable-snippets/blob/master/builtin/builtin.h
)w2c_template"
R"w2c_template(
static MAYBEINLINE int I64_CLZ(unsigned long long v) {
)w2c_template"
R"w2c_template(  unsigned long r = 0;
)w2c_template"
R"w2c_template(#if defined(_M_AMD64) || defined(_M_ARM)
)w2c_template"
R"w2c_template(  if (_BitScanReverse64(&r, v)) {
)w2c_template"
R"w2c_template(    return 63 - r;
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(#else
)w2c_template"
R"w2c_template(  if (_BitScanReverse(&r, (unsigned long)(v >> 32))) {
)w2c_template"
R"w2c_template(    return 31 - r;
)w2c_template"
R"w2c_template(  } else if (_BitScanReverse(&r, (unsigned long)v)) {
)w2c_template"
R"w2c_template(    return 63 - r;
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(  return 64;
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static MAYBEINLINE int I32_CLZ(unsigned long v) {
)w2c_template"
R"w2c_template(  unsigned long r = 0;
)w2c_template"
R"w2c_template(  if (_BitScanReverse(&r, v)) {
)w2c_template"
R"w2c_template(    return 31 - r;
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return 32;
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static MAYBEINLINE int I64_CTZ(unsigned long long v) {
)w2c_template"
R"w2c_template(  if (!v) {
)w2c_template"
R"w2c_template(    return 64;
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  unsigned long r = 0;
)w2c_template"
R"w2c_template(#if defined(_M_AMD64) || defined(_M_ARM)
)w2c_template"
R"w2c_template(  _BitScanForward64(&r, v);
)w2c_template"
R"w2c_template(  return (int)r;
)w2c_template"
R"w2c_template(#else
)w2c_template"
R"w2c_template(  if (_BitScanForward(&r, (unsigned int)(v))) {
)w2c_template"
R"w2c_template(    return (int)(r);
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(
  _BitScanForward(&r, (unsigned int)(v >> 32));
)w2c_template"
R"w2c_template(  return (int)(r + 32);
)w2c_template"
R"w2c_template(#endif
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static MAYBEINLINE int I32_CTZ(unsigned long v) {
)w2c_template"
R"w2c_template(  if (!v) {
)w2c_template"
R"w2c_template(    return 32;
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  unsigned long r = 0;
)w2c_template"
R"w2c_template(  _BitScanForward(&r, v);
)w2c_template"
R"w2c_template(  return (int)r;
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
#define POPCOUNT_DEFINE_PORTABLE(f_n, T)                            \
)w2c_template"
R"w2c_template(  static MAYBEINLINE u32 f_n(T x) {                                 \
)w2c_template"
R"w2c_template(    x = x - ((x >> 1) & (T) ~(T)0 / 3);                             \
)w2c_template"
R"w2c_template(    x = (x & (T) ~(T)0 / 15 * 3) + ((x >> 2) & (T) ~(T)0 / 15 * 3); \
)w2c_template"
R"w2c_template(    x = (x + (x >> 4)) & (T) ~(T)0 / 255 * 15;                      \
)w2c_template"
R"w2c_template(    return (T)(x * ((T) ~(T)0 / 255)) >> (sizeof(T) - 1) * 8;       \
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(
POPCOUNT_DEFINE_PORTABLE(I32_POPCNT, u32)
)w2c_template"
R"w2c_template(POPCOUNT_DEFINE_PORTABLE(I64_POPCNT, u64)
)w2c_template"
R"w2c_template(
#undef POPCOUNT_DEFINE_PORTABLE
)w2c_template"
R"w2c_template(
#else
)w2c_template"
R"w2c_template(
#define I32_CLZ(x) ((x) ? __builtin_clz(x) : 32)
)w2c_template"
R"w2c_template(#define I64_CLZ(x) ((x) ? __builtin_clzll(x) : 64)
)w2c_template"
R"w2c_template(#define I32_CTZ(x) ((x) ? __builtin_ctz(x) : 32)
)w2c_template"
R"w2c_template(#define I64_CTZ(x) ((x) ? __builtin_ctzll(x) : 64)
)w2c_template"
R"w2c_template(#define I32_POPCNT(x) (__builtin_popcount(x))
)w2c_template"
R"w2c_template(#define I64_POPCNT(x) (__builtin_popcountll(x))
)w2c_template"
R"w2c_template(
#endif
)w2c_template"
R"w2c_template(
#define DIV_S(ut, min, x, y)                                  \
)w2c_template"
R"w2c_template(  ((UNLIKELY((y) == 0))                  ? TRAP(DIV_BY_ZERO)  \
)w2c_template"
R"w2c_template(   : (UNLIKELY((x) == min && (y) == -1)) ? TRAP(INT_OVERFLOW) \
)w2c_template"
R"w2c_template(                                         : (ut)((x) / (y)))
)w2c_template"
R"w2c_template(
#define REM_S(ut, min, x, y)                                 \
)w2c_template"
R"w2c_template(  ((UNLIKELY((y) == 0))                  ? TRAP(DIV_BY_ZERO) \
)w2c_template"
R"w2c_template(   : (UNLIKELY((x) == min && (y) == -1)) ? 0                 \
)w2c_template"
R"w2c_template(                                         : (ut)((x) % (y)))
)w2c_template"
R"w2c_template(
#define I32_DIV_S(x, y) DIV_S(u32, INT32_MIN, (s32)x, (s32)y)
)w2c_template"
R"w2c_template(#define I64_DIV_S(x, y) DIV_S(u64, INT64_MIN, (s64)x, (s64)y)
)w2c_template"
R"w2c_template(#define I32_REM_S(x, y) REM_S(u32, INT32_MIN, (s32)x, (s32)y)
)w2c_template"
R"w2c_template(#define I64_REM_S(x, y) REM_S(u64, INT64_MIN, (s64)x, (s64)y)
)w2c_template"
R"w2c_template(
#define DIVREM_U(op, x, y) \
)w2c_template"
R"w2c_template(  ((UNLIKELY((y) == 0)) ? TRAP(DIV_BY_ZERO) : ((x)op(y)))
)w2c_template"
R"w2c_template(
#define DIV_U(x, y) DIVREM_U(/, x, y)
)w2c_template"
R"w2c_template(#define REM_U(x, y) DIVREM_U(%, x, y)
)w2c_template"
R"w2c_template(
#define ROTL(x, y, mask) \
)w2c_template"
R"w2c_template(  (((x) << ((y) & (mask))) | ((x) >> (((mask) - (y) + 1) & (mask))))
)w2c_template"
R"w2c_template(#define ROTR(x, y, mask) \
)w2c_template"
R"w2c_template(  (((x) >> ((y) & (mask))) | ((x) << (((mask) - (y) + 1) & (mask))))
)w2c_template"
R"w2c_template(
#define I32_ROTL(x, y) ROTL(x, y, 31)
)w2c_template"
R"w2c_template(#define I64_ROTL(x, y) ROTL(x, y, 63)
)w2c_template"
R"w2c_template(#define I32_ROTR(x, y) ROTR(x, y, 31)
)w2c_template"
R"w2c_template(#define I64_ROTR(x, y) ROTR(x, y, 63)
)w2c_template"
R"w2c_template(
#define FMIN(x, y)                                           \
)w2c_template"
R"w2c_template(  ((UNLIKELY((x) != (x)))             ? NAN                  \
)w2c_template"
R"w2c_template(   : (UNLIKELY((y) != (y)))           ? NAN                  \
)w2c_template"
R"w2c_template(   : (UNLIKELY((x) == 0 && (y) == 0)) ? (signbit(x) ? x : y) \
)w2c_template"
R"w2c_template(   : (x < y)                          ? x                    \
)w2c_template"
R"w2c_template(                                      : y)
)w2c_template"
R"w2c_template(
#define FMAX(x, y)                                           \
)w2c_template"
R"w2c_template(  ((UNLIKELY((x) != (x)))             ? NAN                  \
)w2c_template"
R"w2c_template(   : (UNLIKELY((y) != (y)))           ? NAN                  \
)w2c_template"
R"w2c_template(   : (UNLIKELY((x) == 0 && (y) == 0)) ? (signbit(x) ? y : x) \
)w2c_template"
R"w2c_template(   : (x > y)                          ? x                    \
)w2c_template"
R"w2c_template(                                      : y)
)w2c_template"
R"w2c_template(
#define TRUNC_S(ut, st, ft, min, minop, max, x)                             \
)w2c_template"
R"w2c_template(  ((UNLIKELY((x) != (x)))                        ? TRAP(INVALID_CONVERSION) \
)w2c_template"
R"w2c_template(   : (UNLIKELY(!((x)minop(min) && (x) < (max)))) ? TRAP(INT_OVERFLOW)       \
)w2c_template"
R"w2c_template(                                                 : (ut)(st)(x))
)w2c_template"
R"w2c_template(
#define I32_TRUNC_S_F32(x) \
)w2c_template"
R"w2c_template(  TRUNC_S(u32, s32, f32, (f32)INT32_MIN, >=, 2147483648.f, x)
)w2c_template"
R"w2c_template(#define I64_TRUNC_S_F32(x) \
)w2c_template"
R"w2c_template(  TRUNC_S(u64, s64, f32, (f32)INT64_MIN, >=, (f32)INT64_MAX, x)
)w2c_template"
R"w2c_template(#define I32_TRUNC_S_F64(x) \
)w2c_template"
R"w2c_template(  TRUNC_S(u32, s32, f64, -2147483649., >, 2147483648., x)
)w2c_template"
R"w2c_template(#define I64_TRUNC_S_F64(x) \
)w2c_template"
R"w2c_template(  TRUNC_S(u64, s64, f64, (f64)INT64_MIN, >=, (f64)INT64_MAX, x)
)w2c_template"
R"w2c_template(
#define TRUNC_U(ut, ft, max, x)                                            \
)w2c_template"
R"w2c_template(  ((UNLIKELY((x) != (x)))                       ? TRAP(INVALID_CONVERSION) \
)w2c_template"
R"w2c_template(   : (UNLIKELY(!((x) > (ft)-1 && (x) < (max)))) ? TRAP(INT_OVERFLOW)       \
)w2c_template"
R"w2c_template(                                                : (ut)(x))
)w2c_template"
R"w2c_template(
#define I32_TRUNC_U_F32(x) TRUNC_U(u32, f32, 4294967296.f, x)
)w2c_template"
R"w2c_template(#define I64_TRUNC_U_F32(x) TRUNC_U(u64, f32, (f32)UINT64_MAX, x)
)w2c_template"
R"w2c_template(#define I32_TRUNC_U_F64(x) TRUNC_U(u32, f64, 4294967296., x)
)w2c_template"
R"w2c_template(#define I64_TRUNC_U_F64(x) TRUNC_U(u64, f64, (f64)UINT64_MAX, x)
)w2c_template"
R"w2c_template(
#define TRUNC_SAT_S(ut, st, ft, min, smin, minop, max, smax, x) \
)w2c_template"
R"w2c_template(  ((UNLIKELY((x) != (x)))         ? 0                           \
)w2c_template"
R"w2c_template(   : (UNLIKELY(!((x)minop(min)))) ? smin                        \
)w2c_template"
R"w2c_template(   : (UNLIKELY(!((x) < (max))))   ? smax                        \
)w2c_template"
R"w2c_template(                                  : (ut)(st)(x))
)w2c_template"
R"w2c_template(
#define I32_TRUNC_SAT_S_F32(x)                                            \
)w2c_template"
R"w2c_template(  TRUNC_SAT_S(u32, s32, f32, (f32)INT32_MIN, INT32_MIN, >=, 2147483648.f, \
)w2c_template"
R"w2c_template(              INT32_MAX, x)
)w2c_template"
R"w2c_template(#define I64_TRUNC_SAT_S_F32(x)                                              \
)w2c_template"
R"w2c_template(  TRUNC_SAT_S(u64, s64, f32, (f32)INT64_MIN, INT64_MIN, >=, (f32)INT64_MAX, \
)w2c_template"
R"w2c_template(              INT64_MAX, x)
)w2c_template"
R"w2c_template(#define I32_TRUNC_SAT_S_F64(x)                                        \
)w2c_template"
R"w2c_template(  TRUNC_SAT_S(u32, s32, f64, -2147483649., INT32_MIN, >, 2147483648., \
)w2c_template"
R"w2c_template(              INT32_MAX, x)
)w2c_template"
R"w2c_template(#define I64_TRUNC_SAT_S_F64(x)                                              \
)w2c_template"
R"w2c_template(  TRUNC_SAT_S(u64, s64, f64, (f64)INT64_MIN, INT64_MIN, >=, (f64)INT64_MAX, \
)w2c_template"
R"w2c_template(              INT64_MAX, x)
)w2c_template"
R"w2c_template(
#define TRUNC_SAT_U(ut, ft, max, smax, x) \
)w2c_template"
R"w2c_template(  ((UNLIKELY((x) != (x)))        ? 0      \
)w2c_template"
R"w2c_template(   : (UNLIKELY(!((x) > (ft)-1))) ? 0      \
)w2c_template"
R"w2c_template(   : (UNLIKELY(!((x) < (max))))  ? smax   \
)w2c_template"
R"w2c_template(                                 : (ut)(x))
)w2c_template"
R"w2c_template(
#define I32_TRUNC_SAT_U_F32(x) \
)w2c_template"
R"w2c_template(  TRUNC_SAT_U(u32, f32, 4294967296.f, UINT32_MAX, x)
)w2c_template"
R"w2c_template(#define I64_TRUNC_SAT_U_F32(x) \
)w2c_template"
R"w2c_template(  TRUNC_SAT_U(u64, f32, (f32)UINT64_MAX, UINT64_MAX, x)
)w2c_template"
R"w2c_template(#define I32_TRUNC_SAT_U_F64(x) TRUNC_SAT_U(u32, f64, 4294967296., UINT32_MAX, x)
)w2c_template"
R"w2c_template(#define I64_TRUNC_SAT_U_F64(x) \
)w2c_template"
R"w2c_template(  TRUNC_SAT_U(u64, f64, (f64)UINT64_MAX, UINT64_MAX, x)
)w2c_template"
R"w2c_template(
#define DEFINE_REINTERPRET(name, t1, t2)         \
)w2c_template"
R"w2c_template(  static MAYBEINLINE t2 name(t1 x) {             \
)w2c_template"
R"w2c_template(    t2 result;                                   \
)w2c_template"
R"w2c_template(    wasm_rt_memcpy(&result, &x, sizeof(result)); \
)w2c_template"
R"w2c_template(    return result;                               \
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(
DEFINE_REINTERPRET(f32_reinterpret_i32, u32, f32)
)w2c_template"
R"w2c_template(DEFINE_REINTERPRET(i32_reinterpret_f32, f32, u32)
)w2c_template"
R"w2c_template(DEFINE_REINTERPRET(f64_reinterpret_i64, u64, f64)
)w2c_template"
R"w2c_template(DEFINE_REINTERPRET(i64_reinterpret_f64, f64, u64)
)w2c_template"
R"w2c_template(
static float quiet_nanf(float x) {
)w2c_template"
R"w2c_template(  uint32_t tmp;
)w2c_template"
R"w2c_template(  wasm_rt_memcpy(&tmp, &x, 4);
)w2c_template"
R"w2c_template(  tmp |= 0x7fc00000lu;
)w2c_template"
R"w2c_template(  wasm_rt_memcpy(&x, &tmp, 4);
)w2c_template"
R"w2c_template(  return x;
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static double quiet_nan(double x) {
)w2c_template"
R"w2c_template(  uint64_t tmp;
)w2c_template"
R"w2c_template(  wasm_rt_memcpy(&tmp, &x, 8);
)w2c_template"
R"w2c_template(  tmp |= 0x7ff8000000000000llu;
)w2c_template"
R"w2c_template(  wasm_rt_memcpy(&x, &tmp, 8);
)w2c_template"
R"w2c_template(  return x;
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static double wasm_quiet(double x) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(isnan(x))) {
)w2c_template"
R"w2c_template(    return quiet_nan(x);
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return x;
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static float wasm_quietf(float x) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(isnan(x))) {
)w2c_template"
R"w2c_template(    return quiet_nanf(x);
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return x;
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static double wasm_floor(double x) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(isnan(x))) {
)w2c_template"
R"w2c_template(    return quiet_nan(x);
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return floor(x);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static float wasm_floorf(float x) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(isnan(x))) {
)w2c_template"
R"w2c_template(    return quiet_nanf(x);
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return floorf(x);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static double wasm_ceil(double x) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(isnan(x))) {
)w2c_template"
R"w2c_template(    return quiet_nan(x);
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return ceil(x);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static float wasm_ceilf(float x) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(isnan(x))) {
)w2c_template"
R"w2c_template(    return quiet_nanf(x);
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return ceilf(x);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static double wasm_trunc(double x) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(isnan(x))) {
)w2c_template"
R"w2c_template(    return quiet_nan(x);
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return trunc(x);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static float wasm_truncf(float x) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(isnan(x))) {
)w2c_template"
R"w2c_template(    return quiet_nanf(x);
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return truncf(x);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static float wasm_nearbyintf(float x) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(isnan(x))) {
)w2c_template"
R"w2c_template(    return quiet_nanf(x);
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return nearbyintf(x);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static double wasm_nearbyint(double x) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(isnan(x))) {
)w2c_template"
R"w2c_template(    return quiet_nan(x);
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return nearbyint(x);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static float wasm_fabsf(float x) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(isnan(x))) {
)w2c_template"
R"w2c_template(    uint32_t tmp;
)w2c_template"
R"w2c_template(    wasm_rt_memcpy(&tmp, &x, 4);
)w2c_template"
R"w2c_template(    tmp = tmp & ~(1UL << 31);
)w2c_template"
R"w2c_template(    wasm_rt_memcpy(&x, &tmp, 4);
)w2c_template"
R"w2c_template(    return x;
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return fabsf(x);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static double wasm_fabs(double x) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(isnan(x))) {
)w2c_template"
R"w2c_template(    uint64_t tmp;
)w2c_template"
R"w2c_template(    wasm_rt_memcpy(&tmp, &x, 8);
)w2c_template"
R"w2c_template(    tmp = tmp & ~(1ULL << 63);
)w2c_template"
R"w2c_template(    wasm_rt_memcpy(&x, &tmp, 8);
)w2c_template"
R"w2c_template(    return x;
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return fabs(x);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static double wasm_sqrt(double x) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(isnan(x))) {
)w2c_template"
R"w2c_template(    return quiet_nan(x);
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return sqrt(x);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static float wasm_sqrtf(float x) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(isnan(x))) {
)w2c_template"
R"w2c_template(    return quiet_nanf(x);
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(  return sqrtf(x);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static inline void memory_fill(wasm_rt_memory_t* mem, u32 d, u32 val, u32 n) {
)w2c_template"
R"w2c_template(  RANGE_CHECK(mem, d, n);
)w2c_template"
R"w2c_template(  memset(mem->data + d, val, n);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static inline void memory_copy(wasm_rt_memory_t* dest,
)w2c_template"
R"w2c_template(                               const wasm_rt_memory_t* src,
)w2c_template"
R"w2c_template(                               u32 dest_addr,
)w2c_template"
R"w2c_template(                               u32 src_addr,
)w2c_template"
R"w2c_template(                               u32 n) {
)w2c_template"
R"w2c_template(  RANGE_CHECK(dest, dest_addr, n);
)w2c_template"
R"w2c_template(  RANGE_CHECK(src, src_addr, n);
)w2c_template"
R"w2c_template(  memmove(dest->data + dest_addr, src->data + src_addr, n);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
static inline void memory_init(wasm_rt_memory_t* dest,
)w2c_template"
R"w2c_template(                               const u8* src,
)w2c_template"
R"w2c_template(                               u32 src_size,
)w2c_template"
R"w2c_template(                               u32 dest_addr,
)w2c_template"
R"w2c_template(                               u32 src_addr,
)w2c_template"
R"w2c_template(                               u32 n) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(src_addr + (uint64_t)n > src_size))
)w2c_template"
R"w2c_template(    TRAP(OOB);
)w2c_template"
R"w2c_template(  LOAD_DATA((*dest), dest_addr, src + src_addr, n);
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
typedef struct {
)w2c_template"
R"w2c_template(  enum { RefFunc, RefNull, GlobalGet } expr_type;
)w2c_template"
R"w2c_template(  wasm_rt_func_type_t type;
)w2c_template"
R"w2c_template(  wasm_rt_function_ptr_t func;
)w2c_template"
R"w2c_template(  size_t module_offset;
)w2c_template"
R"w2c_template(} wasm_elem_segment_expr_t;
)w2c_template"
R"w2c_template(
static inline void funcref_table_init(wasm_rt_funcref_table_t* dest,
)w2c_template"
R"w2c_template(                                      const wasm_elem_segment_expr_t* src,
)w2c_template"
R"w2c_template(                                      u32 src_size,
)w2c_template"
R"w2c_template(                                      u32 dest_addr,
)w2c_template"
R"w2c_template(                                      u32 src_addr,
)w2c_template"
R"w2c_template(                                      u32 n,
)w2c_template"
R"w2c_template(                                      void* module_instance) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(src_addr + (uint64_t)n > src_size))
)w2c_template"
R"w2c_template(    TRAP(OOB);
)w2c_template"
R"w2c_template(  if (UNLIKELY(dest_addr + (uint64_t)n > dest->size))
)w2c_template"
R"w2c_template(    TRAP(OOB);
)w2c_template"
R"w2c_template(  for (u32 i = 0; i < n; i++) {
)w2c_template"
R"w2c_template(    const wasm_elem_segment_expr_t* const src_expr = &src[src_addr + i];
)w2c_template"
R"w2c_template(    wasm_rt_funcref_t* const dest_val = &(dest->data[dest_addr + i]);
)w2c_template"
R"w2c_template(    switch (src_expr->expr_type) {
)w2c_template"
R"w2c_template(      case RefFunc:
)w2c_template"
R"w2c_template(        *dest_val = (wasm_rt_funcref_t){
)w2c_template"
R"w2c_template(            src_expr->type, src_expr->func,
)w2c_template"
R"w2c_template(            (char*)module_instance + src_expr->module_offset};
)w2c_template"
R"w2c_template(        break;
)w2c_template"
R"w2c_template(      case RefNull:
)w2c_template"
R"w2c_template(        *dest_val = wasm_rt_funcref_null_value;
)w2c_template"
R"w2c_template(        break;
)w2c_template"
R"w2c_template(      case GlobalGet:
)w2c_template"
R"w2c_template(        *dest_val = **(wasm_rt_funcref_t**)((char*)module_instance +
)w2c_template"
R"w2c_template(                                            src_expr->module_offset);
)w2c_template"
R"w2c_template(        break;
)w2c_template"
R"w2c_template(    }
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
// Currently wasm2c only supports initializing externref tables with ref.null.
)w2c_template"
R"w2c_template(static inline void externref_table_init(wasm_rt_externref_table_t* dest,
)w2c_template"
R"w2c_template(                                        u32 src_size,
)w2c_template"
R"w2c_template(                                        u32 dest_addr,
)w2c_template"
R"w2c_template(                                        u32 src_addr,
)w2c_template"
R"w2c_template(                                        u32 n) {
)w2c_template"
R"w2c_template(  if (UNLIKELY(src_addr + (uint64_t)n > src_size))
)w2c_template"
R"w2c_template(    TRAP(OOB);
)w2c_template"
R"w2c_template(  if (UNLIKELY(dest_addr + (uint64_t)n > dest->size))
)w2c_template"
R"w2c_template(    TRAP(OOB);
)w2c_template"
R"w2c_template(  for (u32 i = 0; i < n; i++) {
)w2c_template"
R"w2c_template(    dest->data[dest_addr + i] = wasm_rt_externref_null_value;
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(}
)w2c_template"
R"w2c_template(
#define DEFINE_TABLE_COPY(type)                                              \
)w2c_template"
R"w2c_template(  static inline void type##_table_copy(wasm_rt_##type##_table_t* dest,       \
)w2c_template"
R"w2c_template(                                       const wasm_rt_##type##_table_t* src,  \
)w2c_template"
R"w2c_template(                                       u32 dest_addr, u32 src_addr, u32 n) { \
)w2c_template"
R"w2c_template(    if (UNLIKELY(dest_addr + (uint64_t)n > dest->size))                      \
)w2c_template"
R"w2c_template(      TRAP(OOB);                                                             \
)w2c_template"
R"w2c_template(    if (UNLIKELY(src_addr + (uint64_t)n > src->size))                        \
)w2c_template"
R"w2c_template(      TRAP(OOB);                                                             \
)w2c_template"
R"w2c_template(                                                                             \
)w2c_template"
R"w2c_template(    memmove(dest->data + dest_addr, src->data + src_addr,                    \
)w2c_template"
R"w2c_template(            n * sizeof(wasm_rt_##type##_t));                                 \
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(
DEFINE_TABLE_COPY(funcref)
)w2c_template"
R"w2c_template(DEFINE_TABLE_COPY(externref)
)w2c_template"
R"w2c_template(
#define DEFINE_TABLE_GET(type)                        \
)w2c_template"
R"w2c_template(  static inline wasm_rt_##type##_t type##_table_get(  \
)w2c_template"
R"w2c_template(      const wasm_rt_##type##_table_t* table, u32 i) { \
)w2c_template"
R"w2c_template(    if (UNLIKELY(i >= table->size))                   \
)w2c_template"
R"w2c_template(      TRAP(OOB);                                      \
)w2c_template"
R"w2c_template(    return table->data[i];                            \
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(
DEFINE_TABLE_GET(funcref)
)w2c_template"
R"w2c_template(DEFINE_TABLE_GET(externref)
)w2c_template"
R"w2c_template(
#define DEFINE_TABLE_SET(type)                                               \
)w2c_template"
R"w2c_template(  static inline void type##_table_set(const wasm_rt_##type##_table_t* table, \
)w2c_template"
R"w2c_template(                                      u32 i, const wasm_rt_##type##_t val) { \
)w2c_template"
R"w2c_template(    if (UNLIKELY(i >= table->size))                                          \
)w2c_template"
R"w2c_template(      TRAP(OOB);                                                             \
)w2c_template"
R"w2c_template(    table->data[i] = val;                                                    \
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(
DEFINE_TABLE_SET(funcref)
)w2c_template"
R"w2c_template(DEFINE_TABLE_SET(externref)
)w2c_template"
R"w2c_template(
#define DEFINE_TABLE_FILL(type)                                               \
)w2c_template"
R"w2c_template(  static inline void type##_table_fill(const wasm_rt_##type##_table_t* table, \
)w2c_template"
R"w2c_template(                                       u32 d, const wasm_rt_##type##_t val,   \
)w2c_template"
R"w2c_template(                                       u32 n) {                               \
)w2c_template"
R"w2c_template(    if (UNLIKELY((uint64_t)d + n > table->size))                              \
)w2c_template"
R"w2c_template(      TRAP(OOB);                                                              \
)w2c_template"
R"w2c_template(    for (uint32_t i = d; i < d + n; i++) {                                    \
)w2c_template"
R"w2c_template(      table->data[i] = val;                                                   \
)w2c_template"
R"w2c_template(    }                                                                         \
)w2c_template"
R"w2c_template(  }
)w2c_template"
R"w2c_template(
DEFINE_TABLE_FILL(funcref)
)w2c_template"
R"w2c_template(DEFINE_TABLE_FILL(externref)
)w2c_template"
R"w2c_template(
#if defined(__GNUC__) || defined(__clang__)
)w2c_template"
R"w2c_template(#define FUNC_TYPE_DECL_EXTERN_T(x) extern const char* const x
)w2c_template"
R"w2c_template(#define FUNC_TYPE_EXTERN_T(x) const char* const x
)w2c_template"
R"w2c_template(#define FUNC_TYPE_T(x) static const char* const x
)w2c_template"
R"w2c_template(#else
)w2c_template"
R"w2c_template(#define FUNC_TYPE_DECL_EXTERN_T(x) extern const char x[]
)w2c_template"
R"w2c_template(#define FUNC_TYPE_EXTERN_T(x) const char x[]
)w2c_template"
R"w2c_template(#define FUNC_TYPE_T(x) static const char x[]
)w2c_template"
R"w2c_template(#endif
)w2c_template"
;
