#include "BRDF_LUT.h"

BRDF::BRDF(ID3D12Device* device, UINT width, UINT height)
{
	md3dDevice = device;
	mWidth = width;
	mHeight = height;
	mViewport = { 0.0f, 0.0f, (float)width, (float)height, 0.0f, 1.0f };
	mScissorRect = { 0, 0, (int)width, (int)height };
	BuildResource();
}

ID3D12Resource* BRDF::Resource() const
{
	return mBRDFLUT.Get();
}

CD3DX12_CPU_DESCRIPTOR_HANDLE BRDF::Rtv() const
{
	return mCpuRtv;
}

CD3DX12_GPU_DESCRIPTOR_HANDLE BRDF::Srv() const
{
	return mGpuSrv;
}

D3D12_RECT BRDF::ScissorRect() const
{
	return mScissorRect;
}

D3D12_VIEWPORT BRDF::ViewPort() const
{
	return mViewport;
}

void BRDF::BuildResource()
{
	D3D12_RESOURCE_DESC texDesc = {};
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = mWidth;
	texDesc.Height = mHeight;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = mLUTFormat;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_RENDER_TARGET;

	D3D12_CLEAR_VALUE optClear = {};
	optClear.Format = mLUTFormat;
	optClear.Color[0] = 0.0f;
	optClear.Color[1] = 0.0f;
	optClear.Color[2] = 0.0f;
	optClear.Color[3] = 0.0f;

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_GENERIC_READ,
		&optClear,
		IID_PPV_ARGS(&mBRDFLUT)
	));
}

void BRDF::BuildDescriptors(
	CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuRtv,
	CD3DX12_CPU_DESCRIPTOR_HANDLE hCpuSrv,
	CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuSrv
)
{
	mCpuRtv = hCpuRtv;
	mCpuSrv = hCpuSrv;
	mGpuSrv = hGpuSrv;
	BuildDescriptors();
}

void BRDF::BuildDescriptors()
{
	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.Format = mLUTFormat;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.MipLevels = 1;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;
	md3dDevice->CreateShaderResourceView(mBRDFLUT.Get(), &srvDesc, mCpuSrv);

	D3D12_RENDER_TARGET_VIEW_DESC rtvDesc = {};
	rtvDesc.Format = mLUTFormat;
	rtvDesc.ViewDimension = D3D12_RTV_DIMENSION_TEXTURE2D;
	rtvDesc.Texture2D.MipSlice = 0;
	rtvDesc.Texture2D.PlaneSlice = 0;
	md3dDevice->CreateRenderTargetView(mBRDFLUT.Get(), &rtvDesc, mCpuRtv);
}