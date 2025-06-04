#include "D3D12App.h"
#include "FrameResource.hpp"
#include "MeshGeometry.hpp"
#include "GeometryGenerator.h"
#include "Camera.h"
#include "CreateDefaultBuffer.h"
#include "../utils/DDSTextureLoader.h"

const int gNumFrameResources = 3;

enum class RenderLayer
{
	Opaque = 0,
	Count
};

struct RenderItem
{
	RenderItem() = default;

	XMFLOAT4X4 World = MathHelper::Identity4x4();
	XMFLOAT4X4 TexTransform = MathHelper::Identity4x4();

	int NumFrameDirty = gNumFrameResources;

	UINT ObjCBIndex = -1;

	Material* Mat = nullptr;
	MeshGeometry* Geo = nullptr;

	std::vector<InstanceData> Instances;
	UINT InstanceCount = 0;

	D3D12_PRIMITIVE_TOPOLOGY PrimitiveType = D3D11_PRIMITIVE_TOPOLOGY_TRIANGLELIST;

	UINT IndexCount = 0;
	UINT StartIndexLocation = 0;
	UINT BaseVertexLocation = 0;

	//UINT SkinnedCBIndex = -1;
	//SkinnedModelInstance* SkinnedModelInst = nullptr;
};

class MySoftRasterizationApp : public D3D12App
{
public:
	MySoftRasterizationApp(HINSTANCE hInstance, int nShowCmd)
		: D3D12App(hInstance, nShowCmd) {
	}
	~MySoftRasterizationApp() {
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}
	virtual bool Init() override;
private:
	virtual void Draw() override;
	void BuildDescriptorHeaps();
	void BuildRootSignature();
	void BuildShadersAndInputLayout();
	void BuildPSOs();
	void BuildFrameResources();
	void BuildGeometry();
	void BuildMaterial();
	void BuildRenderItems();
	void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*> ritems);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

	void LoadTextures();
	void OnKeyboardInput(GameTime& gt);
	void UpdateCamera(GameTime& gt);
	virtual void Update(GameTime& gt) override;
	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;
	virtual void OnResize() override;
	//virtual void CreateDescriptorHeap() override;

	//void UpdateObjectCBs(GameTime& gt);
	void UpdateInstanceBuffers(GameTime& gt);
	void UpdateMainPassCBs();
	void UpdateMaterialCBs(GameTime& gt);

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;

	std::vector<D3D12_INPUT_ELEMENT_DESC> mInputLayout;
	std::vector<std::unique_ptr<RenderItem>> mAllRitems;
	std::vector<RenderItem*> mRitemLayer[(int)RenderLayer::Count];
	std::vector<std::unique_ptr<FrameResource>> mFrameResources;

	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, std::unique_ptr<MeshGeometry>> mGeometries;
	std::unordered_map<std::string, std::unique_ptr<Material>> mMaterials;
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;

	FrameResource* mCurrFrameResource = nullptr;
	int mCurrFrameResourceIndex = 0;

	UINT mInstanceCount = 0;

	PassConstants mMainPassCB;

	POINT mLastMousePos = { 0, 0 };
	float mTheta = 1.5f * XM_PI;
	float mPhi = XM_PIDIV4;
	float mRadius = 5.0f;
	int mLightsCount = 3;

	XMFLOAT3 mEyePos = { 0.0f, 0.0f, 0.0f };
	XMFLOAT4X4 mView = MathHelper::Identity4x4();
	XMFLOAT4X4 mProj = MathHelper::Identity4x4();

	Camera mCamera;

	UINT mImGuiSrvIndex = 0;
};

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE, LPSTR, int nShowCmd)
{
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif
	try
	{
		MySoftRasterizationApp theApp(hInstance, nShowCmd);
		if (!theApp.Init())
			return 0;
		return theApp.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}

bool MySoftRasterizationApp::Init()
{
	if (!D3D12App::Init())
		return false;

	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// 初始化 ImGui
	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO();
	io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard; // 启用键盘导航
	ImGui::StyleColorsDark(); // 设置 ImGui 主题

	mCamera.SetPosition(0.0f, 2.0f, -15.0f);

	LoadTextures();
	BuildDescriptorHeaps();
	BuildRootSignature();
	BuildShadersAndInputLayout();
	BuildGeometry();
	BuildMaterial();
	BuildRenderItems();
	BuildFrameResources();
	BuildPSOs();

	

	ThrowIfFailed(mCommandList->Close());
	// Execute the initialization commands
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCmdQueue();

	return true;
}

void MySoftRasterizationApp::BuildDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 7; // Adjust as needed
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));

	// 初始化 ImGui 的 Win32 和 DX12 后端
	ImGui_ImplWin32_Init(mhMainWnd);
	ImGui_ImplDX12_Init(
		md3dDevice.Get(),
		gNumFrameResources,
		DXGI_FORMAT_R8G8B8A8_UNORM, // 后台缓冲区格式
		mSrvDescriptorHeap.Get(),
		mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), // ImGui 的 SRV 句柄
		mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart()
	);

	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart(), 1, mCbv_srv_uavDescriptorSize);

	std::vector<ComPtr<ID3D12Resource>> tex2DList =
	{
		mTextures["bricksDiffuseMap"]->Resource,
		mTextures["bricksNormalMap"]->Resource,
		mTextures["tileDiffuseMap"]->Resource,
		mTextures["tileNormalMap"]->Resource,
		mTextures["defaultDiffuseMap"]->Resource,
		mTextures["defaultNormalMap"]->Resource
	};

	auto skyCubeMap = mTextures["skyCubeMap"]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURE2D;
	srvDesc.Texture2D.MostDetailedMip = 0;
	srvDesc.Texture2D.ResourceMinLODClamp = 0.0f;

	for (UINT i = 0; i < (UINT)tex2DList.size(); ++i)
	{
		srvDesc.Format = tex2DList[i]->GetDesc().Format;
		srvDesc.Texture2D.MipLevels = tex2DList[i]->GetDesc().MipLevels;
		md3dDevice->CreateShaderResourceView(tex2DList[i].Get(), &srvDesc, hDescriptor);

		hDescriptor.Offset(1, mCbv_srv_uavDescriptorSize);
	}
}

void MySoftRasterizationApp::BuildRootSignature()
{
	CD3DX12_ROOT_PARAMETER rootParameters[5];

	//SRV for IMGUI
	rootParameters[0].InitAsDescriptorTable(1, &CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0));
	//MainPassCB
	rootParameters[1].InitAsConstantBufferView(0);
	//ObjectCB
	//rootParameters[2].InitAsConstantBufferView(1);
	rootParameters[2].InitAsShaderResourceView(1, 1); // InstanceBuffer
	//MaterialSB
	rootParameters[3].InitAsShaderResourceView(0, 1);
	//SRV for Textures
	rootParameters[4].InitAsDescriptorTable(1, &CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 6, 1));

	auto staticSamplers = GetStaticSamplers();

	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(5, rootParameters, 
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf()));
	ThrowIfFailed(md3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
}

void MySoftRasterizationApp::BuildShadersAndInputLayout()
{
	mShaders["standardVS"] = CompileShader(L"shaders\\Standard.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["opaquePS"] = CompileShader(L"shaders\\Standard.hlsl", nullptr, "PS", "ps_5_1");

	mInputLayout = {
		{ "POSITION", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 0, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "NORMAL", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 12, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TEXCOORD", 0, DXGI_FORMAT_R32G32_FLOAT, 0, 24, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 },
		{ "TANGENT", 0, DXGI_FORMAT_R32G32B32_FLOAT, 0, 32, D3D12_INPUT_CLASSIFICATION_PER_VERTEX_DATA, 0 }
	};
}

void MySoftRasterizationApp::BuildPSOs()
{
	D3D12_GRAPHICS_PIPELINE_STATE_DESC opaquePsoDesc = {};
	opaquePsoDesc.InputLayout = { mInputLayout.data(), (UINT)mInputLayout.size() };
	opaquePsoDesc.pRootSignature = mRootSignature.Get();
	opaquePsoDesc.VS = { mShaders["standardVS"]->GetBufferPointer(), mShaders["standardVS"]->GetBufferSize() };
	opaquePsoDesc.PS = { mShaders["opaquePS"]->GetBufferPointer(), mShaders["opaquePS"]->GetBufferSize() };
	opaquePsoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	opaquePsoDesc.BlendState = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	opaquePsoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	opaquePsoDesc.SampleMask = UINT_MAX;
	opaquePsoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	opaquePsoDesc.NumRenderTargets = 1;
	opaquePsoDesc.RTVFormats[0] = mBackBufferFormat;
	opaquePsoDesc.DSVFormat = mDepthStencilFormat;
	opaquePsoDesc.SampleDesc.Count = 1;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&opaquePsoDesc, IID_PPV_ARGS(&mPSOs["opaque"])));


}

void MySoftRasterizationApp::BuildFrameResources()
{
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(
			md3dDevice.Get(), 1, mAllRitems[0]->Instances.size(), (UINT)mMaterials.size(), 0));
	}
}

void MySoftRasterizationApp::BuildGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.5f, 0.5f, 1.5f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateGeosphere(0.5f, 3);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);
	GeometryGenerator::MeshData quad = geoGen.CreateQuad(0.0f, 0.0f, 1.0f, 1.0f, 0.0f);

	//ÿȫµƫ
	UINT boxVertexOffset = 0;
	UINT gridVertexOffset = (UINT)box.Vertices.size();
	UINT sphereVertexOffset = (UINT)grid.Vertices.size() + gridVertexOffset;
	UINT cylinderVertexOffset = (UINT)sphere.Vertices.size() + sphereVertexOffset;
	UINT quadVertexOffset = (UINT)cylinder.Vertices.size() + cylinderVertexOffset;

	UINT boxIndexOffset = 0;
	UINT gridIndexOffset = (UINT)box.Indices32.size();
	UINT sphereIndexOffset = (UINT)grid.Indices32.size() + gridIndexOffset;
	UINT cylinderIndexOffset = (UINT)sphere.Indices32.size() + sphereIndexOffset;
	UINT quadIndexOffset = (UINT)cylinder.Indices32.size() + cylinderIndexOffset;

	SubmeshGeometry boxSubmesh;
	boxSubmesh.IndexCount = (UINT)box.Indices32.size();
	boxSubmesh.StartIndexLocation = boxIndexOffset;
	boxSubmesh.BaseVertexLocation = boxVertexOffset;

	SubmeshGeometry gridSubmesh;
	gridSubmesh.IndexCount = (UINT)grid.Indices32.size();
	gridSubmesh.StartIndexLocation = gridIndexOffset;
	gridSubmesh.BaseVertexLocation = gridVertexOffset;

	SubmeshGeometry sphereSubmesh;
	sphereSubmesh.IndexCount = (UINT)sphere.Indices32.size();
	sphereSubmesh.StartIndexLocation = sphereIndexOffset;
	sphereSubmesh.BaseVertexLocation = sphereVertexOffset;

	SubmeshGeometry cylinderSubmesh;
	cylinderSubmesh.IndexCount = (UINT)cylinder.Indices32.size();
	cylinderSubmesh.StartIndexLocation = cylinderIndexOffset;
	cylinderSubmesh.BaseVertexLocation = cylinderVertexOffset;

	SubmeshGeometry quadSubmesh;
	quadSubmesh.IndexCount = (UINT)quad.Indices32.size();
	quadSubmesh.BaseVertexLocation = quadVertexOffset;
	quadSubmesh.StartIndexLocation = quadIndexOffset;

	//ȫ�ֶ
	size_t totalVertexCount =
		box.Vertices.size() +
		grid.Vertices.size() +
		sphere.Vertices.size() +
		cylinder.Vertices.size() +
		quad.Vertices.size();
	std::vector<Vertex> vertices(totalVertexCount);

	UINT k = 0;
	for (size_t i = 0; i < box.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = box.Vertices[i].Position;
		//vertices[k].Color = XMFLOAT4(DirectX::Colors::DarkGreen);
		vertices[k].Normal = box.Vertices[i].Normal;
		vertices[k].TexC = box.Vertices[i].TexC;
		vertices[k].TangentU = box.Vertices[i].TangentU;
	}
	for (size_t i = 0; i < grid.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = grid.Vertices[i].Position;
		//vertices[k].Color = XMFLOAT4(DirectX::Colors::ForestGreen);
		vertices[k].Normal = grid.Vertices[i].Normal;
		vertices[k].TexC = grid.Vertices[i].TexC;
		vertices[k].TangentU = grid.Vertices[i].TangentU;
	}
	for (size_t i = 0; i < sphere.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = sphere.Vertices[i].Position;
		//vertices[k].Color = XMFLOAT4(DirectX::Colors::Crimson);
		vertices[k].Normal = sphere.Vertices[i].Normal;
		vertices[k].TexC = sphere.Vertices[i].TexC;
		vertices[k].TangentU = sphere.Vertices[i].TangentU;
	}
	for (size_t i = 0; i < cylinder.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = cylinder.Vertices[i].Position;
		//vertices[k].Color = XMFLOAT4(DirectX::Colors::SteelBlue);
		vertices[k].Normal = cylinder.Vertices[i].Normal;
		vertices[k].TexC = cylinder.Vertices[i].TexC;
		vertices[k].TangentU = sphere.Vertices[i].TangentU;
	}
	for (int i = 0; i < quad.Vertices.size(); ++i, ++k)
	{
		vertices[k].Pos = quad.Vertices[i].Position;
		vertices[k].Normal = quad.Vertices[i].Normal;
		vertices[k].TexC = quad.Vertices[i].TexC;
		vertices[k].TangentU = quad.Vertices[i].TangentU;
	}

	//ȫ
	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), box.GetIndices16().begin(), box.GetIndices16().end());
	indices.insert(indices.end(), grid.GetIndices16().begin(), grid.GetIndices16().end());
	indices.insert(indices.end(), sphere.GetIndices16().begin(), sphere.GetIndices16().end());
	indices.insert(indices.end(), cylinder.GetIndices16().begin(), cylinder.GetIndices16().end());
	indices.insert(indices.end(), quad.GetIndices16().begin(), quad.GetIndices16().end());

	//verticesindices�ֽڴС
	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	//MeshGeometryʵ
	auto mGeo = std::make_unique<MeshGeometry>();
	mGeo->Name = "shapeGeo";

	///ϴGPU
	ThrowIfFailed(D3DCreateBlob(vbByteSize, &mGeo->VertexBufferCPU));
	ThrowIfFailed(D3DCreateBlob(ibByteSize, &mGeo->IndexBufferCPU));
	CopyMemory(mGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
	CopyMemory(mGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);
	mGeo->VertexBufferGPU = CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), vertices.data(), vbByteSize, mGeo->VertexBufferUploader);
	mGeo->IndexBufferGPU = CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), indices.data(), ibByteSize, mGeo->IndexBufferUploader);

	//MeshGeometryʵ
	mGeo->VertexByteStride = sizeof(Vertex);
	mGeo->VertexBufferByteSize = vbByteSize;
	mGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
	mGeo->IndexBufferByteSize = ibByteSize;

	//
	mGeo->DrawArgs["box"] = boxSubmesh;
	mGeo->DrawArgs["grid"] = gridSubmesh;
	mGeo->DrawArgs["sphere"] = sphereSubmesh;
	mGeo->DrawArgs["cylinder"] = cylinderSubmesh;
	mGeo->DrawArgs["quad"] = quadSubmesh;

	mGeometries[mGeo->Name] = std::move(mGeo);
}

void MySoftRasterizationApp::BuildMaterial()
{
	auto bricks0 = std::make_unique<Material>();
	bricks0->Name = "bricks0";
	bricks0->MatCBIndex = 0;
	bricks0->DiffuseSrvHeapIndex = 0;
	bricks0->NormalSrvHeapIndex = 1;
	bricks0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	bricks0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	bricks0->Roughness = 0.8f;

	auto tile0 = std::make_unique<Material>();
	tile0->Name = "tile0";
	tile0->MatCBIndex = 1;
	tile0->DiffuseSrvHeapIndex = 2;
	tile0->NormalSrvHeapIndex = 3;
	tile0->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	tile0->FresnelR0 = XMFLOAT3(0.02f, 0.02f, 0.02f);
	tile0->Roughness = 0.8f;

	auto white1x1 = std::make_unique<Material>();
	white1x1->Name = "white1x1";
	white1x1->MatCBIndex = 2;
	white1x1->DiffuseSrvHeapIndex = 4;
	white1x1->NormalSrvHeapIndex = 5;
	white1x1->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	white1x1->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	white1x1->Roughness = 0.2f;

	mMaterials["bricks0"] = std::move(bricks0);
	mMaterials["tile0"] = std::move(tile0);
	mMaterials["white1x1"] = std::move(white1x1);
}

void MySoftRasterizationApp::BuildRenderItems()
{
	auto sphereRitem = std::make_unique<RenderItem>();
	sphereRitem->ObjCBIndex = 0;
	sphereRitem->Geo = mGeometries["shapeGeo"].get();
	sphereRitem->IndexCount = sphereRitem->Geo->DrawArgs["sphere"].IndexCount;
	sphereRitem->StartIndexLocation = sphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	sphereRitem->BaseVertexLocation = sphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;

	sphereRitem->InstanceCount = 0;
	sphereRitem->Instances.resize(1);
	sphereRitem->Instances[0].World = MathHelper::Identity4x4();
	//sphereRitem->Instances[0].TexTransform = MathHelper::Identity4x4();
	XMStoreFloat4x4(&sphereRitem->Instances[0].TexTransform, XMMatrixScaling(3.0f, 3.0f, 3.0f));
	sphereRitem->Instances[0].MaterialIndex = 1;

	mRitemLayer[(int)RenderLayer::Opaque].push_back(sphereRitem.get());

	mAllRitems.push_back(std::move(sphereRitem));
}

void MySoftRasterizationApp::DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*> ritems)
{
	//UINT objConstSize = CalcConstantBufferByteSize(sizeof(ObjectConstants));

	//auto objCB = mCurrFrameResource->ObjectCB->Resource();

	for (size_t i = 0; i < ritems.size(); ++i)
	{
		auto ri = ritems[i];

		cmdList->IASetVertexBuffers(0, 1, &ri->Geo->VertexBufferView());
		cmdList->IASetIndexBuffer(&ri->Geo->IndexBufferView());
		cmdList->IASetPrimitiveTopology(ri->PrimitiveType);

		//D3D12_GPU_VIRTUAL_ADDRESS objCBAddress = objCB->GetGPUVirtualAddress() + ri->ObjCBIndex * objConstSize;

		auto instanceBuffer = mCurrFrameResource->InstanceBuffer->Resource();

		//cmdList->SetGraphicsRootConstantBufferView(2, objCBAddress);
		cmdList->SetGraphicsRootShaderResourceView(2, instanceBuffer->GetGPUVirtualAddress());

		cmdList->DrawIndexedInstanced(
			ri->IndexCount, // Index count per instance
			ri->InstanceCount,      // Instance count
			ri->StartIndexLocation, // Start index location
			ri->BaseVertexLocation,  // Base vertex location
			0);             // Instance start offset
	}
}

void MySoftRasterizationApp::Draw()
{
	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(mDirectCmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	//设置根签名
	mCommandList->SetGraphicsRootSignature(mRootSignature.Get());

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

	auto matSB = mCurrFrameResource->MatSB->Resource();
	mCommandList->SetGraphicsRootShaderResourceView(3, matSB->GetGPUVirtualAddress());

	CD3DX12_GPU_DESCRIPTOR_HANDLE texDescriptor(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), 1, mCbv_srv_uavDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(4, texDescriptor);

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	// Set the viewport and scissor rect.  This needs to be reset whenever the command list is reset.
	mCommandList->RSSetViewports(1, &viewPort);
	mCommandList->RSSetScissorRects(1, &scissorRect);

	// Clear the back buffer and depth buffer.
	mCommandList->ClearRenderTargetView(CurrentBackBufferView(), Colors::LightSteelBlue, 0, nullptr);
	mCommandList->ClearDepthStencilView(DepthStencilView(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

	// Specify the buffers we are going to render to.
	mCommandList->OMSetRenderTargets(1, &CurrentBackBufferView(), true, &DepthStencilView());

	mCommandList->SetPipelineState(mPSOs["opaque"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	// 渲染 ImGui
	ImGui_ImplDX12_RenderDrawData(ImGui::GetDrawData(), mCommandList.Get());

	// Indicate a state transition on the resource usage.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));

	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// swap the back and front buffers
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrentBackBuffer = (mCurrentBackBuffer + 1) % SwapChainBufferCount;

	// Wait until frame commands are complete.  This waiting is inefficient and is
	// done for simplicity.  Later we will show how to organize our rendering code
	// so we do not have to wait per frame.
	FlushCmdQueue();
}

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> MySoftRasterizationApp::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}

void MySoftRasterizationApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	if (ImGui::GetIO().WantCaptureMouse)
		return; // ImGui 捕获鼠标时，跳过相机控制

	mLastMousePos.x = x;
	mLastMousePos.y = y;
	SetCapture(mhMainWnd);
}

void MySoftRasterizationApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	if (ImGui::GetIO().WantCaptureMouse)
		return; // ImGui 捕获鼠标时，跳过相机控制

	ReleaseCapture();
}

void MySoftRasterizationApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if (ImGui::GetIO().WantCaptureMouse)
		return; // ImGui 捕获鼠标时，跳过相机控制

	if ((btnState & MK_LBUTTON) != 0)
	{
		float dx = XMConvertToRadians(0.25f * static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f * static_cast<float>(y - mLastMousePos.y));
		mCamera.Pitch(dy);
		mCamera.RotateY(dx);
		mCamera.UpdateViewMatrix();
	}
	else if ((btnState & MK_RBUTTON) != 0)
	{
		float dx = 0.05f * static_cast<float>(x - mLastMousePos.x);
		float dy = 0.05f * static_cast<float>(y - mLastMousePos.y);
		mCamera.Zoom(dx - dy);
		mCamera.UpdateViewMatrix();
	}
	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void MySoftRasterizationApp::OnResize()
{
	D3D12App::OnResize();

	mCamera.SetLens(0.25 * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
}

void MySoftRasterizationApp::Update(GameTime& gt)
{
	OnKeyboardInput(gt);
	// ImGui 新帧
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// 示例 ImGui 窗口
	ImGui::Begin("Debug Window");
	ImGui::SliderInt("Light Count", &mLightsCount, 1, 3);
	ImGui::End();

	//UpdateCamera(gt);
	mCurrFrameResourceIndex = (mCurrFrameResourceIndex + 1) % gNumFrameResources;
	mCurrFrameResource = mFrameResources[mCurrFrameResourceIndex].get();
	if (mCurrFrameResource->FenceCPU != 0 && mFence->GetCompletedValue() < mCurrFrameResource->FenceCPU)
	{
		HANDLE eventHandle = CreateEvent(nullptr,
			false,
			false,
			L"FenceSetDone");
		ThrowIfFailed(mFence->SetEventOnCompletion(mCurrFrameResource->FenceCPU, eventHandle));
		WaitForSingleObject(eventHandle, INFINITE);
		CloseHandle(eventHandle);
	}
	mCamera.UpdateViewMatrix();

	UpdateMainPassCBs();
	//UpdateObjectCBs(gt);
	UpdateInstanceBuffers(gt);
	UpdateMaterialCBs(gt);
	// 渲染 ImGui
	ImGui::Render();
}

void MySoftRasterizationApp::UpdateCamera(GameTime& gt)
{
	// Convert Spherical to Cartesian coordinates.
	mEyePos.x = mRadius * sinf(mPhi) * cosf(mTheta);
	mEyePos.z = mRadius * sinf(mPhi) * sinf(mTheta);
	mEyePos.y = mRadius * cosf(mPhi);

	// Build the view matrix.
	XMVECTOR pos = XMVectorSet(mEyePos.x, mEyePos.y, mEyePos.z, 1.0f);
	XMVECTOR target = XMVectorZero();
	XMVECTOR up = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);

	XMMATRIX view = XMMatrixLookAtLH(pos, target, up);
	XMStoreFloat4x4(&mView, view);
}

void MySoftRasterizationApp::UpdateMainPassCBs()
{
	XMMATRIX view = mCamera.GetView();
	XMMATRIX proj = mCamera.GetProj();
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f
	);
	XMMATRIX viewProjTex = XMMatrixMultiply(viewProj, T);
	//XMMATRIX shadowTransform = XMLoadFloat4x4(&mShadowTransform);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProjTex, XMMatrixTranspose(viewProjTex));
	//XMStoreFloat4x4(&mMainPassCB.ShadowTransform, XMMatrixTranspose(shadowTransform));

	mMainPassCB.RenderTargetSize = XMFLOAT2((float)mClientWidth, (float)mClientHeight);
	mMainPassCB.InvRenderTargetSize = XMFLOAT2(1.0f / mClientWidth, 1.0f / mClientHeight);
	mMainPassCB.NearZ = 1.0f;
	mMainPassCB.FarZ = 1000.0f;
	mMainPassCB.EyePosW = mCamera.GetPosition3f();
	mMainPassCB.AmbientLight = { 0.25f, 0.25f, 0.35f, 1.0f };

	memset(mMainPassCB.Lights, 0, sizeof(mMainPassCB.Lights));

	//㲼
	mMainPassCB.Lights[0].Direction = { 0.57735f, -0.57735f, 0.57735f };
	mMainPassCB.Lights[0].Strength = { 0.6f, 0.6f, 0.6f };
	if (mLightsCount >= 2) {
		mMainPassCB.Lights[1].Direction = { -0.57735f, -0.57735f, 0.57735f };
		mMainPassCB.Lights[1].Strength = { 0.3f, 0.3f, 0.3f };
	}
	if (mLightsCount == 3) {
		mMainPassCB.Lights[2].Direction = { 0.0f, -0.707f, -0.707f };
		mMainPassCB.Lights[2].Strength = { 0.15f, 0.15f, 0.15f };
	}
	mMainPassCB.TotalTime = gt.TotalTime();
	mMainPassCB.DeltaTime = gt.DeltaTime();
	// 更新常量缓冲区
	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(0, mMainPassCB);
}

//void MySoftRasterizationApp::UpdateObjectCBs(GameTime& gt)
//{
//	auto currObjCB = mCurrFrameResource->ObjectCB.get();
//	for (auto& e : mAllRitems)
//	{
//		if (e->NumFrameDirty > 0)
//		{
//			XMMATRIX world = XMLoadFloat4x4(&e->World);
//			XMMATRIX texTransform = XMLoadFloat4x4(&e->TexTransform);
//			ObjectConstants objConstants;
//			XMStoreFloat4x4(&objConstants.World, XMMatrixTranspose(world));
//			XMStoreFloat4x4(&objConstants.TexTransform, XMMatrixTranspose(texTransform));
//			objConstants.materialIndex = e->Mat->MatCBIndex;
//			currObjCB->CopyData(e->ObjCBIndex, objConstants);
//			e->NumFrameDirty--;
//		}
//	}
//}

void MySoftRasterizationApp::UpdateInstanceBuffers(GameTime& gt)
{
	auto currInstanceBuffer = mCurrFrameResource->InstanceBuffer.get();
	for (auto& e : mAllRitems)
	{
		const auto& instanceData = e->Instances;
		int instanceCount = 0;
		for (UINT i = 0; i < (UINT)instanceData.size(); ++i)
		{
			InstanceData data;
			XMStoreFloat4x4(&data.World, XMMatrixTranspose(XMLoadFloat4x4(&instanceData[i].World)));
			XMStoreFloat4x4(&data.TexTransform, XMMatrixTranspose(XMLoadFloat4x4(&instanceData[i].TexTransform)));
			data.MaterialIndex = instanceData[i].MaterialIndex;

			currInstanceBuffer->CopyData(instanceCount++, data);
		}
		e->InstanceCount = instanceCount;
	}
}

void MySoftRasterizationApp::UpdateMaterialCBs(GameTime& gt)
{
	auto currMatSB = mCurrFrameResource->MatSB.get();
	for (auto& e : mMaterials)
	{
		Material* mat = e.second.get();
		if (mat->NumFramesDirty > 0)
		{
			MaterialData matData;
			matData.DiffuseAlbedo = mat->DiffuseAlbedo;
			matData.FresnelR0 = mat->FresnelR0;
			matData.Roughness = mat->Roughness;
			XMStoreFloat4x4(&matData.MatTransform, XMMatrixTranspose(XMLoadFloat4x4(&mat->MatTransform)));
			matData.DiffuseMapIndex = mat->DiffuseSrvHeapIndex;
			matData.NormalMapIndex = mat->NormalSrvHeapIndex;

			currMatSB->CopyData(mat->MatCBIndex, matData);
			mat->NumFramesDirty--;
		}
	}
}

void MySoftRasterizationApp::OnKeyboardInput(GameTime& gt)
{
	if (ImGui::GetIO().WantCaptureKeyboard)
		return; // ImGui 捕获键盘时，跳过相机控制
	if (GetAsyncKeyState('W') & 0x8000)
		mCamera.Walk(10.0f * gt.DeltaTime());
	if (GetAsyncKeyState('S') & 0x8000)
		mCamera.Walk(-10.0f * gt.DeltaTime());
	if (GetAsyncKeyState('A') & 0x8000)
		mCamera.Strafe(-10.0f * gt.DeltaTime());
	if (GetAsyncKeyState('D') & 0x8000)
		mCamera.Strafe(10.0f * gt.DeltaTime());
	if (GetAsyncKeyState(VK_UP) & 0x8000)
		mCamera.Pitch(XMConvertToRadians(-90.0f * gt.DeltaTime()));
	if (GetAsyncKeyState(VK_DOWN) & 0x8000)
		mCamera.Pitch(XMConvertToRadians(90.0f * gt.DeltaTime()));
	if (GetAsyncKeyState(VK_LEFT) & 0x8000)
		mCamera.RotateY(XMConvertToRadians(-90.0f * gt.DeltaTime()));
	if (GetAsyncKeyState(VK_RIGHT) & 0x8000)
		mCamera.RotateY(XMConvertToRadians(90.0f * gt.DeltaTime()));
}

void MySoftRasterizationApp::LoadTextures()
{
	std::vector<std::string> texNames =
	{
		"bricksDiffuseMap",
		"bricksNormalMap",
		"tileDiffuseMap",
		"tileNormalMap",
		"defaultDiffuseMap",
		"defaultNormalMap",
		"skyCubeMap"
	};

	std::vector<std::wstring> texFilenames =
	{
		L"D:\\DX12\\d3d12book\\Textures\\bricks2.dds",
		L"D:\\DX12\\d3d12book\\Textures\\bricks2_nmap.dds",
		L"D:\\DX12\\d3d12book\\Textures\\tile.dds",
		L"D:\\DX12\\d3d12book\\Textures\\tile_nmap.dds",
		L"D:\\DX12\\d3d12book\\Textures\\white1x1.dds",
		L"D:\\DX12\\d3d12book\\Textures\\default_nmap.dds",
		L"D:\\DX12\\d3d12book\\Textures\\sunsetcube1024.dds"
	};

	for (int i = 0; i < (int)texNames.size(); ++i)
	{
		auto texMap = std::make_unique<Texture>();
		texMap->Name = texNames[i];
		texMap->Filename = texFilenames[i];
		ThrowIfFailed(CreateDDSTextureFromFile12(md3dDevice.Get(),
			mCommandList.Get(), texMap->Filename.c_str(),
			texMap->Resource, texMap->UploadHeap));

		mTextures[texMap->Name] = std::move(texMap);
	}
}