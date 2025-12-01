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
    uint Pad0;
    uint Pad1;
};

Texture2D gAlbedoMap : register(t0);
Texture2D gNormalMap : register(t1);
Texture2D gPositionMap : register(t2);
Texture2D gDepthMap : register(t3);
Texture2D gSceneColor : register(t4);

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

bool RayMarch(float3 rayOrigin, float3 rayDir, out float2 hitUV, out float hitDepth)
{
    float stepSize = gMaxDistance / (float) gMaxSteps;
    float3 currentPos = rayOrigin;
    //float3 prevPos = rayOrigin;
    
    for (int i = 0; i < gMaxSteps; i++)
    {
        //prevPos = currentPos;
        currentPos += rayDir * stepSize;
        
        float3 screenPos = ViewToScreen(currentPos);
        
        if (screenPos.x < 0.0f || screenPos.x > 1.0f || 
            screenPos.y < 0.0f || screenPos.y > 1.0f || 
            screenPos.z < 0.0f || screenPos.z > 1.0f)
            return false;
        
        float sceneDepth = gDepthMap.SampleLevel(gsamPointClamp, screenPos.xy, 0).r;
        float3 sceneViewPos = ScreenToView(screenPos.xy, sceneDepth);
        
        float depthDiff = currentPos.z - sceneViewPos.z;
        
        //float3 prevScreenPos = ViewToScreen(prevPos);
        //float prevDepth = gDepthMap.SampleLevel(gsamPointClamp, prevScreenPos.xy, 0).r;
        //float3 prevSceneViewPos = ScreenToView(prevScreenPos.xy, prevDepth);
        //float prevDepthDiff = prevPos.z - prevSceneViewPos.z;
        
        //bool crossedSurface = (depthDiff > 0.0f && prevDepthDiff <= 0.0f);
        
        //if (crossedSurface && depthDiff < gThickness)
        if (depthDiff > 0.0001f)
        {
            //float3 hitNormal = gNormalMap.SampleLevel(gsamPointClamp, screenPos.xy, 0).rgb;
            //float3 hitViewNormal = normalize(mul(float4(hitNormal, 0.0f), gView).xyz);
            
            //if (dot(hitViewNormal, -rayDir) >= 0.0f)
            //{
            //    hitUV = screenPos.xy;
            //    hitDepth = sceneDepth;
            //    return true;
            //}
            hitUV = screenPos.xy;
            hitDepth = sceneDepth;
            return true;
        }
        
        //float linearSceneDepth = LinearizeDepth(sceneDepth);
        //float linearCurrentDepth = LinearizeDepth(screenPos.z);
        
        //if (linearCurrentDepth - linearSceneDepth < gThickness)
        //{
        //    hitUV = screenPos.xy;
        //    hitDepth = sceneDepth;
        //    return true;
        //}
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
    if(RayMarch(origin, reflectDir, hitUV, hitDepth))
    {
        float3 reflectedColor = gSceneColor.Sample(gsamLinearClamp, hitUV).rgb;
        
        float2 edgeFade = 1.0f - pow(abs(hitUV * 2.0f - 1.0f), 4.0f);
        float fade = min(edgeFade.x, edgeFade.y);
        
        float distance = length(hitUV - pin.TexC);
        float distanceFade = 1.0f - smoothstep(gFadeStart, gFadeEnd, distance);
        
        fade *= distanceFade;
        
        return float4(reflectedColor, fade);
    }
    return float4(0.0f, 0.0f, 0.0f, 0.0f);
}