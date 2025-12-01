#pragma once
#include "DXHelper.h"

class SceneColorRT
{
public:
    SceneColorRT(ID3D12Device* device, UINT width, UINT height);
    SceneColorRT(const SceneColorRT& rhs) = delete;
    SceneColorRT& operator=(const SceneColorRT& rhs) = delete;
    ~SceneColorRT() = default;

    UINT Width() const { return mWidth; }
    UINT Height() const { return mHeight; }

    ID3D12Resource* Resource() { return mSceneColor.Get(); }
    CD3DX12_GPU_DESCRIPTOR_HANDLE Srv() const { return mGpuSrv; }
    CD3DX12_CPU_DESCRIPTOR_HANDLE Rtv() const { return mCpuRtv; }

    D3D12_VIEWPORT Viewport() const { return mViewport; }
    D3D12_RECT ScissorRect() const { return mScissorRect; }

    DXGI_FORMAT Format() const { return mFormat; }
    FLOAT* ClearColor() { return mClearColor; }

    void BuildDescriptors(
        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
        CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
        CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv);

    void OnResize(UINT newWidth, UINT newHeight);

private:
    void BuildResource();
    void BuildDescriptors();

    ID3D12Device* md3dDevice = nullptr;

    D3D12_VIEWPORT mViewport;
    D3D12_RECT mScissorRect;

    UINT mWidth = 0;
    UINT mHeight = 0;

    // HDR格式，支持大于1.0的值
    //DXGI_FORMAT mFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
    DXGI_FORMAT mFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
    float mClearColor[4] = { 0.0f, 0.0f, 0.0f, 1.0f };

    CD3DX12_CPU_DESCRIPTOR_HANDLE mCpuSrv;
    CD3DX12_GPU_DESCRIPTOR_HANDLE mGpuSrv;
    CD3DX12_CPU_DESCRIPTOR_HANDLE mCpuRtv;

    ComPtr<ID3D12Resource> mSceneColor = nullptr;
};