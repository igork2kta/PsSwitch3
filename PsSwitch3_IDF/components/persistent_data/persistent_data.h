#ifndef PERSISTENT_DATA_H
#define PERSISTENT_DATA_H

#include <stdbool.h>
#include <stddef.h>

void persistent_data_init(void);

bool persistent_data_read_name(char *buffer);
void persistent_data_write_name(const char *name);

bool persistent_data_get_start_state(void);
void persistent_data_set_start_state(bool state);

#endif