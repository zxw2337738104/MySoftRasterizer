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
    uint MatPad0;
    uint MatPad1;
};

// 所有漫反射贴图
//Texture2D gTextureMap[48] : register(t4);
//TextureCube gCubeMap : register(t0);
//Texture2D gShadowMap : register(t2);
//Texture2D gSsaoMap : register(t3);

StructuredBuffer<MaterialData> gMaterialData : register(t0, space1);

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

cbuffer cbPerObject : register(b1)
{
    float4x4 gWorld;
    float4x4 gTexTransform; // UV顶点变换矩阵
    uint gMaterialIndex;
    uint gObjPad0;
    uint gObjPad1;
    uint gObjPad2;
};

//cbuffer cbSkinned : register(b2)
//{
//    float4x4 gBoneTransforms[96];
//}

//float3 normalsampletoworldspace(float3 normalmapsample, float3 uintnormalw, float3 tangentw)
//{
//    float3 normalt = 2.0f * normalmapsample - 1.0f;
    
//    float3 n = uintnormalw;
//    float3 t = normalize(tangentw - dot(tangentw, n) * n);
//    float3 b = cross(n, t);
    
//    float3x3 tbn = float3x3(t, b, n);
    
//    float3 bumpednormalw = mul(normalt, tbn);
    
//    return bumpednormalw;
//}

//float calcshadowfactor(float4 shadowposh)
//{
//    shadowposh.xyz /= shadowposh.w;
    
//    float depth = shadowposh.z;
    
//    uint width, height, nummips;
//    gshadowmap.getdimensions(0, width, height, nummips);
    
//    float dx = 1.0f / (float) width;
    
//    float percentlit = 0.0f;
//    const float2 offsets[25] =
//    {
//        float2(-2 * dx, -2 * dx), float2(-dx, -2 * dx), float2(0.0f, -2 * dx), float2(dx, -2 * dx), float2(2 * dx, -2 * dx),
//        float2(-2 * dx, -dx), float2(-dx, -dx), float2(0.0f, -dx), float2(dx, -dx), float2(2 * dx, -dx),
//        float2(-2 * dx, 0.0f), float2(-dx, 0.0f), float2(0.0f, 0.0f), float2(dx, 0.0f), float2(2 * dx, 0.0f),
//        float2(-2 * dx, dx), float2(-dx, dx), float2(0.0f, dx), float2(dx, dx), float2(2 * dx, dx),
//        float2(-2 * dx, 2 * dx), float2(-dx, 2 * dx), float2(0.0f, 2 * dx), float2(dx, 2 * dx), float2(2 * dx, 2 * dx)
//    };
    
//    [unroll]
//    for (int i = 0; i < 25; ++i)
//    {
//        percentlit += gshadowmap.samplecmplevelzero(gsamshadow, shadowposh.xy + offsets[i], depth).r;
//    }
//    return percentlit / 25.0f;
//}