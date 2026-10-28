
#ifndef SWAP_H
#define SWAP_H

#include <stdint.h>
#include <stdbool.h>
#include "hardware_specs.h"

bool swap_init(void);

bool tratar_page_fault(
    uint32_t vpn,
    hardware_state_t *hardware,
    uint32_t timestamp_ms
);

void swap_close(void);

#endif
