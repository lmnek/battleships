/*******************************************************************
  Battleships - semestral project on MZ_APO board.

  main.c      - main file controlling game state

  Kryštof Gärtner, Filip Doležal

 *******************************************************************/

#define _POSIX_C_SOURCE 200112L

#include <stdlib.h>
#include <stdio.h>
#include <stdint.h>
#include <unistd.h>
#include <stdbool.h>
#include <string.h>

#include "peripheries.h"
#include "tcp.h"
#include "utils.h"
#include "constants.h"
#include "ui.h"

// text shown in top right corner
char *p1 = "P1";
char *p2 = "P2";

typedef struct
{
  short curx;
  short cury;
  short knobX;
  short knobY;
  bool horizontal;
  int (*board)[10];
  int (*boardEnemy)[10];
  int shipsLeftPlayer;
  int shipsLeftEnemy;
  bool player;
} game_info; // struct with all important game data

void draw_board_enemy(game_info *info)
{
  draw_board(info->boardEnemy);
  highlight_box(info->curx, info->cury);
}

// draw ship in frame buffer from given ship length and rotation
void redraw_ship(game_info *info, int length)
{
  for (size_t i = 0; i < length; i++)
  {
    if (info->horizontal)
      fill_board_box(info->curx + i, info->cury, SHIP);
    else
      fill_board_box(info->curx, info->cury + i, SHIP);
  }
}

// place ship on main players board
void place_ship(game_info *info, int length)
{
  printf("horizontal: %d", info->horizontal);
  for (size_t i = 0; i < length; i++)
  {
    if (info->horizontal)
      info->board[info->cury][info->curx + i] = SHIP;
    else
      info->board[info->cury + i][info->curx] = SHIP;
  }
}

// set cursor from given knob values
int set_cursor(short *curKnob, short prevKnob, short *cur, bool modulate)
{
  // how much knob value changed
  int dif = prevKnob - *curKnob;
  if (dif > 200)
    dif = -(255 - (prevKnob - *curKnob));
  else if (dif < -200)
    dif = 255 - (*curKnob - prevKnob);

  // knob value crossed the threshold ? -> new cursor position
  if (dif > KNOB_PRECISION || dif < -KNOB_PRECISION)
  {
    *curKnob = prevKnob;
    if (modulate)
    {
      // modulo cursor
      *cur = ((*cur + (dif > 0 ? -1 : +1)) % 10);
      *cur += *cur < 0 ? 10 : 0;
    }
    else
    {
      // stop at the grid borders
      *cur = (*cur + (dif > 0 ? -1 : +1));
      *cur = *cur < 0 ? 0 : *cur;
      *cur = *cur > 9 ? 9 : *cur;
    }
    return true;
  }
  return false;
}

// check if ship can be placed in the current cursor position
bool can_place(game_info *info, int length)
{
  short x = info->curx;
  short y = info->cury;
  printf("\nx: %d, y: %d\n", x, y);
  for (size_t i = 0; i < length; i++)
  {
    if (info->horizontal)
      x = info->curx + i;
    else
      y = info->cury + i;

    bool l = (x - 1 >= 0) ? (info->board[y][x - 1] == SHIP) : false;
    bool r = (x + 1 <= 9) ? (info->board[y][x + 1] == SHIP) : false;
    bool b = (y + 1 <= 9) ? (info->board[y + 1][x] == SHIP) : false;
    bool t = (y - 1 >= 0) ? (info->board[y - 1][x] == SHIP) : false;

    if (info->board[y][x] == SHIP || l || r || b || t)
      return false;
  }
  printf("can place!\n");
  return true;
}

// game state when player is choosing
// where to place his ships on the board
void place_ships(game_info *info)
{
  for (size_t i = 0; i < TOTAL_SHIPS; i++)
  {
    info->curx = 0;
    info->cury = 0;
    redraw_ship(info, SHIPS[i]);
    clear_place(0, 0, STARTX, STARTY + SIZEY);
    draw_char(15, 15, (TOTAL_SHIPS - i) + '0', light_green);
    draw_lcd();

    uint32_t knobs = get_knobs();
    // break when knob is pressed and ship can be placed
    while (((knobs & 0x7000000) == 0) || !can_place(info, SHIPS[i]))
    {
      knobs = get_knobs();
      bool horizontal = (bool)((((knobs >> 16) & 0xff) / 27) % 2); // ship rotation
      // Moved with cursor or rotated ship?
      if (set_cursor(&(info->knobX), (knobs & 0xff), &(info->curx), false) || set_cursor(&(info->knobY), ((knobs >> 8) & 0xff), &(info->cury), false) || horizontal != info->horizontal)
      {
        // handle ship rotation
        if (info->horizontal != horizontal)
        {
          info->horizontal = horizontal;
          if (!horizontal && info->cury + SHIPS[i] >= BOX_COUNT)
            info->cury = info->cury - (info->cury + SHIPS[i] - BOX_COUNT);
          if (horizontal && info->curx + SHIPS[i] >= BOX_COUNT)
            info->curx = info->curx - (info->curx + SHIPS[i] - BOX_COUNT);
        }
        // Block moving when ship touches the border
        if (!horizontal && info->cury + SHIPS[i] >= BOX_COUNT)
          info->cury--;
        if (horizontal && info->curx + SHIPS[i] >= BOX_COUNT)
          info->curx--;

        draw_board(info->board);
        redraw_ship(info, SHIPS[i]);
        draw_lcd();
      }
    }
    place_ship(info, SHIPS[i]);
    draw_board(info->board);

    // wait for player to unpress the knob
    while ((get_knobs() & 0x7000000) != 0)
    {
    }
    delay(10);
  }
  clear_place(0, 0, STARTX, STARTY + SIZEY);
  draw_char(15, 15, '0', light_green);
  draw_lcd();
  send_ready();
}

// shows players and board and waits for enemy to shoot
int player_state(game_info *info)
{
  draw_string(480 - string_width(0, info->player ? p1 : p2), 12, info->player ? p1 : p2, light_green);
  clear_place(0, 0, STARTX, STARTY + SIZEY);
  draw_char(15, 15, info->shipsLeftPlayer + '0', light_green);
  printf("entered player state\n");
  draw_board(info->board);
  draw_lcd();
  short shotx, shoty;
  // receive info where enemy shot
  receive_coords(&shotx, &shoty);

  int *cell = &(info->board[shoty][shotx]);
  led_line_left();
  // react to enemy shot
  if (*cell == SHIP && flood_filled(info->board, shotx, shoty))
  {
    printf("Naše loď potopena!\n");
    info->shipsLeftPlayer--;
    clear_place(0, 0, STARTX, STARTY + SIZEY);
    draw_char(15, 15, info->shipsLeftPlayer + '0', light_green);
  }
  else if (*cell == SHIP)
  {
    *cell = SHIP_HIT;
    printf("Naše loď zasažena!\n");
  }
  else
  {
    *cell = SEA_HIT;
    printf("HAHAHA!!! Vedle jak ta jedle!\n");
  }
  send_response(*cell); // send enemy response

  draw_board(info->board);
  draw_lcd();
  react_to_cell(*cell, false);

  printf("leaving player state\n");
  clear_place(STARTX + GRID_SIZE + 1, 0, SIZEX, SIZEY);
  // 0 ships left?
  if (info->shipsLeftPlayer <= 0)
    return LOST;
  return RUNNING;
}

// state where enemies board is on the screen
// and player has to choose where to shoot
int enemy_state(game_info *info)
{
  printf("entered enemy state\n");
  draw_string(480 - string_width(0, info->player ? p2 : p1), 12, info->player ? p2 : p1, light_green);
  clear_place(0, 0, STARTX, STARTY + SIZEY);
  draw_char(15, 15, info->shipsLeftEnemy + '0', light_green);
  draw_board_enemy(info);
  draw_lcd();

  bool shot = false;
  while (!shot)
  {
    uint32_t knobs = get_knobs();
    // Knobs rotated? -> cursor changed
    if (set_cursor(&(info->knobX), knobs & 0xff, &(info->curx), true) || set_cursor(&(info->knobY), (knobs >> 8) & 0xff, &(info->cury), true))
    {
      printf("curx:%d cury:%d\n", info->curx, info->cury);
      draw_board_enemy(info);
      draw_lcd();
    }

    // Knobs pressed?
    if ((knobs & 0x7000000) != 0 && info->boardEnemy[info->cury][info->curx] == SEA)
    {
      while ((get_knobs() & 0x7000000) != 0)
      {
      }
      // Shoot
      printf("BANG!!!!!!!\n");
      shot = true;
      int *cell = &(info->boardEnemy[info->cury][info->curx]);
      *cell = send_coord(info->curx, info->cury);

      led_line_right();
      if (*cell == SUNKEN_SHIP)
      {
        flood_filled(info->boardEnemy, info->curx, info->cury);
        info->shipsLeftEnemy--;
        clear_place(0, 0, STARTX, STARTY + SIZEY);
        draw_char(15, 15, info->shipsLeftEnemy + '0', light_green);
      }
      draw_board_enemy(info);
      draw_lcd();
      react_to_cell(*cell, true);
    }
  }
  printf("leaving enemy state\n");
  clear_place(STARTX + GRID_SIZE + 1, 0, SIZEX, SIZEY);
  // 0 ships left?
  if (info->shipsLeftEnemy <= 0)
    return WON;
  return RUNNING;
}

// initialize all needed variables
game_info init()
{
  setup_peripheries();
  fb = (unsigned short *)malloc(320 * 480 * 2);
  // init game boards
  int *gameBoard_p = malloc(BOARD_LEN * BOARD_LEN * sizeof(int));
  int(*board)[BOARD_LEN] = (int(*)[BOARD_LEN])gameBoard_p;
  int *gameBoard_p2 = malloc(BOARD_LEN * BOARD_LEN * sizeof(int));
  int(*boardEnemy)[BOARD_LEN] = (int(*)[BOARD_LEN])gameBoard_p2;
  for (int i = 0; i < 10; i++)
  {
    for (int j = 0; j < 10; j++)
    {
      boardEnemy[i][j] = SEA;
      board[i][j] = SEA;
    }
  }
  uint32_t knobs = get_knobs();
  // init info
  game_info info = (game_info){0, 0, knobs & 0xff, (knobs >> 8) & 0xff, 0, board, boardEnemy, TOTAL_SHIPS, TOTAL_SHIPS, false};
  return info;
}

int main(int argc, char *argv[])
{
  game_info info = init();
  bool onTurn = main_menu();
  info.player = onTurn;
  setup_connection(onTurn);
  clear_screen();
  draw_grid();
  draw_grid_chars();
  place_ships(&info);

  // game loop -> players are switching until one of them win
  int state = RUNNING;
  while (state == RUNNING)
  {
    printf("new turn\n");
    state = onTurn ? enemy_state(&info) : player_state(&info);
    delay(1500);
    onTurn = !onTurn;
  }

  draw_lcd();

  game_over(state);

  printf("Goodbye world\n");
  clear_screen();
  draw_lcd();
  turn_led_off();

  free(fb);
  free(info.board);
  free(info.boardEnemy);
  close_socket();
  return 0;
}
