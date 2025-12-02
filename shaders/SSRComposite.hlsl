#include "Common.hlsl"

Texture2D gAlbedoMap : register(t27);
Texture2D gNormalMap : register(t28);
Texture2D gPositionMap : register(t29);
Texture2D gDepthMap : register(t30);
Texture2D gSceneColor : register(t31);
Texture2D gSSRMap : register(t55);

static const float2 gTexCoords[6] =
{
    float2(0.0f, 1.0f),
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),
    float2(1.0f, 1.0f)
};

struct VertexOut
{
    float4 PosH : SV_Position;
    float2 TexC : TEXCOORD0;
};

VertexOut VS(uint vid : SV_VertexID)
{
    VertexOut vout;
    vout.TexC = gTexCoords[vid];
    vout.PosH = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 0.0f, 1.0f);
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float3 baseColor = gSceneColor.Sample(gsamLinearClamp, pin.TexC).rgb;
    if (gAlbedoMap.Sample(gsamPointClamp, pin.TexC).a == 1.0f)
        return float4(baseColor, 1.0f);
    float4 ssrSample = gSSRMap.Sample(gsamLinearClamp, pin.TexC);
    
    float3 finalColor = lerp(baseColor, ssrSample.rgb, ssrSample.a);
    //float3 finalColor = baseColor + ssrSample.rgb;

    return float4(finalColor, 1.0f);
}
