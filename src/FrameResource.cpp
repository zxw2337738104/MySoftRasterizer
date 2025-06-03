#include "FrameResource.hpp"

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objCount)
{
	ThrowIfFailed(device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));
	PassCB = std::make_unique<UploadBufferResource<PassConstants>>(device, passCount, true);
	ObjectCB = std::make_unique<UploadBufferResource<ObjectConstants>>(device, objCount, true);
}

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objCount, UINT matCount)
{
	ThrowIfFailed(device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));
	PassCB = std::make_unique<UploadBufferResource<PassConstants>>(device, passCount, true);
	ObjectCB = std::make_unique<UploadBufferResource<ObjectConstants>>(device, objCount, true);
	//	WavesVB = std::make_unique<UploadBufferResource<Vertex>>(device, waveVertCount, false);
	/*MaterialCB = std::make_unique<UploadBufferResource<MaterialConstants>>(device, waveVertCount, true);*/
	MatSB = std::make_unique<UploadBufferResource<MaterialData>>(device, matCount, false);
}

//FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objCount, UINT waveVertCount, UINT materialCount)
//{
//	ThrowIfFailed(device->CreateCommandAllocator(
//		D3D12_COMMAND_LIST_TYPE_DIRECT,
//		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));
//	PassCB = std::make_unique<UploadBufferResource<PassConstants>>(device, passCount, true);
//	ObjectCB = std::make_unique<UploadBufferResource<ObjectConstants>>(device, objCount, true);
//	WavesVB = std::make_unique<UploadBufferResource<Vertex>>(device, waveVertCount, false);
//	MaterialCB = std::make_unique<UploadBufferResource<MaterialConstants>>(device, materialCount, true);
//}

FrameResource::~FrameResource() {}