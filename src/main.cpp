#include "lib_2d_game.h"
#include <math.h>
#include <stdlib.h>

int scene_number = 0;

double player_x = 400;
double player_y = 600;
int player_anime = 0;
double jump = 0;
int score = 0;

double sabo_x = 0;
double sabo_y = 600;
int sabo_type = 0;

double cloud_x = 0;
double cloud_y = 300;

void title()
{
  if (get_key(key_enter) == 1 || get_mouse_button_left() == mb_click) {
    play_sound("ok.wav");
    play_bgm("bgm_prehistoric.mp3");
    player_x = 400;
    player_y = 600;
    player_anime = 0;
    jump = 0;
    score = 0;

    sabo_x = 0;
    sabo_y = 600;
    scene_number = 1;
  }

  draw_image(640, 360, "bg.jpg");
  draw_image(640, 360, "logo_title_jp.png");

  draw_text(500, 600, "PRESS ENTER KEY");
}

void game()
{
  if (get_key(key_space) == 1 || get_mouse_button_left() == mb_press_start) {
    play_sound("jump.wav");
    jump = 20;
  }

  if (player_y <= 600) {
    player_y -= jump;
    jump -= 1;
    if (player_y > 600) {
      jump = 0;
      player_y = 600;
    }
  }

  sabo_x -= 10;
  if (sabo_x <= 0) {
    sabo_x = 1280;
    sabo_type = rand() % 4;
  }

  double dx = player_x - sabo_x;
  double dy = player_y - sabo_y;
  if (sqrt(dx * dx + dy * dy) < 64) {
    play_sound("miss.wav");
    stop_bgm();
    scene_number = 2;
  }

  score += 1;

  draw_image(640, 360, "bg.jpg");

  draw_image(cloud_x, cloud_y, "cloud.png", 2, 0);
  cloud_x -= 1;
  if (cloud_x <= -64) {
    cloud_y = rand() % 100 + 250;
    cloud_x = 1280 + 64;
  }

  draw_text(800, 100, "SCORE:%d", score);

  player_anime = (player_anime + 1) % 16;
  if (player_anime < 4) {
    draw_image(player_x, player_y, "dino_0.png", 2, 0);
  } else if (player_anime < 8) {
    draw_image(player_x, player_y, "dino_1.png", 2, 0);
  } else if (player_anime < 12) {
    draw_image(player_x, player_y, "dino_0.png", 2, 0);
  } else if (player_anime < 16) {
    draw_image(player_x, player_y, "dino_2.png", 2, 0);
  }

  if (sabo_type == 0) {
    draw_image(sabo_x, sabo_y, "saboten_0.png", 2, 0);
  }
  if (sabo_type == 1) {
    draw_image(sabo_x, sabo_y, "saboten_1.png", 2, 0);
  }
  if (sabo_type == 2) {
    draw_image(sabo_x, sabo_y, "saboten_2.png", 2, 0);
  }
  if (sabo_type == 3) {
    draw_image(sabo_x, sabo_y, "saboten_3.png", 2, 0);
  }
}

void gameover()
{
  if (get_key(key_enter) == 1 || get_mouse_button_left() == mb_click) {
    play_sound("ok.wav");
    play_bgm("bgm_stroll.mp3");
    scene_number = 0;
  }

  draw_image(640, 360, "bg.jpg");
  draw_text(540, 360, "GAME OVER");
  draw_text(540, 440, "SCORE:%d", score);
}

int main()
{
  initialize("‹°—³ƒ‰ƒ“", 1280, 720);

  play_bgm("bgm_stroll.mp3");

  for (;;) {
    if (update()) {
      break;
    }

    if (scene_number == 0) {
      title();
    } else if (scene_number == 1) {
      game();
    } else if (scene_number == 2) {
      gameover();
    }

    render();
  }

  finalize();

  return 0;
}

