#define NOMINMAX
#include "Sprite.h"
#include "Texture.h"
#include <d3dx12.h>
#include <DirectXPackedVector.h>

namespace EasyLib {
namespace DX12 {

using Microsoft::WRL::ComPtr;
using namespace DirectX;
using namespace DirectX::PackedVector;

struct SpriteInShader
{
	uint32_t texID;
  XMFLOAT3 position;
  float rotation;
  XMFLOAT2 scale;
  XMFLOAT4 color;
};

/**
* スプライトレンダラーを初期化
*/
bool SpriteRenderer::Initialize(DevicePtr device, size_t framebufferCount, size_t maxSpriteCount)
{
	this->framebufferCount = framebufferCount;
	this->maxSpriteCount = maxSpriteCount;

	//pso = device->CreatePipelineState(L"SpriteShader.vs", L"SpriteShader.ps", vertexLayout, std::size(vertexLayout));
	pso = device->CreatePipelineState(L"res/shader/Sprite.hlsl", L"res/shader/Sprite.hlsl",
		BlendMode::Multiply, CullMode::None, DepthStencilMode::None, nullptr, 0);
	if (!pso) {
		return false;
	}

	for (int i = 0; i < static_cast<int>(framebufferCount); i++) {
		wchar_t bufferName[] = L"Sprite Buffer 0";
		bufferName[std::size(bufferName) - 1] += static_cast<wchar_t>(i);
		commandContexts[i].spriteBuffer = device->CreateUploadResource(bufferName, sizeof(SpriteInShader) * maxSpriteCount);
		commandContexts[i].allocator = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT);
		commandContexts[i].list = device->CreateCommandList(D3D12_COMMAND_LIST_TYPE_DIRECT, commandContexts[i].allocator.Get());
		commandContexts[i].heap = device->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024, true);

		if (!commandContexts[i].spriteBuffer || !commandContexts[i].allocator ||
			!commandContexts[i].list || !commandContexts[i].heap.GetHeap()) {
			return false;
		}

		// ヒープの0番目にはスプライトバッファを割り当てる
		// 1番目以降は描画時のテクスチャデスクリプタのコピー先として使う
		D3D12_SHADER_RESOURCE_VIEW_DESC view = 
			CD3DX12_SHADER_RESOURCE_VIEW_DESC::StructuredBuffer(static_cast<UINT>(maxSpriteCount), sizeof(SpriteInShader));
		D3D12_CPU_DESCRIPTOR_HANDLE heapHandle = commandContexts[i].heap.GetCPUDescriptorHandle(0);
		device->CreateShaderResourceView(commandContexts[i].spriteBuffer.Get(), &view, heapHandle);
	}

	indexBuffer = device->CreateUploadResource(L"Sprite Index Buffer", sizeof(uint16_t) * 6);
	if (!indexBuffer) {
		return false;
	}
	void* pIndexBuffer;
	const D3D12_RANGE indexBufferRange = { 0, 0 };
	indexBuffer->Map(0, &indexBufferRange, &pIndexBuffer);
	// 2-3
	// |\|
	// 0-1
	const uint16_t indices[] = { 0, 2, 1, 2, 3, 1 };
	memcpy(pIndexBuffer, indices, sizeof(indices));
	indexBuffer->Unmap(0, nullptr);

	viewIB.BufferLocation = indexBuffer->GetGPUVirtualAddress();
	viewIB.Format = DXGI_FORMAT_R16_UINT;
	viewIB.SizeInBytes = sizeof(indices);

	return true;
}

/**
* スプライト配列を描画するコマンドリストを作成
*/
ID3D12GraphicsCommandList* SpriteRenderer::Draw(
	DevicePtr device, const Sprite* pSprite, size_t count, const SpriteRenderingInfo& renderingInfo)
{
	count = std::min(count, maxSpriteCount);

	CommandContext& context = commandContexts[renderingInfo.framebufferIndex];

	if (count <= 0) {
		context.allocator->Reset();
		context.list->Reset(context.allocator.Get(), nullptr);
		context.list->Close();
 		return context.list.Get();
	}

	void* pTmp;
	const D3D12_RANGE range = { 0, 0 };
	context.spriteBuffer->Map(0, &range, &pTmp);

	uint32_t texCount = 0;
	TexturePtr lastTexture;
	SpriteInShader* p = static_cast<SpriteInShader*>(pTmp);
	for (uint32_t i = 0; i < count; i++) {
		// テクスチャが変わったらテクスチャを追加
		if (lastTexture != pSprite[i].texture) {
			lastTexture = pSprite[i].texture;
			D3D12_CPU_DESCRIPTOR_HANDLE handle = context.heap.GetCPUDescriptorHandle(texCount + 1);
			device->CopyDescriptors(1, handle, pSprite[i].texture->GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
			texCount++;
		}
		p[i].texID = texCount - 1;
		p[i].position = pSprite[i].position;
		p[i].position.y = renderingInfo.viewport.Height - p[i].position.y;
		p[i].rotation = pSprite[i].rotation;
		p[i].scale = pSprite[i].scale;
		p[i].color = pSprite[i].color;
	}
	device->SetDescriptorsToNull(1023 - texCount, context.heap.GetCPUDescriptorHandle(texCount + 1));

	context.spriteBuffer->Unmap(0, nullptr);
	curSpriteCount = count;

	context.allocator->Reset();
	context.list->Reset(context.allocator.Get(), nullptr);

	context.list->SetPipelineState(pso->GetPipelineStateObject());
	context.list->SetGraphicsRootSignature(pso->GetRootSignature());
  context.list->OMSetRenderTargets(1, &renderingInfo.handleRTV, FALSE, &renderingInfo.handleDSV);
  context.list->RSSetViewports(1, &renderingInfo.viewport);
  context.list->RSSetScissorRects(1, &renderingInfo.scissorRect);

	context.list->IASetIndexBuffer(&viewIB);
	context.list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ID3D12DescriptorHeap* heapList[] = { context.heap.GetHeap() };
	context.list->SetDescriptorHeaps(_countof(heapList), heapList);

	// ルートパラメータ0: ビュープロジェクション行列
	// NOTE: DirectXMath は行メジャー、 HLSL は列メジャー、なので、転置が必要
	// https://stackoverflow.com/questions/41405994/hlsl-mul-and-d3dxmatrix-order-mismatch
	const XMMATRIX matInvVP = XMMatrixTranspose(renderingInfo.matViewProjection);
	context.list->SetGraphicsRoot32BitConstants(0, 16, &matInvVP, 0);

	// ルートパラメータ1: スプライトデータ
	context.list->SetGraphicsRootDescriptorTable(1, context.heap.GetGPUDescriptorHandle(0));

	// ルートパラメータ2: テクスチャ
	context.list->SetGraphicsRootDescriptorTable(2, context.heap.GetGPUDescriptorHandle(1));

	context.list->DrawIndexedInstanced(6, static_cast<UINT>(count), 0, 0, 0);

  context.list->Close();
 	return context.list.Get();
}

} // namespace EasyLib
} // namespace DX12

