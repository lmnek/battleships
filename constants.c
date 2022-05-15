
#include "font_types.h"

const int sizeX = 480;
const int sizeY = 320;
const int GRID_SIZE = 276;
const int BOX_COUNT = 11;
const int BOARD_LEN = BOX_COUNT - 1;
const int BOX_SIZE = (GRID_SIZE - (BOX_COUNT + 1)) / BOX_COUNT;
const int LINE_MOD = BOX_SIZE + 1;
const int scale = 2;
const int startX = (sizeX - GRID_SIZE) / 2;
const int startY = (sizeY - GRID_SIZE) / 2;
const int knob_precision = 10;
const int ships[] = {
    2, 2};

const int dark_green = 384;
const int light_green = 40928;

unsigned short *fb;
font_descriptor_t *fdes;