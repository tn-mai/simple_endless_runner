#ifndef EASYLIB_DX12_SPRITE_H
#define EASYLIB_DX12_SPRITE_H
#include "Texture.h"
#include "PSO.h"
#include "Device.h"
#include <d3d12.h>
#include <DirectXMath.h>
#include <wrl/client.h>

namespace EasyLib {
namespace DX12 {

/**
*
*/
struct Sprite
{
  TexturePtr texture;
  DirectX::XMFLOAT3 position;
  float rotation;
  DirectX::XMFLOAT2 scale;
  DirectX::XMFLOAT4 color;
};

struct SpriteRenderingInfo
{
  DirectX::XMMATRIX matViewProjection;
  D3D12_CPU_DESCRIPTOR_HANDLE handleRTV;
  D3D12_CPU_DESCRIPTOR_HANDLE handleDSV;
  D3D12_VIEWPORT viewport;
  D3D12_RECT scissorRect;
  int framebufferIndex;
};

/**
* スプライト描画クラス
*
* 独自のPSOやコマンドリストを持つ
* Draw関数が返すコマンドリストをキューに積んで実行する
*/
class SpriteRenderer
{
public:
  SpriteRenderer() = default;
  ~SpriteRenderer() = default;

  bool Initialize(DevicePtr device, size_t framebufferCount, size_t maxSpriteCount);

  ID3D12GraphicsCommandList* Draw(
    DevicePtr device, const Sprite* p, size_t count, const SpriteRenderingInfo& renderingInfo);

private:
  std::shared_ptr<PSO> pso;
  struct CommandContext {
    DescriptorHeap heap;
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list;
    Microsoft::WRL::ComPtr<ID3D12Resource> spriteBuffer;
  };
  CommandContext commandContexts[3];
  Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;

  D3D12_INDEX_BUFFER_VIEW viewIB;
  size_t framebufferCount = 0;
  size_t maxSpriteCount = 0;
  size_t curSpriteCount = 0;
};

} // namespace EasyLib
} // namespace DX12


#endif // EASYLIB_DX12_SPRITE_H
