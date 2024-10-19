namespace SWA
{
    // TODO: Hedgehog::Base::TSynchronizedPtr<CGameDocument>
    inline CGameDocument* CGameDocument::GetInstance()
    {
        return *(xpointer<CGameDocument>*)g_memory.Translate(0x83367900);
    }
}
