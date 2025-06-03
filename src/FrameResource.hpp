#pragma once

#include "DXHelper.h"
#include "..\utils\MathHelper.h"
#include "UploadBufferResource.h"


//定义顶点结构体
struct Vertex
{
	DirectX::XMFLOAT3 Pos;
	//DirectX::XMFLOAT4 Color;
	DirectX::XMFLOAT3 Normal;
	DirectX::XMFLOAT2 TexC;
};

struct ObjectConstants {
	DirectX::XMFLOAT4X4 world = MathHelper::Identity4x4();
	DirectX::XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	UINT materialIndex = 0;
	UINT objPad0;
	UINT objPad1;
	UINT objPad2;
};

struct PassConstants {
	DirectX::XMFLOAT4X4 viewProj = MathHelper::Identity4x4();

	XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	float TotalTime = 0.0f;
	XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };
	Light lights[MaxLights];
};

struct MaterialData
{
	XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 64.0f;
	XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();

	UINT DiffuseMapIndex = 0;
	UINT MaterialPad0;
	UINT MaterialPad1;
	UINT MaterialPad2;
};

struct FrameResource {
public:
	FrameResource(ID3D12Device* device, UINT passCount, UINT objCount);
	FrameResource(ID3D12Device* device, UINT passCount, UINT objCount, UINT matCount);
	FrameResource(ID3D12Device* device, UINT passCount, UINT objCount, UINT waveVertCount, UINT materialCount);
	//禁用拷贝构造函数和赋值运算符，防止对象被意外拷贝，因为命令分配器是独占的，不能被多个对象共享
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator = (const FrameResource& rhs) = delete;
	~FrameResource();

	Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CmdListAlloc;
	std::unique_ptr<UploadBufferResource<ObjectConstants>> ObjectCB = nullptr;
	std::unique_ptr<UploadBufferResource<PassConstants>> PassCB = nullptr;
	//std::unique_ptr<UploadBufferResource<MaterialConstants>> MaterialCB = nullptr;
	std::unique_ptr<UploadBufferResource<MaterialData>> MatSB = nullptr;

	//std::unique_ptr<UploadBufferResource<Vertex>> WavesVB = nullptr;

	UINT64 FenceCPU = 0;
};
