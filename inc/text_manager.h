#pragma once

#include <genesis.h>

void text_manager_init(void);
void text_manager_init_highscore(void);
void text_manager_init_vs_countdown(void);
void text_manager_init_vs_winner(const char* text);
void text_manager_glyphs_visible(bool state);
void text_manager_set_enabled(bool enabled);
void text_manager_request_exit(void);
void text_manager_update(void);
bool text_manager_is_finished(void);
void text_manager_cleanup(void);
