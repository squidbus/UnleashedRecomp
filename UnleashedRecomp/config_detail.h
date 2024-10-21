#pragma once

#define USER_DIRECTORY "SWA"

#define TOML_FILE "config.toml"

#define TOML_BEGIN_SECTION(name) if (auto pSection = toml[name].as_table()) { const auto& section = *pSection;
#define TOML_END_SECTION() }

#define TOML_READ_STRING(var)     var.Value = section[#var].value_or<std::string>(var.DefaultValue);
#define TOML_READ_BOOLEAN(var)    var.Value = section[#var].value_or(var.DefaultValue);
#define TOML_READ_FLOAT(var)      var.Value = section[#var].value_or(var.DefaultValue);
#define TOML_READ_INTEGER(var)    var.Value = section[#var].value_or(var.DefaultValue);
#define TOML_READ_DOUBLE(var)     var.Value = section[#var].value_or(var.DefaultValue);
#define TOML_READ_ENUM(type, var) var.Value = (type)section[#var].value_or(var.DefaultValue);

#define CONFIG_DEFINE(section, type, name, defaultValue) inline static ConfigDef<type> name{section, #name, defaultValue};
#define CONFIG_VALUE(name) Config::name.Value

#define CONFIG_GET_DEFAULT(name) Config::name.DefaultValue
#define CONFIG_SET_DEFAULT(name) Config::name.MakeDefault();

class ConfigDefBase
{
public:
    virtual ~ConfigDefBase() = default;
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
    std::string Section;
    std::string Name;
    T DefaultValue{};
    T Value{ DefaultValue };

    ConfigDef(std::string section, std::string name, T defaultValue) : Section(section), Name(name), DefaultValue(defaultValue)
    {
        Config::Definitions.emplace_back(this);
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
        else if constexpr (std::is_same<T, bool>::value)
        {
            return Value ? "true" : "false";
        }

        std::ostringstream oss;
        oss << Value;
        return oss.str();
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
