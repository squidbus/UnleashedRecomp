#include "movie_common.hlsl"

Interpolators main(in VertexShaderInput In)
{
#ifdef __spirv__
	VertexShaderConstants constants = vk::RawBufferLoad<VertexShaderConstants>(g_PushConstants.VertexShaderConstants, 0x100);
#endif
    
    Interpolators Out;
    Out.ProjPos = In.ObjPos;
    Out.UV = In.UV;
    return Out;
}
