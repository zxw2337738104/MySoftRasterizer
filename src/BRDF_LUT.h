#pragma once
#include "DXHelper.h"

class BRDF
{
public:
	BRDF(ID3D12Device* device, UINT width, UINT height);
	BRDF(const BRDF& rhs) = delete;
	BRDF& operator=(const BRDF& rhs) = delete;
	~BRDF() = default;

	ID3D12Resource* Resource() const;
	CD3DX12_CPU_DESCRIPTOR_HANDLE Rtv()const;
	CD3DX12_GPU_DESCRIPTOR_HANDLE Srv()const;

	D3D12_RECT ScissorRect() const;
	D3D12_VIEWPORT ViewPort() const;

	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv
	);

private:
	void BuildDescriptors();
	void BuildResource();

	ID3D12Device* md3dDevice = nullptr;
	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;

	UINT mWidth = 0;
	UINT mHeight = 0;
	DXGI_FORMAT mLUTFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mCpuRtv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mGpuSrv;

	ComPtr<ID3D12Resource> mBRDFLUT = nullptr;
};