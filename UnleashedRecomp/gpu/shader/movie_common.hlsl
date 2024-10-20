#include "../../../thirdparty/ShaderRecomp/ShaderRecomp/shader_common.hlsli"

CONSTANT_BUFFER(VertexShaderConstants, b0)
{
    [[vk::offset(2880)]] float4 g_ViewportSize PACK_OFFSET(c180);
};

CONSTANT_BUFFER(PixelShaderConstants, b1)
{
    [[vk::offset(0)]] float fZmin PACK_OFFSET(c0);
    [[vk::offset(4)]] float fZmax PACK_OFFSET(c1);
};

CONSTANT_BUFFER(SharedConstants, b2)
{
    [[vk::offset(0)]] uint Tex0_ResourceDescriptorIndex PACK_OFFSET(c0.x);
    [[vk::offset(4)]] uint Tex1_ResourceDescriptorIndex PACK_OFFSET(c0.y);
    [[vk::offset(8)]] uint Tex2_ResourceDescriptorIndex PACK_OFFSET(c0.z);
    [[vk::offset(12)]] uint Tex3_ResourceDescriptorIndex PACK_OFFSET(c0.w);
    [[vk::offset(16)]] uint Tex4_ResourceDescriptorIndex PACK_OFFSET(c1.x);
    
    [[vk::offset(64)]] uint Tex0_SamplerDescriptorIndex PACK_OFFSET(c4.x);
    [[vk::offset(68)]] uint Tex1_SamplerDescriptorIndex PACK_OFFSET(c4.y);
    [[vk::offset(72)]] uint Tex2_SamplerDescriptorIndex PACK_OFFSET(c4.z);
    [[vk::offset(76)]] uint Tex3_SamplerDescriptorIndex PACK_OFFSET(c4.w);
    [[vk::offset(80)]] uint Tex4_SamplerDescriptorIndex PACK_OFFSET(c5.x);
    
	SHARED_CONSTANTS;
};

#define bCsc (GET_SHARED_CONSTANT(g_Booleans) & (1 << (16 + 0)))
#define bAmv (GET_SHARED_CONSTANT(g_Booleans) & (1 << (16 + 1)))
#define bZmv (GET_SHARED_CONSTANT(g_Booleans) & (1 << (16 + 2)))
 
struct VertexShaderInput
{
    [[vk::location(0)]] float4 ObjPos : POSITION;
    [[vk::location(4)]] float2 UV : TEXCOORD;
};

struct Interpolators
{
    float4 ProjPos : SV_Position;
    float2 UV : TEXCOORD0;
};

struct PixelShaderOutput
{
    float4 Color : SV_Target0;
    float Depth : SV_Depth;
};
