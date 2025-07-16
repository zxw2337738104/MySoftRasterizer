#include "Common.hlsl"

struct VertexIn
{
    float3 PosL : POSITION;
    float2 TexCoord : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_Position;
    float2 UV : TEXCOORD;
};

VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID)
{
    VertexOut vout;
    InstanceData instData = gInstanceData[instanceID];
    float4x4 gWorld = instData.World;
    float4x4 gTexTransform = instData.TexTransform;
    uint gMaterialIndex = instData.MaterialIndex;
    MaterialData matData = gMaterialData[gMaterialIndex];
    
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosH = mul(posW, gViewProj);
    
    float4 texC = mul(float4(vin.TexCoord, 0.0f, 1.0f), gTexTransform);
    vout.UV = mul(texC, matData.MatTransform).xy;
    
    return vout;
}

void PS(VertexOut pin)
{
    
}