/*
 * clock.c — clock tree bring-up for PERDICAN V1.
 *
 * PERDICAN V1 has no crystal (PF0/PF1 and PC14/PC15 are unconnected), so
 * every clock comes from an internal oscillator:
 *
 *   SYSCLK = 170 MHz : HSI16 /4 = 4 MHz, *85 = 340 MHz VCO, /R2 = 170 MHz
 *   USB    =  48 MHz : HSI48, continuously trimmed by CRS against USB SOF
 *   I2C1   =  16 MHz : HSI16 straight through
 *
 * Running above 150 MHz requires voltage Range 1 "boost" mode, which has a
 * mandatory entry sequence (RM0440 §6.1.5): the AHB prescaler must divide
 * by 2 across the switch and only be restored at least 1 us later.
 */

#include "board.h"
#include "clock.h"

/*
 * CMSIS declares this in system_stm32g4xx.h. ST's system_stm32g4xx.c is not
 * vendored here — clock_init() below is the whole clock setup — so the
 * variable is defined at its post-init value and kept current by hand.
 */
uint32_t SystemCoreClock = BOARD_SYSCLK_HZ;

static void flash_set_latency(uint32_t ws)
{
    uint32_t acr = FLASH->ACR;
    acr &= ~FLASH_ACR_LATENCY;
    acr |= ws;
    FLASH->ACR = acr;
    /* The latency change is not immediate; it must be read back. */
    while ((FLASH->ACR & FLASH_ACR_LATENCY) != ws)
        ;
}

void clock_init(void)
{
    /* --- HSI16 on and selected, so we have a known clock to work from --- */
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY))
        ;

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_HSI;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI)
        ;

    /* Drop the PLL so it can be reconfigured. */
    RCC->CR &= ~RCC_CR_PLLON;
    while (RCC->CR & RCC_CR_PLLRDY)
        ;

    /* --- Voltage scaling: Range 1 boost, required for 170 MHz --- */
    RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
    (void)RCC->APB1ENR1;

    PWR->CR1 = (PWR->CR1 & ~PWR_CR1_VOS) | PWR_CR1_VOS_0;   /* Range 1 */
    while (PWR->SR2 & PWR_SR2_VOSF)
        ;
    PWR->CR5 &= ~PWR_CR5_R1MODE;                            /* boost on  */

    /* --- Flash: 4 wait states for 170 MHz in Range 1 boost --- */
    flash_set_latency(FLASH_ACR_LATENCY_4WS);
    FLASH->ACR |= FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;

    /*
     * --- PLL: HSI16 / 4 * 85 / 2 = 170 MHz ---
     * PLL input must land in 2.66..16 MHz  -> 16/4  = 4 MHz    OK
     * VCO must land in 96..344 MHz         -> 4*85  = 340 MHz  OK
     * PLLR output <= 170 MHz               -> 340/2 = 170 MHz  OK
     */
    RCC->PLLCFGR =
          RCC_PLLCFGR_PLLSRC_HSI            /* HSI16 as PLL source        */
        | (3u  << RCC_PLLCFGR_PLLM_Pos)     /* PLLM = 3+1 = 4             */
        | (85u << RCC_PLLCFGR_PLLN_Pos)     /* PLLN = 85                  */
        | (0u  << RCC_PLLCFGR_PLLR_Pos)     /* PLLR = 2                   */
        | RCC_PLLCFGR_PLLREN;               /* enable the R output        */

    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY))
        ;

    /*
     * --- Boost-mode switch sequence ---
     * Step through HCLK/2 so the core never sees a frequency step larger
     * than the LDO can follow.
     */
    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_HPRE) | RCC_CFGR_HPRE_DIV2;

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL)
        ;

    /* Hold for >1 us at 85 MHz. 256 cycles is ~3 us and needs no timer. */
    for (volatile uint32_t i = 0; i < 256; i++)
        __asm volatile ("nop");

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_HPRE) | RCC_CFGR_HPRE_DIV1;

    /* APB1/APB2 stay at /1: both are rated for 170 MHz on STM32G4. */
    RCC->CFGR &= ~(RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);

    SystemCoreClock = BOARD_SYSCLK_HZ;
}

void clock_init_usb(void)
{
    /*
     * USB needs 48 MHz with better than 0.25% accuracy. HSI48 alone is only
     * ~3%, so CRS locks it to the host's 1 kHz start-of-frame. This is what
     * lets the board run USB with no crystal fitted.
     */
    RCC->CRRCR |= RCC_CRRCR_HSI48ON;
    while (!(RCC->CRRCR & RCC_CRRCR_HSI48RDY))
        ;

    /* Route the 48 MHz clock domain to HSI48. */
    RCC->CCIPR &= ~RCC_CCIPR_CLK48SEL;      /* 00 = HSI48 */

    /* CRS: sync source = USB SOF, automatic trimming. */
    RCC->APB1ENR1 |= RCC_APB1ENR1_CRSEN;
    (void)RCC->APB1ENR1;

    /*
     * RELOAD = (48 MHz / 1 kHz) - 1 = 47999, i.e. the expected number of
     * HSI48 cycles between two SOF packets.
     */
    CRS->CFGR = (CRS->CFGR & ~(CRS_CFGR_RELOAD | CRS_CFGR_FELIM | CRS_CFGR_SYNCSRC | CRS_CFGR_SYNCDIV))
              | (47999u << CRS_CFGR_RELOAD_Pos)
              | (34u    << CRS_CFGR_FELIM_Pos)      /* ST-recommended limit */
              | CRS_CFGR_SYNCSRC_1;                 /* 10 = USB SOF         */

    CRS->CR |= CRS_CR_AUTOTRIMEN | CRS_CR_CEN;
}

void clock_init_periph(void)
{
    /*
     * Clock I2C1 from HSI16 rather than PCLK. The I2C timing register then
     * describes a fixed 16 MHz input, so bus timing stays correct even if
     * SYSCLK is changed later.
     */
    RCC->CCIPR = (RCC->CCIPR & ~RCC_CCIPR_I2C1SEL) | (2u << RCC_CCIPR_I2C1SEL_Pos);

    /* GPIO banks used on this board. */
    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN
                  | RCC_AHB2ENR_GPIOCEN | RCC_AHB2ENR_GPIOFEN;
    (void)RCC->AHB2ENR;

    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    (void)RCC->APB2ENR;
}
