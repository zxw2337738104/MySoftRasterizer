#pragma once
#include "DXHelper.h"
#include "GameTime.h"
#include "..\ImGui\imgui.h"
#include "..\ImGui\imgui_impl_win32.h"
#include "..\ImGui\imgui_impl_dx12.h"

using namespace Microsoft::WRL;
using namespace DirectX;

#pragma comment(lib, "d3dcompiler.lib")
#pragma comment(lib, "D3D12.lib")
#pragma comment(lib, "dxgi.lib")



class D3D12App
{
public:
	D3D12App(HINSTANCE hInstance, int nShowCmd);
	~D3D12App();

	static D3D12App* GetApp();
	float AspectRatio()const;

	int Run();
	virtual bool Init();
	virtual void Draw();
	virtual void Update(GameTime gt)=0;
	virtual void OnResize();

	bool InitWindow();
	bool InitDirect3D();
	

	void CreateDevice();
	void CreateFence();
	void GetDescriptorSize();
	void SetMSAA();
	void CreateCommandObject();
	void CreateSwapChain();
	virtual void CreateDescriptorHeap();
	void CreateRTV();
	void CreateDSV();
	void CreateViewPortAndScissorRect();

	void FlushCmdQueue();
	void CalculateFrameState();

	ID3D12Resource* CurrentBackBuffer()const;
	D3D12_CPU_DESCRIPTOR_HANDLE CurrentBackBufferView()const;
	D3D12_CPU_DESCRIPTOR_HANDLE DepthStencilView()const;

	virtual LRESULT MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam);
	virtual void OnMouseDown(WPARAM btnState, int x, int y){}
	virtual void OnMouseUp(WPARAM btnState, int x, int y){}
	virtual void OnMouseMove(WPARAM btnState, int x, int y){}

	static D3D12App* mApp;
	HWND mhMainWnd = 0;
	GameTime gt;
	bool isAppPaused = false;
	bool isMinimized = false;
	bool isMaximized = false;
	bool isResizing = false;

	bool      m4xMsaaState = false;    // 4X MSAA enabled
	UINT      m4xMsaaQuality = 0;      // quality level of 4X MSAA

	static const int SwapChainBufferCount = 2;
	ComPtr<IDXGIFactory4> mdxgiFactory;
	ComPtr<ID3D12Device> md3dDevice;
	ComPtr<ID3D12Fence> mFence;
	ComPtr<ID3D12CommandQueue> mCommandQueue;
	ComPtr<ID3D12CommandAllocator> mDirectCmdListAlloc;
	ComPtr<ID3D12GraphicsCommandList> mCommandList;
	ComPtr<IDXGISwapChain> mSwapChain;
	ComPtr<ID3D12Resource> mSwapChainBuffer[SwapChainBufferCount];
	ComPtr<ID3D12Resource> mDepthStencilBuffer;
	ComPtr<ID3D12DescriptorHeap> mRtvHeap;
	ComPtr<ID3D12DescriptorHeap> mDsvHeap;
	UINT mRtvDescriptorSize = 0;
	UINT mDsvDescriptorSize = 0;
	UINT mCbv_srv_uavDescriptorSize = 0;
	D3D12_FEATURE_DATA_MULTISAMPLE_QUALITY_LEVELS msaaQualityLevels;
	int mCurrentFence = 0;
	D3D12_VIEWPORT viewPort;
	D3D12_RECT scissorRect;
	UINT mCurrentBackBuffer = 0;
	ComPtr<ID3D12Resource> defaultBuffer;

	DXGI_FORMAT mBackBufferFormat = DXGI_FORMAT_R8G8B8A8_UNORM;
	DXGI_FORMAT mDepthStencilFormat = DXGI_FORMAT_D24_UNORM_S8_UINT;

	HINSTANCE mHInstance;
	int mNShowCmd;

	int mClientWidth = 1280;
	int mClientHeight = 720;
};

