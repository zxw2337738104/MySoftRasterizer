#include "D3D12App.h"
#include "FrameResource.hpp"
#include "MeshGeometry.hpp"
#include "GeometryGenerator.h"
#include "Camera.h"

const int gNumFrameResources = 3;

enum class RenderLayer
{
	Opaque = 0,
	Count
};

struct RenderItem
{
	RenderItem() = default;
};

class MySoftRasterization : public D3D12App
{
public:
	MySoftRasterization(HINSTANCE hInstance, int nShowCmd)
		: D3D12App(hInstance, nShowCmd) {
	}
	~MySoftRasterization() {
		ImGui_ImplDX12_Shutdown();
		ImGui_ImplWin32_Shutdown();
		ImGui::DestroyContext();
	}
	virtual bool Init() override;
private:
	virtual void Draw() override;
	void BuildDescriptorHeaps();
	void BuildRootSignature();
	//void BuildShadersAndInputLayout();
	//void BuildPSOs();
	//void BuildFrameResources();
	//void BuildGeometry();
	//void BuildMaterial();
	//void BuildRenderItems();
	//void DrawRenderItems(ID3D12GraphicsCommandList* cmdList, const std::vector<RenderItem*>& ritems);

	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();

	//void LoadTextures();
	//void OnKeyboardInput(GameTime& gt);
	//void UpdateCamera(GameTime& gt);
	virtual void Update(GameTime gt) override;
	virtual void OnMouseDown(WPARAM btnState, int x, int y) override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y) override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y) override;
	virtual void OnResize() override;
	//virtual void CreateDescriptorHeap() override;

	//void UpdateObjectCBs(GameTime& gt);
	//void UpdateMainPassCBs();
	//void UpdateMaterialCBs(GameTime& gt);

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

	PassConstants mMainPassCB;

	POINT mLastMousePos = { 0, 0 };
	float mTheta = 1.5f * XM_PI;
	float mPhi = XM_PIDIV4;
	float mRadius = 5.0f;

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
		MySoftRasterization theApp(hInstance, nShowCmd);
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

bool MySoftRasterization::Init()
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

	//LoadTextures();
	BuildDescriptorHeaps();
	BuildRootSignature();
	//BuildShadersAndInputLayout();
	//BuildPSOs();
	//BuildFrameResources();
	//BuildGeometry();
	//BuildMaterial();
	//BuildRenderItems();

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

	ThrowIfFailed(mCommandList->Close());
	// Execute the initialization commands
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCmdQueue();

	return true;
}

void MySoftRasterization::BuildDescriptorHeaps()
{
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 1; // Adjust as needed
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));
}

void MySoftRasterization::BuildRootSignature()
{
	CD3DX12_ROOT_PARAMETER rootParameters[1];
	rootParameters[0].InitAsDescriptorTable(1, &CD3DX12_DESCRIPTOR_RANGE(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0));
	CD3DX12_ROOT_SIGNATURE_DESC rootSignatureDesc(1, rootParameters, 0, nullptr,
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	ThrowIfFailed(D3D12SerializeRootSignature(&rootSignatureDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf()));
	ThrowIfFailed(md3dDevice->CreateRootSignature(0, serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(), IID_PPV_ARGS(&mRootSignature)));
}

void MySoftRasterization::Draw()
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

void MySoftRasterization::OnMouseDown(WPARAM btnState, int x, int y)
{
	if (ImGui::GetIO().WantCaptureMouse)
		return; // ImGui 捕获鼠标时，跳过相机控制

	mLastMousePos.x = x;
	mLastMousePos.y = y;
	SetCapture(mhMainWnd);
}

void MySoftRasterization::OnMouseUp(WPARAM btnState, int x, int y)
{
	if (ImGui::GetIO().WantCaptureMouse)
		return; // ImGui 捕获鼠标时，跳过相机控制

	ReleaseCapture();
}

void MySoftRasterization::OnMouseMove(WPARAM btnState, int x, int y)
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

void MySoftRasterization::OnResize()
{
	D3D12App::OnResize();

	mCamera.SetLens(0.25 * MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
}

void MySoftRasterization::Update(GameTime gt)
{
	// ImGui 新帧
	ImGui_ImplDX12_NewFrame();
	ImGui_ImplWin32_NewFrame();
	ImGui::NewFrame();

	// 示例 ImGui 窗口
	ImGui::Begin("Debug Window");
	ImGui::Text("Frame Rate: %.1f FPS", 1.0f / gt.DeltaTime());
	ImGui::End();

	// 渲染 ImGui
	ImGui::Render();
}