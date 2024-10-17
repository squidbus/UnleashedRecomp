#pragma once

namespace SWA
{
    class CApplicationDocument // : public Hedgehog::Base::CSynchronizedObject
    {
    public:
        // TODO: use g_memory.Translate?
        // TODO: Hedgehog::Base::TSynchronizedPtr<CApplicationDocument>*
        inline static xpointer<CApplicationDocument>* ms_pInstance = (xpointer<CApplicationDocument>*)0x1833678A0;

        // TODO: Hedgehog::Base::TSynchronizedPtr<CApplicationDocument>
        static CApplicationDocument* GetInstance();

        SWA_INSERT_PADDING(0x18);
        be<uint32_t> m_Region;
    };
}

#include "ApplicationDocument.inl"
