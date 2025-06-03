#pragma once
#include <Windows.h>
#include <wrl.h>
#include <dxgi1_4.h>
#include <d3d12.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <DirectXColors.h>
#include <DirectXCollision.h>
#include <string>
#include <memory>
#include <algorithm>
#include <vector>
#include <array>
#include <unordered_map>
#include <cstdint>
#include <fstream>
#include <sstream>
#include <windowsx.h>
#include <comdef.h>
#include "..\utils\d3dx12.h"
#include "..\utils\MathHelper.h"

using namespace DirectX;
using namespace Microsoft::WRL;

UINT CalcConstantBufferByteSize(UINT byteSize);

ComPtr<ID3DBlob> CompileShader(
	const std::wstring& filename,
	const D3D_SHADER_MACRO* defines,
	const std::string& entrypoint,
	const std::string& target
);

#define MaxLights 16

struct Light
{
	XMFLOAT3 Strength = { 0.5f, 0.5f, 0.5f };//光源颜色
	float FalloffStart = 1.0f;//开始衰减距离
	XMFLOAT3 Direction = { 0.0f, -1.0f, 0.0f };//方向光和聚光灯的方向向量
	float FalloffEnd = 10.0f;//点光源和聚光灯的衰减结束距离
	XMFLOAT3 Position = { 0.0f, 0.0f, 0.0f };//点光和聚光灯的坐标
	float SpotPower = 64.0f;//聚光因子中的参数
};

struct Material
{
	std::string Name;
	int MatCBIndex = -1;
	int DiffuseSrvHeapIndex = -1;
	int NumFramesDirty = 3;
	XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 0.25;
	XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};

struct MaterialConstants
{
	XMFLOAT4 DiffuseAlbedo = { 1.0f, 1.0f, 1.0f, 1.0f };
	XMFLOAT3 FresnelR0 = { 0.01f, 0.01f, 0.01f };
	float Roughness = 0.25;
	XMFLOAT4X4 MatTransform = MathHelper::Identity4x4();
};

struct Texture
{
	// Unique material name for lookup.
	std::string Name;

	std::wstring Filename;

	Microsoft::WRL::ComPtr<ID3D12Resource> Resource = nullptr;
	Microsoft::WRL::ComPtr<ID3D12Resource> UploadHeap = nullptr;
};

inline std::wstring AnsiToWString(const std::string& str)
{
	WCHAR buffer[512];
	MultiByteToWideChar(CP_ACP, 0, str.c_str(), -1, buffer, 512);
	return std::wstring(buffer);
}

class DxException
{
public:
	DxException() = default;
	DxException(HRESULT hr, const std::wstring& functionName, const std::wstring& filename, int lineNumber);

	std::wstring ToString()const;

	HRESULT ErrorCode = S_OK;
	std::wstring FunctionName;
	std::wstring Filename;
	int LineNumber = -1;
};

//L#x是一个字符串化操作符，将x转化为宽字符串常量
#ifndef ThrowIfFailed
#define ThrowIfFailed(x)                                              \
{                                                                     \
    HRESULT hr__ = (x);                                               \
    std::wstring wfn = AnsiToWString(__FILE__);                       \
    if(FAILED(hr__)) { throw DxException(hr__, L#x, wfn, __LINE__); } \
}
#endif