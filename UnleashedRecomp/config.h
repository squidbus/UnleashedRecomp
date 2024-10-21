#pragma once

#define USER_DIRECTORY "SWA"

#define TOML_FILE "config.toml"

#define TOML_BEGIN_SECTION(name) if (auto pSection = toml[name].as_table()) { const auto& section = *pSection;
#define TOML_END_SECTION() }

#define TOML_READ_STRING(var)     var = section[#var].value_or<std::string>(var);
#define TOML_READ_BOOLEAN(var)    var = section[#var].value_or(var);
#define TOML_READ_FLOAT(var)      var = section[#var].value_or(var);
#define TOML_READ_INTEGER(var)    var = section[#var].value_or(var);
#define TOML_READ_DOUBLE(var)     var = section[#var].value_or(var);
#define TOML_READ_ENUM(type, var) var = (type)section[#var].value_or(var);

#define CONFIG_DEFINE(type, name, defaultValue) \
    static const type name##_Default = defaultValue; \
    inline static type name = name##_Default;

#define CONFIG_GET_DEFAULT(name) Config::name##_Default
#define CONFIG_SET_DEFAULT(name) Config::name = CONFIG_GET_DEFAULT(name)

enum ELanguage
{
    ELanguage_English = 1,
    ELanguage_Japanese,
    ELanguage_German,
    ELanguage_French,
    ELanguage_Spanish,
    ELanguage_Italian
};

enum EScoreBehaviour
{
    EScoreBehaviour_CheckpointReset,
    EScoreBehaviour_CheckpointRetain
};

enum EVoiceLanguage
{
    EVoiceLanguage_English,
    EVoiceLanguage_Japanese
};

enum EGraphicsAPI
{
    EGraphicsAPI_D3D12,
    EGraphicsAPI_Vulkan
};

enum EGITextureFiltering
{
    EGITextureFiltering_Linear,
    EGITextureFiltering_Bicubic
};

enum EMovieScaleMode
{
    EMovieScaleMode_Stretch,
    EMovieScaleMode_Fit,
    EMovieScaleMode_Fill
};

enum EUIScaleMode
{
    EUIScaleMode_Stretch,
    EUIScaleMode_Edge,
    EUIScaleMode_Centre
};

class Config
{
public:
    // System
    CONFIG_DEFINE(ELanguage, Language, ELanguage_English);
    CONFIG_DEFINE(bool, Hints, true);
    CONFIG_DEFINE(EScoreBehaviour, ScoreBehaviour, EScoreBehaviour_CheckpointReset);
    CONFIG_DEFINE(bool, UnleashOutOfControlDrain, true);
    CONFIG_DEFINE(bool, WerehogHubTransformVideo, true);
    CONFIG_DEFINE(bool, LogoSkip, false);

    // Controls
    CONFIG_DEFINE(bool, CameraXInvert, false);
    CONFIG_DEFINE(bool, CameraYInvert, false);
    CONFIG_DEFINE(bool, XButtonHoming, true);
    CONFIG_DEFINE(bool, UnleashCancel, false);

    // Audio
    CONFIG_DEFINE(float, MusicVolume, 1.0f);
    CONFIG_DEFINE(float, SEVolume, 1.0f);
    CONFIG_DEFINE(EVoiceLanguage, VoiceLanguage, EVoiceLanguage_English);
    CONFIG_DEFINE(bool, Subtitles, true);
    CONFIG_DEFINE(bool, WerehogBattleMusic, true);

    // Video
    CONFIG_DEFINE(EGraphicsAPI, GraphicsAPI, EGraphicsAPI_D3D12);
    CONFIG_DEFINE(size_t, WindowWidth, 1280);
    CONFIG_DEFINE(size_t, WindowHeight, 720);
    CONFIG_DEFINE(float, ResolutionScale, 1.0f);
    CONFIG_DEFINE(bool, Fullscreen, false);
    CONFIG_DEFINE(bool, VSync, true);
    CONFIG_DEFINE(size_t, BufferCount, 3);
    CONFIG_DEFINE(size_t, FPS, 60);
    CONFIG_DEFINE(float, Brightness, 0.5f);
    CONFIG_DEFINE(size_t, MSAA, 4);
    CONFIG_DEFINE(size_t, AnisotropicFiltering, 16);
    CONFIG_DEFINE(int32_t, ShadowResolution, 4096);
    CONFIG_DEFINE(EGITextureFiltering, GITextureFiltering, EGITextureFiltering_Bicubic);
    CONFIG_DEFINE(bool, AlphaToCoverage, false);
    CONFIG_DEFINE(bool, Xbox360ColorCorrection, false);
    CONFIG_DEFINE(EMovieScaleMode, MovieScaleMode, EMovieScaleMode_Fit);
    CONFIG_DEFINE(EUIScaleMode, UIScaleMode, EUIScaleMode_Centre);

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

    static void Load();
    static void Save();
};
