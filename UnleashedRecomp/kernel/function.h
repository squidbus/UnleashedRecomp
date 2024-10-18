#pragma once
#include <cpu/ppc_context.h>
#include <array>
#include "xbox.h"

template <typename R, typename... T>
constexpr std::tuple<T...> function_args(R(*)(T...)) noexcept
{
    return std::tuple<T...>();
}

template<auto V>
static constexpr decltype(V) constant_v = V;

template<typename T>
static constexpr bool is_precise_v = std::is_same_v<T, float> || std::is_same_v<T, double>;

template<auto Func>
struct arg_count_t
{
    static constexpr size_t value = std::tuple_size_v<decltype(function_args(Func))>;
};

template<typename TCallable, int I = 0, typename ...TArgs>
std::enable_if_t<(I >= sizeof...(TArgs)), void> _tuple_for(std::tuple<TArgs...>&, const TCallable& callable) noexcept
{

}

template<typename TCallable, int I = 0, typename ...TArgs>
std::enable_if_t<(I < sizeof...(TArgs)), void> _tuple_for(std::tuple<TArgs...>& tpl, const TCallable& callable) noexcept
{
    callable(std::get<I>(tpl), I);

    _tuple_for<TCallable, I + 1>(tpl, callable);
}

struct ArgTranslator
{
    FORCEINLINE constexpr static uint64_t GetIntegerArgumentValue(const PPCContext& ctx, uint8_t* base, size_t arg) noexcept
    {
        if (arg <= 7)
        {
            switch (arg)
            {
                case 0: return ctx.r3.u32;
                case 1: return ctx.r4.u32;
                case 2: return ctx.r5.u32;
                case 3: return ctx.r6.u32;
                case 4: return ctx.r7.u32;
                case 5: return ctx.r8.u32;
                case 6: return ctx.r9.u32;
                case 7: return ctx.r10.u32;
                default: break;
            }
        }

        return *reinterpret_cast<XLPDWORD>(base + ctx.r1.u32 + 0x54 + ((arg - 8) * 8));
    }

    FORCEINLINE static double GetPrecisionArgumentValue(const PPCContext& ctx, uint8_t* base, size_t arg) noexcept
    {
        switch (arg)
        {
            case 0: return ctx.f1.f64;
            case 1: return ctx.f2.f64;
            case 2: return ctx.f3.f64;
            case 3: return ctx.f4.f64;
            case 4: return ctx.f5.f64;
            case 5: return ctx.f6.f64;
            case 6: return ctx.f7.f64;
            case 7: return ctx.f8.f64;
            case 8: return ctx.f9.f64;
            case 9: return ctx.f10.f64;
            case 10: return ctx.f11.f64;
            case 11: return ctx.f12.f64;
            case 12: return ctx.f13.f64;
            [[unlikely]] default: break;
        }

        // how did you end up here
        return 0;
    }

    template<typename T>
    FORCEINLINE constexpr static std::enable_if_t<!std::is_pointer_v<T>, T> GetValue(PPCContext& ctx, uint8_t* base, size_t idx) noexcept
    {
        if constexpr (is_precise_v<T>)
        {
            return static_cast<T>(GetPrecisionArgumentValue(ctx, base, idx));
        }
        else
        {
            return static_cast<T>(GetIntegerArgumentValue(ctx, base, idx));
        }
    }

    template<typename T>
    FORCEINLINE constexpr static std::enable_if_t<std::is_pointer_v<T>, T> GetValue(PPCContext& ctx, uint8_t* base, size_t idx) noexcept
    {
        const auto v = GetIntegerArgumentValue(ctx, base, idx);
        if (!v)
        {
            return nullptr;
        }

        return reinterpret_cast<T>(base + static_cast<uint32_t>(v));
    }
};

struct Argument
{
    int type{};
    int ordinal{};
};

template<auto Func>
constexpr std::array<Argument, arg_count_t<Func>::value> GatherFunctionArguments()
{
    std::array<Argument, arg_count_t<Func>::value> args{};

    int intOrdinal{};
    int floatOrdinal{};
    size_t i{};

    if constexpr (!args.empty())
    {
        std::apply([&](const auto& first, const auto&... rest)
            {
                auto append = [&]<typename T>(const T & v)
                {
                    if constexpr (is_precise_v<T>)
                    {
                        args[i] = { 1, floatOrdinal++ };
                    }
                    else
                    {
                        intOrdinal++;
                        args[i] = { 0, static_cast<int>(i) }; // what the fuck
                    }

                    i++;
                };

                append(first);
                (append(rest), ...);
            }, function_args(Func));
    }

    return args;
}

template<auto Func, size_t I>
struct arg_ordinal_t
{
    static constexpr size_t value = GatherFunctionArguments<Func>()[I].ordinal;
};

template<auto Func, int I = 0, typename ...TArgs>
FORCEINLINE void _translate_args(PPCContext& ctx, uint8_t* base, std::tuple<TArgs...>&) noexcept
requires (I >= sizeof...(TArgs))
{
}

template <auto Func, int I = 0, typename ...TArgs>
FORCEINLINE std::enable_if_t<(I < sizeof...(TArgs)), void> _translate_args(PPCContext& ctx, uint8_t* base, std::tuple<TArgs...>& tpl) noexcept
{
    using T = std::tuple_element_t<I, std::remove_reference_t<decltype(tpl)>>;
    std::get<I>(tpl) = ArgTranslator::GetValue<T>(ctx, base, arg_ordinal_t<Func, I>::value);

    _translate_args<Func, I + 1>(ctx, base, tpl);
}

template<auto Func>
FORCEINLINE PPC_FUNC(GuestFunction)
{
    using ret_t = decltype(std::apply(Func, function_args(Func)));

    auto args = function_args(Func);
    _translate_args<Func>(ctx, base, args);

    if constexpr (std::is_same_v<ret_t, void>)
    {
        std::apply(Func, args);
    }
    else
    {
        auto v = std::apply(Func, args);

        if constexpr (std::is_pointer<ret_t>())
        {
            if (v != nullptr)
            {
                ctx.r3.u64 = static_cast<uint32_t>(reinterpret_cast<size_t>(v) - reinterpret_cast<size_t>(base));
            }
            else
            {
                ctx.r3.u64 = NULL;
            }
        }
        else if constexpr (is_precise_v<ret_t>)
        {
            ctx.f1.f64 = v;
        }
        else
        {
            ctx.r3.u64 = (uint64_t)v;
        }
    }
}

#define GUEST_FUNCTION_HOOK(subroutine, function) \
    PPC_FUNC(subroutine) { GuestFunction<function>(ctx, base); }

#define GUEST_FUNCTION_STUB(subroutine) \
    PPC_FUNC(subroutine) { }
