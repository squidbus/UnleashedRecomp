#include <os/logger_detail.h>
#include <print>

#define FOREGROUND_WHITE  (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define FOREGROUND_YELLOW (FOREGROUND_RED | FOREGROUND_GREEN)

HANDLE g_hStandardOutput;

void os::logger::detail::Init()
{
    g_hStandardOutput = GetStdHandle(STD_OUTPUT_HANDLE);
}

void os::logger::detail::Log(const std::string_view str, detail::ELogType type, const char* func)
{
    switch (type)
    {
        case ELogType::Utility:
            SetConsoleTextAttribute(g_hStandardOutput, FOREGROUND_GREEN | FOREGROUND_INTENSITY);
            break;

        case ELogType::Warning:
            SetConsoleTextAttribute(g_hStandardOutput, FOREGROUND_YELLOW | FOREGROUND_INTENSITY);
            break;

        case ELogType::Error:
            SetConsoleTextAttribute(g_hStandardOutput, FOREGROUND_RED | FOREGROUND_INTENSITY);
            break;

        default:
            SetConsoleTextAttribute(g_hStandardOutput, FOREGROUND_WHITE);
            break;
    }

    if (func)
    {
        fmt::println("[{}] {}", func, str);
    }
    else
    {
        fmt::println("{}", str);
    }

    SetConsoleTextAttribute(g_hStandardOutput, FOREGROUND_WHITE);
}
