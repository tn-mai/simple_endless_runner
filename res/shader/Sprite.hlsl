struct PSInput
{
  float4 position : SV_POSITION;
  float4 color : COLOR;
  float2 texcoord : TEXCOORD;
  uint texID : TEXID;
};

cbuffer ConstantData : register(b0)
{
  float4x4 matVP;
};

struct Sprite
{
  uint texID;
  float3 position;
  float rotation;
  float2 scale;
  float4 color;
};
StructuredBuffer<Sprite> sprites : register(t0);

Texture2D tex0[1023] : register(t0);
SamplerState sampler0 : register(s0);

[RootSignature("RootFlags( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
"   DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
"   DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
"   DENY_HULL_SHADER_ROOT_ACCESS )," \
"RootConstants(num32BitConstants=16, b0)," \
"DescriptorTable(SRV(t0, numDescriptors = 1), visibility = SHADER_VISIBILITY_VERTEX)," \
"DescriptorTable(SRV(t0, numDescriptors = 1023), visibility = SHADER_VISIBILITY_PIXEL)," \
"StaticSampler(s0," \
"   filter = FILTER_MIN_MAG_MIP_LINEAR," \
"   addressU = TEXTURE_ADDRESS_CLAMP," \
"   addressV = TEXTURE_ADDRESS_CLAMP," \
"   addressW = TEXTURE_ADDRESS_CLAMP," \
"   visibility = SHADER_VISIBILITY_PIXEL)")]

// 頂点シェーダ
PSInput VSMain(uint vertID : SV_VertexID, uint instID : SV_InstanceID)
{
  // 2-3
  // |\|
  // 0-1
  PSInput result;
  Sprite sprite = sprites[instID];
  float x = float(vertID & 1);
  float y = float((vertID >> 1) & 1);
  float2 v = float2(x - 0.5f, y - 0.5f) * sprite.scale;
  float s = sin(sprite.rotation);
  float c = cos(sprite.rotation);
  float2 p = float2(c * v.x + -s * v.y, s * v.x + c * v.y);
  result.position = mul(float4(p + sprite.position.xy, sprite.position.z, 1.0f), matVP);
  result.color = float4(sprite.color);
  result.texcoord = float2(x, 1.0f - y);
  result.texID = sprite.texID;
  return result;
}

// ピクセルシェーダ
float4 PSMain(PSInput input) : SV_TARGET
{
  return tex0[NonUniformResourceIndex(input.texID)].Sample(sampler0, input.texcoord) * input.color;
}
