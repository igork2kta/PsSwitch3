#pragma once
#include <stddef.h>
#include <stdbool.h>
#include <stdint.h>

int template_build_ssdp_response(
    char *out,
    size_t out_len,
    const char *ip
);


int template_build_description_xml(
    char *out,
    size_t out_len,
    const char *ip,
    const char *mac
);


int template_build_lights_short(
    char *out, 
    size_t out_len, 
    const char *name, 
    const char *type, 
    const char *unique_id
);

int template_build_light_long(
    char *out, 
    size_t out_len, 
    const char *name, 
    const char *type, 
    const char *unique_id, 
    const uint8_t bri, 
    const bool state, 
    const bool timer_state, 
    int minutes,
    const bool start_state
);

int template_build_light_state_success(char *out, 
                                        size_t out_len,
                                        bool state,
                                        uint8_t bri
);

int template_build_light_timer_success(char *out,
                                        size_t out_len,
                                        bool timer_state,
                                        uint16_t minutes);

int template_build_message(char *out, size_t out_len, const char *message);
