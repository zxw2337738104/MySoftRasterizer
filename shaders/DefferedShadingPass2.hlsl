#include "Common.hlsl"

Texture2D gAlbedoMap : register(t27);
Texture2D gNormalMap : register(t28);
Texture2D gPositionMap : register(t29);

struct VertexOut
{
    float4 PosH : SV_Position;
    float2 TexC : TEXCOORD0;
    float2 quadID : TEXCOORD1;
};

static const float2 gTexCoords[6] =
{
    float2(0.0f, 1.0f),
    float2(0.0f, 0.0f),
    float2(1.0f, 0.0f),
    float2(0.0f, 1.0f),
    float2(1.0f, 0.0f),
    float2(1.0f, 1.0f)
};

VertexOut VS(uint vid : SV_VertexID)
{
    VertexOut vout = (VertexOut) 0.0f;
    
    int quadID = vid / 6;
    vid %= 6;
    
    vout.TexC = gTexCoords[vid];
    vout.PosH = float4(vout.TexC.x * 2.0f - 1.0f, 1.0f - vout.TexC.y * 2.0f, 0.0f, 1.0f);
    
    if(quadID >= 1)
    {
        vout.PosH.xy *= 0.25f;
        vout.PosH.x -= (-0.75f + 0.5f * quadID);
        vout.PosH.y -= 0.75f;
    }
    
    vout.quadID.x = quadID;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    float4 posAndR0 = gPositionMap.Sample(gsamPointClamp, pin.TexC);
    float4 normalAndShininess = gNormalMap.Sample(gsamPointClamp, pin.TexC);
    float3 albedo = gAlbedoMap.Sample(gsamPointClamp, pin.TexC).rgb;
    
    float3 posW = posAndR0.xyz;
    float3 fresnelR0 = posAndR0.www;
    float3 normalW = normalAndShininess.xyz;
    float shininess = normalAndShininess.w;
    
    if (pin.quadID.x >= 1)
    {
        if (pin.quadID.x == 1.0f)
            return float4(posW, 1.0f);
        else if (pin.quadID.x == 2.0f)
            return float4(normalW, 1.0f);
        else // if(pin.quadID.x == 3.0f)
            return float4(albedo, 1.0f);
    }
    
    if (fresnelR0.x == 0.0f)
        return float4(albedo, 1.0f);
    
    //float3 ambient = (float3) gAmbientLight * albedo;
    float3 ambient = float3(0.0f, 0.0f, 0.0f);
    
    Material Mat = { float4(albedo, 1.0f), fresnelR0, shininess };
    float3 shadowFactor = 1.0f;
    float4 shadowPosH = mul(float4(posW, 1.0f), gShadowTransform);
    shadowFactor[0] = CalcShadowFactor(shadowPosH);
    
    float4 directLight = ComputeLighting(gLights, Mat, posW, normalW, normalize(gEyePosW - posW), shadowFactor);
    float4 litColor = float4(ambient, 1.0f) + directLight;
    
    return litColor;
    //return float4(normalW, 1.0f);
}