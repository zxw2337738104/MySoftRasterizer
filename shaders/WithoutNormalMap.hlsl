#include "Common.hlsl"

struct VertexIn
{
    float3 PosL : POSITION;
    float3 NormalL : NORMAL;
    float2 TexC : TEXCOORD;
    float3 TangentL : TANGENT;
};

struct VertexOut
{
    float4 PosH : SV_Position;
    float4 ShadowPosH : POSITION0;
    float4 SsaoPosH : POSITION1;
    float3 PosW : POSITION2;
    float3 NormalW : NORMAL;
    float3 TangentW : TANGENT;
    float2 TexC : TEXCOORD;
    
    nointerpolation uint MatIndex : MATINDEX;
};

VertexOut VS(VertexIn vin, uint instanceID : SV_InstanceID)
{
    VertexOut vout = (VertexOut) 0.0f;
    
    InstanceData instData = gInstanceData[instanceID];
    float4x4 gWorld = instData.World;
    float4x4 gTexTransform = instData.TexTransform;
    uint gMaterialIndex = instData.MaterialIndex;
    
    MaterialData matData = gMaterialData[gMaterialIndex];
    
    vout.MatIndex = gMaterialIndex;
    
    float4 posW = mul(float4(vin.PosL, 1.0f), gWorld);
    vout.PosW = posW.xyz;
    
    vout.NormalW = mul(vin.NormalL, (float3x3) gWorld);
    
    vout.TangentW = mul(vin.TangentL, (float3x3) gWorld);
    
    vout.PosH = mul(posW, gViewProj);
    
    vout.SsaoPosH = mul(posW, gViewProjTex);
    
    float4 texC = mul(float4(vin.TexC, 0.0f, 1.0f), gTexTransform);
    vout.TexC = mul(texC, matData.MatTransform).xy;
    
    vout.ShadowPosH = mul(posW, gShadowTransform);
    
    return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
    MaterialData matData = gMaterialData[pin.MatIndex];
    
    float4 diffuseAlbedo = matData.DiffuseAlbedo;
    float gRoughness = matData.Roughness;
    float3 gFresnelR0 = matData.FresnelR0;
    uint diffuseTexIndex = matData.DiffuseMapIndex;
    uint normalTexIndex = matData.NormalMapIndex;
    
    pin.NormalW = normalize(pin.NormalW);
    
    float4 normalMapSample = gTextureMap[normalTexIndex].Sample(gsamAnisotropicWrap, pin.TexC);
    float3 bumpedNormalW = NormalSampleToWorldSpace(normalMapSample.rgb, pin.NormalW, pin.TangentW);
    
    diffuseAlbedo *= gTextureMap[diffuseTexIndex].Sample(gsamAnisotropicWrap, pin.TexC);
    
    pin.NormalW = normalize(pin.NormalW);
    
    float3 toEyeW = normalize(gEyePosW - pin.PosW);
    
    pin.SsaoPosH /= pin.SsaoPosH.w;
    //float ambientAccess = gSsaoMap.Sample(gsamLinearClamp, pin.SsaoPosH.xy, 0.0f).r;
	
    //float4 ambient = ambientAccess * gAmbientLight * diffuseAlbedo;
    float4 ambient = gAmbientLight * diffuseAlbedo;

    //防止 normalMapSample.a 为 0 时，导致除以 0 的情况
    const float shininess = (1.0f - gRoughness) * (normalMapSample.a != 0.0f ? normalMapSample.a : 0.0001f);
    //const float shininess = (1.0f - gRoughness) * 256.0f; // Assuming gRoughness is a float3, using x component for shininess.
    Material mat = { diffuseAlbedo, gFresnelR0, shininess };
    float3 shadowFactor = 1.0f;
    //shadowFactor[0] = CalcShadowFactor(pin.ShadowPosH);
    //float4 directLight = ComputeLighting(gLights, mat, pin.PosW, bumpedNormalW, toEyeW, shadowFactor);
    float4 directLight = ComputeLighting(gLights, mat, pin.PosW, pin.NormalW, toEyeW, shadowFactor);
    
    float4 litColor = ambient + directLight;
    
    //float3 r = reflect(-toEyeW, bumpedNormalW);
    //float4 reflectionColor = gCubeMap.Sample(gsamAnisotropicWrap, r);
    //float3 fresnelFactor = SchlickFresnel(gFresnelR0, bumpedNormalW, r);
    
    //litColor.rgb += shininess * fresnelFactor * reflectionColor.rgb;
    
    //litColor.a = diffuseAlbedo.a;
	
    return litColor;
}