#pragma once

#include "config_detail.h"

class Config
{
public:
    inline static std::vector<std::shared_ptr<ConfigDefBase>> Definitions{};

    CONFIG_DEFINE("System", ELanguage, Language, ELanguage_English);
    CONFIG_DEFINE("System", bool, Hints, true);
    CONFIG_DEFINE("System", EScoreBehaviour, ScoreBehaviour, EScoreBehaviour_CheckpointReset);
    CONFIG_DEFINE("System", bool, UnleashOutOfControlDrain, true);
    CONFIG_DEFINE("System", bool, WerehogHubTransformVideo, true);
    CONFIG_DEFINE("System", bool, LogoSkip, false);

    CONFIG_DEFINE("Controls", bool, CameraXInvert, false);
    CONFIG_DEFINE("Controls", bool, CameraYInvert, false);
    CONFIG_DEFINE("Controls", bool, XButtonHoming, true);
    CONFIG_DEFINE("Controls", bool, UnleashCancel, false);

    CONFIG_DEFINE("Audio", float, MusicVolume, 1.0f);
    CONFIG_DEFINE("Audio", float, SEVolume, 1.0f);
    CONFIG_DEFINE("Audio", EVoiceLanguage, VoiceLanguage, EVoiceLanguage_English);
    CONFIG_DEFINE("Audio", bool, Subtitles, true);
    CONFIG_DEFINE("Audio", bool, WerehogBattleMusic, true);

    CONFIG_DEFINE("Video", EGraphicsAPI, GraphicsAPI, EGraphicsAPI_D3D12);
    CONFIG_DEFINE("Video", size_t, WindowWidth, 1280);
    CONFIG_DEFINE("Video", size_t, WindowHeight, 720);
    CONFIG_DEFINE("Video", float, ResolutionScale, 1.0f);
    CONFIG_DEFINE("Video", bool, Fullscreen, false);
    CONFIG_DEFINE("Video", bool, VSync, true);
    CONFIG_DEFINE("Video", size_t, BufferCount, 3);
    CONFIG_DEFINE("Video", size_t, FPS, 60);
    CONFIG_DEFINE("Video", float, Brightness, 0.5f);
    CONFIG_DEFINE("Video", size_t, MSAA, 4);
    CONFIG_DEFINE("Video", size_t, AnisotropicFiltering, 16);
    CONFIG_DEFINE("Video", int32_t, ShadowResolution, 4096);
    CONFIG_DEFINE("Video", EGITextureFiltering, GITextureFiltering, EGITextureFiltering_Bicubic);
    CONFIG_DEFINE("Video", bool, AlphaToCoverage, false);
    CONFIG_DEFINE("Video", bool, Xbox360ColorCorrection, false);
    CONFIG_DEFINE("Video", EMovieScaleMode, MovieScaleMode, EMovieScaleMode_Fit);
    CONFIG_DEFINE("Video", EUIScaleMode, UIScaleMode, EUIScaleMode_Centre);

    static std::filesystem::path GetUserPath()
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

    static std::filesystem::path GetConfigPath()
    {
        return GetUserPath() / TOML_FILE;
    }

    static void Load();
    static void Save();
};
