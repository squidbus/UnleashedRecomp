#include <os/logger.h>
#include <os/logger_detail.h>

void os::logger::Log(const std::string& str, detail::ELogType type, const char* func)
{
    detail::Log(str, type, func);
}
