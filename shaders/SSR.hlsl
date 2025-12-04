cbuffer ssrConstants : register(b0)
{
    float4x4 gView;
    float4x4 gInvView;
    float4x4 gProj;
    float4x4 gInvProj;
    float4x4 gViewProj;
    float4x4 gInvViewProj;
    
    float2 gRenderTargetSize;
    float2 gInvRenderTargetSize;
    
    float gMaxDistance;
    float gResolution;
    float gThickness;
    int gMaxSteps;
    
    float gFadeStart;
    float gFadeEnd;
    uint gHiZMipLevels;
    uint Pad1;
};

Texture2D gAlbedoMap : register(t0);
Texture2D gNormalMap : register(t1);
Texture2D gPositionMap : register(t2);
Texture2D gDepthMap : register(t3);
Texture2D gSceneColor : register(t4);
Texture2D gHiZBuffer : register(t5);

SamplerState gsamPointClamp : register(s0);
SamplerState gsamLinearClamp : register(s1);

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
    float2 TexC : TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID)
{
    VertexOut vout;
    vout.TexC = gTexCoords[vid];
    
    vout.PosH = float4(2.0f * vout.TexC.x - 1.0f, 1.0f - 2.0f * vout.TexC.y, 0.0f, 1.0f);
    
    return vout;
}

float3 ScreenToView(float2 uv, float depth)
{
    float4 ndc = float4(uv * 2.0f - 1.0f, depth, 1.0f);
    ndc.y = -ndc.y;
    float4 ph = mul(ndc, gInvProj);
    return ph.xyz / ph.w;
}

float3 ViewToScreen(float3 posV)
{
    float4 ph = mul(float4(posV, 1.0f), gProj);
    ph.xyz /= ph.w;
    ph.y = -ph.y;
    return ph.xyz * 0.5f + 0.5f;
}

float LinearizeDepth(float depth)
{
    float nearZ = 0.1f;
    float farZ = 1000.0f;
    return (farZ * nearZ) / (farZ - depth * (farZ - nearZ));
}

////二分查找
//bool RayMarch(float3 rayOrigin, float3 rayDir, out float2 hitUV, out float hitDepth)
//{
//    float stepSize = gMaxDistance / (float) gMaxSteps;
//    float3 currentPos = rayOrigin;
//    float3 prevPos = rayOrigin;
    
//    // ? 第一阶段：粗略步进找到大致交点
//    for (int i = 0; i < gMaxSteps; i++)
//    {
//        prevPos = currentPos;
//        currentPos += rayDir * stepSize;
        
//        float3 screenPos = ViewToScreen(currentPos);
        
//        // 边界检查
//        if (screenPos.x < 0.0f || screenPos.x > 1.0f ||
//            screenPos.y < 0.0f || screenPos.y > 1.0f ||
//            screenPos.z < 0.0f || screenPos.z > 1.0f)
//            return false;
        
//        float sceneDepth = gDepthMap.SampleLevel(gsamPointClamp, screenPos.xy, 0).r;
//        float3 sceneViewPos = ScreenToView(screenPos.xy, sceneDepth);
        
//        float depthDiff = currentPos.z - sceneViewPos.z;
        
//        // 检测是否穿过表面
//        if (depthDiff > 0.0f && depthDiff < gThickness)
//        {
//            // ? 第二阶段：二分搜索精确定位交点
//            float3 refinedPos = prevPos;
//            float3 searchStart = prevPos;
//            float3 searchEnd = currentPos;
            
//            // 进行 8 次二分搜索
//            [unroll]
//            for (int j = 0; j < 8; j++)
//            {
//                refinedPos = (searchStart + searchEnd) * 0.5f;
//                float3 refinedScreenPos = ViewToScreen(refinedPos);
                
//                float refinedSceneDepth = gDepthMap.SampleLevel(gsamPointClamp, refinedScreenPos.xy, 0).r;
//                float3 refinedSceneViewPos = ScreenToView(refinedScreenPos.xy, refinedSceneDepth);
                
//                float refinedDepthDiff = refinedPos.z - refinedSceneViewPos.z;
                
//                if (refinedDepthDiff > 0.0f)
//                    searchEnd = refinedPos;
//                else
//                    searchStart = refinedPos;
//            }
            
//            // 使用细化后的位置
//            float3 finalScreenPos = ViewToScreen(refinedPos);
//            float finalSceneDepth = gDepthMap.SampleLevel(gsamPointClamp, finalScreenPos.xy, 0).r;
//            float3 finalSceneViewPos = ScreenToView(finalScreenPos.xy, finalSceneDepth);
//            float finalDepthDiff = refinedPos.z - finalSceneViewPos.z;
            
//            // 检查最终深度差是否在合理范围内
//            if (abs(finalDepthDiff) < gThickness)
//            {
//                // ? 额外检查：避免自相交
//                float distToOrigin = length(finalSceneViewPos - rayOrigin);
//                if (distToOrigin > 0.1f)
//                {
//                    // ? 法线背面剔除
//                    float3 hitNormal = gNormalMap.SampleLevel(gsamPointClamp, finalScreenPos.xy, 0).rgb;
//                    hitNormal = normalize(hitNormal * 2.0f - 1.0f);
//                    float3 hitViewNormal = normalize(mul(float4(hitNormal, 0.0f), gView).xyz);
                    
//                    if (dot(hitViewNormal, rayDir) < 0.0f)
//                    {
//                        hitUV = finalScreenPos.xy;
//                        hitDepth = finalSceneDepth;
//                        return true;
//                    }
//                }
//            }
//        }
//    }
//    return false;
//}

//分层mip
bool RayMarch(float3 rayOrigin, float3 rayDir, out float2 hitUV, out float hitDepth)
{
    float stepSize = gMaxDistance / (float) gMaxSteps;
    float3 currentPos = rayOrigin;
    
    for (int i = 0; i < gMaxSteps; i++)
    {
        currentPos += rayDir * stepSize;
        
        float3 screenPos = ViewToScreen(currentPos);
        
        if (screenPos.x < 0.0f || screenPos.x > 1.0f ||
            screenPos.y < 0.0f || screenPos.y > 1.0f ||
            screenPos.z < 0.0f || screenPos.z > 1.0f)
            return false;
        
        // 使用分层采样：远处用低精度mipmap
        float mipLevel = clamp((float) i / 32.0f, 0.0f, 3.0f);
        float sceneDepth = gDepthMap.SampleLevel(gsamPointClamp, screenPos.xy, mipLevel).r;
        float3 sceneViewPos = ScreenToView(screenPos.xy, sceneDepth);
        
        float depthDiff = currentPos.z - sceneViewPos.z;
        
        if (depthDiff > 0.0f && depthDiff < gThickness)
        {
            // 命中后再用最高精度采样一次
            sceneDepth = gDepthMap.SampleLevel(gsamPointClamp, screenPos.xy, 0).r;
            sceneViewPos = ScreenToView(screenPos.xy, sceneDepth);
            depthDiff = currentPos.z - sceneViewPos.z;
            
            if (abs(depthDiff) < gThickness * 0.5f)
            {
                float distToOrigin = length(sceneViewPos - rayOrigin);
                if (distToOrigin > 0.1f)
                {
                    hitUV = screenPos.xy;
                    hitDepth = sceneDepth;
                    return true;
                }
            }
        }
    }
    return false;
}

//=============================================================================
// 固定步长 RayMarch（对照组）
//=============================================================================
bool RayMarchFixed(float3 rayOrigin, float3 rayDir, out float2 hitUV, out float hitDepth)
{
    hitUV = float2(0.0f, 0.0f);
    hitDepth = 0.0f;
    
    // 固定步长
    float stepSize = gMaxDistance / (float) gMaxSteps;
    
    float3 currentPos = rayOrigin;
    float3 prevPos = rayOrigin;
    float totalDistance = 0.0f;
    
    for (int i = 0; i < gMaxSteps; i++)
    {
        // 保存上一步位置用于二分查找
        prevPos = currentPos;
        
        // 使用固定步长前进
        currentPos += rayDir * stepSize;
        totalDistance += stepSize;
        
        // 距离限制检查
        if (totalDistance > gMaxDistance)
            return false;
        
        // 转换到屏幕空间
        float3 screenPos = ViewToScreen(currentPos);
        
        // 边界检查
        if (screenPos.x < 0.0f || screenPos.x > 1.0f ||
            screenPos.y < 0.0f || screenPos.y > 1.0f ||
            screenPos.z < 0.0f || screenPos.z > 1.0f)
            return false;
        
        // 采样场景深度
        float sceneDepth = gDepthMap.SampleLevel(gsamPointClamp, screenPos.xy, 0).r;
        float3 sceneViewPos = ScreenToView(screenPos.xy, sceneDepth);
        
        // 计算深度差（正值表示光线在场景后方）
        float depthDiff = currentPos.z - sceneViewPos.z;
        
        // 检测是否穿过表面
        if (depthDiff > 0.0f && depthDiff < gThickness)
        {
            // ========== 二分查找精确定位 ==========
            float3 refinedPos = currentPos;
            float3 searchStart = prevPos;
            float3 searchEnd = currentPos;
            
            [unroll]
            for (int j = 0; j < 8; j++)
            {
                refinedPos = (searchStart + searchEnd) * 0.5f;
                float3 refinedScreenPos = ViewToScreen(refinedPos);
                
                float refinedSceneDepth = gDepthMap.SampleLevel(gsamPointClamp, refinedScreenPos.xy, 0).r;
                float3 refinedSceneViewPos = ScreenToView(refinedScreenPos.xy, refinedSceneDepth);
                
                float refinedDepthDiff = refinedPos.z - refinedSceneViewPos.z;
                
                if (refinedDepthDiff > 0.0f)
                    searchEnd = refinedPos;
                else
                    searchStart = refinedPos;
            }
            
            // 获取最终结果
            float3 finalScreenPos = ViewToScreen(refinedPos);
            float finalSceneDepth = gDepthMap.SampleLevel(gsamPointClamp, finalScreenPos.xy, 0).r;
            float3 finalSceneViewPos = ScreenToView(finalScreenPos.xy, finalSceneDepth);
            float finalDepthDiff = refinedPos.z - finalSceneViewPos.z;
            
            // 验证命中
            if (abs(finalDepthDiff) < gThickness)
            {
                // 自相交检查
                float distToOrigin = length(finalSceneViewPos - rayOrigin);
                if (distToOrigin > 0.1f)
                {
                    hitUV = finalScreenPos.xy;
                    hitDepth = finalSceneDepth;
                    return true;
                }
            }
        }
    }
    
    return false;
}

// 获取 Hi-Z 深度（自动选择合适的 mip 级别）
float GetHiZDepth(float2 uv, float mipLevel)
{
    return gHiZBuffer.SampleLevel(gsamPointClamp, uv, mipLevel).r;
}

// Hi-Z 加速的光线步进
bool RayMarchHiZ(float3 rayOrigin, float3 rayDir, out float2 hitUV, out float hitDepth)
{
    hitUV = float2(0.0f, 0.0f);
    hitDepth = 0.0f;
    
    float3 rayEnd = rayOrigin + rayDir * gMaxDistance;
    float3 baseRayStep = (rayEnd - rayOrigin) / float(gMaxSteps);
    
    float3 currentPos = rayOrigin;
    float3 prevPos = rayOrigin;
    
    int currentMip = 0;
    int maxMip = min((int) gHiZMipLevels - 1, 4);
    
    for (int i = 0; i < gMaxSteps;)
    {
        int stepMultiplier = 1 << currentMip;
        
        prevPos = currentPos;
        currentPos += baseRayStep * (float) stepMultiplier;
        
        float3 screenPos = ViewToScreen(currentPos);
        
        if (screenPos.x < 0.0f || screenPos.x > 1.0f ||
            screenPos.y < 0.0f || screenPos.y > 1.0f ||
            screenPos.z < 0.0f || screenPos.z > 1.0f)
            return false;
        
        float hiZDepth = GetHiZDepth(screenPos.xy, (float) currentMip);
        float3 hiZViewPos = ScreenToView(screenPos.xy, hiZDepth);
        
        float depthDiff = currentPos.z - hiZViewPos.z;
        
        if (depthDiff > 0.0f)
        {
            if (currentMip > 0)
            {
                currentPos = prevPos;
                currentMip--;
                continue;
            }
            
            float preciseDepth = gDepthMap.SampleLevel(gsamPointClamp, screenPos.xy, 0).r;
            float3 preciseViewPos = ScreenToView(screenPos.xy, preciseDepth);
            float preciseDepthDiff = currentPos.z - preciseViewPos.z;
            
            if (preciseDepthDiff > 0.0f && preciseDepthDiff < gThickness)
            {
                float3 refinedPos = currentPos;
                float3 searchStart = prevPos;
                float3 searchEnd = currentPos;
                
                [unroll]
                for (int j = 0; j < 8; j++)
                {
                    refinedPos = (searchStart + searchEnd) * 0.5f;
                    float3 refinedScreenPos = ViewToScreen(refinedPos);
                    
                    float refinedDepth = gDepthMap.SampleLevel(gsamPointClamp, refinedScreenPos.xy, 0).r;
                    float3 refinedViewPos = ScreenToView(refinedScreenPos.xy, refinedDepth);
                    
                    float refinedDepthDiff = refinedPos.z - refinedViewPos.z;
                    
                    if (refinedDepthDiff > 0.0f)
                        searchEnd = refinedPos;
                    else
                        searchStart = refinedPos;
                }
                
                float3 finalScreenPos = ViewToScreen(refinedPos);
                float finalDepth = gDepthMap.SampleLevel(gsamPointClamp, finalScreenPos.xy, 0).r;
                float3 finalViewPos = ScreenToView(finalScreenPos.xy, finalDepth);
                
                float distToOrigin = length(finalViewPos - rayOrigin);
                if (distToOrigin > 0.1f)
                {
                    float3 hitNormal = gNormalMap.SampleLevel(gsamPointClamp, finalScreenPos.xy, 0).rgb;
                    hitNormal = normalize(hitNormal * 2.0f - 1.0f);
                    float3 hitViewNormal = normalize(mul(float4(hitNormal, 0.0f), gView).xyz);
                    
                    if (dot(hitViewNormal, rayDir) < 0.0f)
                    {
                        hitUV = finalScreenPos.xy;
                        hitDepth = finalDepth;
                        return true;
                    }
                }
            }
            
            i += 1;
        }
        else
        {
            i += stepMultiplier;
            
            if (currentMip < maxMip)
            {
                // 只有当深度差足够大时才提升 mip
                float safetyMargin = abs(depthDiff);
                float threshold = gThickness * (float) (1 << (currentMip + 1));
                
                if (safetyMargin > threshold)
                {
                    float nextHiZDepth = GetHiZDepth(screenPos.xy, (float) (currentMip + 1));
                    float3 nextHiZViewPos = ScreenToView(screenPos.xy, nextHiZDepth);
                    
                    if (currentPos.z < nextHiZViewPos.z - gThickness)
                    {
                        currentMip++;
                    }
                }
            }
        }
    }
    
    return false;
}

float4 PS(VertexOut pin) : SV_Target
{
    float3 albedo = gAlbedoMap.Sample(gsamPointClamp, pin.TexC).rgb;
    float3 normal = gNormalMap.Sample(gsamPointClamp, pin.TexC).rgb;
    float3 position = gPositionMap.Sample(gsamPointClamp, pin.TexC).rgb;
    float depth = gDepthMap.Sample(gsamPointClamp, pin.TexC).r;
    
    if (depth >= 1.0f)
        return float4(0.0f, 0.0f, 0.0f, 0.0f);
    
    float3 viewPos = mul(float4(position, 1.0f), gView).xyz;
    float3 viewNormal = normalize(mul(float4(normal, 0.0f), gView).xyz);
    
    float3 viewDir = normalize(-viewPos);
    float3 reflectDir = normalize(reflect(-viewDir, viewNormal));
    
    float2 hitUV;
    float hitDepth;
    
    float3 origin = viewPos + viewNormal * 0.05f;
    if (RayMarchHiZ(origin, reflectDir, hitUV, hitDepth))
    {
        float3 reflectedColor = gSceneColor.Sample(gsamLinearClamp, hitUV).rgb;
        
        //改进的边缘淡化
        float2 edgeFade = smoothstep(0.0f, 0.1f, hitUV) * smoothstep(1.0f, 0.9f, hitUV);
        float fade = min(edgeFade.x, edgeFade.y);
        
        //基于距离的淡化
        float hitDistance = length(hitUV - pin.TexC);
        float distanceFade = 1.0f - smoothstep(gFadeStart, gFadeEnd, hitDistance);
        fade *= distanceFade;
        
        //基于视角的淡化（掠射角处减弱）
        float NdotV = saturate(dot(viewNormal, viewDir));
        float fresnelFade = smoothstep(0.0f, 0.3f, NdotV);
        fade *= fresnelFade;
        
        fade *= distanceFade;
        
        return float4(reflectedColor, fade);
    }
    return float4(0.0f, 0.0f, 0.0f, 0.0f);
}