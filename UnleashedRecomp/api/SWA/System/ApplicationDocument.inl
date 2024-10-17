namespace SWA
{
    // TODO: Hedgehog::Base::TSynchronizedPtr<CApplicationDocument>
    inline CApplicationDocument* CApplicationDocument::GetInstance()
    {
        return *ms_pInstance;
    }
}