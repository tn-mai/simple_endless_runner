#include "Device.h"
#include "PSO.h"
#include "CommandQueue.h"
#include "Framebuffer.h"
#include "Texture.h"
#include <d3dx12.h>
#include <d3dcompiler.h>

namespace EasyLib {
namespace DX12 {

using Microsoft::WRL::ComPtr;

/**
* デストラクタ
*/
Descriptor::~Descriptor()
{
  if (device) {
    device->DeallocateDescriptor(index);
  }
}

/**
* デスクリプタハンドルをデスクリプタヒープにコピー
* 
* @param index      コピー先のインデックス
* @param descriptor コピーするデスクリプタ
*/
void DescriptorHeap::CopyHandle(int index, const DescriptorPtr& descriptor)
{
  descriptor->device->CopyDescriptors(1, GetCPUDescriptorHandle(index), descriptor->handleCPU, heap->GetDesc().Type);
}

/**
* デバイスを初期化
* 
* @param memoryReservation 必要なGPUメモリのバイト数
*/
Device::Result Device::Initialize(uint64_t memoryReservation)
{
  UINT dxgiFactoryFlags = 0;

#ifndef NDEBUG
  // デバッグレイヤーの有効化
  ComPtr<ID3D12Debug> debugController;
  if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController)))) {
    debugController->EnableDebugLayer();
    dxgiFactoryFlags |= DXGI_CREATE_FACTORY_DEBUG;
  }
#endif // NDEBUG

  if (FAILED(CreateDXGIFactory2(dxgiFactoryFlags, IID_PPV_ARGS(&dxgiFactory)))) {
    return Result::False;
  }

  ComPtr<IDXGIAdapter4> dxgiAdapter;
  bool foundAdapter = false;
  for (UINT adapterIndex = 0; ; ++adapterIndex) {
    if (FAILED(dxgiFactory->EnumAdapterByGpuPreference(
      adapterIndex,
      DXGI_GPU_PREFERENCE_HIGH_PERFORMANCE,// DXGI_GPU_PREFERENCE_UNSPECIFIED,
      IID_PPV_ARGS(&dxgiAdapter)))) {
      break;
    }
    DXGI_ADAPTER_DESC1 desc;
    dxgiAdapter->GetDesc1(&desc);

    if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE) {
      // ソフトウェアデバイスはWARPデバイスとして作る
      continue;
    }

    // 機能レベルを満たすデバイスかどうかを確認
    if (SUCCEEDED(D3D12CreateDevice(dxgiAdapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr))) {
      // 機能レベルを満たすので作成に進む
      foundAdapter = true;
      break;
    }
  }

  if (!foundAdapter) {
    // 機能レベル11を満たすハードウェアが見つからない場合、WARPデバイスを選択
    if (FAILED(dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&dxgiAdapter)))) {
      return Result::False;
    }
    isWarp = true;
  }

  {
    const HRESULT hr = dxgiAdapter->SetVideoMemoryReservation(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, memoryReservation);
    if (hr != S_OK) {
      return Result::MemoryReservationFailed;
    }
    //DXGI_QUERY_VIDEO_MEMORY_INFO vramInfo;
    //dxgiAdapter->QueryVideoMemoryInfo(0, DXGI_MEMORY_SEGMENT_GROUP_LOCAL, &vramInfo);
  }

  // デバイスを作成
  if (FAILED(D3D12CreateDevice(dxgiAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&device)))) {
    return Result::False;
  }

  for (auto& e : context) {
    e.allocator = CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT);
    e.list[0] = CreateCommandList(D3D12_COMMAND_LIST_TYPE_DIRECT, e.allocator.Get());
    e.list[1] = CreateCommandList(D3D12_COMMAND_LIST_TYPE_DIRECT, e.allocator.Get());
    e.list[2] = CreateCommandList(D3D12_COMMAND_LIST_TYPE_DIRECT, e.allocator.Get());
    e.slotCSU = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 64, true);
  }
  slotSampler = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_SAMPLER, 16, true);
  slotRTV = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_RTV, 16, false);
  slotDSV = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_DSV, 1, false);

  const D3D12_SHADER_RESOURCE_VIEW_DESC nullDesc = CD3DX12_SHADER_RESOURCE_VIEW_DESC::Tex2D(DXGI_FORMAT_R8G8B8A8_UNORM);
  heapNull = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1, false);
  CreateShaderResourceView(nullptr, &nullDesc, heapNull.GetCPUDescriptorHandle(0));

  heapCSU = CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, 1024, false);
  heapAllocator.Init(1024);


  uploadCommandQueue = CreateCommandQueue();

  return Result::Success;
}

/**
* データをGPU二アップロードするための中間リソースを作成
*
* @param name     デバッグ用のリソース名
* @param byteSize リソースのバイトサイズ
*/
ComPtr<ID3D12Resource> Device::CreateUploadResource(const wchar_t* name, UINT64 byteSize)
{
  ComPtr<ID3D12Resource> resource;
  const CD3DX12_HEAP_PROPERTIES properties(D3D12_HEAP_TYPE_UPLOAD);
  const CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Buffer(byteSize);
	if (FAILED(device->CreateCommittedResource(
		&properties, D3D12_HEAP_FLAG_NONE, &desc, D3D12_RESOURCE_STATE_GENERIC_READ, nullptr,
    IID_PPV_ARGS(&resource)))) { //UPLOADヒープはGENERIC_READを設定しないとダメ
		return nullptr;
	}
	resource->SetName(name);
  return resource;
}

/**
* テクスチャを作成
*
* @param filename テクスチャファイル名(UTF-16)
*/
TexturePtr Device::LoadTexture(const wchar_t* filename)
{
  TextureLoader loader;
  loader.Begin(shared_from_this());
  loader.UploadFromFile(filename);
  auto textures = loader.End(uploadCommandQueue);
  if (textures.empty()) {
    return nullptr;
  }
  return textures[0];
}

/**
* テクスチャを作成
*
* @param filename テクスチャファイル名(SJIS)
*/
TexturePtr Device::LoadTexture(const char* filename)
{
  const std::wstring ws = ToWString(filename);
  return LoadTexture(ws.c_str());
}

/**
* リソースのコピーに必要なバイト数を計算
* 
* @param desc              リソース情報
* @param firstSubresource  desc に含まれる最初のサブリソースのインデックス
* @param numSubresources   コピーするサブリソースの数
* @param baseOffset        サブリソースのコピー位置を決めるバイトオフセット
*/
uint64_t Device::GetCopyableFootPrint(
  const D3D12_RESOURCE_DESC* desc, uint32_t firstSubresoruce, uint32_t numSubresources, uint64_t baseOffset)
{
  uint64_t size;
  device->GetCopyableFootprints(desc, firstSubresoruce, numSubresources, baseOffset, nullptr, nullptr, nullptr, &size);
  return size;
}

/**
* 2Dテクスチャ用のリソースを作成
*
* @param name    デバッグ用のリソース名
* @param format  テクスチャ形式
* @param width   テクスチャの幅
* @param height  テクスチャの高さ
* @param state   リソースの初期状態
*/
ComPtr<ID3D12Resource> Device::CreateTexture2DResource(
  const wchar_t* name, DXGI_FORMAT format, uint32_t width, uint32_t height, D3D12_RESOURCE_STATES state)
{
  ComPtr<ID3D12Resource> resource;
  const CD3DX12_HEAP_PROPERTIES properties(D3D12_HEAP_TYPE_DEFAULT);
  const D3D12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(format, width, height, 1, 1);
  if (FAILED(device->CreateCommittedResource(
    &properties, D3D12_HEAP_FLAG_NONE, &desc, state, nullptr, IID_PPV_ARGS(&resource)))) {
    return nullptr;
  }
  resource->SetName(name);
  return resource;
}

/**
* デスクリプタヒープを作成
*
* @param type             ヒープの種類
* @param size             作成するハンドル数
* @param shaderVisibility シェーダから見えるようにする場合は true 、そうでなければ false
*/
DescriptorHeap Device::CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE type, UINT size, bool shaderVisibility)
{
  D3D12_DESCRIPTOR_HEAP_FLAGS flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
  if (shaderVisibility) {
    flags |= D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
  }
  const D3D12_DESCRIPTOR_HEAP_DESC desc = { type, size, flags, 0 };
  ComPtr<ID3D12DescriptorHeap> heap;
  device->CreateDescriptorHeap(&desc, IID_PPV_ARGS(&heap));
  return { heap, device->GetDescriptorHandleIncrementSize(type) };
}

/**
* コマンドキューを作成
*/
CommandQueuePtr Device::CreateCommandQueue()
{
  ComPtr<ID3D12CommandQueue> queue;
  const D3D12_COMMAND_QUEUE_DESC cqDesc = {};
  device->CreateCommandQueue(&cqDesc, IID_PPV_ARGS(&queue));

  ComPtr<ID3D12Fence> fence;
  device->CreateFence(0, D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&fence));

  // make_sharedからアクセスできるようにする
  struct Impl : CommandQueue {
    Impl(Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue, Microsoft::WRL::ComPtr<ID3D12Fence> fence) :
      CommandQueue(queue, fence)
    {}
  };
  return std::make_shared<Impl>(queue, fence);
}

/**
* スワップチェーンを備えたフレームバッファを作成
*
* @param commandQueue フレームバッファのスワップに使うコマンドキュー
* @param hwnd         スワップチェーンに関連付けられるウィンドウのハンドル
* @param width        フレームバッファの幅
* @param height       フレームバッファの高さ
* @param bufferCount  フレームバッファの数(通常2または3を指定)
*/
FramebufferPtr Device::CreateFramebuffer(
  CommandQueuePtr commandQueue, HWND hwnd, uint16_t width, uint16_t height, uint32_t bufferCount)
{
  // スワップチェーンを作成
  DXGI_SWAP_CHAIN_DESC1 scDesc = {};
  scDesc.Width = width;
  scDesc.Height = height;
  scDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
  scDesc.SampleDesc.Count = 1;
  scDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
  scDesc.BufferCount = bufferCount;
  scDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;
  ComPtr<IDXGISwapChain1> tmpSwapChain;
  if (FAILED(dxgiFactory->CreateSwapChainForHwnd(commandQueue->GetQueue(), hwnd, &scDesc, nullptr, nullptr, &tmpSwapChain))) {
    return nullptr;
  }
  ComPtr<IDXGISwapChain3> swapChain;
  tmpSwapChain.As(&swapChain);

  // レンダーターゲットを作成
  Framebuffer::Resource renderTargets[3];
  for (uint32_t i = 0; i < bufferCount; ++i) {
    if (FAILED(swapChain->GetBuffer(i, IID_PPV_ARGS(&renderTargets[i].resource)))) {
      return nullptr;
    }
    renderTargets[i].handle = slotRTV.GetCPUDescriptorHandle(i);
    wchar_t name[] = L"Render Target 0";
    name[14] += static_cast<wchar_t>(i);
    renderTargets[i].resource->SetName(name);
    device->CreateRenderTargetView(renderTargets[i].resource.Get(), nullptr, renderTargets[i].handle);
  }

  // 深度ステンシルバッファを作成
  Framebuffer::Resource depthStencil;
  const CD3DX12_HEAP_PROPERTIES properties(D3D12_HEAP_TYPE_DEFAULT);
  const CD3DX12_RESOURCE_DESC desc = CD3DX12_RESOURCE_DESC::Tex2D(
    DXGI_FORMAT_D32_FLOAT, width, height, 1, 0, 1, 0, D3D12_RESOURCE_FLAG_ALLOW_DEPTH_STENCIL);
  const CD3DX12_CLEAR_VALUE clearValue(desc.Format, 1.0f, 0);
	if (FAILED(device->CreateCommittedResource(
		&properties,
		D3D12_HEAP_FLAG_NONE,
		&desc,
		D3D12_RESOURCE_STATE_DEPTH_WRITE,
		&clearValue,
		IID_PPV_ARGS(&depthStencil.resource)))) {
		return nullptr;
	}
	depthStencil.resource->SetName(L"Depth Stencil Buffer");

  depthStencil.handle = slotDSV.GetCPUDescriptorHandle(DSVSLOT_FRAMEBUFFER);
  device->CreateDepthStencilView(depthStencil.resource.Get(), nullptr, depthStencil.handle);

  struct Impl : Framebuffer {
    Impl(Microsoft::WRL::ComPtr<IDXGISwapChain3> swapChain,
      Framebuffer::Resource* renderTargets, Framebuffer::Resource* depthStencil,
      uint16_t w, uint16_t h, uint32_t bufferCount) :
      Framebuffer(swapChain, renderTargets, depthStencil, w, h, bufferCount)
    {}
  };
  return std::make_shared<Impl>(swapChain, renderTargets, &depthStencil, width, height, bufferCount);
}

/**
* 頂点バッファを作成
*
* @param name   デバッグ用のリソース名
* @param size   バッファのバイトサイズ
* @param stride ひとつの頂点データのバイト数
* @param data   コピーする頂点データ(データがなければ nullptr を指定)
*/
VertexBuffer Device::CreateVertexBuffer(
  const wchar_t* name, uint32_t size, uint32_t stride, const void* data)
{
  VertexBuffer vb;
  vb.resource = CreateUploadResource(name, size);

  if (data) {
    void* p;
    const D3D12_RANGE readRange = { 0, 0 };
    if (SUCCEEDED(vb.resource->Map(0, &readRange, &p))) {
      memcpy(p, data, size);
      vb.resource->Unmap(0, nullptr);
    }
  }

	vb.view.BufferLocation = vb.resource->GetGPUVirtualAddress();
	vb.view.StrideInBytes = stride;
	vb.view.SizeInBytes = size;

  return vb;
}

/**
* インデックスバッファを作成
*
* @param name   デバッグ用のリソース名
* @param size   バッファのバイトサイズ
* @param format インデックス形式
* @param data   コピーする頂点データ(データがなければ nullptr を指定)
*/
IndexBuffer Device::CreateIndexBuffer(
  const wchar_t* name, uint32_t size, DXGI_FORMAT format, const void* data)
{
  IndexBuffer ib;
  ib.resource = CreateUploadResource(name, size);

  if (data) {
    void* p;
    const D3D12_RANGE readRange = { 0, 0 };
    if (SUCCEEDED(ib.resource->Map(0, &readRange, &p))) {
      memcpy(p, data, size);
      ib.resource->Unmap(0, nullptr);
    }
  }

	ib.view.BufferLocation = ib.resource->GetGPUVirtualAddress();
	ib.view.Format = format;
	ib.view.SizeInBytes = size;

  return ib;
}

/**
* デスクリプタハンドルのバイトサイズを取得
*
* @param type 問い合わせるデスクリプタヒープの種類
*/
UINT Device::GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE type) const
{
  return device->GetDescriptorHandleIncrementSize(type);
}

/**
* テクスチャ用のデスクリプタヒープを取得する
*/
ID3D12DescriptorHeap* Device::GetCSUDescriptorHeap() const
{
  return heapCSU.GetHeap();
}

/**
* コマンドアロケータを作成
*
* @param type 作成するアロケータの種類
*/
ComPtr<ID3D12CommandAllocator> Device::CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE type)
{
  ComPtr<ID3D12CommandAllocator> allocator;
  device->CreateCommandAllocator(type, IID_PPV_ARGS(&allocator));
  return allocator;
}

/**
* コマンドリストを作成
*
* @param type       作成するリストの種類
* @param allocator  使用するアロケータ
*/
ComPtr<ID3D12GraphicsCommandList> Device::CreateCommandList(
  D3D12_COMMAND_LIST_TYPE type, ID3D12CommandAllocator* allocator)
{
  ComPtr<ID3D12GraphicsCommandList> commandList;
  device->CreateCommandList(0, type, allocator, nullptr, IID_PPV_ARGS(&commandList));

  // 作成直後はオープン状態なので、クローズしておく
  if (commandList) {
    commandList->Close();
  }
  return commandList;
}

/**
* パイプラインステートオブジェクトを作成
*
* @param filenameVS          頂点シェーダファイル名
* @param filenamePS          ピクセルシェーダファイル名
* @param blendMode           使用するブレンドモード
* @param cullMode            使用するカリングモード
* @param depthStencilMode    使用する深度ステンシルモード
* @param vertexLayout        頂点データレイアウト配列
* @param layoutElementCount  レイアウト配列の長さ
*/
std::shared_ptr<PSO> Device::CreatePipelineState(
	const wchar_t* filenameVS, const wchar_t* filenamePS,
  BlendMode blendMode, CullMode cullMode, DepthStencilMode depthStencilMode,
  const D3D12_INPUT_ELEMENT_DESC* vertexLayout, size_t layoutElementCount)
{
  auto pso = std::make_shared<PSO>();
  pso->Initialize(device.Get(), isWarp, filenameVS, filenamePS,
    blendMode, cullMode, depthStencilMode, vertexLayout, layoutElementCount);
  return pso;
}

/**
* シェーダリソースビューを作成
*
* @param pResource  元になるリソース
* @param desc       リソースの内容をあらわす記述子
* @param handle     ビューの作成先のハンドル
*/
void Device::CreateShaderResourceView(ID3D12Resource* pResource,
  const D3D12_SHADER_RESOURCE_VIEW_DESC* desc, D3D12_CPU_DESCRIPTOR_HANDLE handle)
{
  device->CreateShaderResourceView(pResource, desc, handle);
}

/**
* デスクリプタハンドルをコピー
*
* @param size      コピーする数
* @param destStart コピー先の先頭ハンドル
* @param srcStart  コピー元の先頭ハンドル
* @param type      コピーするハンドルの種類
*/
void Device::CopyDescriptors(UINT size,
  D3D12_CPU_DESCRIPTOR_HANDLE destStart, D3D12_CPU_DESCRIPTOR_HANDLE srcStart, D3D12_DESCRIPTOR_HEAP_TYPE type)
{
  device->CopyDescriptorsSimple(size, destStart, srcStart, type);
}

/**
* NULLデスクリプタをコピー
*
* @param size      コピーする数
* @param destStart コピー先の先頭ハンドル
*/
void Device::SetDescriptorsToNull(UINT size, D3D12_CPU_DESCRIPTOR_HANDLE destStart)
{
  D3D12_CPU_DESCRIPTOR_HANDLE src = heapNull.GetCPUDescriptorHandle(0);
  for (UINT i = 0; i < size; i++) {
    device->CopyDescriptorsSimple(1, destStart, src, D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    destStart.ptr += heapNull.GetHandleSize();
  }
}

/**
* デスクリプタハンドルを確保
*/
DescriptorPtr Device::AllocateDescriptor()
{
  auto p = std::make_shared<Descriptor>();
  p->index = heapAllocator.Allocate();
  p->handleCPU = heapCSU.GetCPUDescriptorHandle(p->index);
  p->device = shared_from_this();
  return p;
}

/**
* デスクリプタハンドルを解放
*
* @param n  解放するハンドルの管理番号
*/
void Device::DeallocateDescriptor(int n)
{
  heapAllocator.Deallocate(n);
}

/**
* SJIS文字列をワイド文字列に変換
*
* @param s  SJIS文字列
*/
std::wstring ToWString(const char* s)
{
  std::wstring tmp;
  tmp.resize(strlen(s) + 1);
  mbstowcs(&tmp[0], s, tmp.size());
  return tmp;
}

/**
* SJIS文字列をワイド文字列に変換
*
* @param s  SJIS文字列
*/
std::wstring ToWString(const std::string& s)
{
  std::wstring tmp;
  tmp.resize(s.size() + 1);
  mbstowcs(&tmp[0], s.data(), tmp.size());
  return tmp;
}

} // namespace DX12
} // namespace EasyLib

