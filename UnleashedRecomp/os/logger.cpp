#include <os/logger.h>
#include <os/logger_detail.h>

void os::logger::Init()
{
    detail::Init();
}

void os::logger::Log(const std::string_view str, detail::ELogType type, const char* func)
{
    detail::Log(str, type, func);
}
