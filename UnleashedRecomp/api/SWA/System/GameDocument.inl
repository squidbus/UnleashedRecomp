namespace SWA
{
    // TODO: Hedgehog::Base::TSynchronizedPtr<CGameDocument>
    inline CGameDocument* CGameDocument::GetInstance()
    {
        return *ms_pInstance;
    }
}