#include "Framebuffer.h"
#include <d3dx12.h>

namespace EasyLib {
namespace DX12 {

/**
* 描画先バックバッファの番号を取得
*/
uint32_t Framebuffer::GetCurrentBackBufferIndex() const
{
  return swapChain->GetCurrentBackBufferIndex();
}

/**
* 状態遷移バリアを取得
*/
D3D12_RESOURCE_BARRIER Framebuffer::GetTransitionBarrier(
  uint32_t index, D3D12_RESOURCE_STATES stateBegin, D3D12_RESOURCE_STATES stateAfter) const
{
  return CD3DX12_RESOURCE_BARRIER::Transition(renderTargets[index].resource.Get(), stateBegin, stateAfter);
}

/**
* バッファをスワップ
*/
uint32_t Framebuffer::Present(uint32_t syncInterval, uint32_t flags) const
{
  swapChain->Present(syncInterval, flags);
  return swapChain->GetCurrentBackBufferIndex();
}

/**
* コンストラクタ
*/
Framebuffer::Framebuffer(
  Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain,
  Resource* renderTargets, Resource* depthStencil,
  uint16_t width, uint16_t height, uint32_t bufferCount)
{
  this->swapChain = swapChain;
  for (uint32_t i = 0; i < bufferCount; i++) {
    this->renderTargets[i] = renderTargets[i];
  }
  this->depthStencil = *depthStencil;
  this->width = width;
  this->height = height;
  this->bufferCount = bufferCount;
}

} // namespace DX12
} // namespace EasyLib
