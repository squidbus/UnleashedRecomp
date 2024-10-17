#include <cpu/guest_code.h>
#include "api/SWA.h"
#include "ui/window.h"
#include "game.h"

constexpr float m_baseAspectRatio = 16.0f / 9.0f;

void Game::Exit()
{
    s_isSignalExit = true;
}

bool GracefulLoopExitMidAsmHook()
{
    // TODO (Sajid): investigate XAM handle closing causing assertion failure here.
    return Game::s_isSignalExit;
}

bool CameraAspectRatioMidAsmHook(PPCRegister& r31)
{
    auto pCamera = (SWA::CCamera*)g_memory.Translate(r31.u32);
    auto newAspectRatio = (float)Window::s_width / (float)Window::s_height;

    // Dynamically adjust aspect ratio to window dimensions.
    pCamera->m_HorzAspectRatio = newAspectRatio;

    // Jump to 4:3 code for VERT+ adjustments if using a narrow aspect ratio.
    return newAspectRatio < m_baseAspectRatio;
}

void CameraBoostAspectRatioMidAsmHook(PPCRegister& r31, PPCRegister& f0)
{
    auto pCamera = (SWA::CCamera*)g_memory.Translate(r31.u32);

    if (Window::s_width < Window::s_height)
    {
        // Use horizontal FOV for narrow aspect ratios.
        f0.f32 = pCamera->m_HorzFieldOfView;
    }
    else
    {
        // Use vertical FOV for normal aspect ratios.
        f0.f32 = pCamera->m_VertFieldOfView;
    }
}
