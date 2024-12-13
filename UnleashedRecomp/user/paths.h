#pragma once

#define USER_DIRECTORY "SWA"

inline std::filesystem::path GetGamePath()
{
    return std::filesystem::current_path();
}

inline std::filesystem::path GetUserPath()
{
    if (std::filesystem::exists("portable.txt"))
        return std::filesystem::current_path();

    std::filesystem::path userPath{};

    // TODO: handle platform-specific paths.
    PWSTR knownPath = NULL;
    if (SHGetKnownFolderPath(FOLDERID_RoamingAppData, 0, NULL, &knownPath) == S_OK)
        userPath = std::filesystem::path{ knownPath } / USER_DIRECTORY;

    CoTaskMemFree(knownPath);

    return userPath;
}

inline std::filesystem::path GetSavePath()
{
    return GetUserPath() / "save";
}
