// HiZGeneration.hlsl
Texture2D<float> gInputDepth : register(t0);
RWTexture2D<float> gOutputDepth : register(u0);

SamplerState gsamPointClamp : register(s0);

[numthreads(8, 8, 1)]
void CS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 outputPos = dispatchThreadID.xy;
    
    // 获取输出尺寸
    uint width, height;
    gOutputDepth.GetDimensions(width, height);
    
    if (outputPos.x >= width || outputPos.y >= height)
        return;
    
    // 获取输入尺寸
    uint inputWidth, inputHeight;
    gInputDepth.GetDimensions(inputWidth, inputHeight);
    
    // 采样 2x2 区域取最小深度（远平面）
    uint2 inputBase = outputPos * 2;
    
    float depth0 = gInputDepth[min(inputBase + uint2(0, 0), uint2(inputWidth - 1, inputHeight - 1))];
    float depth1 = gInputDepth[min(inputBase + uint2(1, 0), uint2(inputWidth - 1, inputHeight - 1))];
    float depth2 = gInputDepth[min(inputBase + uint2(0, 1), uint2(inputWidth - 1, inputHeight - 1))];
    float depth3 = gInputDepth[min(inputBase + uint2(1, 1), uint2(inputWidth - 1, inputHeight - 1))];
    
    float minDepth = min(min(depth0, depth1), min(depth2, depth3));
    
    gOutputDepth[outputPos] = minDepth;
}