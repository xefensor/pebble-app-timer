#pragma once

#include <stdbool.h>

/* UI preference only. It never belongs to a timer bank or alarm transaction. */
bool timer_appearance_load(void);
bool timer_appearance_save(bool dark);
