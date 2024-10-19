#pragma once

#include "SWA.inl"
#include "PadState.h"

namespace SWA
{
    class CInputState // : public Hedgehog::Base::CSynchronizedObject
    {
    public:
        // TODO: Hedgehog::Base::TSynchronizedPtr<CInputState>
        static CInputState* GetInstance();

        SPadState m_PadStates[40];
        SWA_INSERT_PADDING(0x50);
        be<uint32_t> m_CurrentPadStateIndex;
        SWA_INSERT_PADDING(0x04);

        const SPadState& GetPadState() const;
    };
}

#include "InputState.inl"
