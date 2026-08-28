#include "board.h"
#include "clock.h"

uint32_t SystemCoreClock = BOARD_SYSCLK_HZ;

static void flash_set_latency(uint32_t ws)
{
    uint32_t acr = FLASH->ACR;
    acr &= ~FLASH_ACR_LATENCY;
    acr |= ws;
    FLASH->ACR = acr;
    while ((FLASH->ACR & FLASH_ACR_LATENCY) != ws)
        ;
}

void clock_init(void)
{
    RCC->CR |= RCC_CR_HSION;
    while (!(RCC->CR & RCC_CR_HSIRDY))
        ;

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_HSI;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_HSI)
        ;

    RCC->CR &= ~RCC_CR_PLLON;
    while (RCC->CR & RCC_CR_PLLRDY)
        ;

    RCC->APB1ENR1 |= RCC_APB1ENR1_PWREN;
    (void)RCC->APB1ENR1;

    PWR->CR1 = (PWR->CR1 & ~PWR_CR1_VOS) | PWR_CR1_VOS_0;
    while (PWR->SR2 & PWR_SR2_VOSF)
        ;
    PWR->CR5 &= ~PWR_CR5_R1MODE;

    flash_set_latency(FLASH_ACR_LATENCY_4WS);
    FLASH->ACR |= FLASH_ACR_PRFTEN | FLASH_ACR_ICEN | FLASH_ACR_DCEN;

    RCC->PLLCFGR =
          RCC_PLLCFGR_PLLSRC_HSI
        | (3u  << RCC_PLLCFGR_PLLM_Pos)
        | (85u << RCC_PLLCFGR_PLLN_Pos)
        | (0u  << RCC_PLLCFGR_PLLR_Pos)
        | RCC_PLLCFGR_PLLREN;

    RCC->CR |= RCC_CR_PLLON;
    while (!(RCC->CR & RCC_CR_PLLRDY))
        ;

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_HPRE) | RCC_CFGR_HPRE_DIV2;

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_SW) | RCC_CFGR_SW_PLL;
    while ((RCC->CFGR & RCC_CFGR_SWS) != RCC_CFGR_SWS_PLL)
        ;

    for (volatile uint32_t i = 0; i < 256; i++)
        __asm volatile ("nop");

    RCC->CFGR = (RCC->CFGR & ~RCC_CFGR_HPRE) | RCC_CFGR_HPRE_DIV1;

    RCC->CFGR &= ~(RCC_CFGR_PPRE1 | RCC_CFGR_PPRE2);

    SystemCoreClock = BOARD_SYSCLK_HZ;
}

void clock_init_usb(void)
{
    RCC->CRRCR |= RCC_CRRCR_HSI48ON;
    while (!(RCC->CRRCR & RCC_CRRCR_HSI48RDY))
        ;

    RCC->CCIPR &= ~RCC_CCIPR_CLK48SEL;

    RCC->APB1ENR1 |= RCC_APB1ENR1_CRSEN;
    (void)RCC->APB1ENR1;

    CRS->CFGR = (CRS->CFGR & ~(CRS_CFGR_RELOAD | CRS_CFGR_FELIM | CRS_CFGR_SYNCSRC | CRS_CFGR_SYNCDIV))
              | (47999u << CRS_CFGR_RELOAD_Pos)
              | (34u    << CRS_CFGR_FELIM_Pos)
              | CRS_CFGR_SYNCSRC_1;

    CRS->CR |= CRS_CR_AUTOTRIMEN | CRS_CR_CEN;
}

void clock_init_periph(void)
{
    RCC->CCIPR = (RCC->CCIPR & ~RCC_CCIPR_I2C1SEL) | (2u << RCC_CCIPR_I2C1SEL_Pos);

    RCC->AHB2ENR |= RCC_AHB2ENR_GPIOAEN | RCC_AHB2ENR_GPIOBEN
                  | RCC_AHB2ENR_GPIOCEN | RCC_AHB2ENR_GPIOFEN;
    (void)RCC->AHB2ENR;

    RCC->APB2ENR |= RCC_APB2ENR_SYSCFGEN;
    (void)RCC->APB2ENR;
}
