#ifndef EASYLIB_DX12_PSO_H
#define EASYLIB_DX12_PSO_H
#include <d3d12.h>
#include <wrl/client.h>
#include <memory>

namespace EasyLib {

namespace DX12 {

enum class BlendMode {
  Opaque,
  Multiply,
  Addition,
  Subtraction,
};

enum class CullMode {
  None,
  Front,
  Back,
};

enum class DepthStencilMode {
  None,
  Depth,
  Stencil,
  DepthStencil,
};

/**
* パイプラインステート
*/
class PSO
{
  friend class Device;

public:
  PSO() = default;
  ~PSO() = default;

  static void ClearShaderCache();

  ID3D12RootSignature* GetRootSignature() const { return rootSignature.Get();  }
  ID3D12PipelineState* GetPipelineStateObject() const { return pso.Get(); }

private:
  bool Initialize(ID3D12Device* device, bool warp,
    const wchar_t* filenameVS, const wchar_t* filenamePS,
    BlendMode blendMode, CullMode cullMode, DepthStencilMode depthStencilMode,
    const D3D12_INPUT_ELEMENT_DESC* vertexLayout, size_t layoutElementCount);

private:
  Microsoft::WRL::ComPtr<ID3D12RootSignature> rootSignature;
  Microsoft::WRL::ComPtr<ID3D12PipelineState> pso;
};
using PSOPtr = std::shared_ptr<PSO>;

} // namespace DX12

} // namespace EasyLib

#endif // EASYLIB_DX12_PSO_H
