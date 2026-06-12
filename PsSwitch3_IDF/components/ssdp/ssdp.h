#pragma once
#include <stdbool.h>

extern volatile bool ssdp_running ;

void ssdp_start(void);
void ssdp_stop(void);