/*
 * startup.c — vector table, reset entry and default fault handlers.
 *
 * Written in C rather than assembly so the vector table stays readable and
 * the DFU-magic check can run before anything else touches RAM.
 */

#include "stm32g431xx.h"
#include <stdint.h>
#include <string.h>

extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _estack;

extern int  main(void);
void Reset_Handler(void);
void Default_Handler(void);

/* Defined in dfu.c — checked before RAM init so the magic word survives. */
extern void dfu_check_reboot_magic(void);

#define ALIAS(f) __attribute__((weak, alias(#f)))

/* Cortex-M4 core exceptions */
void NMI_Handler(void)              ALIAS(Default_Handler);
void HardFault_Handler(void)        ALIAS(Default_Handler);
void MemManage_Handler(void)        ALIAS(Default_Handler);
void BusFault_Handler(void)         ALIAS(Default_Handler);
void UsageFault_Handler(void)       ALIAS(Default_Handler);
void SVC_Handler(void)              ALIAS(Default_Handler);
void DebugMon_Handler(void)         ALIAS(Default_Handler);
void PendSV_Handler(void)           ALIAS(Default_Handler);
void SysTick_Handler(void)          ALIAS(Default_Handler);

/* STM32G431 peripheral interrupts (only the ones this board can reach are
 * ever enabled, but the table must be complete and correctly ordered). */
void WWDG_IRQHandler(void)                      ALIAS(Default_Handler);
void PVD_PVM_IRQHandler(void)                   ALIAS(Default_Handler);
void RTC_TAMP_LSECSS_IRQHandler(void)           ALIAS(Default_Handler);
void RTC_WKUP_IRQHandler(void)                  ALIAS(Default_Handler);
void FLASH_IRQHandler(void)                     ALIAS(Default_Handler);
void RCC_IRQHandler(void)                       ALIAS(Default_Handler);
void EXTI0_IRQHandler(void)                     ALIAS(Default_Handler);
void EXTI1_IRQHandler(void)                     ALIAS(Default_Handler);
void EXTI2_IRQHandler(void)                     ALIAS(Default_Handler);
void EXTI3_IRQHandler(void)                     ALIAS(Default_Handler);
void EXTI4_IRQHandler(void)                     ALIAS(Default_Handler);
void DMA1_Channel1_IRQHandler(void)             ALIAS(Default_Handler);
void DMA1_Channel2_IRQHandler(void)             ALIAS(Default_Handler);
void DMA1_Channel3_IRQHandler(void)             ALIAS(Default_Handler);
void DMA1_Channel4_IRQHandler(void)             ALIAS(Default_Handler);
void DMA1_Channel5_IRQHandler(void)             ALIAS(Default_Handler);
void DMA1_Channel6_IRQHandler(void)             ALIAS(Default_Handler);
void DMA1_Channel7_IRQHandler(void)             ALIAS(Default_Handler);
void ADC1_2_IRQHandler(void)                    ALIAS(Default_Handler);
void USB_HP_IRQHandler(void)                    ALIAS(Default_Handler);
void USB_LP_IRQHandler(void)                    ALIAS(Default_Handler);
void FDCAN1_IT0_IRQHandler(void)                ALIAS(Default_Handler);
void FDCAN1_IT1_IRQHandler(void)                ALIAS(Default_Handler);
void EXTI9_5_IRQHandler(void)                   ALIAS(Default_Handler);
void TIM1_BRK_TIM15_IRQHandler(void)            ALIAS(Default_Handler);
void TIM1_UP_TIM16_IRQHandler(void)             ALIAS(Default_Handler);
void TIM1_TRG_COM_TIM17_IRQHandler(void)        ALIAS(Default_Handler);
void TIM1_CC_IRQHandler(void)                   ALIAS(Default_Handler);
void TIM2_IRQHandler(void)                      ALIAS(Default_Handler);
void TIM3_IRQHandler(void)                      ALIAS(Default_Handler);
void TIM4_IRQHandler(void)                      ALIAS(Default_Handler);
void I2C1_EV_IRQHandler(void)                   ALIAS(Default_Handler);
void I2C1_ER_IRQHandler(void)                   ALIAS(Default_Handler);
void I2C2_EV_IRQHandler(void)                   ALIAS(Default_Handler);
void I2C2_ER_IRQHandler(void)                   ALIAS(Default_Handler);
void SPI1_IRQHandler(void)                      ALIAS(Default_Handler);
void SPI2_IRQHandler(void)                      ALIAS(Default_Handler);
void USART1_IRQHandler(void)                    ALIAS(Default_Handler);
void USART2_IRQHandler(void)                    ALIAS(Default_Handler);
void USART3_IRQHandler(void)                    ALIAS(Default_Handler);
void EXTI15_10_IRQHandler(void)                 ALIAS(Default_Handler);
void RTC_Alarm_IRQHandler(void)                 ALIAS(Default_Handler);
void USBWakeUp_IRQHandler(void)                 ALIAS(Default_Handler);
void TIM8_BRK_IRQHandler(void)                  ALIAS(Default_Handler);
void TIM8_UP_IRQHandler(void)                   ALIAS(Default_Handler);
void TIM8_TRG_COM_IRQHandler(void)              ALIAS(Default_Handler);
void TIM8_CC_IRQHandler(void)                   ALIAS(Default_Handler);
void ADC3_IRQHandler(void)                      ALIAS(Default_Handler);
void FMC_IRQHandler(void)                       ALIAS(Default_Handler);
void LPTIM1_IRQHandler(void)                    ALIAS(Default_Handler);
void TIM5_IRQHandler(void)                      ALIAS(Default_Handler);
void SPI3_IRQHandler(void)                      ALIAS(Default_Handler);
void UART4_IRQHandler(void)                     ALIAS(Default_Handler);
void UART5_IRQHandler(void)                     ALIAS(Default_Handler);
void TIM6_DAC_IRQHandler(void)                  ALIAS(Default_Handler);
void TIM7_IRQHandler(void)                      ALIAS(Default_Handler);
void DMA2_Channel1_IRQHandler(void)             ALIAS(Default_Handler);
void DMA2_Channel2_IRQHandler(void)             ALIAS(Default_Handler);
void DMA2_Channel3_IRQHandler(void)             ALIAS(Default_Handler);
void DMA2_Channel4_IRQHandler(void)             ALIAS(Default_Handler);
void DMA2_Channel5_IRQHandler(void)             ALIAS(Default_Handler);
void ADC4_IRQHandler(void)                      ALIAS(Default_Handler);
void ADC5_IRQHandler(void)                      ALIAS(Default_Handler);
void UCPD1_IRQHandler(void)                     ALIAS(Default_Handler);
void COMP1_2_3_IRQHandler(void)                 ALIAS(Default_Handler);
void COMP4_5_6_IRQHandler(void)                 ALIAS(Default_Handler);
void COMP7_IRQHandler(void)                     ALIAS(Default_Handler);
void HRTIM1_Master_IRQHandler(void)             ALIAS(Default_Handler);
void HRTIM1_TIMA_IRQHandler(void)               ALIAS(Default_Handler);
void HRTIM1_TIMB_IRQHandler(void)               ALIAS(Default_Handler);
void HRTIM1_TIMC_IRQHandler(void)               ALIAS(Default_Handler);
void HRTIM1_TIMD_IRQHandler(void)               ALIAS(Default_Handler);
void HRTIM1_TIME_IRQHandler(void)               ALIAS(Default_Handler);
void HRTIM1_FLT_IRQHandler(void)                ALIAS(Default_Handler);
void HRTIM1_TIMF_IRQHandler(void)               ALIAS(Default_Handler);
void CRS_IRQHandler(void)                       ALIAS(Default_Handler);
void SAI1_IRQHandler(void)                      ALIAS(Default_Handler);
void TIM20_BRK_IRQHandler(void)                 ALIAS(Default_Handler);
void TIM20_UP_IRQHandler(void)                  ALIAS(Default_Handler);
void TIM20_TRG_COM_IRQHandler(void)             ALIAS(Default_Handler);
void TIM20_CC_IRQHandler(void)                  ALIAS(Default_Handler);
void FPU_IRQHandler(void)                       ALIAS(Default_Handler);
void I2C4_EV_IRQHandler(void)                   ALIAS(Default_Handler);
void I2C4_ER_IRQHandler(void)                   ALIAS(Default_Handler);
void SPI4_IRQHandler(void)                      ALIAS(Default_Handler);
void FDCAN2_IT0_IRQHandler(void)                ALIAS(Default_Handler);
void FDCAN2_IT1_IRQHandler(void)                ALIAS(Default_Handler);
void FDCAN3_IT0_IRQHandler(void)                ALIAS(Default_Handler);
void FDCAN3_IT1_IRQHandler(void)                ALIAS(Default_Handler);
void RNG_IRQHandler(void)                       ALIAS(Default_Handler);
void LPUART1_IRQHandler(void)                   ALIAS(Default_Handler);
void I2C3_EV_IRQHandler(void)                   ALIAS(Default_Handler);
void I2C3_ER_IRQHandler(void)                   ALIAS(Default_Handler);
void DMAMUX_OVR_IRQHandler(void)                ALIAS(Default_Handler);
void QUADSPI_IRQHandler(void)                   ALIAS(Default_Handler);
void DMA1_Channel8_IRQHandler(void)             ALIAS(Default_Handler);
void DMA2_Channel6_IRQHandler(void)             ALIAS(Default_Handler);
void DMA2_Channel7_IRQHandler(void)             ALIAS(Default_Handler);
void DMA2_Channel8_IRQHandler(void)             ALIAS(Default_Handler);
void CORDIC_IRQHandler(void)                    ALIAS(Default_Handler);
void FMAC_IRQHandler(void)                      ALIAS(Default_Handler);

typedef void (*vector_t)(void);

__attribute__((section(".isr_vector"), used))
const vector_t g_vectors[] = {
    (vector_t)&_estack,
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    MemManage_Handler,
    BusFault_Handler,
    UsageFault_Handler,
    0, 0, 0, 0,
    SVC_Handler,
    DebugMon_Handler,
    0,
    PendSV_Handler,
    SysTick_Handler,

    WWDG_IRQHandler,                    /*  0 */
    PVD_PVM_IRQHandler,
    RTC_TAMP_LSECSS_IRQHandler,
    RTC_WKUP_IRQHandler,
    FLASH_IRQHandler,
    RCC_IRQHandler,
    EXTI0_IRQHandler,                   /*  6 — IMU INT1 (PA0) */
    EXTI1_IRQHandler,                   /*  7 — IMU INT2 (PA1) */
    EXTI2_IRQHandler,
    EXTI3_IRQHandler,
    EXTI4_IRQHandler,
    DMA1_Channel1_IRQHandler,
    DMA1_Channel2_IRQHandler,
    DMA1_Channel3_IRQHandler,
    DMA1_Channel4_IRQHandler,
    DMA1_Channel5_IRQHandler,
    DMA1_Channel6_IRQHandler,
    DMA1_Channel7_IRQHandler,
    ADC1_2_IRQHandler,
    USB_HP_IRQHandler,                  /* 19 */
    USB_LP_IRQHandler,                  /* 20 — USB CDC */
    FDCAN1_IT0_IRQHandler,
    FDCAN1_IT1_IRQHandler,
    EXTI9_5_IRQHandler,
    TIM1_BRK_TIM15_IRQHandler,
    TIM1_UP_TIM16_IRQHandler,
    TIM1_TRG_COM_TIM17_IRQHandler,
    TIM1_CC_IRQHandler,
    TIM2_IRQHandler,
    TIM3_IRQHandler,
    TIM4_IRQHandler,
    I2C1_EV_IRQHandler,                 /* 31 */
    I2C1_ER_IRQHandler,
    I2C2_EV_IRQHandler,
    I2C2_ER_IRQHandler,
    SPI1_IRQHandler,
    SPI2_IRQHandler,
    USART1_IRQHandler,
    USART2_IRQHandler,
    USART3_IRQHandler,
    EXTI15_10_IRQHandler,
    RTC_Alarm_IRQHandler,
    USBWakeUp_IRQHandler,               /* 42 */
    TIM8_BRK_IRQHandler,
    TIM8_UP_IRQHandler,
    TIM8_TRG_COM_IRQHandler,
    TIM8_CC_IRQHandler,
    ADC3_IRQHandler,
    FMC_IRQHandler,
    LPTIM1_IRQHandler,
    TIM5_IRQHandler,
    SPI3_IRQHandler,
    UART4_IRQHandler,
    UART5_IRQHandler,
    TIM6_DAC_IRQHandler,
    TIM7_IRQHandler,
    DMA2_Channel1_IRQHandler,
    DMA2_Channel2_IRQHandler,
    DMA2_Channel3_IRQHandler,
    DMA2_Channel4_IRQHandler,
    DMA2_Channel5_IRQHandler,
    ADC4_IRQHandler,
    ADC5_IRQHandler,
    UCPD1_IRQHandler,
    COMP1_2_3_IRQHandler,
    COMP4_5_6_IRQHandler,
    COMP7_IRQHandler,
    HRTIM1_Master_IRQHandler,
    HRTIM1_TIMA_IRQHandler,
    HRTIM1_TIMB_IRQHandler,
    HRTIM1_TIMC_IRQHandler,
    HRTIM1_TIMD_IRQHandler,
    HRTIM1_TIME_IRQHandler,
    HRTIM1_FLT_IRQHandler,
    HRTIM1_TIMF_IRQHandler,
    CRS_IRQHandler,                     /* 75 */
    SAI1_IRQHandler,
    TIM20_BRK_IRQHandler,
    TIM20_UP_IRQHandler,
    TIM20_TRG_COM_IRQHandler,
    TIM20_CC_IRQHandler,
    FPU_IRQHandler,
    I2C4_EV_IRQHandler,
    I2C4_ER_IRQHandler,
    SPI4_IRQHandler,
    0,
    FDCAN2_IT0_IRQHandler,
    FDCAN2_IT1_IRQHandler,
    FDCAN3_IT0_IRQHandler,
    FDCAN3_IT1_IRQHandler,
    RNG_IRQHandler,
    LPUART1_IRQHandler,
    I2C3_EV_IRQHandler,
    I2C3_ER_IRQHandler,
    DMAMUX_OVR_IRQHandler,
    QUADSPI_IRQHandler,
    DMA1_Channel8_IRQHandler,
    DMA2_Channel6_IRQHandler,
    DMA2_Channel7_IRQHandler,
    DMA2_Channel8_IRQHandler,
    CORDIC_IRQHandler,
    FMAC_IRQHandler,
};

void __attribute__((noreturn)) Reset_Handler(void)
{
    /*
     * Runs before .data/.bss init on purpose: the reboot magic lives in
     * .noinit, and jumping to the ST bootloader must happen while the chip
     * is still in its reset state.
     */
    dfu_check_reboot_magic();

    /* Copy initialised data from flash to RAM. */
    uint32_t *src = &_sidata;
    for (uint32_t *dst = &_sdata; dst < &_edata; )
        *dst++ = *src++;

    /* Zero .bss (.noinit is deliberately skipped — see the linker script). */
    for (uint32_t *dst = &_sbss; dst < &_ebss; )
        *dst++ = 0;

    /* Cortex-M4F: enable CP10/CP11 before any float code runs. */
    SCB->CPACR |= (3UL << 20) | (3UL << 22);
    __asm volatile ("dsb; isb");

    /* Run C++ / __attribute__((constructor)) initialisers, if any. */
    extern void (*__init_array_start[])(void);
    extern void (*__init_array_end[])(void);
    for (void (**fn)(void) = __init_array_start; fn < __init_array_end; fn++)
        (*fn)();

    main();

    for (;;)
        __asm volatile ("wfi");
}

/*
 * Any unhandled exception parks here. The LED is driven directly (no board
 * layer, which may not be initialised) so a fault is visible as a fast
 * blink even if the fault happened during early init.
 */
void Default_Handler(void)
{
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN;
    GPIOA->MODER = (GPIOA->MODER & ~(3u << (3 * 2))) | (1u << (3 * 2));

    for (;;) {
        GPIOA->ODR ^= (1u << 3);
        for (volatile uint32_t i = 0; i < 1500000; i++)
            __asm volatile ("nop");
    }
}
