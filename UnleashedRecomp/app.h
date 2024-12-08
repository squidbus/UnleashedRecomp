#pragma once

class App
{
public:
    inline static bool s_isInit;
    inline static bool s_isMissingDLC;

    inline static double s_deltaTime;

    static void Exit(std::vector<std::string> restartArgs = {});
};
