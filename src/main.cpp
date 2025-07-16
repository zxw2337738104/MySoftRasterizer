#include "D3D12App.h"
#include "FrameResource.hpp"
#include "MeshGeometry.hpp"
#include "GeometryGenerator.h"
#include "Camera.h"
#include "CreateDefaultBuffer.h"
#include "CubeRenderTarget.h"
#include "ShadowMap.h"
#include "../utils/DDSTextureLoader.h"

const int gNumFrameResources = 3;

const UINT CubeMapSize = 512;

enum class RenderLayer
{
	Opaque = 0,
	WithoutNormalMap,
	AlphaTested,
	Transparent,
	Sky,
	OpaqueDynamicReflectors,
	Debug,
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
	UINT InstanceBufferIndex = 0; // Instance buffer index in the FrameResource

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
		mSceneBounds.Center = XMFLOAT3(0.0f, 0.0f, 0.0f);
		mSceneBounds.Radius = sqrtf(10.0f * 10.0f + 15.0f * 15.0f);
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

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> GetStaticSamplers();

	void LoadTextures();
	void OnKeyboardInput(GameTime& gt);
	void UpdateCamera(GameTime& gt);
	virtual void Update(GameTime& gt) override;
	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;
	virtual void OnResize() override;

	//void UpdateObjectCBs(GameTime& gt);
	void UpdateInstanceBuffers(GameTime& gt);
	void UpdateMainPassCBs();
	void UpdateMaterialCBs(GameTime& gt);
	void UpdateCubeMapFacePassCBs();
	void UpdateShadowTransform();
	void UpdateShadowPassCBs();

	virtual void CreateDescriptorHeap() override;

	void BuildCubeDepthStencil();
	void BuildCubeMapCamera(float x, float y, float z);
	void DrawSceneToCubeMap();
	void DrawSceneToShadowMap();

	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;
	ComPtr<ID3D12Resource> mCubeDepthStencilBuffer = nullptr;

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
	Camera mCubeMapCamera[6];

	UINT mImGuiSrvIndex = 0;
	UINT mSkyTexSrvIndex = 0;
	UINT mDynamicSrvIndex = 0;
	UINT mShadowMapSrvIndex = 0;

	std::unique_ptr<CubeRenderTarget> mDynamicCubeMap = nullptr;
	CD3DX12_CPU_DESCRIPTOR_HANDLE mCubeDSV;

	BoundingSphere mSceneBounds;
	float mLightNearZ = 0.0f;
	float mLightFarZ = 0.0f;
	XMFLOAT3 mLightPosW;
	XMFLOAT4X4 mLightView = MathHelper::Identity4x4();
	XMFLOAT4X4 mLightProj = MathHelper::Identity4x4();
	XMFLOAT4X4 mShadowTransform = MathHelper::Identity4x4();
	float mLightRotationAngle = 0.0f;
	XMFLOAT3 mBaseLightDirections[3] = {
		XMFLOAT3(0.57735f, -0.57735f, 0.57735f), // Light 1
		XMFLOAT3(-0.57735f, -0.57735f, 0.57735f), // Light 2
		XMFLOAT3(0.0f, -0.7071f, -0.7071f) // Light 3
	};
	XMFLOAT3 mRotatedLightDirections[3];

	std::unique_ptr<ShadowMap> mShadowMap = nullptr;
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

	BuildCubeMapCamera(8.0f, 0.0f, 0.0f);

	mDynamicCubeMap = std::make_unique<CubeRenderTarget>(md3dDevice.Get(),
		CubeMapSize, CubeMapSize, DXGI_FORMAT_R8G8B8A8_UNORM);

	mShadowMap = std::make_unique<ShadowMap>(md3dDevice.Get(), 2048, 2048);

	LoadTextures();
	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildShadersAndInputLayout();
	BuildGeometry();
	BuildMaterial();
	BuildRenderItems();
	BuildFrameResources();
	BuildCubeDepthStencil();
	BuildPSOs();

	

	ThrowIfFailed(mCommandList->Close());
	// Execute the initialization commands
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCmdQueue();

	return true;
}

void MySoftRasterizationApp::CreateDescriptorHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = SwapChainBufferCount + 6; // 6 for the cube map faces
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&mRtvHeap)));

	D3D12_DESCRIPTOR_HEAP_DESC dsvHeapDesc = {};
	dsvHeapDesc.NumDescriptors = 3;
	dsvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&dsvHeapDesc, IID_PPV_ARGS(&mDsvHeap)));

	mCubeDSV = CD3DX12_CPU_DESCRIPTOR_HANDLE(
		mDsvHeap->GetCPUDescriptorHandleForHeapStart(),
		1,
		mDsvDescriptorSize
	);
}

void MySoftRasterizationApp::BuildDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 12; // Adjust as needed
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
		mTextures["defaultNormalMap"]->Resource,
		mTextures["wireFenceDiffuseMap"]->Resource,
		mTextures["waterDiffuseMap"]->Resource,
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

	// Create SRV for the sky cube map
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.Format = skyCubeMap->GetDesc().Format;
	srvDesc.Texture2D.MipLevels = skyCubeMap->GetDesc().MipLevels;
	md3dDevice->CreateShaderResourceView(skyCubeMap.Get(), &srvDesc, hDescriptor);
	mSkyTexSrvIndex = tex2DList.size() + 1; // Sky texture is at the end of the heap

	mDynamicSrvIndex = mSkyTexSrvIndex + 1; // Dynamic texture will be at the next index
	auto srvCpuStart = mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart();
	auto srvGpuStart = mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart();
	auto rtvCpuStart = mRtvHeap->GetCPUDescriptorHandleForHeapStart();

	int rtvOffset = SwapChainBufferCount;

	CD3DX12_CPU_DESCRIPTOR_HANDLE cubeRtvHandles[6];
	for (int i = 0; i < 6; ++i)
	{
		cubeRtvHandles[i] = CD3DX12_CPU_DESCRIPTOR_HANDLE(
			rtvCpuStart,
			rtvOffset + i,
			mRtvDescriptorSize);
	}

	mDynamicCubeMap->BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, mDynamicSrvIndex, mCbv_srv_uavDescriptorSize),
		CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, mDynamicSrvIndex, mCbv_srv_uavDescriptorSize),
		cubeRtvHandles);

	mShadowMapSrvIndex = mDynamicSrvIndex + 1;
	mShadowMap->BuildDescriptors(
		CD3DX12_CPU_DESCRIPTOR_HANDLE(srvCpuStart, mShadowMapSrvIndex, mCbv_srv_uavDescriptorSize),
		CD3DX12_GPU_DESCRIPTOR_HANDLE(srvGpuStart, mShadowMapSrvIndex, mCbv_srv_uavDescriptorSize),
		CD3DX12_CPU_DESCRIPTOR_HANDLE(mDsvHeap->GetCPUDescriptorHandleForHeapStart(), 2, mDsvDescriptorSize)
	);
}

void MySoftRasterizationApp::BuildCubeDepthStencil()
{
	D3D12_RESOURCE_DESC depthStencilDesc;
	depthStencilDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	depthStencilDesc.Alignment = 0;
	depthStencilDesc.Width = CubeMapSize;
	depthStencilDesc.Height = CubeMapSize;
	depthStencilDesc.DepthOrArraySize = 1;
	depthStencilDesc.MipLevels = 1;
	depthStencilDesc.Format = mDepthStencilFormat;
	depthStencilDesc.SampleDesc.Count = 1;
	depthStencilDesc.SampleDesc.Quality = 0;
	depthStencilDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	depthStencilDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;

	D3D12_CLEAR_VALUE optClear;
	optClear.Format = mDepthStencilFormat;
	optClear.DepthStencil.Depth = 1.0f;
	optClear.DepthStencil.Stencil = 0;

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&depthStencilDesc,
		D3D12_RESOURCE_STATE_COMMON,
		&optClear,
		IID_PPV_ARGS(mCubeDepthStencilBuffer.GetAddressOf())
	));

	md3dDevice->CreateDepthStencilView(mCubeDepthStencilBuffer.Get(), nullptr, mCubeDSV);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		mCubeDepthStencilBuffer.Get(), D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));
}

void MySoftRasterizationApp::BuildCubeMapCamera(float x, float y, float z)
{
	XMFLOAT3 center(x, y, z);
	XMFLOAT3 worldUp(0.0f, 1.0f, 0.0f);

	XMFLOAT3 targets[6] =
	{
		XMFLOAT3(x + 1.0f, y, z), // +X
		XMFLOAT3(x - 1.0f, y, z), // -X
		XMFLOAT3(x, y + 1.0f, z), // +Y
		XMFLOAT3(x, y - 1.0f, z), // -Y
		XMFLOAT3(x, y, z + 1.0f), // +Z
		XMFLOAT3(x, y, z - 1.0f)  // -Z
	};

	XMFLOAT3 ups[6] =
	{
		XMFLOAT3(0.0f, 1.0f, 0.0f), // +X
		XMFLOAT3(0.0f, 1.0f, 0.0f), // -X
		XMFLOAT3(0.0f, 0.0f, -1.0f), // +Y
		XMFLOAT3(0.0f, 0.0f, 1.0f), // -Y
		XMFLOAT3(0.0f, 1.0f, 0.0f), // +Z
		XMFLOAT3(0.0f, 1.0f, 0.0f)  // -Z
	};

	for (int i = 0; i < 6; ++i)
	{
		mCubeMapCamera[i].SetLens(XM_PI / 2.0f, 1.0f, 0.1f, 100.0f);
		mCubeMapCamera[i].LookAt(center, targets[i], ups[i]);
		mCubeMapCamera[i].UpdateViewMatrix();
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
	// InstanceBuffer
	rootParameters[2].InitAsShaderResourceView(1, 1);
	//MaterialSB
	rootParameters[3].InitAsShaderResourceView(0, 1);
	//SRV for Textures
	rootParameters[4].InitAsDescriptorTable(1, &CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 11, 1));

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
	const D3D_SHADER_MACRO alphaTestedDefines[] =
	{
		"ALPHA_TEST","1",
		NULL, NULL
	};

	mShaders["standardVS"] = CompileShader(L"shaders\\Standard.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["opaquePS"] = CompileShader(L"shaders\\Standard.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["withoutNormalMapPS"] = CompileShader(L"shaders\\WithoutNormalMap.hlsl", nullptr, "PS", "ps_5_1");
	mShaders["alphaTestedPS"] = CompileShader(L"shaders\\Standard.hlsl", alphaTestedDefines, "PS", "ps_5_1");

	mShaders["skyVS"] = CompileShader(L"shaders\\Sky.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["skyPS"] = CompileShader(L"shaders\\Sky.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["shadowVS"] = CompileShader(L"shaders\\ShadowMap.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["shadowPS"] = CompileShader(L"shaders\\ShadowMap.hlsl", nullptr, "PS", "ps_5_1");

	mShaders["debugVS"] = CompileShader(L"shaders\\ShadowDebug.hlsl", nullptr, "VS", "vs_5_1");
	mShaders["debugPS"] = CompileShader(L"shaders\\ShadowDebug.hlsl", nullptr, "PS", "ps_5_1");

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
	opaquePsoDesc.VS = { reinterpret_cast<BYTE*>(mShaders["standardVS"]->GetBufferPointer()), mShaders["standardVS"]->GetBufferSize() };
	opaquePsoDesc.PS = { reinterpret_cast<BYTE*>(mShaders["opaquePS"]->GetBufferPointer()), mShaders["opaquePS"]->GetBufferSize() };
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

	D3D12_GRAPHICS_PIPELINE_STATE_DESC withoutNormalMapPsoDesc = opaquePsoDesc;
	withoutNormalMapPsoDesc.PS = { reinterpret_cast<BYTE*>(mShaders["withoutNormalMapPS"]->GetBufferPointer()), mShaders["withoutNormalMapPS"]->GetBufferSize() };
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&withoutNormalMapPsoDesc, IID_PPV_ARGS(&mPSOs["withoutNormalMap"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC alphaTestedPsoDesc = opaquePsoDesc;
	alphaTestedPsoDesc.PS = { reinterpret_cast<BYTE*>(mShaders["alphaTestedPS"]->GetBufferPointer()), mShaders["alphaTestedPS"]->GetBufferSize() };
	alphaTestedPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_NONE; // Disable culling for alpha tested objects
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&alphaTestedPsoDesc, IID_PPV_ARGS(&mPSOs["alphaTested"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC transparentPsoDesc = opaquePsoDesc;
	D3D12_RENDER_TARGET_BLEND_DESC transparentBlendDesc;
	transparentBlendDesc.BlendEnable = true;
	transparentBlendDesc.LogicOpEnable = false;
	transparentBlendDesc.SrcBlend = D3D12_BLEND_SRC_ALPHA;
	transparentBlendDesc.DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
	transparentBlendDesc.BlendOp = D3D12_BLEND_OP_ADD;
	transparentBlendDesc.SrcBlendAlpha = D3D12_BLEND_ONE;
	transparentBlendDesc.DestBlendAlpha = D3D12_BLEND_ZERO;
	transparentBlendDesc.BlendOpAlpha = D3D12_BLEND_OP_ADD;
	transparentBlendDesc.LogicOp = D3D12_LOGIC_OP_NOOP;
	transparentBlendDesc.RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;

	transparentPsoDesc.BlendState.RenderTarget[0] = transparentBlendDesc;
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&transparentPsoDesc, IID_PPV_ARGS(&mPSOs["transparent"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC skyPsoDesc = opaquePsoDesc;
	skyPsoDesc.VS = { reinterpret_cast<BYTE*>(mShaders["skyVS"]->GetBufferPointer()), mShaders["skyVS"]->GetBufferSize() };
	skyPsoDesc.PS = { reinterpret_cast<BYTE*>(mShaders["skyPS"]->GetBufferPointer()), mShaders["skyPS"]->GetBufferSize() };
	skyPsoDesc.RasterizerState.CullMode = D3D12_CULL_MODE_FRONT; // Skybox is rendered with front culling
	skyPsoDesc.DepthStencilState.DepthFunc = D3D12_COMPARISON_FUNC_LESS_EQUAL; // Skybox depth test
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&skyPsoDesc, IID_PPV_ARGS(&mPSOs["sky"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC shadowPsoDesc = opaquePsoDesc;
	shadowPsoDesc.VS = { reinterpret_cast<BYTE*>(mShaders["shadowVS"]->GetBufferPointer()), mShaders["shadowVS"]->GetBufferSize() };
	shadowPsoDesc.PS = { reinterpret_cast<BYTE*>(mShaders["shadowPS"]->GetBufferPointer()), mShaders["shadowPS"]->GetBufferSize() };
	shadowPsoDesc.RasterizerState.DepthBias = 100000;
	shadowPsoDesc.RasterizerState.DepthBiasClamp = 0.0f;
	shadowPsoDesc.RasterizerState.SlopeScaledDepthBias = 1.0f;
	shadowPsoDesc.RTVFormats[0] = DXGI_FORMAT_UNKNOWN; // No render target for shadow map
	shadowPsoDesc.NumRenderTargets = 0; // No render targets for shadow map
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&shadowPsoDesc, IID_PPV_ARGS(&mPSOs["shadow"])));

	D3D12_GRAPHICS_PIPELINE_STATE_DESC debugPsoDesc = opaquePsoDesc;
	debugPsoDesc.VS = { reinterpret_cast<BYTE*>(mShaders["debugVS"]->GetBufferPointer()), mShaders["debugVS"]->GetBufferSize() };
	debugPsoDesc.PS = { reinterpret_cast<BYTE*>(mShaders["debugPS"]->GetBufferPointer()), mShaders["debugPS"]->GetBufferSize() };
	ThrowIfFailed(md3dDevice->CreateGraphicsPipelineState(&debugPsoDesc, IID_PPV_ARGS(&mPSOs["debug"])));
}

void MySoftRasterizationApp::BuildFrameResources()
{
	UINT InstancesSize = 0;
	for (const auto& item : mAllRitems)
	{
		InstancesSize += (UINT)item->Instances.size();
	}
	for (int i = 0; i < gNumFrameResources; ++i)
	{
		mFrameResources.push_back(std::make_unique<FrameResource>(
			md3dDevice.Get(), 1 + 6 + 1, InstancesSize, (UINT)mMaterials.size(), 0));
	}
}

void MySoftRasterizationApp::BuildGeometry()
{
	GeometryGenerator geoGen;
	GeometryGenerator::MeshData box = geoGen.CreateBox(1.5f, 0.5f, 1.5f, 3);
	GeometryGenerator::MeshData grid = geoGen.CreateGrid(20.0f, 30.0f, 60, 40);
	GeometryGenerator::MeshData sphere = geoGen.CreateGeosphere(0.5f, 3);
	GeometryGenerator::MeshData cylinder = geoGen.CreateCylinder(0.5f, 0.3f, 3.0f, 20, 20);
	GeometryGenerator::MeshData quad = geoGen.CreateQuad(0.2f, -0.2f, 0.8f, 0.8f, 0.0f);

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

	std::vector<std::uint16_t> indices;
	indices.insert(indices.end(), box.GetIndices16().begin(), box.GetIndices16().end());
	indices.insert(indices.end(), grid.GetIndices16().begin(), grid.GetIndices16().end());
	indices.insert(indices.end(), sphere.GetIndices16().begin(), sphere.GetIndices16().end());
	indices.insert(indices.end(), cylinder.GetIndices16().begin(), cylinder.GetIndices16().end());
	indices.insert(indices.end(), quad.GetIndices16().begin(), quad.GetIndices16().end());

	//verticesindices
	const UINT vbByteSize = (UINT)vertices.size() * sizeof(Vertex);
	const UINT ibByteSize = (UINT)indices.size() * sizeof(std::uint16_t);

	//MeshGeometry
	auto mGeo = std::make_unique<MeshGeometry>();
	mGeo->Name = "shapeGeo";

	ThrowIfFailed(D3DCreateBlob(vbByteSize, &mGeo->VertexBufferCPU));
	ThrowIfFailed(D3DCreateBlob(ibByteSize, &mGeo->IndexBufferCPU));
	CopyMemory(mGeo->VertexBufferCPU->GetBufferPointer(), vertices.data(), vbByteSize);
	CopyMemory(mGeo->IndexBufferCPU->GetBufferPointer(), indices.data(), ibByteSize);
	mGeo->VertexBufferGPU = CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), vertices.data(), vbByteSize, mGeo->VertexBufferUploader);
	mGeo->IndexBufferGPU = CreateDefaultBuffer(md3dDevice.Get(), mCommandList.Get(), indices.data(), ibByteSize, mGeo->IndexBufferUploader);

	//MeshGeometry
	mGeo->VertexByteStride = sizeof(Vertex);
	mGeo->VertexBufferByteSize = vbByteSize;
	mGeo->IndexFormat = DXGI_FORMAT_R16_UINT;
	mGeo->IndexBufferByteSize = ibByteSize;

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
	white1x1->Roughness = 1.0f;

	auto wireFence = std::make_unique<Material>();
	wireFence->Name = "wireFence";
	wireFence->MatCBIndex = 3;
	wireFence->DiffuseSrvHeapIndex = 6; // Assuming wireFence texture is at index 6
	wireFence->NormalSrvHeapIndex = 5; // Assuming wireFence normal map is at index 7
	wireFence->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	wireFence->FresnelR0 = XMFLOAT3(0.01f, 0.01f, 0.01f);
	wireFence->Roughness = 0.2f;

	auto water = std::make_unique<Material>();
	water->Name = "water";
	water->MatCBIndex = 4;
	water->DiffuseSrvHeapIndex = 7; // Assuming water texture is at index 6
	water->NormalSrvHeapIndex = 5; // Assuming water normal map is at index 7
	water->DiffuseAlbedo = XMFLOAT4(1.0f, 1.0f, 1.0f, 1.0f);
	water->FresnelR0 = XMFLOAT3(0.1f, 0.1f, 0.1f);
	water->Roughness = 0.0f;

	auto mirror = std::make_unique<Material>();
	mirror->Name = "mirror";
	mirror->MatCBIndex = 5;
	mirror->DiffuseSrvHeapIndex = 4;
	mirror->NormalSrvHeapIndex = 5;
	mirror->CubeMapInex = 1;
	mirror->DiffuseAlbedo = XMFLOAT4(0.0f, 0.0f, 0.0f, 1.0f);
	mirror->FresnelR0 = XMFLOAT3(0.98f, 0.97f, 0.95f);
	mirror->Roughness = 0.1f;

	mMaterials["bricks0"] = std::move(bricks0);
	mMaterials["tile0"] = std::move(tile0);
	mMaterials["white1x1"] = std::move(white1x1);
	mMaterials["wireFence"] = std::move(wireFence);
	mMaterials["water"] = std::move(water);
	mMaterials["mirror"] = std::move(mirror);
}

void MySoftRasterizationApp::BuildRenderItems()
{
	auto sphereRitem = std::make_unique<RenderItem>();
	sphereRitem->Geo = mGeometries["shapeGeo"].get();
	sphereRitem->IndexCount = sphereRitem->Geo->DrawArgs["sphere"].IndexCount;
	sphereRitem->StartIndexLocation = sphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	sphereRitem->BaseVertexLocation = sphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
	sphereRitem->InstanceCount = 0;
	sphereRitem->Instances.resize(2);
	sphereRitem->Instances[0].World = MathHelper::Identity4x4();
	sphereRitem->Instances[0].TexTransform = MathHelper::Identity4x4();
	//XMStoreFloat4x4(&sphereRitem->Instances[0].TexTransform, XMMatrixScaling(3.0f, 3.0f, 3.0f));
	sphereRitem->Instances[0].MaterialIndex = 1;
	XMStoreFloat4x4(&sphereRitem->Instances[1].World, XMMatrixTranslation(2.0f, 0.0f, 0.0f));
	sphereRitem->Instances[1].TexTransform = MathHelper::Identity4x4();
	sphereRitem->Instances[1].MaterialIndex = 2;
	mRitemLayer[(int)RenderLayer::Opaque].push_back(sphereRitem.get());
	mAllRitems.push_back(std::move(sphereRitem));

	auto gridRitem = std::make_unique<RenderItem>();
	gridRitem->Geo = mGeometries["shapeGeo"].get();
	gridRitem->IndexCount = gridRitem->Geo->DrawArgs["grid"].IndexCount;
	gridRitem->StartIndexLocation = gridRitem->Geo->DrawArgs["grid"].StartIndexLocation;
	gridRitem->BaseVertexLocation = gridRitem->Geo->DrawArgs["grid"].BaseVertexLocation;
	gridRitem->InstanceCount = 0;
	gridRitem->Instances.resize(1);
	XMStoreFloat4x4(&gridRitem->Instances[0].World, XMMatrixTranslation(0.0f, -3.0f, 0.0f));
	XMStoreFloat4x4(&gridRitem->Instances[0].TexTransform, XMMatrixScaling(8.0f, 8.0f, 1.0f));
	gridRitem->Instances[0].MaterialIndex = 2; // Assuming grid material is at index 0
	mRitemLayer[(int)RenderLayer::Opaque].push_back(gridRitem.get());
	mAllRitems.push_back(std::move(gridRitem));

	auto withoutNormalMapSphereRitem = std::make_unique<RenderItem>();
	withoutNormalMapSphereRitem->Geo = mGeometries["shapeGeo"].get();
	withoutNormalMapSphereRitem->IndexCount = withoutNormalMapSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
	withoutNormalMapSphereRitem->StartIndexLocation = withoutNormalMapSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	withoutNormalMapSphereRitem->BaseVertexLocation = withoutNormalMapSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
	withoutNormalMapSphereRitem->InstanceCount = 0;
	withoutNormalMapSphereRitem->Instances.resize(1);
	XMStoreFloat4x4(&withoutNormalMapSphereRitem->Instances[0].World, XMMatrixTranslation(-2.0f, 0.0f, 0.0f));
	withoutNormalMapSphereRitem->Instances[0].TexTransform = MathHelper::Identity4x4();
	//XMStoreFloat4x4(&withoutNormalMapSphereRitem->Instances[0].TexTransform, XMMatrixScaling(3.0f, 3.0f, 3.0f));
	withoutNormalMapSphereRitem->Instances[0].MaterialIndex = 1;
	mRitemLayer[(int)RenderLayer::WithoutNormalMap].push_back(withoutNormalMapSphereRitem.get());
	mAllRitems.push_back(std::move(withoutNormalMapSphereRitem));

	auto alphaTestedSphereRitem = std::make_unique<RenderItem>();
	alphaTestedSphereRitem->Geo = mGeometries["shapeGeo"].get();
	alphaTestedSphereRitem->IndexCount = alphaTestedSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
	alphaTestedSphereRitem->StartIndexLocation = alphaTestedSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	alphaTestedSphereRitem->BaseVertexLocation = alphaTestedSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
	alphaTestedSphereRitem->InstanceCount = 0;
	alphaTestedSphereRitem->Instances.resize(1);
	XMStoreFloat4x4(&alphaTestedSphereRitem->Instances[0].World, XMMatrixTranslation(4.0f, 0.0f, 0.0f));
	//alphaTestedSphereRitem->Instances[0].TexTransform = MathHelper::Identity4x4();
	XMStoreFloat4x4(&alphaTestedSphereRitem->Instances[0].TexTransform, XMMatrixScaling(3.0f, 3.0f, 3.0f));
	alphaTestedSphereRitem->Instances[0].MaterialIndex = 3;
	mRitemLayer[(int)RenderLayer::AlphaTested].push_back(alphaTestedSphereRitem.get());
	mAllRitems.push_back(std::move(alphaTestedSphereRitem));

	auto transparentSphereRitem = std::make_unique<RenderItem>();
	transparentSphereRitem->Geo = mGeometries["shapeGeo"].get();
	transparentSphereRitem->IndexCount = transparentSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
	transparentSphereRitem->StartIndexLocation = transparentSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	transparentSphereRitem->BaseVertexLocation = transparentSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
	transparentSphereRitem->InstanceCount = 0;
	transparentSphereRitem->Instances.resize(1);
	XMStoreFloat4x4(&transparentSphereRitem->Instances[0].World, XMMatrixTranslation(6.0f, 0.0f, 0.0f));
	//transparentSphereRitem->Instances[0].TexTransform = MathHelper::Identity4x4();
	XMStoreFloat4x4(&transparentSphereRitem->Instances[0].TexTransform, XMMatrixScaling(3.0f, 3.0f, 3.0f));
	transparentSphereRitem->Instances[0].MaterialIndex = 4;
	mRitemLayer[(int)RenderLayer::Transparent].push_back(transparentSphereRitem.get());
	mAllRitems.push_back(std::move(transparentSphereRitem));

	auto skySphereRitem = std::make_unique<RenderItem>();
	skySphereRitem->Geo = mGeometries["shapeGeo"].get();
	skySphereRitem->IndexCount = skySphereRitem->Geo->DrawArgs["sphere"].IndexCount;
	skySphereRitem->StartIndexLocation = skySphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	skySphereRitem->BaseVertexLocation = skySphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
	skySphereRitem->InstanceCount = 0;
	skySphereRitem->Instances.resize(1);
	XMStoreFloat4x4(&skySphereRitem->Instances[0].World, XMMatrixScaling(1.0f, 1.0f, 1.0f));
	skySphereRitem->Instances[0].TexTransform = MathHelper::Identity4x4();
	skySphereRitem->Instances[0].MaterialIndex = 2; // Assuming sky material is at index 5
	mRitemLayer[(int)RenderLayer::Sky].push_back(skySphereRitem.get());
	mAllRitems.push_back(std::move(skySphereRitem));

	auto dynamicReflectionSphereRitem = std::make_unique<RenderItem>();
	dynamicReflectionSphereRitem->Geo = mGeometries["shapeGeo"].get();
	dynamicReflectionSphereRitem->IndexCount = dynamicReflectionSphereRitem->Geo->DrawArgs["sphere"].IndexCount;
	dynamicReflectionSphereRitem->StartIndexLocation = dynamicReflectionSphereRitem->Geo->DrawArgs["sphere"].StartIndexLocation;
	dynamicReflectionSphereRitem->BaseVertexLocation = dynamicReflectionSphereRitem->Geo->DrawArgs["sphere"].BaseVertexLocation;
	dynamicReflectionSphereRitem->InstanceCount = 0;
	dynamicReflectionSphereRitem->Instances.resize(1);
	XMStoreFloat4x4(&dynamicReflectionSphereRitem->Instances[0].World, XMMatrixTranslation(8.0f, 0.0f, 0.0f));
	dynamicReflectionSphereRitem->Instances[0].TexTransform = MathHelper::Identity4x4();
	dynamicReflectionSphereRitem->Instances[0].MaterialIndex = 5; // Assuming mirror material is at index 6
	mRitemLayer[(int)RenderLayer::OpaqueDynamicReflectors].push_back(dynamicReflectionSphereRitem.get());
	mAllRitems.push_back(std::move(dynamicReflectionSphereRitem));

	auto quadRitem = std::make_unique<RenderItem>();
	quadRitem->Geo = mGeometries["shapeGeo"].get();
	quadRitem->IndexCount = quadRitem->Geo->DrawArgs["quad"].IndexCount;
	quadRitem->StartIndexLocation = quadRitem->Geo->DrawArgs["quad"].StartIndexLocation;
	quadRitem->BaseVertexLocation = quadRitem->Geo->DrawArgs["quad"].BaseVertexLocation;
	quadRitem->InstanceCount = 0;
	quadRitem->Instances.resize(1);
	quadRitem->Instances[0].World = MathHelper::Identity4x4();
	quadRitem->Instances[0].TexTransform = MathHelper::Identity4x4();
	quadRitem->Instances[0].MaterialIndex = 0; // Assuming quad material is at index 0
	mRitemLayer[(int)RenderLayer::Debug].push_back(quadRitem.get());
	mAllRitems.push_back(std::move(quadRitem));
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
		D3D12_GPU_VIRTUAL_ADDRESS instanceBufferAddress = instanceBuffer->GetGPUVirtualAddress() +
			ri->InstanceBufferIndex * sizeof(InstanceData);
		//cmdList->SetGraphicsRootConstantBufferView(2, objCBAddress);
		cmdList->SetGraphicsRootShaderResourceView(2, instanceBufferAddress);

		cmdList->DrawIndexedInstanced(
			ri->IndexCount, // Index count per instance
			ri->InstanceCount,      // Instance count
			ri->StartIndexLocation, // Start index location
			ri->BaseVertexLocation,  // Base vertex location
			0);             // Instance start offset
	}
}

void MySoftRasterizationApp::DrawSceneToCubeMap()
{
	mCommandList->RSSetViewports(1, &mDynamicCubeMap->Viewport());
	mCommandList->RSSetScissorRects(1, &mDynamicCubeMap->ScissorRect());

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		mDynamicCubeMap->Resource(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_RENDER_TARGET));

	UINT passCBByteSize = CalcConstantBufferByteSize(sizeof(PassConstants));

	for (int i = 0; i < 6; ++i)
	{
		mCommandList->ClearRenderTargetView(mDynamicCubeMap->Rtv(i), Colors::LightBlue, 0, nullptr);
		mCommandList->ClearDepthStencilView(mCubeDSV, D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);

		mCommandList->OMSetRenderTargets(1, &mDynamicCubeMap->Rtv(i), true, &mCubeDSV);

		auto passCB = mCurrFrameResource->PassCB->Resource();
		D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = passCB->GetGPUVirtualAddress() + (1 + i) * passCBByteSize;
		mCommandList->SetGraphicsRootConstantBufferView(1, passCBAddress);

		DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

		mCommandList->SetPipelineState(mPSOs["withoutNormalMap"].Get());
		DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::WithoutNormalMap]);

		mCommandList->SetPipelineState(mPSOs["sky"].Get());
		DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Sky]);

		mCommandList->SetPipelineState(mPSOs["alphaTested"].Get());
		DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::AlphaTested]);

		mCommandList->SetPipelineState(mPSOs["transparent"].Get());
		DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Transparent]);

		mCommandList->SetPipelineState(mPSOs["opaque"].Get());
	}
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		mDynamicCubeMap->Resource(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_GENERIC_READ));
}

void MySoftRasterizationApp::DrawSceneToShadowMap()
{
	mCommandList->RSSetViewports(1, &mShadowMap->Viewport());
	mCommandList->RSSetScissorRects(1, &mShadowMap->ScissorRect());
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		mShadowMap->Resource(),
		D3D12_RESOURCE_STATE_GENERIC_READ, D3D12_RESOURCE_STATE_DEPTH_WRITE));
	UINT passCBByteSize = CalcConstantBufferByteSize(sizeof(PassConstants));
	mCommandList->ClearDepthStencilView(mShadowMap->Dsv(), D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL, 1.0f, 0, 0, nullptr);
	mCommandList->OMSetRenderTargets(0, nullptr, false, &mShadowMap->Dsv());
	auto passCB = mCurrFrameResource->PassCB->Resource();
	D3D12_GPU_VIRTUAL_ADDRESS passCBAddress = passCB->GetGPUVirtualAddress() + (1 + 6) * passCBByteSize;
	mCommandList->SetGraphicsRootConstantBufferView(1, passCBAddress);
	mCommandList->SetPipelineState(mPSOs["shadow"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);
	mCommandList->SetPipelineState(mPSOs["withoutNormalMap"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::WithoutNormalMap]);
	mCommandList->SetPipelineState(mPSOs["alphaTested"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::AlphaTested]);
	mCommandList->SetPipelineState(mPSOs["transparent"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Transparent]);
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::OpaqueDynamicReflectors]);
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(
		mShadowMap->Resource(),
		D3D12_RESOURCE_STATE_DEPTH_WRITE, D3D12_RESOURCE_STATE_GENERIC_READ));
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

	auto matSB = mCurrFrameResource->MatSB->Resource();
	mCommandList->SetGraphicsRootShaderResourceView(3, matSB->GetGPUVirtualAddress());

	CD3DX12_GPU_DESCRIPTOR_HANDLE texDescriptor(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart(), 1, mCbv_srv_uavDescriptorSize);
	mCommandList->SetGraphicsRootDescriptorTable(4, texDescriptor);

	DrawSceneToShadowMap();

	DrawSceneToCubeMap();

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

	auto passCB = mCurrFrameResource->PassCB->Resource();
	mCommandList->SetGraphicsRootConstantBufferView(1, passCB->GetGPUVirtualAddress());

	mCommandList->SetPipelineState(mPSOs["opaque"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::OpaqueDynamicReflectors]);
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Opaque]);

	mCommandList->SetPipelineState(mPSOs["withoutNormalMap"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::WithoutNormalMap]);

	mCommandList->SetPipelineState(mPSOs["debug"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Debug]);

	mCommandList->SetPipelineState(mPSOs["sky"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Sky]);

	mCommandList->SetPipelineState(mPSOs["alphaTested"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::AlphaTested]);

	mCommandList->SetPipelineState(mPSOs["transparent"].Get());
	DrawRenderItems(mCommandList.Get(), mRitemLayer[(int)RenderLayer::Transparent]);

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

std::array<const CD3DX12_STATIC_SAMPLER_DESC, 7> MySoftRasterizationApp::GetStaticSamplers()
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

	const CD3DX12_STATIC_SAMPLER_DESC shadow(
		6,
		D3D12_FILTER_COMPARISON_MIN_MAG_LINEAR_MIP_POINT,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		D3D12_TEXTURE_ADDRESS_MODE_BORDER,
		0.0f,
		16,
		D3D12_COMPARISON_FUNC_LESS_EQUAL,
		D3D12_STATIC_BORDER_COLOR_OPAQUE_BLACK
	);

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp,
		shadow};
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

	mLightRotationAngle += 0.1f * gt.DeltaTime();
	XMMATRIX R = XMMatrixRotationY(mLightRotationAngle);
	for (int i = 0; i < 3; ++i)
	{
		XMVECTOR lightDir = XMLoadFloat3(&mBaseLightDirections[i]);
		lightDir = XMVector3TransformNormal(lightDir, R);
		XMStoreFloat3(&mRotatedLightDirections[i], lightDir);
	}

	//UpdateObjectCBs(gt);
	UpdateInstanceBuffers(gt);
	UpdateMaterialCBs(gt);
	UpdateShadowTransform();
	UpdateMainPassCBs();
	UpdateShadowPassCBs();
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
	XMMATRIX shadowTransform = XMLoadFloat4x4(&mShadowTransform);

	XMStoreFloat4x4(&mMainPassCB.View, XMMatrixTranspose(view));
	XMStoreFloat4x4(&mMainPassCB.InvView, XMMatrixTranspose(invView));
	XMStoreFloat4x4(&mMainPassCB.Proj, XMMatrixTranspose(proj));
	XMStoreFloat4x4(&mMainPassCB.InvProj, XMMatrixTranspose(invProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProj, XMMatrixTranspose(viewProj));
	XMStoreFloat4x4(&mMainPassCB.InvViewProj, XMMatrixTranspose(invViewProj));
	XMStoreFloat4x4(&mMainPassCB.ViewProjTex, XMMatrixTranspose(viewProjTex));
	XMStoreFloat4x4(&mMainPassCB.ShadowTransform, XMMatrixTranspose(shadowTransform));

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

	UpdateCubeMapFacePassCBs();
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
	int instanceIndex = 0;
	for (auto& e : mAllRitems)
	{
		const auto& instanceData = e->Instances;
		e->InstanceBufferIndex = instanceIndex;
		for (UINT i = 0; i < (UINT)instanceData.size(); ++i)
		{
			InstanceData data;
			XMStoreFloat4x4(&data.World, XMMatrixTranspose(XMLoadFloat4x4(&instanceData[i].World)));
			XMStoreFloat4x4(&data.TexTransform, XMMatrixTranspose(XMLoadFloat4x4(&instanceData[i].TexTransform)));
			data.MaterialIndex = instanceData[i].MaterialIndex;

			currInstanceBuffer->CopyData(instanceIndex++, data);
		}
		e->InstanceCount = (UINT)instanceData.size();
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
			matData.CubeMapIndex = mat->CubeMapInex;

			currMatSB->CopyData(mat->MatCBIndex, matData);
			mat->NumFramesDirty--;
		}
	}
}

void MySoftRasterizationApp::UpdateCubeMapFacePassCBs()
{
	for (int i = 0; i < 6; ++i)
	{
		PassConstants cubeMapFacePassCB = mMainPassCB;

		XMMATRIX view = mCubeMapCamera[i].GetView();
		XMMATRIX proj = mCubeMapCamera[i].GetProj();
		XMMATRIX viewProj = view * proj;

		XMStoreFloat4x4(&cubeMapFacePassCB.ViewProj, XMMatrixTranspose(viewProj));

		cubeMapFacePassCB.EyePosW = mCubeMapCamera[i].GetPosition3f();

		auto currPassCB = mCurrFrameResource->PassCB.get();
		currPassCB->CopyData(i + 1, cubeMapFacePassCB); // 从1开始存储，因为0是主渲染通道
	}
}

void MySoftRasterizationApp::UpdateShadowTransform()
{
	XMVECTOR lightDir = XMLoadFloat3(&mRotatedLightDirections[0]);
	XMVECTOR lightPos = -2.0f * mSceneBounds.Radius * lightDir;
	XMVECTOR targetPos = XMLoadFloat3(&mSceneBounds.Center);
	XMVECTOR lightUp = XMVectorSet(0.0f, 1.0f, 0.0f, 0.0f);
	XMMATRIX lightView = XMMatrixLookAtLH(lightPos, targetPos, lightUp);

	XMStoreFloat3(&mLightPosW, lightPos);

	XMFLOAT3 sphereCenterLS;
	XMStoreFloat3(&sphereCenterLS, XMVector3TransformCoord(targetPos, lightView));

	float l = sphereCenterLS.x - mSceneBounds.Radius;
	float r = sphereCenterLS.x + mSceneBounds.Radius;
	float b = sphereCenterLS.y - mSceneBounds.Radius;
	float t = sphereCenterLS.y + mSceneBounds.Radius;
	float n = sphereCenterLS.z - mSceneBounds.Radius;
	float f = sphereCenterLS.z + mSceneBounds.Radius;

	mLightNearZ = n;
	mLightFarZ = f;

	XMMATRIX lightProj = XMMatrixOrthographicOffCenterLH(l, r, b, t, n, f);

	XMMATRIX T(
		0.5f, 0.0f, 0.0f, 0.0f,
		0.0f, -0.5f, 0.0f, 0.0f,
		0.0f, 0.0f, 1.0f, 0.0f,
		0.5f, 0.5f, 0.0f, 1.0f
	);

	XMMATRIX S = lightView * lightProj * T;

	XMStoreFloat4x4(&mLightView, lightView);
	XMStoreFloat4x4(&mLightProj, lightProj);
	XMStoreFloat4x4(&mShadowTransform, S);
}

void MySoftRasterizationApp::UpdateShadowPassCBs()
{
	PassConstants mShadowMapPassCB = mMainPassCB;

	XMMATRIX view = XMLoadFloat4x4(&mLightView);
	XMMATRIX proj = XMLoadFloat4x4(&mLightProj);

	XMMATRIX viewProj = XMMatrixMultiply(view, proj);

	UINT w = mShadowMap->Width();
	UINT h = mShadowMap->Height();

	XMStoreFloat4x4(&mShadowMapPassCB.ViewProj, XMMatrixTranspose(viewProj));
	mShadowMapPassCB.EyePosW = mLightPosW;
	mShadowMapPassCB.RenderTargetSize = XMFLOAT2((float)w, (float)h);
	mShadowMapPassCB.NearZ = mLightNearZ;
	mShadowMapPassCB.FarZ = mLightFarZ;

	auto currPassCB = mCurrFrameResource->PassCB.get();
	currPassCB->CopyData(7, mShadowMapPassCB);
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
		"wireFenceDiffuseMap",
		"waterDiffuseMap",
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
		L"D:\\DX12\\d3d12book\\Textures\\WireFence.dds",
		L"D:\\DX12\\d3d12book\\Textures\\water1.dds",
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