#include <cpu/guest_code.h>
#include "api/SWA.h"
#include "ui/window.h"
#include "game.h"

constexpr float m_baseAspectRatio = 16.0f / 9.0f;

uint32_t m_lastCheckpointScore = 0;

void Game::Exit()
{
    s_isSignalExit = true;
}

bool GracefulLoopExitMidAsmHook()
{
    // TODO (Sajid): investigate XAM handle closing causing assertion failure here.
    return Game::s_isSignalExit;
}

#pragma region Aspect Ratio Hooks

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

void CSDAspectRatioMidAsmHook(PPCRegister& f1, PPCRegister& f2)
{
    auto newAspectRatio = (float)Window::s_width / (float)Window::s_height;

    if (newAspectRatio > m_baseAspectRatio)
    {
        f1.f64 = 1280.0f / ((newAspectRatio * 720.0f) / 1280.0f);
    }
    else if (newAspectRatio < m_baseAspectRatio)
    {
        f2.f64 = 720.0f / ((1280.0f / newAspectRatio) / 720.0f);
    }
}

void CSDOffsetMidAsmHook(PPCRegister& f1, PPCRegister& f2)
{
    auto newAspectRatio = (float)Window::s_width / (float)Window::s_height;

    if (newAspectRatio > m_baseAspectRatio)
    {
        *(be<float>*)g_memory.Translate(0x8339C5D0) = ((newAspectRatio * 720.0f) - 1280.0f) / 2.0f / 1280.0f;
    }
    else
    {
        *(be<float>*)g_memory.Translate(0x8339C5D4) = ((1280.0f / newAspectRatio) - 720.0f) / 2.0f / 720.0f;
    }
}

#pragma endregion

#pragma region Score Hooks

/* Hook function for when checkpoints are activated
   to preserve the current checkpoint score. */
PPC_FUNC_IMPL(__imp__sub_82624308);
PPC_FUNC(sub_82624308)
{
    __imp__sub_82624308(ctx, base);

    if (Config::ScoreBehaviour != EScoreBehaviour_CheckpointRetain)
        return;

    auto pGameDocument = SWA::CGameDocument::GetInstance();

    if (!pGameDocument)
        return;

    m_lastCheckpointScore = pGameDocument->m_pMember->m_Score;
}

/* Hook function that resets the score
   and restore the last checkpoint score. */
PPC_FUNC_IMPL(__imp__sub_8245F048);
PPC_FUNC(sub_8245F048)
{
    __imp__sub_8245F048(ctx, base);

    if (Config::ScoreBehaviour != EScoreBehaviour_CheckpointRetain)
        return;

    auto pGameDocument = SWA::CGameDocument::GetInstance();

    if (!pGameDocument)
        return;

    printf("Resetting score to %d\n", m_lastCheckpointScore);

    pGameDocument->m_pMember->m_Score = m_lastCheckpointScore;
}

void ResetScoreOnRestartMidAsmHook()
{
    m_lastCheckpointScore = 0;
}

#pragma endregion

#pragma region Loading Hooks

// Loading screen hook.
PPC_FUNC_IMPL(__imp__sub_824DCF38);
PPC_FUNC(sub_824DCF38)
{
    /* Force the Werehog transition ID
       to use a different transition. */
    if (!Config::WerehogHubTransformVideo)
    {
        /*
            0 - Tails Electric NOW LOADING
            1 - No Transition
            2 - Werehog Transition
            3 - Tails Electric NOW LOADING w/ Info (requires context)
            4 - Arrows In/Out
            5 - NOW LOADING
            6 - Theatre Mode
            7 - NOW LOADING
            8 - Black Screen
        */
        if (ctx.r4.u32 == 2)
            ctx.r4.u32 = 4;
    }

    __imp__sub_824DCF38(ctx, base);
}

#pragma endregion

#pragma region Miscellaneous Hooks

bool DisableHintsMidAsmHook()
{
    return !Config::Hints;
}

/* Hook function that gets the game region
   and force result to zero for Japanese
   to display the correct logos. */
PPC_FUNC_IMPL(__imp__sub_825197C0);
PPC_FUNC(sub_825197C0)
{
    if (Config::Language == ELanguage_Japanese)
    {
        ctx.r3.u64 = 0;
        return;
    }

    __imp__sub_825197C0(ctx, base);
}

#pragma endregion
