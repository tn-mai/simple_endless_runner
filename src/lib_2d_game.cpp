#define NOMINMAX
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#include <d3dx12.h>
#include <dxgi1_6.h>
#include <d3dcompiler.h>
#include <DirectXMath.h>
#include <algorithm>
#include <unordered_map>
#include <unordered_set>
#include <string>
#include <stdint.h>
#include <stdlib.h>
#include <locale.h>
#include <time.h>
#include "lib_2d_game.h"
#include "lib/PSO.h"
#include "lib/Device.h"
#include "lib/CommandQueue.h"
#include "lib/Framebuffer.h"
#include "lib/Texture.h"
#include "lib/Sprite.h"
#include "lib/Font.h"
#include "lib/Audio.h"

#pragma comment(lib, "d3d12.lib")
#pragma comment(lib, "dxgi.lib")
#pragma comment(lib, "d3dcompiler.lib")

//#define DEBUG_KEY

using namespace DirectX;
using Microsoft::WRL::ComPtr;

int main();

namespace /* unnamed */ {

HWND hwnd = nullptr;
int result = 0;

constexpr uint32_t framebufferCount = 3;

EasyLib::DX12::DevicePtr device;
EasyLib::DX12::FramebufferPtr framebuffer;
EasyLib::DX12::CommandQueuePtr commandQueue;
EasyLib::DX12::SpriteRenderer spriteRenderer;
std::vector<EasyLib::DX12::Sprite> spriteBuffer;
std::unordered_map<std::string, EasyLib::DX12::TexturePtr> textureCache;
std::unordered_set<std::string> textureMissCache;

XMFLOAT2 textScale(1, 1);
XMFLOAT4 textColor(1, 1, 1, 1);
std::vector<EasyLib::DX12::Text> textBuffer;
EasyLib::DX12::FontRenderer fontRenderer;

// TODO: Deviceクラスに統合すること
struct RenderCommandContext
{
  EasyLib::DX12::DescriptorHeap slot;
  ComPtr<ID3D12CommandAllocator> allocator;
  ComPtr<ID3D12GraphicsCommandList> listPre;
  ComPtr<ID3D12GraphicsCommandList> listMain;
  ComPtr<ID3D12GraphicsCommandList> listPost;
  uint64_t fenceValue = 0;
} renderCommandContexts[framebufferCount];

D3D12_VIEWPORT viewport;
D3D12_RECT scissorRect;

bool fullscreenMode = false;
int currentFrameIndex;

// 音声制御変数
std::string bgmFilename;
EasyLib::Audio::SoundPtr bgm;
float bgmVolume = 0.8f;

enum class KeyState {
  Release,
  StartPressing,
  Press,
};
KeyState keyStates[128];

#ifdef DEBUG_KEY
const char* const keyNames[128] = {
  "", "VK_LBUTTON", "VK_RBUTTON", "", "VK_MBUTTON", "", "", "", "", "", "", "", "", "VK_RETURN", "", "",
  "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
  "VK_SPACE", "", "", "", "", "VK_LEFT", "VK_UP", "VK_RIGHT", "VK_DOWN", "", "", "", "", "", "", "",
  "0", "1", "2", "3", "4", "5", "6", "7", "8", "9", ":", ";", "", "", "", "",
  "@", "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L", "M", "N", "O",
  "P", "Q", "R", "S", "T", "U", "V", "W", "X", "Y", "Z", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
  "", "", "", "", "", "", "", "", "", "", "", "", "", "", "", "",
};
#endif // DEBUG_KEY

enum class MouseButtonState {
  Release,
  PressStart,
  Press,
  Click,
};
MouseButtonState mouseButtonStates[3];
XMINT2 mousePosition = { 0, 0 };

/**
* ウィンドウメッセージ処理コールバック
*/
LRESULT CALLBACK WindowProc(HWND hWnd, UINT message, WPARAM wParam, LPARAM lParam)
{
  switch (message)
  {
  case WM_CREATE:
  {
    // Save the DXSample* passed in to CreateWindow.
    //LPCREATESTRUCT pCreateStruct = reinterpret_cast<LPCREATESTRUCT>(lParam);
    //SetWindowLongPtr(hWnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(pCreateStruct->lpCreateParams));
  }
  return 0;

  case WM_KEYDOWN:
    if (wParam == VK_ESCAPE) {
      DestroyWindow(hwnd);
      return 0;
    }

    if (wParam < 128 && ((lParam & 0x40000000) == 0)) {
      if (keyStates[wParam] != KeyState::StartPressing) {
        keyStates[wParam] = KeyState::StartPressing;
#ifdef DEBUG_KEY
        std::string str("KEYDOWN: ");
        str += keyNames[wParam];
        str += '\n';
        OutputDebugStringA(str.c_str());
#endif // DEBUG_KEY
      }
    }
    return 0;

  case WM_KEYUP:
    if (wParam < 128) {
      if (keyStates[wParam] != KeyState::Release) {
        keyStates[wParam] = KeyState::Release;
#ifdef DEBUG_KEY
        std::string str("KEYUP: ");
        str += keyNames[wParam];
        str += '\n';
        OutputDebugStringA(str.c_str());
#endif // DEBUG_KEY
      }
    }
    return 0;

  case WM_SYSKEYDOWN:
    // Handle ALT+ENTER:
    //if ((wParam == VK_RETURN) && (lParam & (1 << 29)))
    //{
    //  if (pSample && pSample->GetTearingSupport())
    //  {
    //    ToggleFullscreenWindow(pSample->GetSwapchain());
    //    return 0;
    //  }
    //}
    // Send all other WM_SYSKEYDOWN messages to the default WndProc.
    break;

    //case WM_PAINT:
    //  return 0;

    //case WM_SIZE:
      //if (pSample)
      //{
      //  RECT windowRect = {};
      //  GetWindowRect(hWnd, &windowRect);
      //  pSample->SetWindowBounds(windowRect.left, windowRect.top, windowRect.right, windowRect.bottom);

      //  RECT clientRect = {};
      //  GetClientRect(hWnd, &clientRect);
      //  pSample->OnSizeChanged(clientRect.right - clientRect.left, clientRect.bottom - clientRect.top, wParam == SIZE_MINIMIZED);
      //}
      //return 0;

    //case WM_MOVE:
      //if (pSample)
      //{
      //  RECT windowRect = {};
      //  GetWindowRect(hWnd, &windowRect);
      //  pSample->SetWindowBounds(windowRect.left, windowRect.top, windowRect.right, windowRect.bottom);

      //  int xPos = (int)(short)LOWORD(lParam);
      //  int yPos = (int)(short)HIWORD(lParam);
      //  pSample->OnWindowMoved(xPos, yPos);
      //}
      //return 0;

    //case WM_DISPLAYCHANGE:
      //if (pSample)
      //{
      //  pSample->OnDisplayChanged();
      //}
      //return 0;

  case WM_MOUSEMOVE:
    mousePosition.x = LOWORD(lParam);
    mousePosition.y = HIWORD(lParam);
    return 0;

  case WM_LBUTTONDOWN:
    if (mouseButtonStates[0] == MouseButtonState::Release) {
      mouseButtonStates[0] = MouseButtonState::PressStart;
      mousePosition.x = LOWORD(lParam);
      mousePosition.y = HIWORD(lParam);
#ifdef DEBUG_KEY
      OutputDebugStringA("MOUSEBUTTONDOWN: L\n");
#endif // DEBUG_KEY
    }
    return 0;

  case WM_LBUTTONUP:
    if (mouseButtonStates[0] == MouseButtonState::Press || mouseButtonStates[0] == MouseButtonState::PressStart) {
      mouseButtonStates[0] = MouseButtonState::Click;
#ifdef DEBUG_KEY
      OutputDebugStringA("MOUSEBUTTONUP: L\n");
#endif // DEBUG_KEY
    }
    return 0;

  case WM_RBUTTONDOWN:
    if (mouseButtonStates[1] == MouseButtonState::Release) {
      mouseButtonStates[1] = MouseButtonState::PressStart;
      mousePosition.x = LOWORD(lParam);
      mousePosition.y = HIWORD(lParam);
#ifdef DEBUG_KEY
      OutputDebugStringA("MOUSEBUTTONDOWN: R\n");
#endif // DEBUG_KEY
    }
    return 0;

  case WM_RBUTTONUP:
    if (mouseButtonStates[1] == MouseButtonState::Press || mouseButtonStates[1] == MouseButtonState::PressStart) {
      mouseButtonStates[1] = MouseButtonState::Click;
#ifdef DEBUG_KEY
      OutputDebugStringA("MOUSEBUTTONUP: R\n");
#endif // DEBUG_KEY
    }
    return 0;

  case WM_DESTROY:
    PostQuitMessage(0);
    return 0;
  }

  // Handle any messages the switch statement didn't.
  return DefWindowProc(hWnd, message, wParam, lParam);
}

} // namespace unnamed

/**
* エントリーポイント
*/
int WINAPI WinMain(
  _In_ HINSTANCE hInstance,
  _In_opt_ HINSTANCE,
  [[maybe_unused]] _In_ LPSTR lpCmdLine,
  _In_ int nCmdShow)
{
  if (FAILED(CoInitializeEx(nullptr, COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE))) {
    return -1;
  }

  // Initialize the window class.
  WNDCLASSEX windowClass = {};
  windowClass.cbSize = sizeof(WNDCLASSEX);
  windowClass.style = CS_HREDRAW | CS_VREDRAW;
  windowClass.lpfnWndProc = WindowProc;
  windowClass.hInstance = hInstance;
  windowClass.hCursor = LoadCursor(NULL, IDC_ARROW);
  windowClass.lpszClassName = L"EndlessRunningGame";
  RegisterClassEx(&windowClass);

  RECT windowRect = { 0, 0, 1280, 720 };
  AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);

  // Create the window and store a handle to it.
  hwnd = CreateWindow(
    windowClass.lpszClassName,
    L"Endless Running Game",
    WS_OVERLAPPEDWINDOW,
    CW_USEDEFAULT,
    CW_USEDEFAULT,
    windowRect.right - windowRect.left,
    windowRect.bottom - windowRect.top,
    nullptr,        // 親ウィンドウのアドレス
    nullptr,        // メニューオブジェクトのアドレス
    hInstance,
    nullptr); // WM_CREATE の lParam に渡されるパラメータ

  ShowWindow(hwnd, nCmdShow);
  UpdateWindow(hwnd);

  EasyLib::Audio::Engine::Get().Initialize();

  setlocale(LC_CTYPE, "JPN");

  const int result = main();

  EasyLib::Audio::Engine::Get().Destroy();

  CoUninitialize();

  return result;
}

/**
* 初期化
*/
int initialize(const std::string& app_name, int clientWidth, int clientHeight)
{
  SetWindowTextA(hwnd, app_name.c_str());

  RECT windowRect;
  GetWindowRect(hwnd, &windowRect);
  windowRect.right = windowRect.left + clientWidth;
  windowRect.bottom = windowRect.top + clientHeight;
  AdjustWindowRect(&windowRect, WS_OVERLAPPEDWINDOW, FALSE);
  MoveWindow(hwnd, windowRect.left, windowRect.top,
    windowRect.right - windowRect.left, windowRect.bottom - windowRect.top, TRUE);

  device = std::make_shared<EasyLib::DX12::Device>();
  const EasyLib::DX12::Device::Result dr = device->Initialize(1ULL * 1024 * 1024 * 1024);
  if (dr == EasyLib::DX12::Device::Result::MemoryReservationFailed) {
    MessageBox(hwnd, L"このゲームの起動にはVRAMが最低1GB必要です", L"VRAM不足", MB_OK | MB_ICONINFORMATION);
    return S_FALSE;
  } else if (dr != EasyLib::DX12::Device::Result::Success) {
    return S_FALSE;
  }

  // コマンドキューを作成
  commandQueue = device->CreateCommandQueue();
  if (!commandQueue) {
    return S_FALSE;
  }

  // スワップチェーンを作成
  framebuffer = device->CreateFramebuffer(commandQueue, hwnd,
    static_cast<uint16_t>(clientWidth), static_cast<uint16_t>(clientHeight), framebufferCount);

  spriteBuffer.reserve(1024);
  textureCache.reserve(1024);
  textureMissCache.reserve(1024);
  spriteRenderer.Initialize(device, framebufferCount, 10'000);

  textBuffer.reserve(1024);
  fontRenderer.Initialize(device, framebufferCount, 10'000);
  fontRenderer.LoadFromFile("res/font/font.fnt");

  viewport.TopLeftX = 0;
	viewport.TopLeftY = 0;
	viewport.Width = static_cast<float>(clientWidth);
	viewport.Height = static_cast<float>(clientHeight);
	viewport.MinDepth = 0.0f;
	viewport.MaxDepth = 1.0f;

	scissorRect.left = 0;
	scissorRect.top = 0;
	scissorRect.right = clientWidth;
	scissorRect.bottom = clientHeight;

  srand(static_cast<int>(time(nullptr)));

  // 後でアプリケーションオブジェクトを追加したら、そのアドレスを渡す
  SetWindowLongPtr(hwnd, GWLP_USERDATA, reinterpret_cast<LONG_PTR>(nullptr));

  return S_OK;
}

/**
* ゲーム状態の更新
*/
int update()
{
  EasyLib::Audio::Engine::Get().Update();

  for (auto& e : keyStates) {
    if (e == KeyState::StartPressing) {
      e = KeyState::Press;
    }
  }
  for (auto& e : mouseButtonStates) {
    if (e == MouseButtonState::Click) {
      e = MouseButtonState::Release;
    } else if (e == MouseButtonState::PressStart) {
      e = MouseButtonState::Press;
    }
  }

  // Process any messages in the queue.
  MSG msg = {};
  if (PeekMessage(&msg, NULL, 0, 0, PM_REMOVE))
  {
    TranslateMessage(&msg);
    DispatchMessage(&msg);
  }

  if (msg.message == WM_QUIT)
  {
    // Return this part of the WM_QUIT message to Windows.
    result = static_cast<int>(msg.wParam);
    return 1;
  }

  spriteBuffer.clear();
  textBuffer.clear();

  return 0;
}

/**
* 描画
*/
void render()
{
  auto& context = device->GetCommandContext(currentFrameIndex);

  context.WaitForFence(commandQueue);

  context.ResetAllocator();

  ID3D12GraphicsCommandList* listPre = context.GetList(EasyLib::DX12::GraphicsCommandContext::ListType::Pre);
  listPre->Reset(context.GetAllocator(), nullptr);
  auto barrier1 = framebuffer->GetTransitionBarrier(
    currentFrameIndex, D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_RENDER_TARGET);
  listPre->ResourceBarrier(1, &barrier1);
  // バックバッファの内容を消去
  const D3D12_CPU_DESCRIPTOR_HANDLE& rtvHandle = framebuffer->GetRenderTargetHandle(currentFrameIndex);
  const D3D12_CPU_DESCRIPTOR_HANDLE& dsvHandle = framebuffer->GetDepthStencilHandle();
  const float clearColor[] = { 0.8f, 0.2f, 0.4f, 1.0f }; // 注意: 0か1以外の数値ではハードウェア最適化されない
  listPre->ClearRenderTargetView(rtvHandle, clearColor, 0, nullptr);
  listPre->ClearDepthStencilView(dsvHandle, D3D12_CLEAR_FLAG_DEPTH, 1.0f, 0, 0, nullptr);
  listPre->Close();

  EasyLib::DX12::SpriteRenderingInfo spriteRenderingInfo = {};
  spriteRenderingInfo.handleRTV = framebuffer->GetRenderTargetHandle(currentFrameIndex);
  spriteRenderingInfo.handleDSV = framebuffer->GetDepthStencilHandle();
  spriteRenderingInfo.viewport = viewport;
  spriteRenderingInfo.scissorRect = scissorRect;
	spriteRenderingInfo.matViewProjection =
    XMMatrixOrthographicOffCenterLH(0, framebuffer->GetWidth(), 0, framebuffer->GetHeight(), 1, 1000);
  spriteRenderingInfo.framebufferIndex = currentFrameIndex;
  auto spriteCommandList = spriteRenderer.Draw(device, spriteBuffer.data(), spriteBuffer.size(), spriteRenderingInfo);

  EasyLib::DX12::FontRenderingInfo fontRenderingInfo = {};
  fontRenderingInfo.handleRTV = framebuffer->GetRenderTargetHandle(currentFrameIndex);
  fontRenderingInfo.handleDSV = framebuffer->GetDepthStencilHandle();
  fontRenderingInfo.viewport = viewport;
  fontRenderingInfo.scissorRect = scissorRect;
	fontRenderingInfo.matViewProjection =
    XMMatrixOrthographicOffCenterLH(0, framebuffer->GetWidth(), 0, framebuffer->GetHeight(), 1, 1000);
  fontRenderingInfo.framebufferIndex = currentFrameIndex;
  auto fontCommandList = fontRenderer.Draw(textBuffer.data(), textBuffer.size(), fontRenderingInfo);

  ID3D12GraphicsCommandList* listPost = context.GetList(EasyLib::DX12::GraphicsCommandContext::ListType::Post);
  listPost->Reset(context.GetAllocator(), nullptr);
  auto barrier2 = framebuffer->GetTransitionBarrier(
    currentFrameIndex, D3D12_RESOURCE_STATE_RENDER_TARGET, D3D12_RESOURCE_STATE_PRESENT);
  listPost->ResourceBarrier(1, &barrier2);
  listPost->Close();

  ID3D12CommandList* commandLists[] = {
    listPre,
    spriteCommandList,
    fontCommandList,
    listPost,
  };
  context.SetFenceValue(commandQueue->ExecuteCommandLists(std::size(commandLists), commandLists));

  currentFrameIndex = framebuffer->Present(1, 0);
}

/**
* 終了処理
*/
void finalize()
{
  commandQueue.reset();
}

// 画像を準備する
image_handle prepare_image(const char* image)
{
  image_handle tex;
  auto itr = textureCache.find(image);
  if (itr != textureCache.end()) {
    tex = itr->second;
  } else {
    std::string s;
    s.reserve(1024);
    s += "res/images/";
    s += image;

    int size_needed = MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), NULL, 0);
    std::wstring ws( size_needed, 0 );
    MultiByteToWideChar(CP_UTF8, 0, s.c_str(), static_cast<int>(s.size()), ws.data(), size_needed);
    
    tex = device->LoadTexture(ws.c_str());
    if (!tex) {
      auto itrMiss = textureMissCache.find(image);
      if (itrMiss == textureMissCache.end()) {
        textureMissCache.emplace(image);
        const auto str = std::string("ERROR: 画像ファイル名") + image + "が見つかりません. ファイル名を確認してください\n";
        OutputDebugStringA(str.c_str());
      }
      return {};
    }
    textureCache.emplace(std::string(image), tex);
  }

  return tex;
}

// 画像を描画する
void draw_image(double x, double y, const image_handle& image, double scale, double rotation)
{
  if (image) {
    EasyLib::DX12::Sprite sprite;
    sprite.texture = image;
    sprite.position.x = static_cast<float>(x);
    sprite.position.y = static_cast<float>(y);
    sprite.position.z = 100;
    sprite.rotation = static_cast<float>(rotation * 3.141592657 / 360.0);
    sprite.scale.x = static_cast<float>(scale * image->GetWidth());
    sprite.scale.y = static_cast<float>(scale * image->GetHeight());
    sprite.color = XMFLOAT4(1, 1, 1, 1);
    spriteBuffer.push_back(sprite);
  }
}

// 画像を描画する
void draw_image(double x, double y, const std::string& image)
{
  draw_image(x, y, prepare_image(image.c_str()), 1, 0);
}

// 画像を描画する
void draw_image(double x, double y, const std::string& image, double scale, double rotation)
{
  draw_image(x, y, prepare_image(image.c_str()), scale, rotation);
}

// 文章を描画する
void draw_text(double x, double y, const char* format, ...)
{
  va_list ap;
  va_start(ap, format);
  char tmp[1024];
  vsnprintf(tmp, sizeof(tmp), format, ap);
  va_end(ap);

  const std::wstring ws = EasyLib::DX12::ToWString(tmp);
  textBuffer.push_back({ ws, XMFLOAT2(static_cast<float>(x), static_cast<float>(y)), textScale, textColor });
}

// 文字の大きさを変更する
void set_text_scale(double scale_x, double scale_y)
{
  textScale.x = static_cast<float>(scale_x);
  textScale.y = static_cast<float>(scale_y);
}

// 文字の色を変更する
void set_text_color(double red, double green, double blue, double alpha)
{
  textColor.x = static_cast<float>(red);
  textColor.y = static_cast<float>(green);
  textColor.z = static_cast<float>(blue);
  textColor.w = static_cast<float>(alpha);
}

// 効果音を再生する
void play_sound(const char* filename, double volume)
{
  std::string str;
  str.reserve(1024);
  str += "res/音声/";
  str += filename;
  const std::wstring ws = EasyLib::DX12::ToWString(str.c_str());
  EasyLib::Audio::SoundPtr p = EasyLib::Audio::Engine::Get().PrepareMFStream(ws.c_str());
  if (p) {
    p->Play(EasyLib::Audio::Flag_None);
    p->SetVolume(static_cast<float>(volume));
  }
}

// 効果音を再生する
void play_sound(const char* filename) { play_sound(filename, 0.8); }

// BGMを再生する
void play_bgm(const char* filename)
{
  if (bgmFilename != filename || !bgm || !(bgm->GetState() & EasyLib::Audio::State_Playing)) {
    if (bgm) {
      bgm->Stop();
    }
    bgmFilename = filename;
    std::string str;
    str.reserve(1024);
    str += "res/音声/";
    str += filename;
    const std::wstring ws = EasyLib::DX12::ToWString(str.c_str());
    bgm = EasyLib::Audio::Engine::Get().PrepareMFStream(ws.c_str());
    if (bgm) {
      bgm->Play(EasyLib::Audio::Flag_Loop);
      bgm->SetVolume(bgmVolume);
    }
  }
}

// BGMを停止する
void stop_bgm()
{
  bgmFilename.clear();
  if (bgm) {
    bgm->Stop();
    bgm.reset();
  }
}

void set_bgm_volume(double volume)
{
  bgmVolume = static_cast<float>(volume);
  if (bgm) {
    bgm->SetVolume(bgmVolume);
  }
}

// キーの押下状態を調べる
int get_key(int key)
{
  if (key == key_enter) {
    return static_cast<int>(keyStates[VK_RETURN]);
  } else if (key == key_space) {
    return static_cast<int>(keyStates[VK_SPACE]);
  } else if (key >= '0' && key <= 'Z') {
    return static_cast<int>(keyStates[key]);
  }
  return 0;
}

// 左マウスボタンの押下状態を調べる
int get_mouse_button_left()
{
  return static_cast<int>(mouseButtonStates[0]);
}

// 右マウスボタンの押下状態を調べる
int get_mouse_button_right()
{
  return static_cast<int>(mouseButtonStates[1]);
}

// マウスのX座標を調べる
int get_mouse_position_x()
{
  return mousePosition.x;
}

// マウスのY座標を調べる
int get_mouse_position_y()
{
  return mousePosition.y;
}

