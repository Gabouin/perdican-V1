#ifndef PERDICAN_DFU_H
#define PERDICAN_DFU_H

#include <stdbool.h>

void dfu_reboot_to_bootloader(void) __attribute__((noreturn));

void dfu_check_reboot_magic(void);

bool dfu_boot_button_held(void);

#endif
