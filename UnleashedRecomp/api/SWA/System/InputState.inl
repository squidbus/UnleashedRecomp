namespace SWA
{
    // TODO: Hedgehog::Base::TSynchronizedPtr<CInputState>
    inline CInputState* CInputState::GetInstance()
    {
        return *ms_pInstance;
    }

    inline const SPadState& CInputState::GetPadState() const
    {
        return m_PadStates[m_CurrentPadStateIndex];
    }
}