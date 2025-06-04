#include "FrameResource.hpp"

FrameResource::FrameResource(ID3D12Device* device, UINT passCount, UINT objCount, UINT matCount, UINT skinnedObjectCount)
{
	ThrowIfFailed(device->CreateCommandAllocator(
		D3D12_COMMAND_LIST_TYPE_DIRECT,
		IID_PPV_ARGS(CmdListAlloc.GetAddressOf())));
	PassCB = std::make_unique<UploadBufferResource<PassConstants>>(device, passCount, true);
	//SsaoCB = std::make_unique<UploadBufferResource<SsaoConstants>>(device, 1, true);
	//ObjectCB = std::make_unique<UploadBufferResource<ObjectConstants>>(device, objCount, true);
	InstanceBuffer = std::make_unique<UploadBufferResource<InstanceData>>(device, objCount, false);
	MatSB = std::make_unique<UploadBufferResource<MaterialData>>(device, matCount, false);
	//SkinnedCB = std::make_unique<UploadBufferResource<SkinnedConstants>>(device, skinnedObjectCount, true);
}

FrameResource::~FrameResource() {}