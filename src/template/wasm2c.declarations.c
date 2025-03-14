
#define TRAP(x) (wasm_rt_trap(WASM_RT_TRAP_##x), 0)

#if WASM_RT_USE_STACK_DEPTH_COUNT
#define FUNC_PROLOGUE                                            \
  if (++wasm_rt_call_stack_depth > WASM_RT_MAX_CALL_STACK_DEPTH) \
    TRAP(EXHAUSTION);

#define FUNC_EPILOGUE --wasm_rt_call_stack_depth
#else
#define FUNC_PROLOGUE

#define FUNC_EPILOGUE
#endif

#define UNREACHABLE TRAP(UNREACHABLE)

#if WASM_RT_MEMCHECK_MASK_PDEP || WASM_RT_MEMCHECK_MASK_PDEP48 || WASM_RT_MEMCHECK_MASK_PEXT || WASM_RT_MEMCHECK_SHADOW_BYTES_BITSCAN
#include <x86intrin.h>
#endif

static inline bool func_types_eq(const wasm_rt_func_type_t a,
                                 const wasm_rt_func_type_t b) {
  return (a == b) || LIKELY(a && b && !memcmp(a, b, 32));
}

#define CALL_INDIRECT(table, t, ft, x, ...)              \
  (LIKELY((x) < table.size && table.data[x].func &&      \
          func_types_eq(ft, table.data[x].func_type)) || \
       TRAP(CALL_INDIRECT),                              \
   ((t)table.data[x].func)(__VA_ARGS__))

#ifdef SUPPORT_MEMORY64
#define RANGE_CHECK(mem, offset, len)              \
  do {                                             \
    uint64_t res;                                  \
    if (__builtin_add_overflow(offset, len, &res)) \
      TRAP(OOB);                                   \
    if (UNLIKELY(res > mem->size))                 \
      TRAP(OOB);                                   \
  } while (0);
#else
#define RANGE_CHECK(mem, offset, len)               \
  if (UNLIKELY(offset + (uint64_t)len > mem->size)) \
    TRAP(OOB);

#define RANGE_CHECK_MASKED(mem, offset, len)             \
  do {                                                   \
    uint8_t is_oob = offset + (uint64_t)len > mem->size; \
    uint64_t zero = -1;                                  \
    if (UNLIKELY(is_oob))                                \
      TRAP(OOB);                                         \
    asm("test %[is_oob],%[is_oob]\n"                     \
        "cmovne %[zero], %[offset_var]\n"                \
        : [offset_var] "+r"(offset)                      \
        : [is_oob] "r"(is_oob), [zero] "r"(zero)         \
        : "cc");                                         \
  } while (0)

#if WASM_RT_MEMCHECK_BOUNDS_CHECK_TRAP_SCHEME == 1
#define RANGE_CHECK_TRAP(mem, offset, len)          \
  offset = (UNLIKELY(offset + (uint64_t)len > mem->size)) ? mem->size : offset;
#elif WASM_RT_MEMCHECK_BOUNDS_CHECK_TRAP_SCHEME == 2
#define RANGE_CHECK_TRAP(mem, offset, len)          \
  offset = (UNLIKELY(offset + (uint64_t)len > mem->size)) ? -1 : offset;
#endif

#define RANGE_CHECK_ASM(mem, offset, len)                             \
  do {                                                                \
    bool is_oob;                                                      \
    uint64_t offset_plus_len = offset + len;                          \
    uint64_t size = mem->size;                                        \
    if (!__builtin_constant_p(offset)) {                              \
      asm("cmpq %[offset_plus_len], %[size]\n"                        \
          : "=@ccb"(is_oob)                                           \
          : [offset_plus_len] "rm"(offset_plus_len), [size] "r"(size) \
          : "cc");                                                    \
      if (UNLIKELY(is_oob))                                           \
        TRAP(OOB);                                                    \
    }                                                                 \
  } while (0)

#define RANGE_CHECK_ASM_MASKED(mem, offset, len)                       \
  do {                                                                 \
    uint64_t zero = 0;                                                 \
    bool is_oob;                                                       \
    uint64_t offset_plus_len = offset + len;                           \
    uint64_t size = mem->size;                                         \
    if (!__builtin_constant_p(offset)) {                               \
      asm("cmpq %[offset_plus_len], %[size]\n"                         \
          "cmovbq %[zero], %0\n"                                       \
          : "+r"(offset), "=@ccb"(is_oob)                              \
          : [offset_plus_len] "rm"(offset_plus_len), [size] "r"(size), \
            [zero] "rm"(zero)                                          \
          : "cc");                                                     \
      if (UNLIKELY(is_oob))                                            \
        TRAP(OOB);                                                     \
    }                                                                  \
  } while (0)
#endif

#ifdef __GNUC__
#define FORCE_READ_INT(var) __asm__("" ::"r"(var));
// Clang on Mips requires "f" constraints on floats
// See https://github.com/llvm/llvm-project/issues/64241
#if defined(__clang__) && \
    (defined(mips) || defined(__mips__) || defined(__mips))
#define FORCE_READ_FLOAT(var) __asm__("" ::"f"(var));
#else
#define FORCE_READ_FLOAT(var) __asm__("" ::"r"(var));
#endif
#else
#error "No force read supported"
#endif

#define WASM_RT_GS_REF(TYPE, ptr) (*((TYPE __seg_gs*)(uintptr_t)(ptr)))

#if WASM_RT_MEMCHECK_GUARD_PAGES
#define MEMCHECK(mem, a, t)
#elif WASM_RT_MEMCHECK_SSWRITE_SIM
#define MEMCHECK(mem, a, t)  asm( \
    "mov %0, %0\n"                \
    "mov %0, %0\n"                \
    "mov %0, %0\n"                \
    "mov %0, %0\n"                \
    "mov %0, %0\n"                \
    "mov %0, %0\n"                \
    "mov %0, %0\n"                \
    "mov %0, %0\n"                \
    : "+r" (a)                    \
    :                             \
    :                             \
  );
#elif WASM_RT_MEMCHECK_SHADOW_PAGE

// Access to the first 64k Wasm page should map to an access of the first 4k
// shadow page Access to the second 64k Wasm page should map to an access of the
// second 4k shadow page
// ... and so on ...

#if WASM_RT_USE_SHADOW_SEGUE
#if WASM_RT_MEMCHECK_SHADOW_PAGE_SCHEME == 1 && !WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) FORCE_READ_INT(WASM_RT_GS_REF(u8, a >> 4))
#elif WASM_RT_MEMCHECK_SHADOW_PAGE_SCHEME == 2 && !WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) FORCE_READ_INT(WASM_RT_GS_REF(u8, (a >> 16) << 12))
#elif WASM_RT_MEMCHECK_SHADOW_PAGE_SCHEME == 1 && WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) a = (WASM_RT_GS_REF(u8, a >> 4)) ? 0 : a
#elif WASM_RT_MEMCHECK_SHADOW_PAGE_SCHEME == 2 && WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) a = (WASM_RT_GS_REF(u8, (a >> 16) << 12)) ? 0 : a
#elif WASM_RT_MEMCHECK_SHADOW_PAGE_SCHEME == 3
#define MEMCHECK(mem, a, t) WASM_RT_GS_REF(u8, a >> 4) = 0
#elif WASM_RT_MEMCHECK_SHADOW_PAGE_SCHEME == 4
#define MEMCHECK(mem, a, t) WASM_RT_GS_REF(u8, (a >> 16) << 12) = 0
#elif WASM_RT_MEMCHECK_SHADOW_PAGE_SCHEME == 5
#define MEMCHECK(mem, a, t) FORCE_READ_INT(WASM_RT_GS_REF(u8, a >> 40))
#endif
#else
#if WASM_RT_MEMCHECK_SHADOW_PAGE_SCHEME == 1 && !WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) FORCE_READ_INT(mem->shadow_memory[a >> 4])
#elif WASM_RT_MEMCHECK_SHADOW_PAGE_SCHEME == 2 && !WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) FORCE_READ_INT(mem->shadow_memory[(a >> 16) << 12])
#elif WASM_RT_MEMCHECK_SHADOW_PAGE_SCHEME == 1 && WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) a = (mem->shadow_memory[a >> 4]) ? 0 : a
#elif WASM_RT_MEMCHECK_SHADOW_PAGE_SCHEME == 2 && WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) a = (mem->shadow_memory[(a >> 16) << 12]) ? 0 : a
#elif WASM_RT_MEMCHECK_SHADOW_PAGE_SCHEME == 3
#define MEMCHECK(mem, a, t) mem->shadow_memory[a >> 4] = 0
#elif WASM_RT_MEMCHECK_SHADOW_PAGE_SCHEME == 4
#define MEMCHECK(mem, a, t) mem->shadow_memory[(a >> 16) << 12] = 0
#elif WASM_RT_MEMCHECK_SHADOW_PAGE_SCHEME == 5
#define MEMCHECK(mem, a, t) FORCE_READ_INT(mem->shadow_memory[a >> 40])
#endif
#endif

#elif WASM_RT_MEMCHECK_PRESHADOW_PAGE

#if WASM_RT_USE_SHADOW_SEGUE
#define MEMCHECK(mem, a, t) FORCE_READ_INT(WASM_RT_GS_REF(u8, -(a >> 16) - 1))
#else
#define MEMCHECK(mem, a, t) FORCE_READ_INT(mem->data[-(a >> 16) - 1])
#endif

#elif WASM_RT_MEMCHECK_SHADOW_BYTES_TAG
  bool floatzone_y_init_done = false;
  float floatzone_y;
#if WASM_RT_USE_SHADOW_SEGUE

#if WASM_RT_MEMCHECK_SHADOW_BYTES_TAG_SCHEME == 1
#define MEMCHECK(mem, a, t) FORCE_READ_FLOAT(WASM_RT_GS_REF(float, a >> 32) + floatzone_y)
#elif WASM_RT_MEMCHECK_SHADOW_BYTES_TAG_SCHEME == 2
#define MEMCHECK(mem, a, t) asm("addss  %%gs:0x0,%%xmm15\n" \
        :                                                 \
        :                                                 \
        : "xmm15");
#endif

#else

#if WASM_RT_MEMCHECK_SHADOW_BYTES_TAG_SCHEME == 1
#define MEMCHECK(mem, a, t) FORCE_READ_FLOAT(mem->shadow_bytes[(u32)(a >> 32)] + floatzone_y)
#elif WASM_RT_MEMCHECK_SHADOW_BYTES_TAG_SCHEME == 2
#define MEMCHECK(mem, a, t)                                  \
do {                                                         \
float* tag_ptr = &mem->shadow_bytes[(u32)(a >> 32)];         \
asm("addss  %[tag_ptr],%%xmm15\n"                             \
        :                                                    \
        :  [tag_ptr] "m"(tag_ptr)                            \
        : "xmm15");                                          \
} while(0)
#endif

#endif

#elif WASM_RT_MEMCHECK_SHADOW_BYTES

#if WASM_RT_USE_SHADOW_SEGUE
#if WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 1 && !WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) FORCE_READ_INT(WASM_RT_GS_REF(u32, (a >> 32) << 2))
#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 2 && !WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) FORCE_READ_INT(WASM_RT_GS_REF(u8, a >> 32))
#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 3 && !WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) FORCE_READ_INT(WASM_RT_GS_REF(u8, a >> 16))
#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 4 && !WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) FORCE_READ_INT(WASM_RT_GS_REF(u8, a >> 24))
#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 1 && WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) a = (WASM_RT_GS_REF(u32, (a >> 32) << 2)) ? 0 : a
#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 2 && WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) a = (WASM_RT_GS_REF(u8, a >> 32)) ? 0 : a
#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 3 && WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) a = (WASM_RT_GS_REF(u8, a >> 16)) ? 0 : a
#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 4 && WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) a = (WASM_RT_GS_REF(u8, a >> 24)) ? 0 : a
#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 5 && !WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) WASM_RT_GS_REF(u32, (a >> 32) << 2) = 0
#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 6 && !WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) WASM_RT_GS_REF(u8, a >> 32) = 0
#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 7 && !WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) WASM_RT_GS_REF(u8, a >> 16) = 0
#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 8 && !WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) WASM_RT_GS_REF(u8, a >> 24) = 0
#endif
#else
#if WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 1 && !WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) FORCE_READ_INT(mem->shadow_bytes[(u32)(a >> 32)])
#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 2 && !WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) FORCE_READ_INT(mem->shadow_bytes[(u32)(a >> 32)])
#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 3 && !WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) FORCE_READ_INT(mem->shadow_bytes[a >> 16])
#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 4 && !WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) FORCE_READ_INT(mem->shadow_bytes[a >> 24])
#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 1 && WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) a = (mem->shadow_bytes[(u32)(a >> 32)]) ? 0 : a
#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 2 && WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) a = (mem->shadow_bytes[(u32)(a >> 32)]) ? 0 : a
#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 3 && WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) a = (mem->shadow_bytes[a >> 16]) ? 0 : a
#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 4 && WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) a = (mem->shadow_bytes[a >> 24]) ? 0 : a
#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 5 && !WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) mem->shadow_bytes[(u32)(a >> 32)] = 0
#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 6 && !WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) mem->shadow_bytes[(u32)(a >> 32)] = 0
#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 7 && !WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) mem->shadow_bytes[a >> 16] = 0
#elif WASM_RT_MEMCHECK_SHADOW_BYTES_SCHEME == 8 && !WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) mem->shadow_bytes[a >> 24] = 0
#endif
#endif


#elif WASM_RT_MEMCHECK_SHADOW_BYTES_BITSCAN

// Allow 28 bits to be set.
// Read/write should occur at 0x100000 - 28 * 4 + __bsrq(a) * 4 = 0xfff90 + __bsrq(a) * 4
#if WASM_RT_MEMCHECK_SHADOW_BYTES_BITSCAN_SCHEME == 1
#define MEMCHECK(mem, a, t) FORCE_READ_INT(*(uint32_t*) (uintptr_t) (0xfff90 + __bsrq(a) * 4))
#elif WASM_RT_MEMCHECK_SHADOW_BYTES_BITSCAN_SCHEME == 2
#define MEMCHECK(mem, a, t) *((uint32_t*) (uintptr_t) (0xfff90 + __bsrq(a) * 4)) = 0
#endif

#elif WASM_RT_MEMCHECK_PRESHADOW_BYTES

#if WASM_RT_USE_SHADOW_SEGUE && !WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t)      \
  FORCE_READ_INT(WASM_RT_GS_REF( \
      u8, -mem->shadow_bytes_distance_from_heap + (u32)(a >> 32)))
#elif WASM_RT_USE_SHADOW_SEGUE && WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t)                                                    \
  a = (WASM_RT_GS_REF(u8,                                                      \
                      -mem->shadow_bytes_distance_from_heap + (u32)(a >> 32))) \
          ? 0                                                                  \
          : a
#elif !WASM_RT_USE_SHADOW_SEGUE && !WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) \
  FORCE_READ_INT(           \
      mem->data[-mem->shadow_bytes_distance_from_heap + (u32)(a >> 32)])
#elif !WASM_RT_USE_SHADOW_SEGUE && WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t)                                                   \
  a = (mem->data[-mem->shadow_bytes_distance_from_heap + (u32)(a >> 32)]) ? 0 \
                                                                          : a
#endif

#elif WASM_RT_MEMCHECK_DEBUG_WATCH

#if WASM_RT_USE_SHADOW_SEGUE
#define MEMCHECK(mem, a, t) WASM_RT_GS_REF(u64, 0) = a
#else
#define MEMCHECK(mem, a, t) mem->debug_watch_buffer = a
#endif

#elif WASM_RT_MEMCHECK_MPX

#define MEMCHECK(mem, a, t)           \
    asm volatile(                     \
        "bndcu %[addr_val], %%bnd1\n" \
        :                             \
        : [addr_val] "r"(a)           \
        :)

#elif WASM_RT_MEMCHECK_DUMMY

#define MEMCHECK(mem, a, t)           \
    asm volatile(                     \
        "add $0x0, %[addr_val]\n"     \
        :                             \
        : [addr_val] "r"(a)           \
        :)

#elif WASM_RT_MEMCHECK_DUMMY2

#define MEMCHECK(mem, a, t)           \
    asm volatile(                     \
        "add $0x0, %[addr_val]\n"     \
        "add $0x0, %[addr_val]\n"     \
        :                             \
        : [addr_val] "r"(a)           \
        :)

#elif WASM_RT_MEMCHECK_DUMMY3

#define MEMCHECK(mem, a, t)           \
    asm volatile(                     \
        "add $0x0, %[addr_val]\n"     \
        "add $0x0, %[addr_val]\n"     \
        "add $0x0, %[addr_val]\n"     \
        :                             \
        : [addr_val] "r"(a)           \
        :)

#elif WASM_RT_MEMCHECK_MASK_PDEP

#define MEMCHECK(mem, a, t) a = a | _pdep_u64(a, (uint64_t)0xf00000000fffffff)

#elif WASM_RT_MEMCHECK_MASK_PDEP48

#define MEMCHECK(mem, a, t) a = _pdep_u64(a, (uint64_t)0xf00000000fffffff)

#elif WASM_RT_MEMCHECK_MASK_PEXT

#if WASM_RT_USE_SHADOW_SEGUE
#define MEMCHECK(mem, a, t) FORCE_READ_INT(WASM_RT_GS_REF(-_pext_u64(a, (uint64_t)0xfffffffff0000000)));
#else
#define MEMCHECK(mem, a, t) FORCE_READ_INT(mem->data[-_pext_u64(a, (uint64_t)0xfffffffff0000000)]);
#endif

#elif WASM_RT_MEMCHECK_BOUNDS_CHECK_ASM

#if WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) RANGE_CHECK_ASM_MASKED(mem, a, sizeof(t))
#else
#define MEMCHECK(mem, a, t) RANGE_CHECK_ASM(mem, a, sizeof(t))
#endif

#elif WASM_RT_MEMCHECK_BOUNDS_CHECK_TRAP

#if WASM_RT_SPECTREMASK
#error "Not impl"
#else
#define MEMCHECK(mem, a, t) RANGE_CHECK_TRAP(mem, a, sizeof(t))
#endif

#elif WASM_RT_MEMCHECK_BOUNDS_CHECK

#if WASM_RT_SPECTREMASK
#define MEMCHECK(mem, a, t) RANGE_CHECK_MASKED(mem, a, sizeof(t))
#else
#define MEMCHECK(mem, a, t) RANGE_CHECK(mem, a, sizeof(t))
#endif

#endif

#ifdef WASM_RT_NOINLINE
#define MAYBEINLINE __attribute__((noinline))
#else
#define MAYBEINLINE inline
#endif

static void init_memchk() {
#if WASM_RT_MEMCHECK_SHADOW_BYTES_TAG
  if (!floatzone_y_init_done) {
    uint32_t crash_value_int = 0x8b8b8b8b;
    memcpy(&floatzone_y, &crash_value_int, sizeof(float));
    floatzone_y_init_done = true;
  }
#endif
}

#if WABT_BIG_ENDIAN
#error "Big endian not supported"
#else
static MAYBEINLINE void load_data(void* dest, const void* src, size_t n) {
  if (!n) {
    return;
  }
  wasm_rt_memcpy(dest, src, n);
}
#define LOAD_DATA(m, o, i, s)      \
  do {                             \
    RANGE_CHECK((&m), o, s);       \
    load_data(&(m.data[o]), i, s); \
  } while (0)
#define DEFINE_LOAD_REGULAR(name, t1, t2, t3, force_read)       \
  static MAYBEINLINE t3 name(wasm_rt_memory_t* mem, u64 addr) { \
    MEMCHECK(mem, addr, t1);                                    \
    t1 result;                                                  \
    wasm_rt_memcpy(&result, &mem->data[addr], sizeof(t1));      \
    force_read(result);                                         \
    return (t3)(t2)result;                                      \
  }

#define DEFINE_STORE_REGULAR(name, t1, t2)                                  \
  static MAYBEINLINE void name(wasm_rt_memory_t* mem, u64 addr, t2 value) { \
    MEMCHECK(mem, addr, t1);                                                \
    t1 wrapped = (t1)value;                                                 \
    wasm_rt_memcpy(&mem->data[addr], &wrapped, sizeof(t1));                 \
  }

#define DEFINE_LOAD_GS(name, t1, t2, t3, force_read)                           \
  static MAYBEINLINE t3 name(wasm_rt_memory_t* mem, u64 addr) {                \
    MEMCHECK(mem, addr, t1);                                                   \
    t1 result;                                                                 \
    wasm_rt_memcpy(&result, ((uint8_t __seg_gs*)(uintptr_t)addr), sizeof(t1)); \
    force_read(result);                                                        \
    return (t3)(t2)result;                                                     \
  }

#define DEFINE_STORE_GS(name, t1, t2)                                           \
  static MAYBEINLINE void name(wasm_rt_memory_t* mem, u64 addr, t2 value) {     \
    MEMCHECK(mem, addr, t1);                                                    \
    t1 wrapped = (t1)value;                                                     \
    wasm_rt_memcpy(((uint8_t __seg_gs*)(uintptr_t)addr), &wrapped, sizeof(t1)); \
  }

#endif

#if WASM_RT_USE_SEGUE
#define DEFINE_LOAD DEFINE_LOAD_GS
#define DEFINE_STORE DEFINE_STORE_GS
#elif WASM_RT_USE_SEGUE_LOAD
#define DEFINE_LOAD DEFINE_LOAD_GS
#define DEFINE_STORE DEFINE_STORE_REGULAR
#elif WASM_RT_USE_SEGUE_STORE
#define DEFINE_LOAD DEFINE_LOAD_REGULAR
#define DEFINE_STORE DEFINE_STORE_GS
#else
#define DEFINE_LOAD DEFINE_LOAD_REGULAR
#define DEFINE_STORE DEFINE_STORE_REGULAR
#endif

DEFINE_LOAD(i32_load, u32, u32, u32, FORCE_READ_INT)
DEFINE_LOAD(i64_load, u64, u64, u64, FORCE_READ_INT)
DEFINE_LOAD(f32_load, f32, f32, f32, FORCE_READ_FLOAT)
DEFINE_LOAD(f64_load, f64, f64, f64, FORCE_READ_FLOAT)
DEFINE_LOAD(i32_load8_s, s8, s32, u32, FORCE_READ_INT)
DEFINE_LOAD(i64_load8_s, s8, s64, u64, FORCE_READ_INT)
DEFINE_LOAD(i32_load8_u, u8, u32, u32, FORCE_READ_INT)
DEFINE_LOAD(i64_load8_u, u8, u64, u64, FORCE_READ_INT)
DEFINE_LOAD(i32_load16_s, s16, s32, u32, FORCE_READ_INT)
DEFINE_LOAD(i64_load16_s, s16, s64, u64, FORCE_READ_INT)
DEFINE_LOAD(i32_load16_u, u16, u32, u32, FORCE_READ_INT)
DEFINE_LOAD(i64_load16_u, u16, u64, u64, FORCE_READ_INT)
DEFINE_LOAD(i64_load32_s, s32, s64, u64, FORCE_READ_INT)
DEFINE_LOAD(i64_load32_u, u32, u64, u64, FORCE_READ_INT)
DEFINE_STORE(i32_store, u32, u32)
DEFINE_STORE(i64_store, u64, u64)
DEFINE_STORE(f32_store, f32, f32)
DEFINE_STORE(f64_store, f64, f64)
DEFINE_STORE(i32_store8, u8, u32)
DEFINE_STORE(i32_store16, u16, u32)
DEFINE_STORE(i64_store8, u8, u64)
DEFINE_STORE(i64_store16, u16, u64)
DEFINE_STORE(i64_store32, u32, u64)

#if defined(_MSC_VER)

// Adapted from
// https://github.com/nemequ/portable-snippets/blob/master/builtin/builtin.h

static MAYBEINLINE int I64_CLZ(unsigned long long v) {
  unsigned long r = 0;
#if defined(_M_AMD64) || defined(_M_ARM)
  if (_BitScanReverse64(&r, v)) {
    return 63 - r;
  }
#else
  if (_BitScanReverse(&r, (unsigned long)(v >> 32))) {
    return 31 - r;
  } else if (_BitScanReverse(&r, (unsigned long)v)) {
    return 63 - r;
  }
#endif
  return 64;
}

static MAYBEINLINE int I32_CLZ(unsigned long v) {
  unsigned long r = 0;
  if (_BitScanReverse(&r, v)) {
    return 31 - r;
  }
  return 32;
}

static MAYBEINLINE int I64_CTZ(unsigned long long v) {
  if (!v) {
    return 64;
  }
  unsigned long r = 0;
#if defined(_M_AMD64) || defined(_M_ARM)
  _BitScanForward64(&r, v);
  return (int)r;
#else
  if (_BitScanForward(&r, (unsigned int)(v))) {
    return (int)(r);
  }

  _BitScanForward(&r, (unsigned int)(v >> 32));
  return (int)(r + 32);
#endif
}

static MAYBEINLINE int I32_CTZ(unsigned long v) {
  if (!v) {
    return 32;
  }
  unsigned long r = 0;
  _BitScanForward(&r, v);
  return (int)r;
}

#define POPCOUNT_DEFINE_PORTABLE(f_n, T)                            \
  static MAYBEINLINE u32 f_n(T x) {                                 \
    x = x - ((x >> 1) & (T) ~(T)0 / 3);                             \
    x = (x & (T) ~(T)0 / 15 * 3) + ((x >> 2) & (T) ~(T)0 / 15 * 3); \
    x = (x + (x >> 4)) & (T) ~(T)0 / 255 * 15;                      \
    return (T)(x * ((T) ~(T)0 / 255)) >> (sizeof(T) - 1) * 8;       \
  }

POPCOUNT_DEFINE_PORTABLE(I32_POPCNT, u32)
POPCOUNT_DEFINE_PORTABLE(I64_POPCNT, u64)

#undef POPCOUNT_DEFINE_PORTABLE

#else

#define I32_CLZ(x) ((x) ? __builtin_clz(x) : 32)
#define I64_CLZ(x) ((x) ? __builtin_clzll(x) : 64)
#define I32_CTZ(x) ((x) ? __builtin_ctz(x) : 32)
#define I64_CTZ(x) ((x) ? __builtin_ctzll(x) : 64)
#define I32_POPCNT(x) (__builtin_popcount(x))
#define I64_POPCNT(x) (__builtin_popcountll(x))

#endif

#define DIV_S(ut, min, x, y)                                  \
  ((UNLIKELY((y) == 0))                  ? TRAP(DIV_BY_ZERO)  \
   : (UNLIKELY((x) == min && (y) == -1)) ? TRAP(INT_OVERFLOW) \
                                         : (ut)((x) / (y)))

#define REM_S(ut, min, x, y)                                 \
  ((UNLIKELY((y) == 0))                  ? TRAP(DIV_BY_ZERO) \
   : (UNLIKELY((x) == min && (y) == -1)) ? 0                 \
                                         : (ut)((x) % (y)))

#define I32_DIV_S(x, y) DIV_S(u32, INT32_MIN, (s32)x, (s32)y)
#define I64_DIV_S(x, y) DIV_S(u64, INT64_MIN, (s64)x, (s64)y)
#define I32_REM_S(x, y) REM_S(u32, INT32_MIN, (s32)x, (s32)y)
#define I64_REM_S(x, y) REM_S(u64, INT64_MIN, (s64)x, (s64)y)

#define DIVREM_U(op, x, y) \
  ((UNLIKELY((y) == 0)) ? TRAP(DIV_BY_ZERO) : ((x)op(y)))

#define DIV_U(x, y) DIVREM_U(/, x, y)
#define REM_U(x, y) DIVREM_U(%, x, y)

#define ROTL(x, y, mask) \
  (((x) << ((y) & (mask))) | ((x) >> (((mask) - (y) + 1) & (mask))))
#define ROTR(x, y, mask) \
  (((x) >> ((y) & (mask))) | ((x) << (((mask) - (y) + 1) & (mask))))

#define I32_ROTL(x, y) ROTL(x, y, 31)
#define I64_ROTL(x, y) ROTL(x, y, 63)
#define I32_ROTR(x, y) ROTR(x, y, 31)
#define I64_ROTR(x, y) ROTR(x, y, 63)

#define FMIN(x, y)                                           \
  ((UNLIKELY((x) != (x)))             ? NAN                  \
   : (UNLIKELY((y) != (y)))           ? NAN                  \
   : (UNLIKELY((x) == 0 && (y) == 0)) ? (signbit(x) ? x : y) \
   : (x < y)                          ? x                    \
                                      : y)

#define FMAX(x, y)                                           \
  ((UNLIKELY((x) != (x)))             ? NAN                  \
   : (UNLIKELY((y) != (y)))           ? NAN                  \
   : (UNLIKELY((x) == 0 && (y) == 0)) ? (signbit(x) ? y : x) \
   : (x > y)                          ? x                    \
                                      : y)

#define TRUNC_S(ut, st, ft, min, minop, max, x)                             \
  ((UNLIKELY((x) != (x)))                        ? TRAP(INVALID_CONVERSION) \
   : (UNLIKELY(!((x)minop(min) && (x) < (max)))) ? TRAP(INT_OVERFLOW)       \
                                                 : (ut)(st)(x))

#define I32_TRUNC_S_F32(x) \
  TRUNC_S(u32, s32, f32, (f32)INT32_MIN, >=, 2147483648.f, x)
#define I64_TRUNC_S_F32(x) \
  TRUNC_S(u64, s64, f32, (f32)INT64_MIN, >=, (f32)INT64_MAX, x)
#define I32_TRUNC_S_F64(x) \
  TRUNC_S(u32, s32, f64, -2147483649., >, 2147483648., x)
#define I64_TRUNC_S_F64(x) \
  TRUNC_S(u64, s64, f64, (f64)INT64_MIN, >=, (f64)INT64_MAX, x)

#define TRUNC_U(ut, ft, max, x)                                            \
  ((UNLIKELY((x) != (x)))                       ? TRAP(INVALID_CONVERSION) \
   : (UNLIKELY(!((x) > (ft)-1 && (x) < (max)))) ? TRAP(INT_OVERFLOW)       \
                                                : (ut)(x))

#define I32_TRUNC_U_F32(x) TRUNC_U(u32, f32, 4294967296.f, x)
#define I64_TRUNC_U_F32(x) TRUNC_U(u64, f32, (f32)UINT64_MAX, x)
#define I32_TRUNC_U_F64(x) TRUNC_U(u32, f64, 4294967296., x)
#define I64_TRUNC_U_F64(x) TRUNC_U(u64, f64, (f64)UINT64_MAX, x)

#define TRUNC_SAT_S(ut, st, ft, min, smin, minop, max, smax, x) \
  ((UNLIKELY((x) != (x)))         ? 0                           \
   : (UNLIKELY(!((x)minop(min)))) ? smin                        \
   : (UNLIKELY(!((x) < (max))))   ? smax                        \
                                  : (ut)(st)(x))

#define I32_TRUNC_SAT_S_F32(x)                                            \
  TRUNC_SAT_S(u32, s32, f32, (f32)INT32_MIN, INT32_MIN, >=, 2147483648.f, \
              INT32_MAX, x)
#define I64_TRUNC_SAT_S_F32(x)                                              \
  TRUNC_SAT_S(u64, s64, f32, (f32)INT64_MIN, INT64_MIN, >=, (f32)INT64_MAX, \
              INT64_MAX, x)
#define I32_TRUNC_SAT_S_F64(x)                                        \
  TRUNC_SAT_S(u32, s32, f64, -2147483649., INT32_MIN, >, 2147483648., \
              INT32_MAX, x)
#define I64_TRUNC_SAT_S_F64(x)                                              \
  TRUNC_SAT_S(u64, s64, f64, (f64)INT64_MIN, INT64_MIN, >=, (f64)INT64_MAX, \
              INT64_MAX, x)

#define TRUNC_SAT_U(ut, ft, max, smax, x) \
  ((UNLIKELY((x) != (x)))        ? 0      \
   : (UNLIKELY(!((x) > (ft)-1))) ? 0      \
   : (UNLIKELY(!((x) < (max))))  ? smax   \
                                 : (ut)(x))

#define I32_TRUNC_SAT_U_F32(x) \
  TRUNC_SAT_U(u32, f32, 4294967296.f, UINT32_MAX, x)
#define I64_TRUNC_SAT_U_F32(x) \
  TRUNC_SAT_U(u64, f32, (f32)UINT64_MAX, UINT64_MAX, x)
#define I32_TRUNC_SAT_U_F64(x) TRUNC_SAT_U(u32, f64, 4294967296., UINT32_MAX, x)
#define I64_TRUNC_SAT_U_F64(x) \
  TRUNC_SAT_U(u64, f64, (f64)UINT64_MAX, UINT64_MAX, x)

#define DEFINE_REINTERPRET(name, t1, t2)         \
  static MAYBEINLINE t2 name(t1 x) {             \
    t2 result;                                   \
    wasm_rt_memcpy(&result, &x, sizeof(result)); \
    return result;                               \
  }

DEFINE_REINTERPRET(f32_reinterpret_i32, u32, f32)
DEFINE_REINTERPRET(i32_reinterpret_f32, f32, u32)
DEFINE_REINTERPRET(f64_reinterpret_i64, u64, f64)
DEFINE_REINTERPRET(i64_reinterpret_f64, f64, u64)

static float quiet_nanf(float x) {
  uint32_t tmp;
  wasm_rt_memcpy(&tmp, &x, 4);
  tmp |= 0x7fc00000lu;
  wasm_rt_memcpy(&x, &tmp, 4);
  return x;
}

static double quiet_nan(double x) {
  uint64_t tmp;
  wasm_rt_memcpy(&tmp, &x, 8);
  tmp |= 0x7ff8000000000000llu;
  wasm_rt_memcpy(&x, &tmp, 8);
  return x;
}

static double wasm_quiet(double x) {
  if (UNLIKELY(isnan(x))) {
    return quiet_nan(x);
  }
  return x;
}

static float wasm_quietf(float x) {
  if (UNLIKELY(isnan(x))) {
    return quiet_nanf(x);
  }
  return x;
}

static double wasm_floor(double x) {
  if (UNLIKELY(isnan(x))) {
    return quiet_nan(x);
  }
  return floor(x);
}

static float wasm_floorf(float x) {
  if (UNLIKELY(isnan(x))) {
    return quiet_nanf(x);
  }
  return floorf(x);
}

static double wasm_ceil(double x) {
  if (UNLIKELY(isnan(x))) {
    return quiet_nan(x);
  }
  return ceil(x);
}

static float wasm_ceilf(float x) {
  if (UNLIKELY(isnan(x))) {
    return quiet_nanf(x);
  }
  return ceilf(x);
}

static double wasm_trunc(double x) {
  if (UNLIKELY(isnan(x))) {
    return quiet_nan(x);
  }
  return trunc(x);
}

static float wasm_truncf(float x) {
  if (UNLIKELY(isnan(x))) {
    return quiet_nanf(x);
  }
  return truncf(x);
}

static float wasm_nearbyintf(float x) {
  if (UNLIKELY(isnan(x))) {
    return quiet_nanf(x);
  }
  return nearbyintf(x);
}

static double wasm_nearbyint(double x) {
  if (UNLIKELY(isnan(x))) {
    return quiet_nan(x);
  }
  return nearbyint(x);
}

static float wasm_fabsf(float x) {
  if (UNLIKELY(isnan(x))) {
    uint32_t tmp;
    wasm_rt_memcpy(&tmp, &x, 4);
    tmp = tmp & ~(1UL << 31);
    wasm_rt_memcpy(&x, &tmp, 4);
    return x;
  }
  return fabsf(x);
}

static double wasm_fabs(double x) {
  if (UNLIKELY(isnan(x))) {
    uint64_t tmp;
    wasm_rt_memcpy(&tmp, &x, 8);
    tmp = tmp & ~(1ULL << 63);
    wasm_rt_memcpy(&x, &tmp, 8);
    return x;
  }
  return fabs(x);
}

static double wasm_sqrt(double x) {
  if (UNLIKELY(isnan(x))) {
    return quiet_nan(x);
  }
  return sqrt(x);
}

static float wasm_sqrtf(float x) {
  if (UNLIKELY(isnan(x))) {
    return quiet_nanf(x);
  }
  return sqrtf(x);
}

static inline void memory_fill(wasm_rt_memory_t* mem, u32 d, u32 val, u32 n) {
  RANGE_CHECK(mem, d, n);
  memset(mem->data + d, val, n);
}

static inline void memory_copy(wasm_rt_memory_t* dest,
                               const wasm_rt_memory_t* src,
                               u32 dest_addr,
                               u32 src_addr,
                               u32 n) {
  RANGE_CHECK(dest, dest_addr, n);
  RANGE_CHECK(src, src_addr, n);
  memmove(dest->data + dest_addr, src->data + src_addr, n);
}

static inline void memory_init(wasm_rt_memory_t* dest,
                               const u8* src,
                               u32 src_size,
                               u32 dest_addr,
                               u32 src_addr,
                               u32 n) {
  if (UNLIKELY(src_addr + (uint64_t)n > src_size))
    TRAP(OOB);
  LOAD_DATA((*dest), dest_addr, src + src_addr, n);
}

typedef struct {
  enum { RefFunc, RefNull, GlobalGet } expr_type;
  wasm_rt_func_type_t type;
  wasm_rt_function_ptr_t func;
  size_t module_offset;
} wasm_elem_segment_expr_t;

static inline void funcref_table_init(wasm_rt_funcref_table_t* dest,
                                      const wasm_elem_segment_expr_t* src,
                                      u32 src_size,
                                      u32 dest_addr,
                                      u32 src_addr,
                                      u32 n,
                                      void* module_instance) {
  if (UNLIKELY(src_addr + (uint64_t)n > src_size))
    TRAP(OOB);
  if (UNLIKELY(dest_addr + (uint64_t)n > dest->size))
    TRAP(OOB);
  for (u32 i = 0; i < n; i++) {
    const wasm_elem_segment_expr_t* const src_expr = &src[src_addr + i];
    wasm_rt_funcref_t* const dest_val = &(dest->data[dest_addr + i]);
    switch (src_expr->expr_type) {
      case RefFunc:
        *dest_val = (wasm_rt_funcref_t){
            src_expr->type, src_expr->func,
            (char*)module_instance + src_expr->module_offset};
        break;
      case RefNull:
        *dest_val = wasm_rt_funcref_null_value;
        break;
      case GlobalGet:
        *dest_val = **(wasm_rt_funcref_t**)((char*)module_instance +
                                            src_expr->module_offset);
        break;
    }
  }
}

// Currently wasm2c only supports initializing externref tables with ref.null.
static inline void externref_table_init(wasm_rt_externref_table_t* dest,
                                        u32 src_size,
                                        u32 dest_addr,
                                        u32 src_addr,
                                        u32 n) {
  if (UNLIKELY(src_addr + (uint64_t)n > src_size))
    TRAP(OOB);
  if (UNLIKELY(dest_addr + (uint64_t)n > dest->size))
    TRAP(OOB);
  for (u32 i = 0; i < n; i++) {
    dest->data[dest_addr + i] = wasm_rt_externref_null_value;
  }
}

#define DEFINE_TABLE_COPY(type)                                              \
  static inline void type##_table_copy(wasm_rt_##type##_table_t* dest,       \
                                       const wasm_rt_##type##_table_t* src,  \
                                       u32 dest_addr, u32 src_addr, u32 n) { \
    if (UNLIKELY(dest_addr + (uint64_t)n > dest->size))                      \
      TRAP(OOB);                                                             \
    if (UNLIKELY(src_addr + (uint64_t)n > src->size))                        \
      TRAP(OOB);                                                             \
                                                                             \
    memmove(dest->data + dest_addr, src->data + src_addr,                    \
            n * sizeof(wasm_rt_##type##_t));                                 \
  }

DEFINE_TABLE_COPY(funcref)
DEFINE_TABLE_COPY(externref)

#define DEFINE_TABLE_GET(type)                        \
  static inline wasm_rt_##type##_t type##_table_get(  \
      const wasm_rt_##type##_table_t* table, u32 i) { \
    if (UNLIKELY(i >= table->size))                   \
      TRAP(OOB);                                      \
    return table->data[i];                            \
  }

DEFINE_TABLE_GET(funcref)
DEFINE_TABLE_GET(externref)

#define DEFINE_TABLE_SET(type)                                               \
  static inline void type##_table_set(const wasm_rt_##type##_table_t* table, \
                                      u32 i, const wasm_rt_##type##_t val) { \
    if (UNLIKELY(i >= table->size))                                          \
      TRAP(OOB);                                                             \
    table->data[i] = val;                                                    \
  }

DEFINE_TABLE_SET(funcref)
DEFINE_TABLE_SET(externref)

#define DEFINE_TABLE_FILL(type)                                               \
  static inline void type##_table_fill(const wasm_rt_##type##_table_t* table, \
                                       u32 d, const wasm_rt_##type##_t val,   \
                                       u32 n) {                               \
    if (UNLIKELY((uint64_t)d + n > table->size))                              \
      TRAP(OOB);                                                              \
    for (uint32_t i = d; i < d + n; i++) {                                    \
      table->data[i] = val;                                                   \
    }                                                                         \
  }

DEFINE_TABLE_FILL(funcref)
DEFINE_TABLE_FILL(externref)

#if defined(__GNUC__) || defined(__clang__)
#define FUNC_TYPE_DECL_EXTERN_T(x) extern const char* const x
#define FUNC_TYPE_EXTERN_T(x) const char* const x
#define FUNC_TYPE_T(x) static const char* const x
#else
#define FUNC_TYPE_DECL_EXTERN_T(x) extern const char x[]
#define FUNC_TYPE_EXTERN_T(x) const char x[]
#define FUNC_TYPE_T(x) static const char x[]
#endif
