#pragma once

namespace SWA
{
    class CGameDocument // : public Hedgehog::Base::CSynchronizedObject
    {
    public:
        class CMember
        {
        public:
            SWA_INSERT_PADDING(0x20C);
            be<uint32_t> m_Score;
        };

        // TODO: use g_memory.Translate?
        // TODO: Hedgehog::Base::TSynchronizedPtr<CGameDocument>*
        inline static xpointer<CGameDocument>* ms_pInstance = (xpointer<CGameDocument>*)0x183367900;

        // TODO: Hedgehog::Base::TSynchronizedPtr<CGameDocument>
        static CGameDocument* GetInstance();

        SWA_INSERT_PADDING(0x08);
        xpointer<CMember> m_pMember;
    };
}

#include "GameDocument.inl"
