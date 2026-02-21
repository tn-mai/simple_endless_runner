#ifndef EASYLIB_DX12_COMMANDQUEUE_H
#define EASYLIB_DX12_COMMANDQUEUE_H
#include <d3d12.h>
#include <wrl/client.h>
#include <stdint.h>

namespace EasyLib {
namespace DX12 {

// コマンドキューはコマンドリストタイプごとに作るが、描画とテクスチャは同じDIRECTタイプなので分ける必要がない
// 他のCOMPUTE, BUNDLE, COPYは今回は使わない
// フェンスはコマンドキューごとに必要
class CommandQueue
{
  friend class Device;
public:
  ~CommandQueue();

  uint64_t ExecuteCommandList(ID3D12CommandList* commandList);
  uint64_t ExecuteCommandLists(uint32_t count, ID3D12CommandList** commandLists);
  void WaitForFence(uint64_t fenceValue);
  bool WaitForIdle();
  ID3D12CommandQueue* GetQueue() const { return queue.Get(); }

private:
  CommandQueue() = default;
  CommandQueue(
    Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue, Microsoft::WRL::ComPtr<ID3D12Fence> fence);

  Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue;
  Microsoft::WRL::ComPtr<ID3D12Fence> fence;
  HANDLE fenceEvent;
  uint64_t nextFenceValue;
  uint64_t completedFenceValue;
};

} // namespace DX12
} // namespace EasyLib

#endif // EASYLIB_DX12_COMMANDQUEUE_H
