#include "Common.hlsl"

struct VertexIn
{
    float3 PosL : POSITION;
    float2 TexC : TEXCOORD;
};

struct VertexOut
{
    float4 PosH : SV_Position;
    float2 TexC : TEXCOORD;
    uint InstanceID : SV_InstanceID;
};

VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID)
{
    VertexOut vout = (VertexOut) 0.0f;
    vout.PosH = float4(vin.PosL.x, vin.PosL.y - 1.5f * instanceID, vin.PosL.z, 1.0f);
    vout.TexC = vin.TexC;
    vout.InstanceID = instanceID;
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    if(pin.InstanceID == 0)
        return float4(gSsaoMap.Sample(gsamLinearWrap, pin.TexC).rrr, 1.0f);
    return float4(gShadowMap.Sample(gsamLinearWrap, pin.TexC).rrr, 1.0f);
}