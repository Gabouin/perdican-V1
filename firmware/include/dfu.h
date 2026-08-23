/*
 * dfu.h — reboot into the STM32 system bootloader (USB DFU).
 *
 * PERDICAN V1 exposes D+/D- on its USB-C port, and the STM32G431's factory
 * bootloader speaks DFU over exactly those pins. So the board can be
 * reflashed with no debugger at all:
 *
 *   - hold BOOT, tap RESET, release BOOT     (hardware route, always works)
 *   - or call dfu_reboot_to_bootloader()     (software route, used by the
 *     console `dfu` command and by the 1200-baud touch)
 */

#ifndef PERDICAN_DFU_H
#define PERDICAN_DFU_H

#include <stdbool.h>

/* Arms the magic word and resets. Does not return. */
void dfu_reboot_to_bootloader(void) __attribute__((noreturn));

/* Called from Reset_Handler before RAM init. If the magic word is armed,
 * clears it and jumps to the system bootloader instead of to main(). */
void dfu_check_reboot_magic(void);

/* True if the BOOT button is being held right now. */
bool dfu_boot_button_held(void);

#endif
