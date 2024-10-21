#pragma once

#define USER_DIRECTORY "SWA"

#define TOML_FILE "config.toml"

#define CONFIG_DEFINE(section, type, name, defaultValue) \
    inline static ConfigDef<type> name{section, #name, defaultValue};

#define CONFIG_DEFINE_CALLBACK(section, type, name, defaultValue, readCallback) \
    inline static ConfigDef<type> name{section, #name, defaultValue, [](ConfigDef<type>* def) readCallback};

#define CONFIG_GET_DEFAULT(name) Config::name.DefaultValue
#define CONFIG_SET_DEFAULT(name) Config::name.MakeDefault();

class ConfigDefBase
{
public:
    virtual ~ConfigDefBase() = default;
    virtual void ReadValue(toml::v3::ex::parse_result& toml) = 0;
    virtual void MakeDefault() = 0;
    virtual std::string GetSection() const = 0;
    virtual std::string GetName() const = 0;
    virtual std::string GetDefinition(bool withSection = false) const = 0;
    virtual std::string ToString() const = 0;
};

template<typename T>
class ConfigDef : public ConfigDefBase
{
public:
    std::string Section{};
    std::string Name{};
    T DefaultValue{};
    T Value{ DefaultValue };
    std::function<void(ConfigDef<T>*)> ReadCallback;

    ConfigDef(std::string section, std::string name, T defaultValue)
        : Section(section), Name(name), DefaultValue(defaultValue)
    {
        Config::Definitions.emplace_back(this);
    }

    ConfigDef(std::string section, std::string name, T defaultValue, std::function<void(ConfigDef<T>*)> readCallback)
        : Section(section), Name(name), DefaultValue(defaultValue), ReadCallback(readCallback)
    {
        Config::Definitions.emplace_back(this);
    }

    void ReadValue(toml::v3::ex::parse_result& toml) override
    {
        if (auto pSection = toml[Section].as_table())
        {
            const auto& section = *pSection;

            if constexpr (std::is_same<T, std::string>::value)
            {
                Value = section[Name].value_or<std::string>(DefaultValue);
            }
            else if constexpr (std::is_enum_v<T>)
            {
                Value = T(section[Name].value_or(std::underlying_type_t<T>(DefaultValue)));
            }
            else
            {
                Value = section[Name].value_or(DefaultValue);
            }
        }
        
        if (ReadCallback)
            ReadCallback(this);
    }

    void MakeDefault() override
    {
        Value = DefaultValue;
    }

    std::string GetSection() const override
    {
        return Section;
    }

    std::string GetName() const override
    {
        return Name;
    }

    std::string GetDefinition(bool withSection = false) const override
    {
        std::string result;

        if (withSection)
            result += "[" + Section + "]\n";

        result += Name + " = " + ToString();

        return result;
    }

    std::string ToString() const override
    {
        if constexpr (std::is_same<T, std::string>::value)
        {
            return Value;
        }
        else if constexpr (std::is_enum_v<T>)
        {
            return std::format("{}", std::underlying_type_t<T>(Value));
        }
        else
        {
            return std::format("{}", Value);
        }
    }

    ConfigDef& operator=(const ConfigDef& other)
    {
        if (this != &other)
            Value = other.Value;

        return *this;
    }

    operator T() const
    {
        return Value;
    }

    void operator=(const T& other)
    {
        Value = other;
    }
};

enum class Language : uint32_t
{
    English = 1,
    Japanese,
    German,
    French,
    Spanish,
    Italian
};

enum class ScoreBehaviour : uint32_t
{
    CheckpointReset,
    CheckpointRetain
};

enum class VoiceLanguage : uint32_t
{
    English,
    Japanese
};

enum class GraphicsAPI : uint32_t
{
    D3D12,
    Vulkan
};

enum class GITextureFiltering : uint32_t
{
    Linear,
    Bicubic
};

enum class MovieScaleMode : uint32_t
{
    Stretch,
    Fit,
    Fill
};

enum class UIScaleMode : uint32_t
{
    Stretch,
    Edge,
    Centre
};
