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
    uint cubeMapIndex = matData.CubeMapIndex;
    float metallic = matData.Metallic;
    
    pin.NormalW = normalize(pin.NormalW);
    
    float4 normalMapSample = gTextureMap[normalTexIndex].Sample(gsamAnisotropicWrap, pin.TexC);
    float3 bumpedNormalW = NormalSampleToWorldSpace(normalMapSample.rgb, pin.NormalW, pin.TangentW);
    
    diffuseAlbedo *= gTextureMap[diffuseTexIndex].Sample(gsamAnisotropicWrap, pin.TexC);
    
#ifdef ALPHA_TEST
        clip(diffuseAlbedo.a - 0.1f);
#endif
    
    //观察方向
    float3 viewDir = normalize(gEyePosW - pin.PosW);
    //光源方向
    float3 lightDir = -gLights[0].Direction;
    //光源强度
    float3 lightRadiance = gLights[0].Strength;
    //半程向量
    float3 halfVec = normalize(viewDir + lightDir);
    //法线贴图的A通道存储的光泽度来控制粗糙度
    gRoughness *= normalMapSample.a;
    
    //1.直接光
    //（漫反射 + 镜面反射）* 光源强度 * LdotN
    //漫反射 = kd * c / PI，其中kd = (1 - ks)(1 - metallic)
    //镜面反射 = DGF / (4 * VdotN * LdotN)
    
    float NdotL = max(dot(bumpedNormalW, lightDir), 0.0f);
    float NdotV = max(dot(bumpedNormalW, viewDir), 0.0f);
    float NdotH = max(dot(bumpedNormalW, halfVec), 0.0f);
    float VdotH = max(dot(viewDir, halfVec), 0.0f);
    gFresnelR0 = lerp(gFresnelR0, diffuseAlbedo.rgb, metallic);
    float3 F = SchlickFresnelApproximation(gFresnelR0, VdotH);
    float D = NDFGGXApproximation(NdotH, gRoughness);
    float G = G_Smith(NdotL, NdotV, gRoughness);
    
    float3 E_avg = gLUT_Eavg.Sample(gsamLinearClamp, float2(0.5f, gRoughness)).rgb;
    float3 E_v = gBRDFLUT_Eu.Sample(gsamLinearClamp, float2(NdotV, gRoughness)).rgb;
    float3 E_l = gBRDFLUT_Eu.Sample(gsamLinearClamp, float2(NdotL, gRoughness)).rgb;
    
    float3 edgetint = float3(0.827f, 0.792f, 0.678f);
    float3 F_avg = AverageFresnel(pow(diffuseAlbedo.rgb, float(2.2f)), edgetint);
    
    //1.1 直接光的镜面反射
    float3 specular = (D * F * G) / (4.0f * NdotV * NdotL + 0.001f);
    
    //1.2 使用多重散射代替原有漫反射
    float3 mutiscatter_numerator = (1.0f - E_v) * (1.0f - E_l);
    float3 mutiscatter_denominator = PI * (1.0f - E_avg + 0.0001f);
    float3 mutiscatter = (diffuseAlbedo.rgb * mutiscatter_numerator) / mutiscatter_denominator;
    
    float3 F_add = F_avg * E_avg / (1.0f - F_avg * (1.0f - E_avg) + 0.0001f);
    
    //1.3 直接光的总和
    float3 litColor = (mutiscatter * F_add + specular) * lightRadiance * NdotL;
    
    //2.间接光
    
    //2.1 间接光漫反射
    float3 iblIrradiance = IBLDiffuseIrradiance(bumpedNormalW);
    float3 iblF = SchlickFresnelApproximation(gFresnelR0, NdotV);
    float3 iblKd = (1 - iblF) * (1 - metallic);
    //float3 iblKd_ms = (1.0f - E_v) * (1.0f - metallic);
    float3 iblDiffuse = iblKd * iblIrradiance * diffuseAlbedo.rgb;
    
    //2.2 间接光镜面反射
    float3 r = reflect(-viewDir, bumpedNormalW);
    float maxLod = 8.0f;
    float3 iblSpecularIrradiance = gCubeMap[cubeMapIndex].SampleLevel(gsamAnisotropicWrap, r, gRoughness * maxLod).rgb;
    float2 lut = gBRDFLUT.Sample(gsamLinearClamp, float2(NdotV, gRoughness)).rg;
    float3 iblSpecularBRDF = iblF * lut.x + lut.y;
    float3 iblSpecular = iblSpecularIrradiance * iblSpecularBRDF;
    
    //2.3 间接光的总和
    litColor += iblDiffuse + iblSpecular;
    
    //3.最终输出
    //色调映射 + 伽马矫正
    //litColor = litColor / (litColor + float3(1.0f, 1.0f, 1.0f));
    
    //litColor = pow(litColor, 1.0f / 2.2f); // Gamma correction
    
    return float4(litColor, diffuseAlbedo.a);
}