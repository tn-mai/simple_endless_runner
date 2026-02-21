/**
* @file Font.cpp
*/
#define NOMINMAX
#include "Font.h"
#include "Device.h"
#include "PSO.h"
#include "Log.h"
#include <d3dx12.h>
#include <iostream>
#include <stdio.h>

namespace EasyLib {
namespace DX12 {

using namespace DirectX;
using namespace DirectX::PackedVector;
using Microsoft::WRL::ComPtr;

/**
* シェーダ用フォントデータ
*/
struct FontInfoInShader
{
  uint32_t page;
  XMFLOAT2 uv[2];
  XMFLOAT2 size;
  XMFLOAT2 offset;
};

struct CharacterInShader
{
  XMFLOAT2 position;
  XMFLOAT2 scale;
  XMFLOAT4 color;
  XMFLOAT4 subColor;
  float thickness;
  float outline;
  uint32_t fontIndex;
};

/**
* フォント描画オブジェクトを初期化
*
* @param devide            D3D12デバイス
* @param framebufferCount  描画バッファの数
* @param capacity          最大描画文字数
*
* @retval true  初期化成功
* @retval false 初期化失敗
*/
bool FontRenderer::Initialize(DevicePtr device, size_t framebufferCount, size_t capacity)
{
  this->framebufferCount = framebufferCount;
  this->capacity = capacity;
  this->device = device;

	pso = device->CreatePipelineState(L"res/shader/Font.hlsl", L"res/shader/Font.hlsl",
		BlendMode::Multiply, CullMode::None, DepthStencilMode::None, nullptr, 0);
	if (!pso) {
		return false;
	}

  heap = device->CreateDescriptorHeap(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV, num_HeapID, true);
	if (!heap.GetHeap()) {
		return false;
	}

	for (int i = 0; i < static_cast<int>(framebufferCount); i++) {
		wchar_t bufferName[] = L"Font Character Buffer 0";
		bufferName[std::size(bufferName) - 1] += static_cast<wchar_t>(i);
		commandContexts[i].characterBuffer = device->CreateUploadResource(bufferName, sizeof(CharacterInShader) * capacity);
		commandContexts[i].allocator = device->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT);
		commandContexts[i].list = device->CreateCommandList(D3D12_COMMAND_LIST_TYPE_DIRECT, commandContexts[i].allocator.Get());

		if (!commandContexts[i].characterBuffer || !commandContexts[i].allocator || !commandContexts[i].list) {
			return false;
		}

		// ヒープの0番目にはフォント情報バッファを割り当てる
		// ヒープの1-3番目には文字バッファを割り当てる
		// 4番目以降は描画時のテクスチャを割り当てる
		D3D12_SHADER_RESOURCE_VIEW_DESC view = 
			CD3DX12_SHADER_RESOURCE_VIEW_DESC::StructuredBuffer(static_cast<UINT>(capacity), sizeof(CharacterInShader));
		D3D12_CPU_DESCRIPTOR_HANDLE heapHandle = heap.GetCPUDescriptorHandle(HeapID_Character0 + i);
		device->CreateShaderResourceView(commandContexts[i].characterBuffer.Get(), &view, heapHandle);
	}

	indexBuffer = device->CreateUploadResource(L"Font Index Buffer", sizeof(uint16_t) * 6);
	if (!indexBuffer) {
		return false;
	}
	void* pIndexBuffer;
	const D3D12_RANGE indexBufferRange = { 0, 0 };
	indexBuffer->Map(0, &indexBufferRange, &pIndexBuffer);
	// 2-3
	// |\|
	// 0-1
	const uint16_t indices[] = { 0, 2, 1, 2, 3, 1 };
	memcpy(pIndexBuffer, indices, sizeof(indices));
	indexBuffer->Unmap(0, nullptr);

	viewIB.BufferLocation = indexBuffer->GetGPUVirtualAddress();
	viewIB.Format = DXGI_FORMAT_R16_UINT;
	viewIB.SizeInBytes = sizeof(indices);

	fontBuffer = device->CreateUploadResource(L"Font Buffer", sizeof(FontInfoInShader) * 65536);
	if (!fontBuffer) {
		return false;
	}
	D3D12_SHADER_RESOURCE_VIEW_DESC viewFont = 
		CD3DX12_SHADER_RESOURCE_VIEW_DESC::StructuredBuffer(65536, sizeof(FontInfoInShader));
	D3D12_CPU_DESCRIPTOR_HANDLE handleFont = heap.GetCPUDescriptorHandle(HeapID_FontInfo);
	device->CreateShaderResourceView(fontBuffer.Get(), &viewFont, handleFont);

  return true;
}

/**
* フォントファイルを読み込む
*
* @param filename フォントファイル名
*
* @retval true  読み込み成功
* @retval false 読み込み失敗
*/
bool FontRenderer::LoadFromFile(const char* filename)
{
  const std::unique_ptr<FILE, decltype(&fclose)> fp(fopen(filename, "r"), fclose);
  if (!fp) {
    return false;
  }

  int line = 1;
  float fontSize;
  int ret = fscanf(fp.get(), "info face=%*s size=%f bold=%*d italic=%*d charset=%*s"
    " unicode=%*d stretchH=%*d smooth=%*d aa=%*d padding=%d,%d,%d,%d spacing=%*d,%*d",
    &fontSize, &paddingUp, &paddingRight, &paddingDown, &paddingLeft);
  if (ret < 5) {
    LOG("ERROR: %sの読み込みに失敗(line=%d)\n", filename, line);
    return false;
  }
  fontHeight = fontSize + static_cast<float>(paddingUp + paddingDown + 4); // 4 = 表示上の余白(雰囲気で決めた)
  ++line;

  XMFLOAT2 textureSize;
  ret = fscanf(fp.get(), " common lineHeight=%*d base=%*d scaleW=%f scaleH=%f pages=%*d packed=%*d",
    &textureSize.x, &textureSize.y);
  if (ret < 2) {
    LOG("ERROR: %sの読み込みに失敗(line=%d)\n", filename, line);
    return false;
  }
  const XMFLOAT2 reciprocalTextureSize = XMFLOAT2(1.0f / textureSize.x, 1.0f / textureSize.y);
  ++line;

  std::vector<std::string> texNameList;
  for (;;) {
    char tex[128];
    ret = fscanf(fp.get(), " page id=%*d file=%127s", tex);
    if (ret < 1) {
      break;
    }
    std::string texFilename = filename;
    const size_t lastSlashIndex = texFilename.find_last_of('/', std::string::npos);
    if (lastSlashIndex == std::string::npos) {
      texFilename.clear();
    } else {
      texFilename.resize(lastSlashIndex + 1);
    }
    texFilename.append(tex + 1); // 最初の「"」を抜いて追加.
    texFilename.pop_back(); // 最後の「"」を消す.
    texNameList.push_back(texFilename);
    ++line;
  }
  if (texNameList.empty()) {
    LOG("ERROR: %sの読み込みに失敗(line=%d)\n", filename, line);
    return false;
  }

  int charCount;
  ret = fscanf(fp.get(), " chars count=%d", &charCount);
  if (ret < 1) {
    LOG("ERROR: %sの読み込みに失敗(line=%d)\n", filename, line);
    return false;
  }
  ++line;

  std::vector<FontInfoInShader> shaderFonts;
  shaderFonts.resize(65536);

  fixedAdvance = 0;
  fontList.resize(65536);
  for (int i = 0; i < charCount; ++i) {
    FontInfo font;
    FontInfoInShader shaderFont;
    XMFLOAT2 uv;
    ret = fscanf(fp.get(), " char id=%d x=%f y=%f width=%f height=%f xoffset=%f yoffset=%f xadvance=%f page=%d chnl=%*d",
      &font.id, &uv.x, &uv.y, &font.size.x, &font.size.y, &font.offset.x, &font.offset.y, &font.xadvance, &font.page);
    if (ret < 8) {
      LOG("ERROR: %sの読み込みに失敗(line=%d)\n", filename, line);
      return false;
    }
    if (ret < 9) {
      font.page = 0;
    }
    //font.offset.y *= -1;
    //uv.y = textureSize.y - uv.y - font.size.y;
    font.uv[0].x = uv.x * reciprocalTextureSize.x;
    font.uv[0].y = uv.y * reciprocalTextureSize.y;
    font.uv[1].x = (uv.x + font.size.x) * reciprocalTextureSize.x;
    font.uv[1].y = (uv.y + font.size.y) * reciprocalTextureSize.y;
    shaderFont.page = font.page;
    shaderFont.offset = font.offset;
    shaderFont.size = font.size;
    shaderFont.uv[0] = font.uv[0];
    shaderFont.uv[1] = font.uv[1];
    if (font.id < 65536) {
      fontList[font.id] = font;
      shaderFonts[font.id] = shaderFont;
      if (font.xadvance > fixedAdvance) {
        fixedAdvance = font.xadvance;
      }
    }
    ++line;
  }

  D3D12_RANGE range = { 0, 0 };
  void* p;
  fontBuffer->Map(0, &range, &p);
  memcpy(p, shaderFonts.data(), shaderFonts.size() * sizeof(FontInfoInShader));
  fontBuffer->Unmap(0, nullptr);

  texList.clear();
  texList.reserve(texNameList.size());
  D3D12_CPU_DESCRIPTOR_HANDLE handleTex = heap.GetCPUDescriptorHandle(HeapID_Texture0);
  for (const auto& e : texNameList) {
    TexturePtr tex = device->LoadTexture(e.c_str());
    if (!tex) {
      return false;
    }
    texList.push_back(tex);
    device->CopyDescriptors(1, handleTex, tex->GetCPUHandle(), D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);
    handleTex.ptr += heap.GetHandleSize();
  }
  const int texCount = static_cast<int>(texList.size());
  if (texCount < 2) {
    device->SetDescriptorsToNull(2 - texCount, heap.GetCPUDescriptorHandle(HeapID_Texture0 + texCount));
  }
  return true;
}

/**
* 文字列の横のピクセル数を調べる
*/
XMFLOAT2 FontRenderer::CalcStringSize(const wchar_t* str) const
{
  XMFLOAT2 pos(0, 0);
  for (const wchar_t* itr = str; *itr; ++itr) {
    if (*itr == L'\n') {
      pos.x = 0;
      pos.y += fontHeight * scale.y;
    }
    const FontInfo& font = fontList[*itr];
    pos.x += (propotional ? font.xadvance : fixedAdvance) * static_cast<float>(paddingRight + paddingLeft) * scale.x;
  }
  return pos;
}

/**
* 文字列表示用のコマンドリストを作成
*
* @param pText  表示データ配列の先頭アドレス
* @param count  配列の長さ
* @param renderingInfo 描画に必要な各種情報
*
* @retval コマンドリストを返す
*/
ID3D12GraphicsCommandList* FontRenderer::Draw(const Text* pText, size_t count, const FontRenderingInfo& renderingInfo)
{
	count = std::min(count, capacity);
	CommandContext& context = commandContexts[renderingInfo.framebufferIndex];

	if (count <= 0) {
		context.allocator->Reset();
		context.list->Reset(context.allocator.Get(), nullptr);
		context.list->Close();
 		return context.list.Get();
	}

	void* pTmp;
	const D3D12_RANGE range = { 0, 0 };
	context.characterBuffer->Map(0, &range, &pTmp);
	CharacterInShader* const pBegin = static_cast<CharacterInShader*>(pTmp);
  const CharacterInShader* const pEnd = pBegin + capacity;

  CharacterInShader* pCharacter = pBegin;
  for (size_t i = 0; i < count; i++) {
    const Text& text = pText[i];
    XMFLOAT2 pos = text.position;
    for (const wchar_t* itr = text.text.c_str(); *itr; ++itr) {
      if (pCharacter >= pEnd) {
        break;
      }
      if (*itr == L'\n') {
        pos.x = text.position.x;
        pos.y -= fontHeight * scale.y;
      }
      const FontInfo& font = fontList[*itr];
      if (font.id >= 0 && font.size.x && font.size.y) {
        pCharacter->position = pos;
        pCharacter->position.y = renderingInfo.viewport.Height - pCharacter->position.y;
        pCharacter->scale = scale;
        pCharacter->color = text.color;
        pCharacter->subColor = subColor;
        pCharacter->fontIndex = font.id;
        pCharacter->thickness = 1.0f - thickness;// 0.625f - thickness * 0.375f;
        pCharacter->outline = border;
        ++pCharacter;
      }
      pos.x += ((propotional ? font.xadvance : fixedAdvance) + static_cast<float>(paddingRight + paddingLeft)) * scale.x;
    }
  }
  context.characterBuffer->Unmap(0, nullptr);

	context.allocator->Reset();
	context.list->Reset(context.allocator.Get(), nullptr);

	context.list->SetPipelineState(pso->GetPipelineStateObject());
	context.list->SetGraphicsRootSignature(pso->GetRootSignature());
  context.list->OMSetRenderTargets(1, &renderingInfo.handleRTV, FALSE, &renderingInfo.handleDSV);
  context.list->RSSetViewports(1, &renderingInfo.viewport);
  context.list->RSSetScissorRects(1, &renderingInfo.scissorRect);

	context.list->IASetIndexBuffer(&viewIB);
	context.list->IASetPrimitiveTopology(D3D_PRIMITIVE_TOPOLOGY_TRIANGLELIST);
	ID3D12DescriptorHeap* heapList[] = { heap.GetHeap() };
	context.list->SetDescriptorHeaps(_countof(heapList), heapList);

	// ルートパラメータ0: ビュープロジェクション行列
	// NOTE: DirectXMath は行メジャー、 HLSL は列メジャー、なので、転置が必要
	// https://stackoverflow.com/questions/41405994/hlsl-mul-and-d3dxmatrix-order-mismatch
	const XMMATRIX matInvVP = XMMatrixTranspose(renderingInfo.matViewProjection);
	context.list->SetGraphicsRoot32BitConstants(0, 16, &matInvVP, 0);

	// ルートパラメータ1: フォントデータ
	context.list->SetGraphicsRootDescriptorTable(1, heap.GetGPUDescriptorHandle(HeapID_FontInfo));

	// ルートパラメータ2: 文字データ
	context.list->SetGraphicsRootDescriptorTable(2,
    heap.GetGPUDescriptorHandle(HeapID_Character0 + renderingInfo.framebufferIndex));

	// ルートパラメータ3: テクスチャ
	context.list->SetGraphicsRootDescriptorTable(3, heap.GetGPUDescriptorHandle(HeapID_Texture0));

	context.list->DrawIndexedInstanced(6, static_cast<UINT>(pCharacter - pBegin), 0, 0, 0);
  context.list->Close();

  return context.list.Get();
}

/**
* 文字色を設定
*
* @param c 文字色
*/
void FontRenderer::Color(const XMFLOAT4& c)
{
  color = c;
}

/**
* 文字色取得
*
* @return 文字色
*/
XMFLOAT4 FontRenderer::Color() const
{
  return color;
}

/**
* サブ文字色を設定
*
* @param c 文字色
*/
void FontRenderer::SubColor(const XMFLOAT4& c)
{
  subColor = c;
}

/**
* サブ文字色取得
*
* @return 文字色
*/
XMFLOAT4 FontRenderer::SubColor() const
{
  return subColor;
}

} // namespace EasyLib
} // namespace DX12

