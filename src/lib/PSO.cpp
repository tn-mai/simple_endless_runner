#include "PSO.h"
#include <d3dcompiler.h>
#include <d3dx12.h>
#include <unordered_map>
#include <string>
using Microsoft::WRL::ComPtr;

namespace EasyLib {

namespace DX12 {

namespace {

// シェーダキャッシュ
std::unordered_map<std::wstring, ComPtr<ID3DBlob>> vertexShaderCache;
std::unordered_map<std::wstring, ComPtr<ID3DBlob>> pixelShaderCache;

/**
* シェーダを読み込む
*/
bool LoadShader(const wchar_t* filename, const char* entryPoint, const char* target, ID3DBlob** blob)
{
	ComPtr<ID3DBlob> errorBuffer;
	HRESULT hr = D3DCompileFromFile(filename, nullptr, nullptr, entryPoint, target,
		D3DCOMPILE_DEBUG | D3DCOMPILE_SKIP_OPTIMIZATION, 0, blob, &errorBuffer);
	if (hr < 0) {
		if (errorBuffer) {
			OutputDebugStringA(static_cast<char*>(errorBuffer->GetBufferPointer()));
		}
		return false;
	}
	return true;
}

} // namespace

/**
* シェーダキャッシュを空にする
*/
void PSO::ClearShaderCache()
{
	vertexShaderCache.clear();
	pixelShaderCache.clear();
}

/**
* パイプラインステートオブジェクトを初期化
*/
bool PSO::Initialize(ID3D12Device* device, bool warp,
	const wchar_t* filenameVS, const wchar_t* filenamePS,
	BlendMode blendMode, CullMode cullMode, DepthStencilMode depthStencilMode,
  const D3D12_INPUT_ELEMENT_DESC* vertexLayout, size_t layoutElementCount)
{
	// 頂点シェーダを作成
	ComPtr<ID3DBlob> vertexShaderBlob;
  auto vs = vertexShaderCache.find(filenameVS);
  if (vs != vertexShaderCache.end()) {
    vertexShaderBlob = vs->second;
  } else {
    if (!LoadShader(filenameVS, "VSMain", "vs_5_1", &vertexShaderBlob)) {
      return false;
    }
    vertexShaderCache.emplace(filenameVS, vertexShaderBlob);
	}

	// ピクセルシェーダを作成
	ComPtr<ID3DBlob> pixelShaderBlob;
  auto ps = pixelShaderCache.find(filenamePS);
  if (ps != pixelShaderCache.end()) {
    pixelShaderBlob = ps->second;
  } else {
    if (!LoadShader(filenamePS, "PSMain", "ps_5_1", &pixelShaderBlob)) {
      return false;
    }
    pixelShaderCache.emplace(filenamePS, pixelShaderBlob);
  }

	// ルートシグネチャの作成方法は次の3種類
	//   1. C++コードで定義して D3D12SerializeRootSignature でシリアライズ
	//      基本。これができないと3ができない。
	//   2. HLSL属性で定義して D3DGetBlobPart でシリアライズ済みブロブを読み取る
	//      とにかく簡単。おすすめ。
	//   3. ID3D12ShaderReflection でシェーダの構成を解析して D3D12SerializeRootSignature でシリアライズ
	//      コード書くのが面倒だが一度書けば使い回せるし、汎用性は一番高い。
	// 今回は2を採用(読み込みコードが3つの中で一番簡単で、記述もDX12にしては簡単なため)
	ComPtr<ID3DBlob> signatureBlob;
	if (FAILED(D3DGetBlobPart(vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize(),
		D3D_BLOB_ROOT_SIGNATURE, 0, &signatureBlob))) {
		return false;
	}
	if (FAILED(device->CreateRootSignature(
		0, signatureBlob->GetBufferPointer(), signatureBlob->GetBufferSize(), IID_PPV_ARGS(&rootSignature)))) {
		return false;
	}

	D3D12_BLEND_DESC blendDesc = CD3DX12_BLEND_DESC(D3D12_DEFAULT);
	switch (blendMode) {
	case BlendMode::Opaque: break;

	case BlendMode::Multiply:
		blendDesc.RenderTarget[0].BlendEnable = TRUE;
		blendDesc.RenderTarget[0].LogicOpEnable = FALSE;
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_INV_SRC_ALPHA;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		break;

	case BlendMode::Addition:
		blendDesc.RenderTarget[0].BlendEnable = TRUE;
		blendDesc.RenderTarget[0].LogicOpEnable = FALSE;
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_ADD;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		break;

	case BlendMode::Subtraction:
		blendDesc.RenderTarget[0].BlendEnable = TRUE;
		blendDesc.RenderTarget[0].LogicOpEnable = FALSE;
		blendDesc.RenderTarget[0].SrcBlend = D3D12_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlend = D3D12_BLEND_ONE;
		blendDesc.RenderTarget[0].BlendOp = D3D12_BLEND_OP_SUBTRACT;
		blendDesc.RenderTarget[0].SrcBlendAlpha = D3D12_BLEND_SRC_ALPHA;
		blendDesc.RenderTarget[0].DestBlendAlpha = D3D12_BLEND_ONE;
		blendDesc.RenderTarget[0].BlendOpAlpha = D3D12_BLEND_OP_SUBTRACT;
		blendDesc.RenderTarget[0].RenderTargetWriteMask = D3D12_COLOR_WRITE_ENABLE_ALL;
		break;
	}

	// パイプラインステートオブジェクト(PSO)を作成
	// PSOは、レンダリングパイプラインの状態を素早く、一括して変更できるように導入された
	// PSOによって、多くのステートに対してそれぞれ状態変更コマンドを送らずとも、単にPSOを切り替えるコマンドを送るだけで済む
	D3D12_GRAPHICS_PIPELINE_STATE_DESC psoDesc = {};
	psoDesc.pRootSignature = rootSignature.Get();
	psoDesc.VS = { vertexShaderBlob->GetBufferPointer(), vertexShaderBlob->GetBufferSize() };
	psoDesc.PS = { pixelShaderBlob->GetBufferPointer(), pixelShaderBlob->GetBufferSize() };
	psoDesc.BlendState = blendDesc;
	psoDesc.SampleMask = 0xffffffff;
	psoDesc.RasterizerState = CD3DX12_RASTERIZER_DESC(D3D12_DEFAULT);
	psoDesc.RasterizerState.CullMode = static_cast<D3D12_CULL_MODE>(static_cast<int>(cullMode) + 1);
	psoDesc.DepthStencilState = CD3DX12_DEPTH_STENCIL_DESC(D3D12_DEFAULT);
	switch (depthStencilMode) {
	case DepthStencilMode::None:
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		break;
	case DepthStencilMode::Depth:
		psoDesc.DepthStencilState.StencilEnable = FALSE;
		break;
	case DepthStencilMode::Stencil:
		psoDesc.DepthStencilState.DepthEnable = FALSE;
		break;
	case DepthStencilMode::DepthStencil: break;
	}
	psoDesc.InputLayout.pInputElementDescs = vertexLayout;
	psoDesc.InputLayout.NumElements = static_cast<UINT>(layoutElementCount);
	psoDesc.PrimitiveTopologyType = D3D12_PRIMITIVE_TOPOLOGY_TYPE_TRIANGLE;
	psoDesc.NumRenderTargets = 1;
	psoDesc.RTVFormats[0] = DXGI_FORMAT_R8G8B8A8_UNORM;
	psoDesc.DSVFormat = DXGI_FORMAT_D32_FLOAT;
	psoDesc.SampleDesc = { 1, 0 };
	if (warp) {
		psoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_TOOL_DEBUG;
	}
	if (FAILED(device->CreateGraphicsPipelineState(&psoDesc, IID_PPV_ARGS(&pso)))) {
		return false;
	}

	return true;
}

} // namespace DX12

} // namespace EasyLib
