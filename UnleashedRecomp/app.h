#pragma once

#include <user/config_detail.h>

class App
{
public:
    inline static bool s_isInit;
    inline static bool s_isMissingDLC;

    inline static ELanguage s_language;

    inline static double s_deltaTime;

    static void Restart(std::vector<std::string> restartArgs = {});
    static void Exit();
};
