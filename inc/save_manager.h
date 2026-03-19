#pragma once

#include <genesis.h>

#define SAVE_MAGIC      0x54455452
#define SAVE_VERSION    1

#define ADDR_MAGIC      0
#define ADDR_VERSION    4
#define ADDR_OPTIONS    10
#define ADDR_HIGHSCORES 128
#define ADDR_CHECKSUM   510

void save_init();
void save_load();
void save_execute();
void save_options();
void save_highscores();
void save_clear();