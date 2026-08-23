/*
 * dfu.c — software entry into the STM32G431 system bootloader.
 *
 * Jumping straight to 0x1FFF0000 from a running application is unreliable:
 * peripherals (especially USB) are still live, clocks are far from their
 * reset values, and the bootloader assumes a freshly reset chip. Instead
 * this arms a magic word, triggers a system reset, and performs the jump
 * out of Reset_Handler while the hardware really is in its reset state.
 *
 * The magic lives in a .noinit RAM word rather than a TAMP backup register,
 * because VBAT is left floating on PERDICAN V1 and the backup domain is
 * therefore not trustworthy (see board.h).
 */

#include "board.h"
#include "dfu.h"
#include "gpio.h"

/* System memory base for STM32G4. */
#define SYSTEM_MEMORY_BASE  0x1FFF0000u

#define DFU_MAGIC           0xB00710ADu

/* Not cleared by startup — that is the whole point. */
__attribute__((section(".noinit"))) static volatile uint32_t s_dfu_magic;

void dfu_reboot_to_bootloader(void)
{
    s_dfu_magic = DFU_MAGIC;

    __asm volatile ("dsb");
    NVIC_SystemReset();

    for (;;)
        ;   /* NVIC_SystemReset does not return */
}

void dfu_check_reboot_magic(void)
{
    if (s_dfu_magic != DFU_MAGIC)
        return;

    s_dfu_magic = 0;

    /*
     * Undo the little the bootloader will not: make sure no interrupt can
     * fire between here and the jump, and hand it a clean vector table.
     */
    __disable_irq();
    SysTick->CTRL = 0;
    SysTick->VAL  = 0;

    for (unsigned i = 0; i < 8u; i++) {
        NVIC->ICER[i] = 0xFFFFFFFFu;
        NVIC->ICPR[i] = 0xFFFFFFFFu;
    }

    /* Relocate the vector table to system memory. */
    SCB->VTOR = SYSTEM_MEMORY_BASE;

    const uint32_t *vectors = (const uint32_t *)SYSTEM_MEMORY_BASE;
    const uint32_t  sp      = vectors[0];
    const uint32_t  pc      = vectors[1];

    __set_MSP(sp);
    __asm volatile ("dsb; isb");

    ((void (*)(void))pc)();

    for (;;)
        ;
}

bool dfu_boot_button_held(void)
{
    /* PB8 has a 10k pull-down (R3); the BOOT button pulls it up to 3V3. */
    return gpio_read(BOOT0_PORT, BOOT0_PIN);
}
