#include "CommandQueue.h"
#include "Device.h"

namespace EasyLib {
namespace DX12 {

/**
* コンストラクタ
*/
CommandQueue::CommandQueue(
  Microsoft::WRL::ComPtr<ID3D12CommandQueue> queue, Microsoft::WRL::ComPtr<ID3D12Fence> fence)
{
  this->queue = queue;
  this->fence = fence;
  fenceEvent = CreateEvent(nullptr, FALSE, FALSE, nullptr);

  // 56bitシフトの理由は以下のページを参照
  // https://alextardif.com/D3D11To12P1.html
  // なおlog2(60*60*60*24*365)が31弱なので、31bitあれば60fpsで1年放置できる
  nextFenceValue = (uint64_t)D3D12_COMMAND_LIST_TYPE_DIRECT << 56 | 1;
  completedFenceValue = (uint64_t)D3D12_COMMAND_LIST_TYPE_DIRECT << 56 | 0;
}

/**
* デストラクタ
*/
CommandQueue::~CommandQueue()
{
  WaitForIdle();
  CloseHandle(fenceEvent);
}

/**
* コマンドリストを実行
*/
uint64_t CommandQueue::ExecuteCommandList(ID3D12CommandList* commandList)
{
  ID3D12CommandList* ppCommandLists[] = { commandList };
  queue->ExecuteCommandLists(_countof(ppCommandLists), ppCommandLists);
  queue->Signal(fence.Get(), nextFenceValue);
  return nextFenceValue++;
}

/**
* コマンドリストの配列を実行
*/
uint64_t CommandQueue::ExecuteCommandLists(uint32_t count, ID3D12CommandList** commandLists)
{
  queue->ExecuteCommandLists(count, commandLists);
  queue->Signal(fence.Get(), nextFenceValue);
  return nextFenceValue++;
}

/**
* フェンス到達を待機
*
* @param fenceValue  待機するフェンス値
*/
void CommandQueue::WaitForFence(uint64_t fenceValue)
{
  if (fenceValue > completedFenceValue) {
    completedFenceValue = std::max<UINT64>(completedFenceValue, fence->GetCompletedValue());
    if (fenceValue > completedFenceValue) {
      fence->SetEventOnCompletion(fenceValue, fenceEvent);
      WaitForSingleObject(fenceEvent, INFINITE);
      completedFenceValue = fenceValue;
    }
  }
}

/**
* コマンドキューの実行完了を待機
*/
bool CommandQueue::WaitForIdle()
{
  ++nextFenceValue;
  queue->Signal(fence.Get(), nextFenceValue);
  fence->SetEventOnCompletion(nextFenceValue, fenceEvent);
  WaitForSingleObject(fenceEvent, INFINITE);
  return true;
}

} // namespace DX12
} // namespace EasyLib
