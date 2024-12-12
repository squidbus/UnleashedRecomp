#pragma once

#include <source_location>

namespace os::logger::detail
{
    enum class ELogType
    {
        None,
        Utility,
        Warning,
        Error
    };

    void Init();
    void Log(const std::string_view str, ELogType type = ELogType::None, const char* func = nullptr);
}
