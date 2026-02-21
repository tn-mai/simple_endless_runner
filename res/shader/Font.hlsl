struct PSInput
{
  float4 position : SV_POSITION;
  float4 color : COLOR0;
  float4 subColor : COLOR1;
  float2 texcoord : TEXCOORD;
  uint texID : TEXID;
  float thickness : THICKNESS;
  float outline : OUTLINE;
};

cbuffer ConstantData : register(b0)
{
  float4x4 matVP;
};

struct FontInfo
{
  uint page;
  float2 uv[2];
  float2 size;
  float2 offset;
};
StructuredBuffer<FontInfo> fontInfos : register(t0);

struct Character
{
  float2 position;
  float2 scale;
  float4 color;
  float4 subColor;
  float thickness;
  float outline;
  uint fontIndex;
};
StructuredBuffer<Character> characters : register(t1);

Texture2D tex[2] : register(t0);
SamplerState sampler0 : register(s0);

[RootSignature("RootFlags( ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT |" \
"   DENY_DOMAIN_SHADER_ROOT_ACCESS |" \
"   DENY_GEOMETRY_SHADER_ROOT_ACCESS |" \
"   DENY_HULL_SHADER_ROOT_ACCESS )," \
"RootConstants(num32BitConstants=16, b0)," \
"DescriptorTable(SRV(t0, numDescriptors = 1), visibility = SHADER_VISIBILITY_VERTEX)," \
"DescriptorTable(SRV(t1, numDescriptors = 1), visibility = SHADER_VISIBILITY_VERTEX)," \
"DescriptorTable(SRV(t0, numDescriptors = 2), visibility = SHADER_VISIBILITY_PIXEL)," \
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

  Character chr = characters[instID];
  FontInfo font= fontInfos[chr.fontIndex];
  float2 size = font.size * chr.scale;
  float2 offset = font.offset * chr.scale;

  PSInput result;
  float x = float(vertID & 1);
  float y = float((vertID >> 1) & 1);
  float px = x * size.x + chr.position.x + offset.x;
  float py = y * size.y + chr.position.y + offset.y;
  result.position = mul(float4(px, py, 100.0f, 1.0f), matVP);
  result.color = chr.color;
  result.subColor = chr.subColor;
  result.texcoord = float2(font.uv[int(x)].x, font.uv[1 - int(y)].y);
  result.texID = font.page;
  result.thickness = chr.thickness;
  result.outline = chr.outline;
  return result;
}

// ピクセルシェーダ
float4 PSMain(PSInput input) : SV_TARGET
{
  const float smoothing = 1.0f / 16.0f;
  float distance = tex[NonUniformResourceIndex(input.texID)].Sample(sampler0, input.texcoord).r;
  //return float4(1, 1, 1, distance);
  float outline = smoothstep(input.thickness - smoothing, input.thickness + smoothing, distance);
  float4 color = lerp(input.subColor, input.color, outline);
  color.a *= smoothstep(input.outline - smoothing, input.outline + smoothing, distance);
  color.a = clamp(color.a, 0.0f, 1.0f);
  return color;
}
