#include "Common.hlsl"

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_Position;
    float3 PosL : POSITION;
    
    nointerpolation uint MatIndex : MATINDEX;
};

VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID)
{
    VertexOut vout = (VertexOut) 0.0f;
    
    InstanceData instData = gInstanceData[instanceID];
    float4x4 gWorld = instData.World;
    vout.MatIndex = instData.MaterialIndex;
    
    vout.PosL = vin.PosL;
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    posW.xyz += gEyePosW;
    vout.PosH = mul(posW, gViewProj).xyww;
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    MaterialData matData = gMaterialData[pin.MatIndex];
    uint cubeMapIndex = matData.CubeMapIndex;
    return gCubeMap[cubeMapIndex].Sample(gsamLinearWrap, pin.PosL);
}