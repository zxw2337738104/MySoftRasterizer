#pragma once

#include "DXHelper.h"

template<typename T>
class UploadBufferResource
{
public:
	UploadBufferResource(Microsoft::WRL::ComPtr<ID3D12Device> d3dDevice, UINT elementCount, bool isConstantBuffer) : mIsConstantBuffer(isConstantBuffer)
	{
		elementByteSize = sizeof(T);

		if (isConstantBuffer)
			elementByteSize = CalcConstantBufferByteSize(sizeof(T));//如果是常量缓冲，以256的倍数计算缓存大小

		//创建上传堆和资源
		ThrowIfFailed(d3dDevice->CreateCommittedResource(
			&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_UPLOAD),
			D3D12_HEAP_FLAG_NONE,
			&CD3DX12_RESOURCE_DESC::Buffer(elementByteSize * elementCount),
			D3D12_RESOURCE_STATE_GENERIC_READ,
			nullptr,
			IID_PPV_ARGS(&uploadBuffer)));

		//返回欲更新资源的指针
		ThrowIfFailed(uploadBuffer->Map(0,//子资源索引，缓冲区的子资源就是自己
			nullptr,//对整个资源进行映射
			reinterpret_cast<void**>(&mappedData)));//返回待映射资源数据的目标内存块
	}
	~UploadBufferResource()
	{
		if (uploadBuffer != nullptr)
			uploadBuffer->Unmap(0, nullptr);//取消映射
		mappedData = nullptr;//释放映射内存
	}

	//将CPU上的常量数据复制到GPU的缓冲区中
	void CopyData(int elementIndex, const T& Data)
	{
		memcpy(&mappedData[elementIndex * elementByteSize], &Data, sizeof(T));
	}

	//返回创建的上传堆的指针
	Microsoft::WRL::ComPtr<ID3D12Resource> Resource()const
	{
		return uploadBuffer;
	}
private:
	Microsoft::WRL::ComPtr<ID3D12Resource> uploadBuffer;
	UINT elementByteSize = 0;
	//mappedData是一个指向上传缓冲区映射后内存区域的指针
	//为CPU提供了一种直接访问和修改GPU缓冲区数据的途径
	BYTE* mappedData = nullptr;
	bool mIsConstantBuffer;
};