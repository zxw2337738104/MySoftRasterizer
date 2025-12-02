#pragma once
#include "DXHelper.h"

using Microsoft::WRL::ComPtr;

class HiZBuffer
{
public:
    HiZBuffer(ID3D12Device* device, UINT width, UINT height);
    HiZBuffer(const HiZBuffer& rhs) = delete;
    HiZBuffer& operator=(const HiZBuffer& rhs) = delete;
    ~HiZBuffer() = default;

    UINT Width() const { return mWidth; }
    UINT Height() const { return mHeight; }
    UINT MipLevels() const { return mMipLevels; }

    ID3D12Resource* Resource() const { return mHiZBuffer.Get(); }

    CD3DX12_GPU_DESCRIPTOR_HANDLE Srv() const { return mGpuSrv; }
    CD3DX12_CPU_DESCRIPTOR_HANDLE Srv(UINT mipLevel) const
    {
        auto cpuSrv = mCpuSrv;
        return cpuSrv.Offset(1 + mipLevel, mSrvDescriptorSize);
    }

    CD3DX12_GPU_DESCRIPTOR_HANDLE Uav() const { return mGpuUav;  }
    CD3DX12_CPU_DESCRIPTOR_HANDLE Uav(UINT mipLevel) const
    {
        return CD3DX12_CPU_DESCRIPTOR_HANDLE(
            mCpuUav,
            mipLevel,
            mUavDescriptorSize);
    }

    void BuildDescriptors(
        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuDescriptor,
        CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor,
        UINT DescriptorSize);

    void OnResize(UINT width, UINT height);
    void RebuildDescriptors();

    // Éú³ÉHi-Z Mipmap
    void GenerateMips(
        ID3D12GraphicsCommandList* cmdList,
        ID3D12RootSignature* rootSig,
        ID3D12PipelineState* pso0,
        ID3D12PipelineState* pso1,
		CD3DX12_GPU_DESCRIPTOR_HANDLE inputSrv
    );

    static const DXGI_FORMAT HiZFormat = DXGI_FORMAT_R32_FLOAT;

private:
    void BuildResource();
    UINT CalculateMipLevels(UINT width, UINT height);

    ID3D12Device* md3dDevice = nullptr;

    UINT mWidth = 0;
    UINT mHeight = 0;
    UINT mMipLevels = 0;

    ComPtr<ID3D12Resource> mHiZBuffer = nullptr;

    CD3DX12_CPU_DESCRIPTOR_HANDLE mCpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE mGpuSrv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE mCpuUav;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mGpuUav;

    UINT mSrvDescriptorSize = 0;
    UINT mUavDescriptorSize = 0;
};