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
    float Metallic;
};

struct InstanceData
{
    float4x4 World;
    float4x4 InvTpsWorld;
    float4x4 TexTransform;
    uint MaterialIndex;
    uint AOType;
    uint InstPad1;
    uint InstPad2;
};

// 所有漫反射贴图
Texture2D gTextureMap[13] : register(t1);
TextureCube gCubeMap[2] : register(t14);
Texture2D gShadowMap : register(t16);
Texture2D gBRDFLUT : register(t17);
Texture2D gBRDFLUT_Eu : register(t18);
Texture2D gLUT_Eavg : register(t19); // Eavg LUT
Texture2D gSsaoMap : register(t20);

StructuredBuffer<MaterialData> gMaterialData : register(t0, space1);
StructuredBuffer<InstanceData> gInstanceData : register(t1, space1);

// 7个不同类型的采样器
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
    float3 gEyePosW;
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

static float PI = 3.1415926;

static const uint SAMPLE_COUNT = 512u;

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

float3 SchlickFresnelApproximation(float3 F0, float cosTheta)
{
    return F0 + (1.0f - F0) * pow(1.0f - cosTheta, 5.0f);
}

float SchlickGGXApproximation(float cosTheta, float k)
{
    float denom = cosTheta * (1.0f - k) + k;
    return cosTheta / denom;
}

float G_Smith(float NdotL, float NdotV, float roughness)
{
    float a = roughness + 1.0f;
    float k = (a * a) / 8.0f;
    
    float GGXL = SchlickGGXApproximation(NdotL, k);
    float GGXV = SchlickGGXApproximation(NdotV, k);
    
    return GGXL * GGXV;
}

float G_Smith_IBL(float NdotL, float NdotV, float roughness)
{
    float k = (roughness * roughness) / 2.0f;
    return SchlickGGXApproximation(NdotL, k) * SchlickGGXApproximation(NdotV, k);
}

float NDFGGXApproximation(float NdotH, float roughness)
{
    roughness = lerp(0.04f, 1.0f, roughness);
    float a = roughness * roughness;
    float a2 = a * a;
    float denom = (NdotH * NdotH) * (a2 - 1.0f) + 1.0f;
    return a2 / (PI * denom * denom);
}

//计算漫反射辐照度图
float3 IBLDiffuseIrradiance(float3 normal)
{
    float3 up = { 0.0f, 1.0f, 0.0f };
    float3 right = cross(up, normal);
    up = cross(normal, right);
    
    float3 irradiance = 0.0f;
    float sampleDelta = 0.35f;
    float nrSamples = 0.0f;
    for (float phi = 0.0f; phi < 2.0f * PI; phi += sampleDelta)
    {
        for (float theta = 0.0f; theta < 0.5f * PI; theta += sampleDelta)
        {
            float3 tangentSample = { sin(theta) * cos(phi), sin(theta) * sin(phi), cos(theta) };
            float3 sampleVec = normalize(tangentSample.x * right + tangentSample.y * up + tangentSample.z * normal);
            irradiance += gCubeMap[0].Sample(gsamAnisotropicWrap, sampleVec).rgb * cos(theta) * sin(theta);
            nrSamples += 1.0f;
        }
    }
    irradiance = PI * irradiance / nrSamples;
    
    return irradiance;
}

//------------------------------------------------------
//  计算镜面反射部分的BRDF项
//  公式：F0 * |BRDF(1 - k) * NdotL + |BRDF * k * NdotL
//  注 : |代表积分符号
//  其中 k = a * a * a * a * a,  a = (1 - NdotV)
//  上述公式简化为 F0 * A + B
//  A = |BRDF(1 - k) * NdotL,  B = |BRDF * k * NdotL
//  需要计算的就是A和B
//-------------------------------------------------------

//  获取低差异序列
float RadicalInverse_VdC(uint bits)
{
    bits = (bits << 16u) | (bits >> 16u);
    bits = ((bits & 0x55555555u) << 1u) | ((bits & 0xAAAAAAAAu) >> 1u);
    bits = ((bits & 0x33333333u) << 2u) | ((bits & 0xCCCCCCCCu) >> 2u);
    bits = ((bits & 0x0F0F0F0Fu) << 4u) | ((bits & 0xF0F0F0F0u) >> 4u);
    bits = ((bits & 0x00FF00FFu) << 8u) | ((bits & 0xFF00FF00u) >> 8u);
    return float(bits) * 2.3283064365386963e-10; // / 0x100000000
}
float2 Hammersley(uint i, uint N)
{
    return float2(float(i) / float(N), RadicalInverse_VdC(i));
}

float3 ImportanceSampleGGX(float2 Xi, float3 N, float roughness)
{
    float a = roughness * roughness;
    float phi = 2.0f * PI * Xi.x;
    float cosTheta = sqrt((1.0f - Xi.y) / (1.0f + (a * a - 1.0f) * Xi.y));
    float sinTheta = sqrt(1.0f - cosTheta * cosTheta);
    
    float3 H = { cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta };

    float3 up = abs(N.z) < 0.999f ? float3(0.0f, 0.0f, 1.0f) : float3(1.0f, 0.0f, 0.0f);
    float3 tangent = normalize(cross(up, N));
    float3 bitangent = cross(N, tangent);
    
    float3 sampleVec = tangent * H.x + bitangent * H.y + N * H.z;
    return normalize(sampleVec);
}

float2 IntegrateBRDF(float NdotV, float roughness)
{
    float3 V = { sqrt(1.0f - NdotV * NdotV), 0.0f, NdotV };
    float A = 0.0f, B = 0.0f;
    
    roughness = lerp(0.04f, 1.0f, roughness); // 确保粗糙度在合理范围内
    
    float3 N = float3(0.0f, 0.0f, 1.0f);
    
    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, roughness);
        float3 L = normalize(2.0f * dot(V, H) * H - V);
        
        float NdotL = max(L.z, 0.0f);
        float NdotH = max(H.z, 0.0f);
        float VdotH = max(dot(V, H), 0.0f);
        
        if(NdotL > 0.0f)
        {
            float G = G_Smith_IBL(NdotL, NdotV, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV + 0.0001f);
            float Fc = pow(1.0f - VdotH, 5.0f);
            
            A += (1.0f - Fc) * G_Vis;
            B += Fc * G_Vis;
        }
    }
    A /= float(SAMPLE_COUNT);
    B /= float(SAMPLE_COUNT);
    return float2(A, B);
}

float4 IntegrateBRDF_Eu(float NdotV, float roughness)
{
    float3 V = { sqrt(1.0f - NdotV * NdotV), 0.0f, NdotV };
    float E = 0.0f;
    
    float3 N = float3(0.0f, 0.0f, 1.0f);
    
    roughness = lerp(0.04f, 1.0f, roughness); // 确保粗糙度在合理范围内
    
    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float2 Xi = Hammersley(i, SAMPLE_COUNT);
        float3 H = ImportanceSampleGGX(Xi, N, roughness);
        float3 L = normalize(2.0f * dot(V, H) * H - V);
        
        float NdotL = max(L.z, 0.0f);
        float NdotH = max(H.z, 0.0f);
        float VdotH = max(dot(V, H), 0.0f);
        
        if (NdotL > 0.0f)
        {
            float G = G_Smith(NdotL, NdotV, roughness);
            float G_Vis = (G * VdotH) / (NdotH * NdotV + 0.0001f);
            E += G_Vis;
        }
    }
    E /= float(SAMPLE_COUNT);
    return float4(E, E, E, 1.0f);
}

float4 CalcEavg(float roughness)
{
    float E_avg = 0.0f;
    roughness = lerp(0.04f, 1.0f, roughness); // 确保粗糙度在合理范围内
    for (uint i = 0u; i < SAMPLE_COUNT; ++i)
    {
        float NdotV = (float(i) + 0.5f) / float(SAMPLE_COUNT);
        float E = gBRDFLUT_Eu.Sample(gsamLinearClamp, float2(NdotV, roughness)).r;
        E_avg += E * NdotV;
    }
    E_avg *= 2.0f * (1.0f / float(SAMPLE_COUNT));
    return float4(E_avg, E_avg, E_avg, 1.0f);
}

float3 AverageFresnel(float3 r, float3 g)
{
    return float3(0.087237f, 0.087237f, 0.087237f) + 0.0230685 * g - 0.0864902 * g * g + 0.0774594 * g * g * g
           + 0.782654 * r - 0.136432 * r * r + 0.278708 * r * r * r
           + 0.19744 * g * r + 0.0360605 * g * g * r - 0.2586 * g * r * r;
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

#define SHADOW_FILTER_SAMPLE_NUM 64
#define LIGHT_WORLD_SIZE 1.0f
#define LIGHT_FRUSTUM_WIDTH 100.0f
#define LIGHT_SIZE_UV (LIGHT_WORLD_SIZE / LIGHT_FRUSTUM_WIDTH)
#define NEAR_PLANE 0.0001f
static const float pi = 3.14159265359;
static const float pi2 = 6.28318530718;

static float2 poissonDisk[SHADOW_FILTER_SAMPLE_NUM];

float rand_2tol(float2 uv)
{
    const float a = 12.9898, b = 78.233, c = 43758.5453;
    float dt = dot(uv.xy, float2(a, b)), sn = fmod(dt, pi);
    return frac(sin(sn) * c);
}
void poissonDiskSamples(float2 randomSeed)
{
    float ANGLE_STEP = pi2 * 10.0f / float(SHADOW_FILTER_SAMPLE_NUM);
    float INV_NUM = 1.0f / float(SHADOW_FILTER_SAMPLE_NUM);
    float angle = rand_2tol(randomSeed) * pi2;
    float radius = INV_NUM;
    float radiusStep = radius;
    
    for (int i = 0; i < SHADOW_FILTER_SAMPLE_NUM; i++)
    {
        poissonDisk[i] = float2(cos(angle), sin(angle)) * pow(radius, 0.75);
        radius += radiusStep;
        angle += ANGLE_STEP;
    }
}

float findBlocker(float2 uv, float d)
{
    float depth = 0.0f;
    float count = 0.0f;
    
    float searchWidth = LIGHT_SIZE_UV * (d - NEAR_PLANE) / d;
    const float MIN_SEARCH_RADIUS_UV = 1.0f / 2048.0f;
    searchWidth = max(searchWidth, MIN_SEARCH_RADIUS_UV);
    for (int i = 0; i < SHADOW_FILTER_SAMPLE_NUM; ++i)
    {
        float shadowDepth = gShadowMap.Sample(gsamLinearClamp, uv + poissonDisk[i] * searchWidth).r;
        if (shadowDepth < d - 0.005f)
        {
            depth += shadowDepth;
            count += 1.0f;
        }
    }
    if (count == 0.0f)
        return -1.0f;
    return depth / count;
}

float PCF(float4 shadowPosH, float filterRadiusUV)
{
    float sum = 0.0f;
    for (int i = 0; i < SHADOW_FILTER_SAMPLE_NUM; ++i)
    {
        float2 offset = poissonDisk[i] * filterRadiusUV;
        sum += gShadowMap.SampleCmpLevelZero(gsamShadow, shadowPosH.xy + offset, shadowPosH.z).r;
    }
    return sum / SHADOW_FILTER_SAMPLE_NUM;
}

float PCSS(float4 shadowPosH)
{
    shadowPosH /= shadowPosH.w;
    float zReceiver = shadowPosH.z;
    poissonDiskSamples(shadowPosH.xy);
    float avgBlockDepth = findBlocker(shadowPosH.xy, zReceiver);
    
    if (avgBlockDepth < 0.0f)
        return 1.0f;
    
    float penumbra = (zReceiver - avgBlockDepth) * LIGHT_SIZE_UV / avgBlockDepth;
    float filterRadiusUV = max(penumbra, 0.0f);
    
    return PCF(shadowPosH, filterRadiusUV);
}