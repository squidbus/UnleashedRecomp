#pragma once

namespace os::process::detail
{
    std::filesystem::path GetExecutablePath();
    std::filesystem::path GetWorkingDirectory();
    bool StartProcess(const std::filesystem::path path, const std::vector<std::string> args, std::filesystem::path work = {});
}
