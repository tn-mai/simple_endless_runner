#ifndef EASYLIBLIB_DX12_TEXTURE_H
#define EASYLIBLIB_DX12_TEXTURE_H
#include <d3d12.h>
#include <wincodec.h>
#include <wrl/client.h>
#include <vector>
#include <string>
#include <memory>

namespace EasyLib {
namespace DX12 {

class Device;
using DevicePtr = std::shared_ptr<Device>;

class Descriptor;
using DescriptorPtr = std::shared_ptr<Descriptor>;

class CommandQueue;
using CommandQueuePtr = std::shared_ptr<CommandQueue>;

/**
*
*/
class Texture
{
  friend class TextureLoader;

public:
  ~Texture() = default;

  ID3D12Resource* GetResource() const { return resource.Get(); }
  D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle() const;
  const DescriptorPtr& GetDescriptor() const { return descriptor; }
  uint32_t GetWidth() const;
  uint32_t GetHeight() const;

private:
  Texture() = default;

  Microsoft::WRL::ComPtr<ID3D12Resource> resource;
  DXGI_FORMAT format;
  DescriptorPtr descriptor;
  std::wstring name;
};
using TexturePtr = std::shared_ptr<Texture>;

/**
*
*/
class TextureLoader
{
public:
  TextureLoader() = default;
  ~TextureLoader() = default;
  bool Begin(DevicePtr device);
  bool Upload(const wchar_t* name, const D3D12_RESOURCE_DESC& desc, const void* data);
  bool UploadFromFile(const wchar_t* filename);
  std::vector<TexturePtr> End(CommandQueuePtr queue);

private:
  DevicePtr device;
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list;
  std::vector<Microsoft::WRL::ComPtr<ID3D12Resource>> trackedResources;
  std::vector<TexturePtr> textures;
  Microsoft::WRL::ComPtr<IWICImagingFactory> imagingFactory;
};
using TextureLoaderPtr = std::shared_ptr<TextureLoader>;

} // namespace DX12
} // namespace EasyLib

#endif // EASYLIBLIB_DX12_TEXTURE_H
