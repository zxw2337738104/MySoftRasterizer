#include "D3D12App.h"


LRESULT CALLBACK MainWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	return D3D12App::GetApp()->MsgProc(hwnd, msg, wParam, lParam);
}

LRESULT D3D12App::MsgProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam)
{
	extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam))
		return true;
	//消息处理
	switch (msg)
	{
	case WM_SIZE:
		mClientWidth = LOWORD(lParam);
		mClientHeight = HIWORD(lParam);
		if (md3dDevice)
		{
			//如果最小化，则暂停，调整最小化和最大化状态
			if (wParam == SIZE_MINIMIZED)
			{
				isAppPaused = true;
				isMinimized = true;
				isMaximized = false;
			}
			else if (wParam == SIZE_MAXIMIZED)
			{
				isAppPaused = false;
				isMinimized = false;
				isMaximized = true;
				OnResize();
			}
			else if (wParam == SIZE_RESTORED)
			{
				if (isMinimized)
				{
					isAppPaused = false;
					isMinimized = false;
					OnResize();
				}
				else if (isMaximized)
				{
					isAppPaused = false;
					isMaximized = false;
					OnResize();
				}
				else if (isResizing)
				{

				}
				else
				{
					OnResize();
				}
			}
		}
		return 0;
		//鼠标键按下
	case WM_LBUTTONDOWN:
	case WM_MBUTTONDOWN:
	case WM_RBUTTONDOWN:
		//wParam为输入的虚拟键代码，lParam为系统反馈的光标信息
		OnMouseDown(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
		//鼠标键抬起
	case WM_LBUTTONUP:
	case WM_MBUTTONUP:
	case WM_RBUTTONUP:
		OnMouseUp(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_MOUSEMOVE:
		OnMouseMove(wParam, GET_X_LPARAM(lParam), GET_Y_LPARAM(lParam));
		return 0;
	case WM_DESTROY:
		PostQuitMessage(0);
		return 0;
	default:
		break;
	}
	return DefWindowProc(hwnd, msg, wParam, lParam);
}

D3D12App* D3D12App::mApp = nullptr;
D3D12App* D3D12App::GetApp()
{
	return mApp;
}

float D3D12App::AspectRatio()const
{
	return static_cast<float>(mClientWidth) / mClientHeight;
}

D3D12App::D3D12App(HINSTANCE hInstance, int nShowCmd) :
	mHInstance(hInstance), mNShowCmd(nShowCmd)
{
	assert(mApp == nullptr);
	mApp = this;
}

D3D12App::~D3D12App() {}

//设备是通过DXGIFactory接口来创建的，
//所以先创建DXGIFactory，然后Device
void D3D12App::CreateDevice()
{
	//是一个Windows API函数，用于创建和管理如SwapChain和Adapter的DXGI对象的核心组件
	ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&mdxgiFactory)));//IID_PPV_ARGS是一个宏，将该地址转换为所需格式
	//创建DX12设备，用于管理资源、队列命令等
	ThrowIfFailed(D3D12CreateDevice(nullptr,//默认的适配器
		D3D_FEATURE_LEVEL_12_0,//指定DX12.0级别
		IID_PPV_ARGS(&md3dDevice)));
}

//创建围栏fence，便于同步CPU和GPU
void D3D12App::CreateFence()
{
	ThrowIfFailed(md3dDevice->CreateFence(0,//围栏初始值
		D3D12_FENCE_FLAG_NONE,//不使用任何特殊标志
		IID_PPV_ARGS(&mFence)));
}

//获取描述符大小
void D3D12App::GetDescriptorSize()
{
	mRtvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);                     //渲染目标缓冲区描述符
	mDsvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_DSV);                     //深度模板缓冲区描述符
	mCbv_srv_uavDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);     //常量缓冲区、着色器资源、随机访问视图描述符
}

//设置MSAA抗锯齿属性
void D3D12App::SetMSAA()
{
	//初始化msaaQualityLevels结构体
	msaaQualityLevels.Format = DXGI_FORMAT_R8G8B8A8_UNORM;//指定了颜色缓冲区的格式为8位红、绿、蓝、透明通道的无符号归一化整数格式
	msaaQualityLevels.SampleCount = 1;//每个像素的采样数量为1
	msaaQualityLevels.Flags = D3D12_MULTISAMPLE_QUALITY_LEVELS_FLAG_NONE;//不使用额外的标志
	msaaQualityLevels.NumQualityLevels = 0;//初始化支持的质量级别数量是0
	//当前图形驱动对MSAA多重采样的支持（注意：第二个参数即是输入又是输出）
	ThrowIfFailed(md3dDevice->CheckFeatureSupport(D3D12_FEATURE_MULTISAMPLE_QUALITY_LEVELS, &msaaQualityLevels, sizeof(msaaQualityLevels)));
	//NumQualityLevels在Check函数里会进行设置
	//如果支持MSAA，则Check函数返回的NumQualityLevels > 0
	//expression为假（即为0），则终止程序运行，并打印一条出错信息
	assert(msaaQualityLevels.NumQualityLevels > 0);
}

// 创建命令队列、命令列表和命令分配器
void D3D12App::CreateCommandObject()
{
	D3D12_COMMAND_QUEUE_DESC commandQueueDesc = {};//初始化命令队列描述结构体
	commandQueueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;//指定命令队列的类型为直接命令队列
	commandQueueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateCommandQueue(&commandQueueDesc, IID_PPV_ARGS(&mCommandQueue)));
	ThrowIfFailed(md3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&mDirectCmdListAlloc)));
	ThrowIfFailed(md3dDevice->CreateCommandList(0, //掩码值为0，单GPU
		D3D12_COMMAND_LIST_TYPE_DIRECT,            //命令列表类型
		mDirectCmdListAlloc.Get(),                 //命令分配器接口指针
		nullptr,                                   //流水线状态对象PSO，这里不绘制，所以空指针
		IID_PPV_ARGS(&mCommandList)));             //返回创建的命令列表
	mCommandList->Close();                         //重置命令列表前必须将其关闭
}

//创建交换链
void D3D12App::CreateSwapChain()
{
	mSwapChain.Reset();
	DXGI_SWAP_CHAIN_DESC swapChainDesc;                                                   //交换链描述结构体
	swapChainDesc.BufferDesc.Width = mClientWidth;                                                //缓冲区分辨率的宽度
	swapChainDesc.BufferDesc.Height = mClientHeight;											      //缓冲区分辨率的高度
	swapChainDesc.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;                         //缓冲区的显示格式
	swapChainDesc.BufferDesc.RefreshRate.Denominator = 1;                                 //刷新率的分子
	swapChainDesc.BufferDesc.RefreshRate.Numerator = 60;                                  //刷新率的分母
	swapChainDesc.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;                     //图像相对屏幕的拉伸（未指定的）
	swapChainDesc.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;     //逐行扫描VS隔行扫描(未指定的)
	swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;                          //将数据渲染至后台缓冲区（即作为渲染目标）
	swapChainDesc.OutputWindow = mhMainWnd;                                               //渲染窗口句柄
	swapChainDesc.SampleDesc.Count = 1;                                                   //多重采样数量
	swapChainDesc.SampleDesc.Quality = 0;                                                 //多重采样质量
	swapChainDesc.Windowed = true;                                                        //是否窗口化
	swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;                             //交换缓冲区后，旧的后台缓冲区被丢弃
	swapChainDesc.BufferCount = 2;                                                        //后台缓冲区数量（双缓冲）
	swapChainDesc.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;                         //自适应窗口模式（自动选择最适于当前窗口尺寸的显示模式）
	//利用DXGI接口下的Factory类创建交换链
	ThrowIfFailed(mdxgiFactory->CreateSwapChain(mCommandQueue.Get(),//指定与交换链关联的命令队列，用于执行渲染命令
		&swapChainDesc,//传递交换链描述符
		mSwapChain.GetAddressOf()));//返回创建好的交换链对象的指针
}

//创建描述符堆
void D3D12App::CreateDescriptorHeap()
{
	D3D12_DESCRIPTOR_HEAP_DESC rtvDescriptorHeapDesc;
	rtvDescriptorHeapDesc.NumDescriptors = 2;//设置RTV描述符堆中可存储的描述符数量为2，对应双缓冲的两个渲染目标视图
	rtvDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	rtvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvDescriptorHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&rtvDescriptorHeapDesc, IID_PPV_ARGS(&mRtvHeap)));

	D3D12_DESCRIPTOR_HEAP_DESC dsvDescriptorHeapDesc;
	dsvDescriptorHeapDesc.NumDescriptors = 1;
	dsvDescriptorHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	dsvDescriptorHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_DSV;
	dsvDescriptorHeapDesc.NodeMask = 0;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&dsvDescriptorHeapDesc, IID_PPV_ARGS(&mDsvHeap)));
}

//创建描述符
void D3D12App::CreateRTV()
{
	CD3DX12_CPU_DESCRIPTOR_HANDLE rtvHeapHandle(mRtvHeap->GetCPUDescriptorHandleForHeapStart());//管理CPU端的描述符句柄
	for (int i = 0; i < 2; i++)
	{
		//获得存于交换链中的后台缓冲区资源
		mSwapChain->GetBuffer(i, IID_PPV_ARGS(mSwapChainBuffer[i].GetAddressOf()));
		//创建RTV
		md3dDevice->CreateRenderTargetView(mSwapChainBuffer[i].Get(),
			nullptr,          //在交换链创建中已经定义了该资源的数据格式，所以这里指定为空指针
			rtvHeapHandle);   //描述符句柄结构体（这里是变体，继承自CD3DX12_CPU_DESCRIPTOR_HANDLE）
		//将描述符句柄偏移到下一个缓冲区
		rtvHeapHandle.Offset(1,//1表示偏移一个描述符的位置
			mRtvDescriptorSize);//每个RTV描述符的大小
	}
}

void D3D12App::CreateDSV()
{
	D3D12_RESOURCE_DESC dsvResourceDesc;
	dsvResourceDesc.Alignment = 0;//默认对齐
	dsvResourceDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;//指定资源维度（类型）为TEXTURE2D
	dsvResourceDesc.DepthOrArraySize = 1;//纹理深度为1（即纹理数组的大小）
	dsvResourceDesc.Width = mClientWidth;
	dsvResourceDesc.Height = mClientHeight;
	dsvResourceDesc.MipLevels = 1;//MIPMAP层级数量
	dsvResourceDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;//指定纹理布局（这里不指定）
	dsvResourceDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL;//深度模板资源的Flag
	dsvResourceDesc.Format = DXGI_FORMAT_R24G8_TYPELESS;//24位深度，8位模板,还有个无类型的格式DXGI_FORMAT_R24G8_TYPELESS也可以使用
	dsvResourceDesc.SampleDesc.Count = 1;//多重采样数量
	dsvResourceDesc.SampleDesc.Quality = msaaQualityLevels.NumQualityLevels - 1;//多重采样质量
	CD3DX12_CLEAR_VALUE optClear;//清除资源的优化值，提高清除操作的执行速度（CreateCommittedResource函数中传入）
	optClear.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;//24位深度，8位模板,还有个无类型的格式DXGI_FORMAT_R24G8_TYPELESS也可以使用
	optClear.DepthStencil.Depth = 1;//初始深度值为1
	optClear.DepthStencil.Stencil = 0;//初始模板值为0
	//创建一个资源和一个堆，并将资源提交至堆中（将深度模板数据提交至GPU显存中）
	ThrowIfFailed(md3dDevice->CreateCommittedResource(&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),//堆类型为默认堆（不能写入）
		D3D12_HEAP_FLAG_NONE,
		&dsvResourceDesc,//定义的DSV资源指针
		D3D12_RESOURCE_STATE_COMMON,//资源的状态为初始状态
		&optClear,//上面定义的优化值指针
		IID_PPV_ARGS(&mDepthStencilBuffer)));//返回深度模板资源
	//创建DSV(必须填充DSV属性结构体，和创建RTV不同，RTV是通过句柄)
	D3D12_DEPTH_STENCIL_VIEW_DESC dsvDesc;
	dsvDesc.Flags = D3D12_DSV_FLAG_NONE;
	dsvDesc.Format = DXGI_FORMAT_D24_UNORM_S8_UINT;
	dsvDesc.ViewDimension = D3D12_DSV_DIMENSION_TEXTURE2D;
	dsvDesc.Texture2D.MipSlice = 0;
	md3dDevice->CreateDepthStencilView(mDepthStencilBuffer.Get(),
		&dsvDesc,		//D3D12_DEPTH_STENCIL_VIEW_DESC类型指针，可填&dsvDesc（见上注释代码），
						//由于在创建深度模板资源时已经定义深度模板数据属性，所以这里可以指定为空指针
		mDsvHeap->GetCPUDescriptorHandleForHeapStart());//DSV句柄
}

//实现围栏代码
void D3D12App::FlushCmdQueue()
{
	mCurrentFence++;//CPU围栏,CPU传完命令并关闭后，CPU围栏值+1
	mCommandQueue->Signal(mFence.Get(), mCurrentFence);//当GPU处理完CPU传入的命令后，将fence接口中的围栏值更新为mCurrentFence
	if (mFence->GetCompletedValue() < mCurrentFence)//小于说明GPU未处理完所有命令
	{
		HANDLE eventHandle = CreateEvent(nullptr,//表示使用默认的安全属性
			false,//表示创建的是一个自动重置的事件对象
			false,//表示事件对象的初始状态为未触发
			L"FenceSetDone");//事件对象的名称
		mFence->SetEventOnCompletion(mCurrentFence, eventHandle);//当围栏达到mCurrentFence值（即执行到Signal（）指令修改了围栏值）时触发的eventHandle事件
		WaitForSingleObject(eventHandle, INFINITE);//等待GPU命中围栏，激发事件（阻塞当前线程直到事件触发，注意此Event需先设置再等待，
												   //如果没有Set就Wait，就死锁了，Set永远不会调用，所以也就没线程可以唤醒这个线程）
												   //INFINITE表示无限期等待
		CloseHandle(eventHandle);//释放事件对象的句柄
	}
}

//设置视口和裁剪矩形
void D3D12App::CreateViewPortAndScissorRect()
{
	viewPort.TopLeftX = 0;
	viewPort.TopLeftY = 0;
	viewPort.Width = static_cast<float>(mClientWidth);
	viewPort.Height = static_cast<float>(mClientHeight);
	viewPort.MaxDepth = 1.0f;
	viewPort.MinDepth = 0.0f;

	scissorRect.left = 0;
	scissorRect.top = 0;
	scissorRect.right = mClientWidth;
	scissorRect.bottom = mClientHeight;
}

bool D3D12App::InitWindow()
{
	WNDCLASS wc;
	wc.style = CS_HREDRAW | CS_VREDRAW;    //当窗口大小改变时，重新绘制整个窗口
	wc.lpfnWndProc = MainWndProc;	//指定窗口过程(消息处理函数)
	wc.cbClsExtra = 0;	//借助这两个字段来为当前窗口类分配额外的内存空间（这里不分配，所以置0）
	wc.cbWndExtra = 0;	//借助这两个字段来为当前窗口实例分配额外的内存空间（这里不分配，所以置0）
	wc.hInstance = mHInstance;	//应用程序实例句柄（由WinMain传入）
	wc.hIcon = LoadIcon(0, IDC_ARROW);	//使用默认的应用程序图标
	wc.hCursor = LoadCursor(0, IDC_ARROW);	//使用标准的鼠标指针样式
	wc.hbrBackground = (HBRUSH)GetStockObject(WHITE_BRUSH);	//指定了白色背景画刷句柄
	wc.lpszMenuName = 0;	//没有菜单栏
	wc.lpszClassName = L"MainWnd";	//窗口名
	//窗口类注册失败
	if (!RegisterClass(&wc))
	{
		//消息框函数，参数1：消息框所属窗口句柄，可为NULL。参数2：消息框显示的文本信息。参数3：标题文本。参数4：消息框样式
		MessageBox(0, L"RegisterClass Failed", 0, 0);
		return 0;
	}

	//窗口类注册成功
	RECT R;	//裁剪矩形
	R.left = 0;
	R.top = 0;
	R.right = mClientWidth;
	R.bottom = mClientHeight;
	AdjustWindowRect(&R, WS_OVERLAPPEDWINDOW, false);	//根据窗口的客户区大小计算窗口的大小
	int width = R.right - R.left;
	int height = R.bottom - R.top;

	//创建窗口,返回布尔值
	mhMainWnd = CreateWindow(L"MainWnd", L"DX12Initialize", WS_OVERLAPPEDWINDOW, CW_USEDEFAULT, CW_USEDEFAULT, width, height, 0, 0, mHInstance, 0);
	//窗口创建失败
	if (!mhMainWnd)
	{
		MessageBox(0, L"CreateWindow Failed", 0, 0);
		return 0;
	}
	//窗口创建成功,则显示并更新窗口
	ShowWindow(mhMainWnd, mNShowCmd);
	UpdateWindow(mhMainWnd);

	return true;
}

int D3D12App::Run()
{
	//消息循环
	//定义消息结构体
	MSG msg = { 0 };
	//每次循环开始都要重置计时器
	gt.Reset();
	//如果GetMessage函数不等于0，说明没有接受到WM_QUIT
	while (msg.message != WM_QUIT)
	{
		//如果有窗口消息就进行处理
		if (PeekMessage(&msg, 0, 0, 0, PM_REMOVE)) //PeekMessage函数会自动填充msg结构体元素，PM_REMOVE表示消息被获取后移除消息
		{
			TranslateMessage(&msg);	//键盘按键转换，将虚拟键消息转换为字符消息
			DispatchMessage(&msg);	//把消息分派给相应的窗口过程
		}
		//否则就执行动画和游戏逻辑
		else
		{
			gt.Tick();//计算每两帧间隔时间
			if (!gt.IsStopped())
			{
				CalculateFrameState();
				Update(gt);
				Draw();
			}
			//如果是暂停状态，则休眠100秒
			else
			{
				Sleep(100);
			}
		}
	}
	return (int)msg.wParam;//返回msg.wParam作为退出码，通常包含程序退出的原因或状态信息
}

bool D3D12App::InitDirect3D()
{
#if defined(DEBUG) || defined(_DEBUG)//DEBUG模式下才会执行这段代码
	{
		ComPtr<ID3D12Debug> debugController;
		ThrowIfFailed(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)));
		debugController->EnableDebugLayer();//启用调试层
	}
#endif
	CreateDevice();//创建DXGIFactory和D3D12Device，前者用于创建交换链、适配器等，后者负责管理和执行各种渲染操作
	CreateFence();//创建Fence，用于同步CPU和GPU的操作，确保CPU在GPU完成特定任务后再继续执行后续操作
	GetDescriptorSize();//获取描述符大小
	SetMSAA();//设置MSAA状态
	CreateCommandObject();//创建命令队列、命令分配器和命令列表。
						  //命令队列用于管理和执行命令列表，命令分配器用于分配命令列表所需内存，命令列表存储渲染命令
	CreateSwapChain();//创建交换链，管理前后缓冲区，实现双缓冲或多重缓冲渲染
	CreateDescriptorHeap();//创建描述符堆，为资源视图提供了存储和管理的空间
	//CreateRTV();//创建渲染目标视图
	//CreateDSV();//创建深度模板视图
	//CreateViewPortAndScissorRect();//创建视口和裁剪矩形

	return true;
}

bool D3D12App::Init()
{
	if (!InitWindow())
	{
		return false;
	}
	else if (!InitDirect3D())
	{
		return false;
	}
	OnResize();
	return true;
}

void D3D12App::Draw()
{
	//重置命令分配器，复用相关内存
	ThrowIfFailed(mDirectCmdListAlloc->Reset());//复用命令相关的内存
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));//复用命令列表及其内存

	//将后台缓冲资源从呈现状态转换到渲染目标状态(准备接收图像渲染)
	UINT& ref_mCurrentBackBuffer = mCurrentBackBuffer;
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSwapChainBuffer[ref_mCurrentBackBuffer].Get(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET));

	//设置视口和裁剪矩形
	mCommandList->RSSetViewports(1, &viewPort);
	mCommandList->RSSetScissorRects(1, &scissorRect);

	//清除后台缓冲区和深度缓冲区并赋值
	D3D12_CPU_DESCRIPTOR_HANDLE rtvHandle = CD3DX12_CPU_DESCRIPTOR_HANDLE(mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
		ref_mCurrentBackBuffer, mRtvDescriptorSize);//获得堆中描述符句柄
	mCommandList->ClearRenderTargetView(rtvHandle, DirectX::Colors::LightBlue, 0, nullptr);//清除RT背景色为暗红，且不设置裁剪矩形
	D3D12_CPU_DESCRIPTOR_HANDLE dsvHandle = mDsvHeap->GetCPUDescriptorHandleForHeapStart();
	mCommandList->ClearDepthStencilView(dsvHandle,//描述符句柄
		D3D12_CLEAR_FLAG_DEPTH | D3D12_CLEAR_FLAG_STENCIL,//Flag
		1.0f,//默认深度值
		0,//默认模板值
		0,//裁剪矩形数量
		nullptr);//裁剪矩形指针

	//指定将要渲染的缓冲区
	mCommandList->OMSetRenderTargets(1,//待绑定的RTV数量
		&rtvHandle,//指向RTV数组的指针
		true,//RTV对象在堆内存中是连续存放的
		&dsvHandle);//指向DSV的指针

	//渲染完成后，将后台缓冲区的状态改为呈现状态，使其之后推到前台缓冲区显示
	//然后关闭命令列表，等待传入命令队列
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mSwapChainBuffer[ref_mCurrentBackBuffer].Get(),
		D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT));
	ThrowIfFailed(mCommandList->Close());

	//等CPU将命令都准备好后，将待执行的命令列表加入GPU的命令队列
	ID3D12CommandList* commandLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(commandLists), commandLists);

	ThrowIfFailed(mSwapChain->Present(0, 0));//Present方法用于将后台缓冲区的内容呈现到屏幕上
	ref_mCurrentBackBuffer = (ref_mCurrentBackBuffer + 1) % 2;//更新前后台缓冲区索引
	FlushCmdQueue();//确保GPU完成所有命令
}

void D3D12App::CalculateFrameState()
{
	static int frameCnt = 0;//总帧数
	static float timeElapsed = 0.0f;//流逝时间
	frameCnt++;//每帧++

	if (gt.TotalTime() - timeElapsed >= 1.0f)//每过一秒满足一次条件
	{
		float fps = (float)frameCnt;
		float mspf = 1000.0f / fps;//每帧多少毫秒

		std::wstring fpsStr = std::to_wstring(fps);
		std::wstring mspfStr = std::to_wstring(mspf);

		std::wstring windowText = L"D3D12Init  fps:" + fpsStr + L"   " + L"mspf:" + mspfStr;
		SetWindowText(mhMainWnd, windowText.c_str());

		frameCnt = 0;
		timeElapsed += 1.0f;
	}
}

ID3D12Resource* D3D12App::CurrentBackBuffer()const
{
	return mSwapChainBuffer[mCurrentBackBuffer].Get();
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12App::CurrentBackBufferView()const
{
	return CD3DX12_CPU_DESCRIPTOR_HANDLE(
		mRtvHeap->GetCPUDescriptorHandleForHeapStart(),
		mCurrentBackBuffer,
		mRtvDescriptorSize
	);
}

D3D12_CPU_DESCRIPTOR_HANDLE D3D12App::DepthStencilView()const
{
	return mDsvHeap->GetCPUDescriptorHandleForHeapStart();
}

void D3D12App::OnResize()
{
	assert(md3dDevice);
	assert(mSwapChain);
	assert(mDirectCmdListAlloc);

	FlushCmdQueue();
	//函数重置命令列表，以便开始记录新的命令
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	//释放交换链缓冲区和深度模板缓冲区
	for (int i = 0; i < SwapChainBufferCount; i++)
		mSwapChainBuffer[i].Reset();
	mDepthStencilBuffer.Reset();

	//调整交换链缓冲区大小
	ThrowIfFailed(mSwapChain->ResizeBuffers(
		2,
		mClientWidth,
		mClientHeight,
		mBackBufferFormat,
		DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH));

	//将当前后台缓冲区的索引重置为 0，确保从第一个缓冲区开始渲染
	mCurrentBackBuffer = 0;

	//重新创建渲染目标视图（RTV）和深度模板视图（DSV）
	CreateRTV();
	CreateDSV();
	// 转换深度模板缓冲区的资源状态以便进行写入操作
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mDepthStencilBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_DEPTH_WRITE));

	// Execute the resize commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until resize is complete.
	FlushCmdQueue();

	CreateViewPortAndScissorRect();
}