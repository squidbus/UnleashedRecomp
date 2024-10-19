namespace SWA
{
    // TODO: Hedgehog::Base::TSynchronizedPtr<CApplicationDocument>
    inline CApplicationDocument* CApplicationDocument::GetInstance()
    {
        return *(xpointer<CApplicationDocument>*)g_memory.Translate(0x833678A0);
    }
}
