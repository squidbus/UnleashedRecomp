#pragma once

#include <cpu/guest_code.h>
#include <cpu/guest_stack_var.h>
#include <kernel/function.h>

#define SWA_CONCAT2(x, y) x##y
#define SWA_CONCAT(x, y) SWA_CONCAT2(x, y)

#define SWA_INSERT_PADDING(length) \
    uint8_t SWA_CONCAT(pad, __LINE__)[length]

#define SWA_ASSERT_OFFSETOF(type, field, offset) \
    static inline swa_assert_offsetof<BB_OFFSETOF(type, field), offset> SWA_CONCAT(_, __COUNTER__)

#define SWA_ASSERT_SIZEOF(type, size) \
    static inline swa_assert_sizeof<sizeof type, size> SWA_CONCAT(_, __COUNTER__)

#define SWA_VIRTUAL_FUNCTION(returnType, virtualIndex, ...) \
    GuestToHostFunction<returnType>(*(be<uint32_t>*)(g_memory.Translate(*(be<uint32_t>*)(this) + (4 * virtualIndex))), __VA_ARGS__)

struct swa_null_ctor
{
};

template<int TActual, int TExpected>
struct swa_assert_offsetof
{
    static_assert(TActual == TExpected, "offsetof assertion failed");
};

template<int TActual, int TExpected>
struct swa_assert_sizeof
{
    static_assert(TActual == TExpected, "sizeof assertion failed");
};
