#pragma once
#include "DXHelper.h"

class SSR
{
public:
	SSR(ID3D12Device* device, UINT width, UINT height);
	SSR(const SSR& rhs) = delete;
	SSR& operator=(const SSR& rhs) = delete;
	~SSR() = default;

	UINT Width()const { return mWidth; }
	UINT Height()const { return mHeight; }

	ID3D12Resource* Resource() const { return mSSRMap.Get(); }

	CD3DX12_GPU_DESCRIPTOR_HANDLE Srv() const { return mGpuSrv; }
	CD3DX12_CPU_DESCRIPTOR_HANDLE Rtv() const { return mCpuRtv; }

	D3D12_VIEWPORT Viewport() const { return mViewport; }
	D3D12_RECT ScissorRect() const { return mScissorRect; }

	void BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
		CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv,
		CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv
	);

	void OnResize(UINT width, UINT height);

	void BuildDescriptors();

	struct ssrParams
	{
		float maxDistance = 50.0f;
		float resolution = 0.5f;
		float thickness = 0.5f;
		int maxSteps = 128;
		float fadeStart = 0.8f;
		float fadeEnd = 1.0f;
	};

	ssrParams& GetParams() { return mParams; }

	DXGI_FORMAT GetFormat() const { return mFormat; }
private:
	void BuildResources();

	ID3D12Device* md3dDevice = nullptr;

	D3D12_VIEWPORT mViewport;
	D3D12_RECT mScissorRect;

	UINT mWidth = 0;
	UINT mHeight = 0;

	ComPtr<ID3D12Resource> mSSRMap = nullptr;

	CD3DX12_CPU_DESCRIPTOR_HANDLE mCpuSrv;
	CD3DX12_GPU_DESCRIPTOR_HANDLE mGpuSrv;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mCpuRtv;

	ssrParams mParams;

	DXGI_FORMAT mFormat = DXGI_FORMAT_R16G16B16A16_FLOAT;
};