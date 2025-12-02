#include "HiZBuffer.h"
#include <algorithm>

HiZBuffer::HiZBuffer(ID3D12Device* device, UINT width, UINT height)
    : md3dDevice(device), mWidth(width), mHeight(height)
{
    mMipLevels = CalculateMipLevels(width, height);
    BuildResource();
}

UINT HiZBuffer::CalculateMipLevels(UINT width, UINT height)
{
    UINT levels = 1;
    while (width > 1 || height > 1)
    {
        width = max(1u, width / 2);
        height = max(1u, height / 2);
        levels++;
    }
    return levels;
}

void HiZBuffer::BuildResource()
{
    D3D12_RESOURCE_DESC texDesc = {};
    texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
    texDesc.Alignment = 0;
    texDesc.Width = mWidth;
    texDesc.Height = mHeight;
    texDesc.DepthOrArraySize = 1;
    texDesc.MipLevels = mMipLevels;
    texDesc.Format = HiZFormat;
    texDesc.SampleDesc.Count = 1;
    texDesc.SampleDesc.Quality = 0;
    texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
    texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

    ThrowIfFailed(md3dDevice->CreateCommittedResource(
        &CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
        D3D12_HEAP_FLAG_NONE,
        &texDesc,
        D3D12_RESOURCE_STATE_GENERIC_READ,
        nullptr,
        IID_PPV_ARGS(&mHiZBuffer)
    ));
}

void HiZBuffer::BuildDescriptors(
    CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor,
    CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor,
    UINT DescriptorSize)
{
    mCpuSrv = hCpuDescriptor;
    mGpuSrv = hGpuDescriptor;
    mCpuUav = hCpuDescriptor.Offset(1 + mMipLevels, DescriptorSize);
    mGpuUav = hGpuDescriptor.Offset(1 + mMipLevels, DescriptorSize);

    mSrvDescriptorSize = DescriptorSize;
    mUavDescriptorSize = DescriptorSize;

    RebuildDescriptors();
}

void HiZBuffer::RebuildDescriptors()
{
    // 创建完整的SRV（所有Mip级别）
    D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
    srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
    srvDesc.Format = HiZFormat;
    srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
    srvDesc.Texture2D.MostDetailedMip = 0;
    srvDesc.Texture2D.MipLevels = mMipLevels;
    md3dDevice->CreateShaderResourceView(mHiZBuffer.Get(), &srvDesc, mCpuSrv);

    auto cpuSrv = mCpuSrv;
    cpuSrv.Offset(1, mSrvDescriptorSize);
    // 为每个Mip级别创建SRV
    for (UINT i = 0; i < mMipLevels; ++i)
    {
        srvDesc.Texture2D.MostDetailedMip = i;
        srvDesc.Texture2D.MipLevels = 1;
        md3dDevice->CreateShaderResourceView(mHiZBuffer.Get(), &srvDesc, cpuSrv);
		cpuSrv.Offset(1, mSrvDescriptorSize);
    }

    // 为每个Mip级别创建UAV
    D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
    uavDesc.Format = HiZFormat;
    uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;

	auto cpuUav = mCpuUav;
    for (UINT i = 0; i < mMipLevels; ++i)
    {
        uavDesc.Texture2D.MipSlice = i;
        md3dDevice->CreateUnorderedAccessView(
            mHiZBuffer.Get(),
            nullptr,
            &uavDesc,
            cpuUav);
		cpuUav.Offset(1, mUavDescriptorSize);
    }
}

void HiZBuffer::GenerateMips(
    ID3D12GraphicsCommandList* cmdList,
    ID3D12RootSignature* rootSig,
    ID3D12PipelineState* psoFromDepth,
	ID3D12PipelineState* pso,
    CD3DX12_GPU_DESCRIPTOR_HANDLE inputSrv)
{
    cmdList->SetComputeRootSignature(rootSig);
    cmdList->SetPipelineState(psoFromDepth);

    // 转换到UAV状态
    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        mHiZBuffer.Get(),
        D3D12_RESOURCE_STATE_GENERIC_READ,
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS));

    UINT width = mWidth;
    UINT height = mHeight;

    {
        // 设置源深度图（SRV）
        cmdList->SetComputeRootDescriptorTable(0, inputSrv);

        // 设置目标 Mip0（UAV）
        cmdList->SetComputeRootDescriptorTable(1,
            CD3DX12_GPU_DESCRIPTOR_HANDLE(mGpuUav, 0, mUavDescriptorSize));

        UINT threadGroupX = (width + 7) / 8;
        UINT threadGroupY = (height + 7) / 8;

        cmdList->Dispatch(threadGroupX, threadGroupY, 1);

        // 等待完成
        cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(mHiZBuffer.Get()));
    }

	cmdList->SetPipelineState(pso);
    // 生成后续 Mip 级别（从前一级 Mip）
    for (UINT mipLevel = 1; mipLevel < mMipLevels; ++mipLevel)
    {
        width = max(1u, width / 2);
        height = max(1u, height / 2);

        // 设置源 Mip 级别（前一级，作为 SRV）
        // 注意：要从 mGpuSrv 偏移，因为我们需要单个 mip 级别的 SRV
        CD3DX12_GPU_DESCRIPTOR_HANDLE srcSrv = mGpuSrv;
        srcSrv.Offset(mipLevel, mSrvDescriptorSize);  // mipLevel 因为第一个是总 SRV
        cmdList->SetComputeRootDescriptorTable(0, srcSrv);

        // 设置目标 Mip 级别（当前级，作为 UAV）
        CD3DX12_GPU_DESCRIPTOR_HANDLE dstUav = mGpuUav;
        dstUav.Offset(mipLevel, mUavDescriptorSize);
        cmdList->SetComputeRootDescriptorTable(1, dstUav);

        UINT threadGroupX = (width + 7) / 8;
        UINT threadGroupY = (height + 7) / 8;

        cmdList->Dispatch(threadGroupX, threadGroupY, 1);

        // 等待完成
        cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::UAV(mHiZBuffer.Get()));
    }

    // 转换回通用读取状态
    cmdList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
        mHiZBuffer.Get(),
        D3D12_RESOURCE_STATE_UNORDERED_ACCESS,
        D3D12_RESOURCE_STATE_GENERIC_READ));
}

void HiZBuffer::OnResize(UINT width, UINT height)
{
    if (mWidth != width || mHeight != height)
    {
        mWidth = width;
        mHeight = height;
        mMipLevels = CalculateMipLevels(width, height);

        BuildResource();
        RebuildDescriptors();
    }
}