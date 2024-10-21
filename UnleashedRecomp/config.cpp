void Config::Load()
{
    try
    {
        auto toml = toml::parse_file(GetConfigPath().string());

        for (auto def : Definitions)
            def->ReadValue(toml);
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
