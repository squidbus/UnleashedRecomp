#pragma once

// TODO (Hyper): use toml.

#define INI_FILE "SWA.ini"

#define INI_BEGIN_SECTION(section) { std::string CurrentSection = section;
#define INI_END_SECTION() }

#define INI_READ_STRING(var)     var = BasicIni::Get(ini, CurrentSection, #var, var)
#define INI_READ_BOOLEAN(var)    var = BasicIni::GetBoolean(ini, CurrentSection, #var, var)
#define INI_READ_FLOAT(var)      var = BasicIni::GetFloat(ini, CurrentSection, #var, var)
#define INI_READ_INTEGER(var)    var = BasicIni::GetInteger(ini, CurrentSection, #var, var)
#define INI_READ_DOUBLE(var)     var = BasicIni::GetDouble(ini, CurrentSection, #var, var)
#define INI_READ_ENUM(type, var) var = (type)BasicIni::GetInteger(ini, CurrentSection, #var, var)

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
    inline static bool WerehogHubTransformVideo = true;
    inline static bool BootToTitle = false;

    // Controls
    inline static bool XButtonHoming = true;
    inline static bool UnleashCancel = false;

    // Audio
    inline static bool WerehogBattleMusic = true;

    // Video
    inline static uint32_t Width = 1280;
    inline static uint32_t Height = 720;
    inline static int32_t ShadowResolution = 4096;
    inline static size_t MSAA = 4;
    inline static EMovieScaleMode MovieScaleMode = EMovieScaleMode_Fit;
    inline static EUIScaleMode UIScaleMode = EUIScaleMode_Centre;
    inline static bool AlphaToCoverage = false;
    inline static bool Fullscreen = false;
    inline static bool VSync = false;
    inline static uint32_t BufferCount = 3;

    static void Read();
};
