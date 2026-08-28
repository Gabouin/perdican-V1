#include "board.h"
#include "dfu.h"
#include "gpio.h"

#define SYSTEM_MEMORY_BASE  0x1FFF0000u

#define DFU_MAGIC           0xB00710ADu

__attribute__((section(".noinit"))) static volatile uint32_t s_dfu_magic;

void dfu_reboot_to_bootloader(void)
{
    s_dfu_magic = DFU_MAGIC;

    __asm volatile ("dsb");
    NVIC_SystemReset();

    for (;;)
        ;
}

void dfu_check_reboot_magic(void)
{
    if (s_dfu_magic != DFU_MAGIC)
        return;

    s_dfu_magic = 0;

    __disable_irq();
    SysTick->CTRL = 0;
    SysTick->VAL  = 0;

    for (unsigned i = 0; i < 8u; i++) {
        NVIC->ICER[i] = 0xFFFFFFFFu;
        NVIC->ICPR[i] = 0xFFFFFFFFu;
    }

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
    return gpio_read(BOOT0_PORT, BOOT0_PIN);
}
