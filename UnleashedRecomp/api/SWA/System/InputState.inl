namespace SWA
{
    // TODO: Hedgehog::Base::TSynchronizedPtr<CInputState>
    inline CInputState* CInputState::GetInstance()
    {
        return *(xpointer<CInputState>*)g_memory.Translate(0x833671EC);
    }

    inline const SPadState& CInputState::GetPadState() const
    {
        return m_PadStates[m_CurrentPadStateIndex];
    }
}
