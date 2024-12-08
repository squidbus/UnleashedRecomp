#include <os/process_detail.h>

std::filesystem::path os::process::detail::GetExecutablePath()
{
    char exePath[MAX_PATH];

    if (!GetModuleFileNameA(nullptr, exePath, MAX_PATH))
        return std::filesystem::path();

    return std::filesystem::path(exePath);
}

std::filesystem::path os::process::detail::GetWorkingDirectory()
{
    char workPath[MAX_PATH];

    if (!GetCurrentDirectoryA(MAX_PATH, workPath))
        return std::filesystem::path();

    return std::filesystem::path(workPath);
}

bool os::process::detail::StartProcess(const std::filesystem::path path, const std::vector<std::string> args, std::filesystem::path work)
{
    if (path.empty())
        return false;

    if (work.empty())
        work = path.parent_path();

    auto cli = path.string();
    for (auto& arg : args)
        cli += " " + arg;

    STARTUPINFOA startInfo{ sizeof(STARTUPINFOA) };
    PROCESS_INFORMATION procInfo{};

    if (!CreateProcessA(path.string().c_str(), cli.data(), nullptr, nullptr, false, 0, nullptr, work.string().c_str(), &startInfo, &procInfo))
        return false;

    CloseHandle(procInfo.hProcess);
    CloseHandle(procInfo.hThread);

    return true;
}
