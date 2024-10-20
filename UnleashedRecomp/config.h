#pragma once

// TODO: move this outside of the game directory?
#define TOML_FILE "SWA.toml"

#define TOML_BEGIN_SECTION(name) if (auto pSection = toml[name].as_table()) { const auto& section = *pSection;
#define TOML_END_SECTION() }

#define TOML_READ_STRING(var)     var = section[#var].value_or<std::string>(var);
#define TOML_READ_BOOLEAN(var)    var = section[#var].value_or(var);
#define TOML_READ_FLOAT(var)      var = section[#var].value_or(var);
#define TOML_READ_INTEGER(var)    var = section[#var].value_or(var);
#define TOML_READ_DOUBLE(var)     var = section[#var].value_or(var);
#define TOML_READ_ENUM(type, var) var = (type)section[#var].value_or(var);

enum ELanguage : uint32_t
{
    ELanguage_English = 1,
    ELanguage_Japanese,
    ELanguage_German,
    ELanguage_French,
    ELanguage_Spanish,
    ELanguage_Italian
};

enum EScoreBehaviour : uint32_t
{
    EScoreBehaviour_CheckpointReset,
    EScoreBehaviour_CheckpointRetain
};

enum EGraphicsAPI
{
    EGraphicsAPI_D3D12,
    EGraphicsAPI_Vulkan
};

enum EMovieScaleMode : uint32_t
{
    EMovieScaleMode_Stretch,
    EMovieScaleMode_Fit,
    EMovieScaleMode_Fill
};

enum EUIScaleMode : uint32_t
{
    EUIScaleMode_Stretch,
    EUIScaleMode_Edge,
    EUIScaleMode_Centre
};

class Config
{
public:
    // System
    inline static ELanguage Language = ELanguage_English;
    inline static EScoreBehaviour ScoreBehaviour = EScoreBehaviour_CheckpointReset;
    inline static bool Hints = true;
    inline static bool UnleashOutOfControlDrain = true;
    inline static bool WerehogHubTransformVideo = true;
    inline static bool LogoSkip = false;

    // Controls
    inline static bool XButtonHoming = true;
    inline static bool UnleashCancel = false;

    // Audio
    inline static bool WerehogBattleMusic = true;

    // Video
    inline static EGraphicsAPI GraphicsAPI = EGraphicsAPI_D3D12;
    inline static uint32_t WindowWidth = 1280;
    inline static uint32_t WindowHeight = 720;
    inline static float ResolutionScale = 1.0f;
    inline static int32_t ShadowResolution = 4096;
    inline static size_t MSAA = 4;
    inline static EMovieScaleMode MovieScaleMode = EMovieScaleMode_Fit;
    inline static EUIScaleMode UIScaleMode = EUIScaleMode_Centre;
    inline static bool AlphaToCoverage = false;
    inline static bool Fullscreen = false;
    inline static bool VSync = false;
    inline static uint32_t BufferCount = 3;

    static void Load();
    static void Save();
};
