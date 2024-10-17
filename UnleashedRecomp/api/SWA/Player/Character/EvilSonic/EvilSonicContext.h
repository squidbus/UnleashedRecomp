#pragma once

#include "SWA.inl"

namespace SWA::Player
{
    class CEvilSonicContext // : public CPlayerContext
    {
    public:
        SWA_INSERT_PADDING(0x688);
        be<float> m_DarkGaiaEnergy;
        SWA_INSERT_PADDING(0x138);
        be<uint32_t> m_AnimationID;
    };
}
