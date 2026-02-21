struct VSInput
{
    float3 position : POSITION;
    float2 texcoord : TEXCOORD;
    float4 color : COLOR;
};

struct PSInput
{
    float4 position : SV_POSITION;
    float2 texcoord : TEXCOORD;
    float4 color : COLOR;
};

cbuffer Constats
{
    float4x4 matViewProjection;
};

[RootSignature("RootFlags(ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
"   DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
"   DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
"   DENY_HULL_SHADER_ROOT_ACCESS)," \
"RootConstants( num32BitConstants=16, b0)," \
"DescriptorTable(" \
"   SRV(t0, numDescriptors = 1)," \
"   visibility = SHADER_VISIBILITY_PIXEL)," \
"StaticSampler(s0," \
"   filter = FILTER_MIN_MAG_MIP_LINEAR," \
"   addressU = TEXTURE_ADDRESS_CLAMP," \
"   addressV = TEXTURE_ADDRESS_CLAMP," \
"   addressW = TEXTURE_ADDRESS_CLAMP," \
"   visibility = SHADER_VISIBILITY_PIXEL)" )]

PSInput VSMain(VSInput input)
{
    PSInput output;
    output.position = float4(input.position, 1.0f);
    output.texcoord = input.texcoord;
    output.color = input.color;
	return output;
}
