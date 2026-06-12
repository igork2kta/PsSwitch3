#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>
#include "Global.h"

// Inicializa estado Hue (gera username, etc)
void init(void);

// Getters
const char *get_device_name(void);
bool get_state(void);
uint8_t get_bri(void);
bool get_timer_state(void);
int get_timer_minutes(void);
bool get_start_state(void);
bool get_ota_state(void);

// Setters
void set_device_name(const char *name);
void set_state(bool on, uint8_t value);
void set_timer(bool new_state, uint8_t new_bri, uint16_t new_minutes);
void set_start_state(bool new_start_state);
void set_ota_state(bool new_ota_state);

// Utils
void toggle_state(void);
void apply_outputs();
void start_timer(int minutes, bool target_state);
void stop_light_timer(void);