// HiZGeneration_FromDepth.hlsl
// 第一次生成：从深度缓冲到 Mip0
Texture2D<float> gInputDepth : register(t0); // R24_UNORM 格式
RWTexture2D<float> gOutputDepth : register(u0); // R32_FLOAT 格式

[numthreads(8, 8, 1)]
void CS(uint3 dispatchThreadID : SV_DispatchThreadID)
{
    uint2 pos = dispatchThreadID.xy;
    
    uint outWidth, outHeight;
    gOutputDepth.GetDimensions(outWidth, outHeight);
    
    if (pos.x >= outWidth || pos.y >= outHeight)
        return;
    
    uint inWidth, inHeight;
    gInputDepth.GetDimensions(inWidth, inHeight);
    
    if (pos.x < inWidth && pos.y < inHeight)
    {
        gOutputDepth[pos] = gInputDepth[pos];
    }
    else
    {
        // 边界外填充远平面值（反向Z用1.0，正向Z用0.0）
        gOutputDepth[pos] = 0.0;
    }
}