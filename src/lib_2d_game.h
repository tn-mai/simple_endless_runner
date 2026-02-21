#ifndef EASYLIB_2D_GAME_H_INCLUDED
#define EASYLIB_2D_GAME_H_INCLUDED
#include <string>
#include <memory>

// 画像管理ハンドル型
namespace EasyLib { namespace DX12 { class Texture; } }
using image_handle = std::shared_ptr<EasyLib::DX12::Texture>;

// 画像を表示する
//   x        X座標
//   y        Y座標
//   image    画像ファイル
//   scale    大きさ
//   rotation 回転
void draw_image(double x, double y, const std::string& image);
void draw_image(double x, double y, const std::string& image, double scale, double rotation);
void draw_image(double x, double y, const image_handle& image, double scale, double rotation);

// 文章を表示する
//   x        X座標
//   y        Y座標
//   format   表示する文章
void draw_text(double x, double y, const char* format, ...);

// 文章の大きさを設定する
//   scale_x 横の大きさ
//   scale_y 縦の大きさ
void set_text_scale(double scale_x, double scale_y);

// 文章の色を設定する
//   red   赤成分(0.0～1.0)
//   green 緑成分(0.0～1.0)
//   blue  青成分(0.0～1.0)
//   alpha 透明度(0.0=透明 1.0=不透明)
void set_text_color(double red, double green, double blue, double alpha);

// 音声
void play_sound(const char* filename); // 効果音を再生する
void play_sound(const char* filename, double volume); // 効果音を再生する
void play_bgm(const char* filename);   // BGMを再生する
void stop_bgm();                       // BGMを止める
void set_bgm_volume(double volume);    // BGMの音量を変更する

// マウスボタンの状態
constexpr int mb_release = 0;     // 押されていない
constexpr int mb_press_start = 1; // 押された瞬間
constexpr int mb_press = 2;       // 押されている
constexpr int mb_click = 3;       // 離された瞬間

// マウスの状態を調べる
int get_mouse_button_left();  // マウスの左ボタンの状態
int get_mouse_button_right(); // マウスの右ボタンの状態
int get_mouse_position_x();   // マウスのX座標
int get_mouse_position_y();   // マウスのY座標

// キーの状態
constexpr int kb_release = 0;     // 押されていない
constexpr int kb_press_start = 1; // 押された瞬間
constexpr int kb_press = 2;       // 押されている

// 特殊キー番号
constexpr int key_enter = 13;
constexpr int key_space = 32;
constexpr int key_left  = 37;
constexpr int key_up    = 38;
constexpr int key_right = 39;
constexpr int key_down  = 40;

// キーの状態を調べる
int get_key(int key);

// 基本関数
int initialize(const std::string& app_name, int clientWidth, int clientHeight);
int update();
void render();
void finalize();

#endif // EASYLIB_2D_GAME_H_INCLUDED
