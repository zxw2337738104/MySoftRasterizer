#pragma once

#include "DXHelper.h"
#include "..\utils\MathHelper.h"
#include "UploadBufferResource.h"


//定义顶点结构体
struct Vertex
{
	XMFLOAT3 Pos;
	XMFLOAT3 Normal;
	XMFLOAT2 TexC;
	XMFLOAT3 TangentU;
};

struct ObjectConstants {
	XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	UINT materialIndex = 0;
	UINT objPad0;
	UINT objPad1;
	UINT objPad2;
};

struct PassConstants {
	XMFLOAT4X4 View = MathHelper::Identity4x4();
	XMFLOAT4X4 InvView = MathHelper::Identity4x4();
	XMFLOAT4X4 Proj = MathHelper::Identity4x4();
	XMFLOAT4X4 InvProj = MathHelper::Identity4x4();
	XMFLOAT4X4 ViewProj = MathHelper::Identity4x4();
	XMFLOAT4X4 InvViewProj = MathHelper::Identity4x4();
	XMFLOAT4X4 ViewProjTex = MathHelper::Identity4x4();
	XMFLOAT3 EyePosW = { 0.0f, 0.0f, 0.0f };
	float PassConstantPad0;
	XMFLOAT2 RenderTargetSize = { 0.0f, 0.0f };
	XMFLOAT2 InvRenderTargetSize = { 0.0f, 0.0f };
	float NearZ = 0.0f;
	float FarZ = 0.0f;
	float TotalTime = 0.0f;
	float DeltaTime = 0.0f;
	XMFLOAT4X4 ShadowTransform = MathHelper::Identity4x4();
	XMFLOAT4 AmbientLight = { 0.0f, 0.0f, 0.0f, 1.0f };
	Light Lights[MaxLights];
};

struct MaterialData
{
	XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 64.0f;
	XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();

	UINT DiffuseMapIndex = 0;
	UINT NormalMapIndex = 0;
	UINT MaterialPad0;
	UINT MaterialPad1;
};

struct FrameResource {
public:
	FrameResource(ID3D12Device* device, UINT passCount, UINT objCount, UINT matCount, UINT skinnedObjectCount);
	//禁用拷贝构造函数和赋值运算符，防止对象被意外拷贝，因为命令分配器是独占的，不能被多个对象共享
	FrameResource(const FrameResource& rhs) = delete;
	FrameResource& operator = (const FrameResource& rhs) = delete;
	~FrameResource();

	ComPtr<ID3D12CommandAllocator> CmdListAlloc;
	std::unique_ptr<UploadBufferResource<ObjectConstants>> ObjectCB = nullptr;
	std::unique_ptr<UploadBufferResource<PassConstants>> PassCB = nullptr;
	//std::unique_ptr<UploadBufferResource<SsaoConstants>> SsaoCB = nullptr;
	std::unique_ptr<UploadBufferResource<MaterialData>> MatSB = nullptr;
	//std::unique_ptr<UploadBufferResource<SkinnedConstants>> SkinnedCB = nullptr;

	UINT64 FenceCPU = 0;
};