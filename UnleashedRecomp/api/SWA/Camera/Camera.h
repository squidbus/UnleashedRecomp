#pragma once

#include "SWA.inl"

namespace SWA
{
    class CCamera // : public CGameObject, public Hedgehog::Universe::TStateMachine<CCamera>
    {
    public:
        SWA_INSERT_PADDING(0x184);
        be<float> m_VertAspectRatio;
        SWA_INSERT_PADDING(0x48);
        be<float> m_HorzAspectRatio;
        SWA_INSERT_PADDING(0x178);
        be<float> m_FieldOfView;
        be<float> m_VertFieldOfView;
        be<float> m_HorzFieldOfView;
    };
}
