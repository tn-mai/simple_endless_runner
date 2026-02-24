#ifndef EASYLIB_DX12_DEVICE_H
#define EASYLIB_DX12_DEVICE_H
#include "PSO.h"
#include <d3d12.h>
#include <dxgi1_6.h>
#include <wrl/client.h>
#include <string>
#include <memory>
#include <vector>

namespace EasyLib {
namespace DX12 {

class Device;
using DevicePtr = std::shared_ptr<Device>;

class Framebuffer;
using FramebufferPtr = std::shared_ptr<Framebuffer>;

class CommandQueue;
using CommandQueuePtr = std::shared_ptr<CommandQueue>;

class Texture;
using TexturePtr = std::shared_ptr<Texture>;

struct VertexBuffer
{
  Microsoft::WRL::ComPtr<ID3D12Resource> resource;
  D3D12_VERTEX_BUFFER_VIEW view;
};

struct IndexBuffer
{
  Microsoft::WRL::ComPtr<ID3D12Resource> resource;
  D3D12_INDEX_BUFFER_VIEW view;
};

/**
* リソースデスクリプタの抽象化
*
* Device::AllocateDescriptor で取得
*/
class Descriptor
{
  friend class Device;
  friend class DescriptorHeap;
public:
  Descriptor() = default;
  ~Descriptor();

  D3D12_CPU_DESCRIPTOR_HANDLE GetCPUHandle() const { return handleCPU; }

private:
  D3D12_CPU_DESCRIPTOR_HANDLE handleCPU;
  DevicePtr device;
  int index;
};
using DescriptorPtr = std::shared_ptr<Descriptor>;

/**
* デスクリプタヒープの抽象化
*/
class DescriptorHeap
{
  friend class Device;

public:
  DescriptorHeap() = default;
  DescriptorHeap(Microsoft::WRL::ComPtr<ID3D12DescriptorHeap>& heap, uint32_t handleSize) :
    heap(heap), handleSize(handleSize) {}
  ~DescriptorHeap() = default;
  DescriptorHeap(const DescriptorHeap&) = default;
  DescriptorHeap& operator=(const DescriptorHeap&) = default;
  DescriptorHeap(const DescriptorHeap&& other) : heap(std::move(other.heap)), handleSize(other.handleSize) {}
  DescriptorHeap& operator=(const DescriptorHeap&& other) {
    heap = std::move(other.heap);
    handleSize = other.handleSize;
    return *this;
  }

  D3D12_CPU_DESCRIPTOR_HANDLE GetCPUDescriptorHandle(size_t n) const {
    auto h = heap->GetCPUDescriptorHandleForHeapStart();
    h.ptr += n * handleSize;
    return h;
  }
  D3D12_GPU_DESCRIPTOR_HANDLE GetGPUDescriptorHandle(size_t n) const {
    auto h = heap->GetGPUDescriptorHandleForHeapStart();
    h.ptr += n * handleSize;
    return h;
  }
  ID3D12DescriptorHeap* GetHeap() const { return heap.Get(); }
  uint32_t GetHandleSize() const { return handleSize;  }
  void CopyHandle(int index, const DescriptorPtr& descriptor);

private:
  Microsoft::WRL::ComPtr<ID3D12DescriptorHeap> heap;
  uint32_t handleSize = 0;
};

/**
* 描画に必要なオブジェクトをまとめるクラス
*/
class GraphicsCommandContext
{
  friend class Device;
public:
  enum class ListType {
    Pre,
    Main,
    Post,
  };
  ID3D12GraphicsCommandList* GetList(ListType type) const { return list[static_cast<int>(type)].Get(); }
  ID3D12CommandAllocator* GetAllocator() const { return allocator.Get(); }
  void ResetAllocator() { allocator->Reset(); }
  void ResetList(ListType type) { list[static_cast<int>(type)]->Reset(allocator.Get(), nullptr); }
  void SetFenceValue(uint64_t newValue) { fenceValue = newValue; }
  void WaitForFence(const CommandQueuePtr&) const;

private:
  // アロケータは実行完了まで存続する必要があるためフレームバッファごとに必要
  // コマンドリストはExecuteCommadListに渡した段階でコマンドキュー側にコピーされるので、用途ごとにひとつで十分
  // ただし、マルチスレッド生成を考慮する場合はアロケータと1対1に作成するとよい
  // NOTE: DX12サンプルのMiniEngineでは1対1で、さらに必要に応じて作成する仕組みになっている
  DescriptorHeap slotCSU;
  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> allocator;
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> list[3];
  uint64_t fenceValue = 0;
};

enum RTVSlot
{
  RTVSLOT_FRAMEBUFFER_0,
  RTVSLOT_FRAMEBUFFER_1,
  RTVSLOT_FRAMEBUFFER_2,
};

enum DSVSlot
{
  DSVSLOT_FRAMEBUFFER,
};

/**
* D3Dデバイスを抽象化し、関連するオブジェクトの生成機能を追加したクラス
*
* NOTE: 動作はするが、まだ完成していない
* TODO: ID3D12Heapを使ったテクスチャメモリの管理を追加する
*/
class Device : public std::enable_shared_from_this<Device>
{
public:
  Device() {}
  ~Device() = default;
  Device(const Device&) = delete;
  Device& operator=(const Device&) = delete;

  enum class Result {
    Success, // 初期化成功
    False,   // 初期化失敗
    MemoryReservationFailed, // メモリ不足
  };
  Result Initialize(uint64_t memoryReservation);
  bool IsWarp() const { return isWarp; }

  DescriptorHeap CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT size, bool shaderVisibility);
  CommandQueuePtr CreateCommandQueue();
  FramebufferPtr CreateFramebuffer(
    CommandQueuePtr commandQueue, HWND hwnd, uint16_t width, uint16_t height, uint32_t bufferCount);
  VertexBuffer CreateVertexBuffer(const wchar_t* name, uint32_t size, uint32_t stride, const void* data);
  IndexBuffer CreateIndexBuffer(const wchar_t* name, uint32_t size, DXGI_FORMAT format, const void* data);
  PSOPtr CreatePipelineState(const wchar_t* filenameVS, const wchar_t* filenamePS,
    BlendMode blendMode, CullMode cullMode, DepthStencilMode depthStencilMode,
    const D3D12_INPUT_ELEMENT_DESC* vertexLayout, size_t layoutElementCount);

  // デスクリプタ操作
  UINT GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const;
  ID3D12DescriptorHeap* GetCSUDescriptorHeap() const;
  void CopyDescriptors(UINT size,
    D3D12_CPU_DESCRIPTOR_HANDLE destStart, D3D12_CPU_DESCRIPTOR_HANDLE srcStart, D3D12_DESCRIPTOR_HEAP_TYPE type);
  void SetDescriptorsToNull(UINT size, D3D12_CPU_DESCRIPTOR_HANDLE destStart);

  GraphicsCommandContext& GetCommandContext(int frameIndex) { return context[frameIndex]; }

  // テクスチャ操作
  TexturePtr LoadTexture(const wchar_t* filename);
  TexturePtr LoadTexture(const char* filename);
  uint64_t GetCopyableFootPrint(
    const D3D12_RESOURCE_DESC* desc, uint32_t firstSubresoruce, uint32_t numSubresources, uint64_t baseOffset);
  Microsoft::WRL::ComPtr<ID3D12Resource> CreateUploadResource(const wchar_t* name, UINT64 byteSize);
  Microsoft::WRL::ComPtr<ID3D12Resource> CreateTexture2DResource(
    const wchar_t* name, DXGI_FORMAT format, uint32_t width, uint32_t height, D3D12_RESOURCE_STATES state);
  void CreateShaderResourceView(ID3D12Resource* pResource,
    const D3D12_SHADER_RESOURCE_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE handle);

  Microsoft::WRL::ComPtr<ID3D12CommandAllocator> CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE type);
  Microsoft::WRL::ComPtr<ID3D12GraphicsCommandList> CreateCommandList(
    D3D12_COMMAND_LIST_TYPE type, ID3D12CommandAllocator* allocator);

  DescriptorPtr AllocateDescriptor();
  void DeallocateDescriptor(int n);

private:
  Microsoft::WRL::ComPtr<ID3D12Device> device;
  Microsoft::WRL::ComPtr<IDXGIFactory6> dxgiFactory;

  GraphicsCommandContext context[3];
  DescriptorHeap slotSampler;
  DescriptorHeap slotRTV;
  DescriptorHeap slotDSV;
  DescriptorHeap heapNull;

  struct HeapAllocator {
    std::vector<int> freeHandles;

    void Init(size_t size) {
      freeHandles.resize(size);
      for (int i = 0; i < static_cast<int>(size); i++) {
        freeHandles[i] = i;
      }
    }

    int Allocate() {
      int a = freeHandles.back();
      freeHandles.pop_back();
      return a;
    }

    void Deallocate(int a) {
      freeHandles.push_back(a);
    }
  };
  DescriptorHeap heapCSU;
  HeapAllocator heapAllocator;

  CommandQueuePtr uploadCommandQueue;

  bool isWarp = false;
};
using DevicePtr = std::shared_ptr<Device>;

std::wstring ToWString(const char* s);
std::wstring ToWString(const std::string& s);

} // namespace DX12
} // namespace EasyLib

#endif // EASYLIB_DX12_DEVICE_H
