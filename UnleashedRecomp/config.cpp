void Config::Load()
{
    try
    {
        auto toml = toml::parse_file(TOML_FILE);

        TOML_BEGIN_SECTION("System")
        {
            TOML_READ_ENUM(ELanguage, Language);
            TOML_READ_ENUM(EScoreBehaviour, ScoreBehaviour);
            TOML_READ_BOOLEAN(Hints);
            TOML_READ_BOOLEAN(UnleashOutOfControlDrain);
            TOML_READ_BOOLEAN(WerehogHubTransformVideo);
            TOML_READ_BOOLEAN(LogoSkip);
        }
        TOML_END_SECTION();

        TOML_BEGIN_SECTION("Controls")
        {
            TOML_READ_BOOLEAN(XButtonHoming);
            TOML_READ_BOOLEAN(UnleashCancel);
        }
        TOML_END_SECTION();

        TOML_BEGIN_SECTION("Audio")
        {
            TOML_READ_BOOLEAN(WerehogBattleMusic);
        }
        TOML_END_SECTION();

        TOML_BEGIN_SECTION("Video")
        {
            TOML_READ_INTEGER(WindowWidth);
            TOML_READ_INTEGER(WindowHeight);
            TOML_READ_FLOAT(ResolutionScale);
            TOML_READ_INTEGER(ShadowResolution);
            TOML_READ_INTEGER(MSAA);
            TOML_READ_ENUM(EMovieScaleMode, MovieScaleMode);
            TOML_READ_ENUM(EUIScaleMode, UIScaleMode);
            TOML_READ_BOOLEAN(AlphaToCoverage);
            TOML_READ_BOOLEAN(Fullscreen);
            TOML_READ_BOOLEAN(VSync);
            TOML_READ_INTEGER(BufferCount);
        }
        TOML_END_SECTION();
    }
    catch (toml::parse_error& err)
    {
        printf("Failed to parse configuration: %s\n", err.what());
    }
}

void Config::Save()
{
    // TODO
}
