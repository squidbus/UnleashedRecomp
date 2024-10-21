void Config::Load()
{
    try
    {
        auto toml = toml::parse_file(GetConfigPath().string());

        TOML_BEGIN_SECTION("System")
        {
            TOML_READ_ENUM(ELanguage, Language);
            TOML_READ_BOOLEAN(Hints);
            TOML_READ_ENUM(EScoreBehaviour, ScoreBehaviour);
            TOML_READ_BOOLEAN(UnleashOutOfControlDrain);
            TOML_READ_BOOLEAN(WerehogHubTransformVideo);
            TOML_READ_BOOLEAN(LogoSkip);
        }
        TOML_END_SECTION();

        TOML_BEGIN_SECTION("Controls")
        {
            TOML_READ_BOOLEAN(CameraXInvert);
            TOML_READ_BOOLEAN(CameraYInvert);
            TOML_READ_BOOLEAN(XButtonHoming);
            TOML_READ_BOOLEAN(UnleashCancel);
        }
        TOML_END_SECTION();

        TOML_BEGIN_SECTION("Audio")
        {
            TOML_READ_FLOAT(MusicVolume);
            TOML_READ_FLOAT(SEVolume);
            TOML_READ_ENUM(EVoiceLanguage, VoiceLanguage);
            TOML_READ_FLOAT(Subtitles);
            TOML_READ_BOOLEAN(WerehogBattleMusic);
        }
        TOML_END_SECTION();

        TOML_BEGIN_SECTION("Video")
        {
            TOML_READ_ENUM(EGraphicsAPI, GraphicsAPI);
            TOML_READ_INTEGER(WindowWidth);
            TOML_READ_INTEGER(WindowHeight);
            TOML_READ_FLOAT(ResolutionScale);
            TOML_READ_BOOLEAN(Fullscreen);
            TOML_READ_BOOLEAN(VSync);
            TOML_READ_INTEGER(BufferCount);
            TOML_READ_INTEGER(FPS);
            TOML_READ_FLOAT(Brightness);
            TOML_READ_INTEGER(MSAA);
            TOML_READ_INTEGER(AnisotropicFiltering);
            TOML_READ_INTEGER(ShadowResolution);
            TOML_READ_ENUM(EGITextureFiltering, GITextureFiltering);
            TOML_READ_BOOLEAN(AlphaToCoverage);
            TOML_READ_BOOLEAN(Xbox360ColorCorrection);
            TOML_READ_ENUM(EMovieScaleMode, MovieScaleMode);
            TOML_READ_ENUM(EUIScaleMode, UIScaleMode);
        }
        TOML_END_SECTION();
    }
    catch (toml::parse_error& err)
    {
        printf("Failed to parse configuration: %s\n", err.what());
    }

    ResolutionScale = std::clamp(ResolutionScale.Value, 0.25f, 2.0f);
}

void Config::Save()
{
    std::string result;
    std::string section;

    for (auto def : Definitions)
    {
        auto isFirstSection = section.empty();
        auto isDefWithSection = section != def->GetSection();
        auto tomlDef = def->GetDefinition(isDefWithSection);

        section = def->GetSection();

        // Don't output prefix space for first section.
        if (!isFirstSection && isDefWithSection)
            result += '\n';

        result += tomlDef + '\n';
    }

    std::ofstream out(GetConfigPath());

    if (out.is_open())
    {
        out << result;
        out.close();
    }
    else
    {
        printf("Failed to write configuration.\n");
    }
}
