#pragma once

namespace SWA
{
    class CApplicationDocument // : public Hedgehog::Base::CSynchronizedObject
    {
    public:
        class CMember
        {
        public:
            SWA_INSERT_PADDING(0x138);
            void* m_spGameParameter;
        };

        // TODO: Hedgehog::Base::TSynchronizedPtr<CApplicationDocument>
        static CApplicationDocument* GetInstance();

        SWA_INSERT_PADDING(0x04);
        xpointer<CMember> m_pMember;
        SWA_INSERT_PADDING(0x14);
        be<uint32_t> m_Region;
    };
}

#include "ApplicationDocument.inl"
