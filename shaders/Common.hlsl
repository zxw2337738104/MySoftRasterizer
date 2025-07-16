#ifndef NUM_DIR_LIGHTS
	#define NUM_DIR_LIGHTS 3
#endif

#ifndef NUM_POINT_LIGHTS
	#define NUM_POINT_LIGHTS 0
#endif

#ifndef NUM_SPOT_LIGHTS
	#define NUM_SPOT_LIGHTS 0
#endif

#include "LightingUtil.hlsl"

struct MaterialData
{
    float4 DiffuseAlbedo;
    float3 FresnelR0;
    float Roughness;
    float4x4 MatTransform;
    uint DiffuseMapIndex;
    uint NormalMapIndex;
    uint CubeMapIndex;
    uint MatPad1;
};

struct InstanceData
{
    float4x4 World;
    float4x4 TexTransform;
    uint MaterialIndex;
    uint InstPad0;
    uint InstPad1;
    uint InstPad2;
};

// 所有漫反射贴图
Texture2D gTextureMap[8] : register(t1);
TextureCube gCubeMap[2] : register(t9);
Texture2D gShadowMap : register(t11);
//Texture2D gSsaoMap : register(t3);

StructuredBuffer<MaterialData> gMaterialData : register(t0, space1);
StructuredBuffer<InstanceData> gInstanceData : register(t1, space1);

// 6个不同类型的采样器
SamplerState gsamPointWrap : register(s0);
SamplerState gsamPointClamp : register(s1);
SamplerState gsamLinearWrap : register(s2);
SamplerState gsamLinearClamp : register(s3);
SamplerState gsamAnisotropicWrap : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);
SamplerComparisonState gsamShadow : register(s6);

cbuffer cbPass : register(b0)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    float4x4 gViewProjTex;
    float3 gEyePosw;
    float gPassCBPad0;
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    float gNearZ;
    float gFarZ;
    float gTotalTime;
    float gDeltaTime;
    float4x4 gShadowTransform;
    float4 gAmbientLight;
	
    Light gLights[MaxLights];
};

//cbuffer cbPerObject : register(b1)
//{
//    float4x4 gWorld;
//    float4x4 gTexTransform; // UV顶点变换矩阵
//    uint gMaterialIndex;
//    uint gObjPad0;
//    uint gObjPad1;
//    uint gObjPad2;
//};

//cbuffer cbSkinned : register(b2)
//{
//    float4x4 gBoneTransforms[96];
//}

float3 NormalSampleToWorldSpace(float3 normalMapSample, float3 uintNormalW, float3 tangentW)
{
    float3 normalT = 2.0f * normalMapSample - 1.0f;
    
    float3 N = uintNormalW;
    float3 T = normalize(tangentW - dot(tangentW, N) * N);
    float3 B = cross(N, T);
    
    float3x3 TBN = float3x3(T, B, N);
    
    float3 bumpedNormalW = mul(normalT, TBN);
    
    return bumpedNormalW;
}

float CalcShadowFactor(float4 shadowPosH)
{
    shadowPosH.xyz /= shadowPosH.w;
    
    float depth = shadowPosH.z;
    
    uint width, height, numMips;
    gShadowMap.GetDimensions(0, width, height, numMips);
    
    float dx = 1.0f / (float) width;
    
    float percentLit = 0.0f;
    const float2 offsets[25] =
    {
        float2(-2 * dx, -2 * dx), float2(-dx, -2 * dx), float2(0.0f, -2 * dx), float2(dx, -2 * dx), float2(2 * dx, -2 * dx),
        float2(-2 * dx, -dx), float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx), float2(2 * dx, -dx),
        float2(-2 * dx, 0.0f), float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f), float2(2 * dx, 0.0f),
        float2(-2 * dx, dx), float2(-dx, dx), float2(0.0f, dx), float2(dx, dx), float2(2 * dx, dx),
        float2(-2 * dx, 2 * dx), float2(-dx, 2 * dx), float2(0.0f, 2 * dx), float2(dx, 2 * dx), float2(2 * dx, 2 * dx)
    };
    
    [unroll]
    for (int i = 0; i < 25; ++i)
    {
        percentLit += gShadowMap.SampleCmpLevelZero(gsamShadow, shadowPosH.xy + offsets[i], depth).r;
    }
    return percentLit / 25.0f;
}