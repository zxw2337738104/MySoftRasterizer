#pragma once
#include "D3D12App.h"

using Microsoft::WRL::ComPtr;

//定义一个几何体的子物体，用于混合的情况
struct SubmeshGeometry {
	UINT IndexCount = 0;//索引数量
	UINT StartIndexLocation = 0;//基准索引位置
	INT BaseVertexLocation = 0;//基准顶点位置
};

//几何体
struct MeshGeometry {
	//给MeshGeometry起一个名字，便于通过关联容器等方式对其进行查找
	std::string Name;

	//系统变量内存的复制样本，使用Blob是因为vertex/index可以用自定义方式来对其进行查找
	ComPtr<ID3DBlob> VertexBufferCPU = nullptr;
	ComPtr<ID3DBlob> IndexBufferCPU = nullptr;

	//上传到GPU的顶点/索引资源
	ComPtr<ID3D12Resource> VertexBufferGPU = nullptr;
	ComPtr<ID3D12Resource> IndexBufferGPU = nullptr;

	//顶点/索引的上传堆资源
	ComPtr<ID3D12Resource> VertexBufferUploader = nullptr;
	ComPtr<ID3D12Resource> IndexBufferUploader = nullptr;

	//缓冲区相关数据
	UINT VertexByteStride = 0;//顶点缓冲区单个byte
	UINT VertexBufferByteSize = 0;//顶点缓冲区总byte
	DXGI_FORMAT IndexFormat = DXGI_FORMAT_R16_UINT;//数据索引格式
	UINT IndexBufferByteSize = 0;//数据索引总byte

	//一个MeshGeometry可以在一个全局顶点/索引缓冲区存储多个几何体
	//使用该容器来定义Submesh的几何图形，我们就可以利用submesh的name来查找并它进行单独绘制
	std::unordered_map<std::string, SubmeshGeometry> DrawArgs;

	//获取顶点缓冲视图
	D3D12_VERTEX_BUFFER_VIEW VertexBufferView()const {
		D3D12_VERTEX_BUFFER_VIEW vbv;
		vbv.BufferLocation = VertexBufferGPU->GetGPUVirtualAddress();
		vbv.SizeInBytes = VertexBufferByteSize;
		vbv.StrideInBytes = VertexByteStride;

		return vbv;
	}

	//获取索引缓冲视图
	D3D12_INDEX_BUFFER_VIEW IndexBufferView()const {
		D3D12_INDEX_BUFFER_VIEW ibv;
		ibv.BufferLocation = IndexBufferGPU->GetGPUVirtualAddress();
		ibv.Format = IndexFormat;
		ibv.SizeInBytes = IndexBufferByteSize;

		return ibv;
	}

	//数据上传后释放掉上传堆的内存
	void DisposUploaders() {
		VertexBufferUploader = nullptr;
		IndexBufferUploader = nullptr;
	}
};