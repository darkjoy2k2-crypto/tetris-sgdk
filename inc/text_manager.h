#pragma once

#include <genesis.h>

void text_manager_init(void);
void text_manager_set_enabled(bool enabled);
void text_manager_update(void);
bool text_manager_is_finished(void);
void text_manager_cleanup(void);
