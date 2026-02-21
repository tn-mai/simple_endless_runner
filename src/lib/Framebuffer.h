#ifndef EASYLIB_DX12_FRAMEBUFFER_H
#define EASYLIB_DX12_FRAMEBUFFER_H
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <stdint.h>
#include <memory>

namespace EasyLib {
namespace DX12 {

class Framebuffer
{
  friend class Device;

public:
  struct Resource {
    Microsoft::WRL::ComPtr<ID3D12Resource> resource;
    D3D12_CPU_DESCRIPTOR_HANDLE handle;
  };

  Framebuffer() = default;
  ~Framebuffer() = default;

  uint32_t GetBufferCount() const { return bufferCount; }
  uint32_t GetCurrentBackBufferIndex() const;
  const D3D12_CPU_DESCRIPTOR_HANDLE& GetRenderTargetHandle(int i) const { return renderTargets[i].handle; }
  const D3D12_CPU_DESCRIPTOR_HANDLE& GetDepthStencilHandle() const { return depthStencil.handle; }
  D3D12_RESOURCE_BARRIER GetTransitionBarrier(
    uint32_t index, D3D12_RESOURCE_STATES stateBegin, D3D12_RESOURCE_STATES stateAfter) const;
  uint32_t Present(uint32_t syncInterval, uint32_t flags) const;

  uint16_t GetWidth() const { return width; }
  uint16_t GetHeight() const { return height; }

private:
  Framebuffer(
    Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain,
    Resource* renderTargets, Resource* depthStencil,
    uint16_t width, uint16_t height, uint32_t bufferCount);

  Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain;
  Resource renderTargets[3];
  Resource depthStencil;
  uint16_t width = 1280;
  uint16_t height = 720;
  uint32_t bufferCount = 3;
};
using FramebufferPtr = std::shared_ptr<Framebuffer>;

} // namespace DX12
} // namespace EasyLib

#endif // EASYLIB_DX12_FRAMEBUFFER_H

