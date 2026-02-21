/**
* @file Font.h
*/
#ifndef EASYLIB_DX12_FONT_H
#define EASYLIB_DX12_FONT_H
#include "Texture.h"
#include "Device.h"
#include <d3d12.h>
#include <DirectXMath.h>
#include <DirectXPackedVector.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <memory>

namespace EasyLib {
namespace DX12 {

struct Text
{
  std::wstring text;
  DirectX::XMFLOAT2 position;
  DirectX::XMFLOAT2 scale;
  DirectX::XMFLOAT4 color;
};

struct FontRenderingInfo
{
  DirectX::XMMATRIX matViewProjection;
  D3D12_CPU_DESCRIPTOR_HANDLE handleRTV;
  D3D12_CPU_DESCRIPTOR_HANDLE handleDSV;
  D3D12_VIEWPORT viewport;
  D3D12_RECT scissorRect;
  int framebufferIndex;
};

/**
* ビットマップフォント描画クラス
*/
class FontRenderer
{
public:
  FontRenderer() = default;
  ~FontRenderer() = default;
  FontRenderer(const FontRenderer&) = delete;
  FontRenderer& operator=(const FontRenderer&) = delete;

  bool Initialize(DevicePtr device, size_t framebufferCount, size_t capacity);
  bool LoadFromFile(const char* filename);

  ID3D12GraphicsCommandList* Draw(const Text* p, size_t count, const FontRenderingInfo& renderingInfo);

  void Scale(const DirectX::XMFLOAT2& s) { scale = s; }
  const DirectX::XMFLOAT2& Scale() const { return scale; }
  void Color(const DirectX::XMFLOAT4& c);
  DirectX::XMFLOAT4 Color() const;
  void SubColor(const DirectX::XMFLOAT4& c);
  DirectX::XMFLOAT4 SubColor() const;
  void Thickness(float t) { thickness = t; }
  float Thickness() const { return thickness; }
  void Border(float b) { border = b; }
  float Border() const { return border; }
  void Propotional(bool b) { propotional = b; }
  bool Propotional() const { return propotional; }
  void XAdvance(float x) { fixedAdvance = x; }
  float XAdvance() const { return fixedAdvance; }
  DirectX::XMFLOAT2 CalcStringSize(const wchar_t* str) const;

private:
  DevicePtr device;
  PSOPtr pso;

  enum HeapID
  {
    HeapID_FontInfo,
    HeapID_Character0,
    HeapID_Character1,
    HeapID_Character2,
    HeapID_Texture0,
    HeapID_Texture1,
    num_HeapID,
  };
  DescriptorHeap heap;

  struct CommandContext {
    Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
    Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list;
    Microsoft::WRL::ComPtr<ID3D12Resource> characterBuffer;
  };
  CommandContext commandContexts[3];
  Microsoft::WRL::ComPtr<ID3D12Resource> indexBuffer;
  Microsoft::WRL::ComPtr<ID3D12Resource> fontBuffer;

  D3D12_INDEX_BUFFER_VIEW viewIB;
  size_t framebufferCount = 0;
  size_t capacity = 0;

  struct FontInfo {
    int id = -1;
    int page = 0;
    DirectX::XMFLOAT2 uv[2];
    DirectX::XMFLOAT2 size;
    DirectX::XMFLOAT2 offset;
    float xadvance = 0;
  };
  std::vector<FontInfo> fontList;
  std::vector<TexturePtr> texList;
  DirectX::XMFLOAT2 reciprocalScreenSize;
  float fontHeight;
  int paddingUp = 0;
  int paddingRight = 0;
  int paddingLeft = 0;
  int paddingDown = 0;

  DirectX::XMFLOAT2 scale = { 1, 1 };
  DirectX::XMFLOAT4 color = { 1, 1, 1, 1 };
  DirectX::XMFLOAT4 subColor = { 0.125f, 0.125f, 0.125f, 1 };
  float thickness = 0.5f; // 縁取りの内側の位置(0.0=最外縁 1.0=最内縁)
  float border = 0.25f; // 縁取りの外側の位置(0.0=最外縁 1.0=最内縁)
  bool propotional = true;
  float fixedAdvance = 0;
};

} // namespace EasyLib
} // namespace DX12

#endif // EASYLIB_DX12_FONT_H