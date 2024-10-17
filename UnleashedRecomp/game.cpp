#include "game.h"

void Game::Exit()
{
    s_isSignalExit = true;
}

bool GracefulLoopExitMidAsmHook()
{
    // TODO (Sajid): investigate XAM handle closing causing assertion failure here.
    return Game::s_isSignalExit;
}
