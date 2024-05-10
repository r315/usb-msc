#include "at32f4xx.h"
#include "at32f4xx_rcc.h"

#define VECT_TAB_OFFSET 0x0

uint32_t SystemCoreClock;
static const uint8_t AHBPscTable[16] = {0, 0, 0, 0, 0, 0, 0, 0, 1, 2, 3, 4, 6, 7, 8, 9};

void SystemCoreClockUpdate(void)
{
    uint32_t tmp = 0, pllmult = 0, pllrefclk = 0, tempcfg = 0;

#if defined(AT32F415xx) || defined(AT32F421xx)
    uint32_t pllcfgen = 0, pllns = 0, pllms = 0, pllfr = 0;
    uint32_t pllsrcfreq = 0, retfr = 0;
#endif
#if defined(AT32F403Axx) || defined(AT32F407xx)
    uint32_t prediv = 0;
#endif
    /* Get SYSCLK source -------------------------------------------------------*/
    tmp = RCC->CFG & RCC_CFG_SYSCLKSTS;

    switch (tmp)
    {
    case RCC_CFG_SYSCLKSTS_HSI: /* HSI used as system clock */
        SystemCoreClock = HSI_VALUE;
        break;

    case RCC_CFG_SYSCLKSTS_HSE: /* HSE used as system clock */
        SystemCoreClock = HSE_VALUE;
        break;

    case RCC_CFG_SYSCLKSTS_PLL: /* PLL used as system clock */
        /* Get PLL clock source and multiplication factor ----------------------*/
        pllrefclk = RCC->CFG & RCC_CFG_PLLRC;
        tempcfg = RCC->CFG;
        pllmult = RCC_GET_PLLMULT(tempcfg);

#if defined(AT32F415xx) || defined(AT32F421xx)
        /* Get_ClocksFreq for PLLconfig2 */
        pllcfgen = BIT_READ(RCC->PLL, PLL_CFGEN_MASK);

        if (pllcfgen == PLL_CFGEN_ENABLE)
        {
            pllns = BIT_READ(RCC->PLL, PLL_NS_MASK);
            pllms = BIT_READ(RCC->PLL, PLL_MS_MASK);
            pllfr = BIT_READ(RCC->PLL, PLL_FR_MASK);

            RCC_FR_VALUE(pllfr, retfr);

            if (pllrefclk == 0x00)
            {
                /* HSI oscillator clock divided by 2 selected as PLL clock entry */
                pllsrcfreq = (HSI_VALUE >> 1);
            }
            else
            {
                /* HSE selected as PLL clock entry */
                if ((RCC->CFG & RCC_CFG_PLLHSEPSC) != (uint32_t)RESET)
                {
                    pllsrcfreq = (HSE_VALUE >> 1);
                }
                else
                {
                    pllsrcfreq = HSE_VALUE;
                }
            }

            SystemCoreClock = (pllsrcfreq * (pllns >> PLL_NS_POS)) /
                              ((pllms >> PLL_MS_POS) * retfr);
        }
        else
#endif
        {
            /* Get PLL clock source and multiplication factor ----------------------*/
            pllmult = BIT_READ(RCC->CFG, RCC_CFG_PLLMULT);
            pllmult = RCC_GET_PLLMULT(pllmult);

            if (pllrefclk == 0x00)
            {
                /* HSI oscillator clock divided by 2 selected as PLL clock entry */
                SystemCoreClock = (HSI_VALUE >> 1) * pllmult;
            }
            else
            {
                /* HSE selected as PLL clock entry */
                if ((RCC->CFG & RCC_CFG_PLLHSEPSC) != (uint32_t)RESET)
                {
#if defined(AT32F403Axx) || defined(AT32F407xx)
                    prediv = (RCC->MISC2 & RCC_HSE_DIV_MASK);
                    prediv = prediv >> RCC_HSE_DIV_POS;
                    /* HSE oscillator clock divided by prediv */
                    SystemCoreClock = (HSE_VALUE / (prediv + 2)) * pllmult;
#else
                    /* HSE oscillator clock divided by 2 */
                    SystemCoreClock = (HSE_VALUE >> 1) * pllmult;
#endif
                }
                else
                {
                    SystemCoreClock = HSE_VALUE * pllmult;
                }
            }
        }

        break;

    default:
        SystemCoreClock = HSI_VALUE;
        break;
    }

    /* Compute HCLK clock frequency ----------------*/
    /* Get HCLK prescaler */
    tmp = AHBPscTable[((RCC->CFG & RCC_CFG_AHBPSC) >> 4)];
    /* HCLK clock frequency */
    SystemCoreClock >>= tmp;
}

static void SystemCoreClockSet(void)
{
    __IO uint32_t StartUpCounter = 0, HSIStatus = 0;

    /* SYSCLK, HCLK, PCLK2 and PCLK1 configuration ---------------------------*/
    /* Enable HSI */
    RCC->CTRL |= ((uint32_t)RCC_CTRL_HSIEN);

    /* Wait till HSI is ready and if Time out is reached exit */
    do
    {
        HSIStatus = RCC->CTRL & RCC_CTRL_HSISTBL;
        StartUpCounter++;
    } while ((HSIStatus == 0) && (StartUpCounter != 0xFFFF));

    if ((RCC->CTRL & RCC_CTRL_HSISTBL) != RESET)
    {
        HSIStatus = (uint32_t)0x01;
    }
    else
    {
        HSIStatus = (uint32_t)0x00;
    }

    if (HSIStatus == (uint32_t)0x01)
    {
#if defined(AT32F415xx)
        /* Enable Prefetch Buffer */
        FLASH->ACR |= FLASH_ACR_PRFTBE;

        /* Flash 1 wait state */
        FLASH->ACR &= (uint32_t)((uint32_t)~FLASH_ACR_LATENCY);
        FLASH->ACR |= (uint32_t)FLASH_ACR_LATENCY_4;
#endif
        /* HCLK = SYSCLK */
        RCC->CFG |= (uint32_t)RCC_CFG_AHBPSC_DIV1;

        /* PCLK2 = HCLK/2 */
        RCC->CFG &= 0xFFFFC7FF;
        RCC->CFG |= (uint32_t)RCC_CFG_APB2PSC_DIV2;

        /* PCLK1 = HCLK/2 */
        RCC->CFG &= 0xFFFFF8FF;
        RCC->CFG |= (uint32_t)RCC_CFG_APB1PSC_DIV2;

        /*  PLL configuration: PLLCLK = ((HSI/2) * 150) / (1 * 4) = 150 MHz */
        RCC->CFG &= RCC_CFG_PLLCFG_MASK;
        RCC->CFG |= (uint32_t)(RCC_CFG_PLLRC_HSI_DIV2);
        RCC_PLLconfig2(150, 1, PLL_FR_4);

        /* Enable PLL */
        RCC->CTRL |= RCC_CTRL_PLLEN;

        /* Wait till PLL is ready */
        while ((RCC->CTRL & RCC_CTRL_PLLSTBL) == 0)
        {
        }
#if defined(AT32F413xx) || defined(AT32F403Axx) || \
    defined(AT32F407xx) || defined(AT32F415xx)
        RCC_StepModeCmd(ENABLE);
#endif
        /* Select PLL as system clock source */
        RCC->CFG &= (uint32_t)((uint32_t) ~(RCC_CFG_SYSCLKSEL));
        RCC->CFG |= (uint32_t)RCC_CFG_SYSCLKSEL_PLL;

        /* Wait till PLL is used as system clock source */
        while ((RCC->CFG & (uint32_t)RCC_CFG_SYSCLKSTS) != RCC_CFG_SYSCLKSTS_PLL)
        {
        }
#ifdef AT32F403xx
        WaitHseStbl(PLL_STABLE_DELAY);
#endif
#if defined(AT32F413xx) || defined(AT32F403Axx) || \
    defined(AT32F407xx) || defined(AT32F415xx)
        RCC_StepModeCmd(DISABLE);
#endif
    }
}

void SystemInit(void)
{
#if defined(__FPU_USED) && (__FPU_USED == 1U)
    SCB->CPACR |= ((3UL << 10 * 2) | (3UL << 11 * 2)); /* set CP10 and CP11 Full Access */
#endif
    RCC_APB1PeriphClockCmd(RCC_APB1PERIPH_PWR, ENABLE);
    /* Enable low power mode, 0x40007050[bit2] */
    *(volatile uint8_t *)(0x40007050) |= (uint8_t)(0x1 << 2);
    RCC_APB1PeriphClockCmd(RCC_APB1PERIPH_PWR, DISABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2PERIPH_GPIOA, ENABLE);
    RCC_APB2PeriphClockCmd(RCC_APB2PERIPH_GPIOB, ENABLE);

    /* Reset the RCC clock configuration to the default reset state(for debug purpose) */
    /* Set HSIEN bit */
    BIT_SET(RCC->CTRL, RCC_CTRL_HSIEN);

    /* Reset SW, AHBPSC, APB1PSC, APB2PSC, ADCPSC and CLKOUT bits */
    BIT_CLEAR(RCC->CFG, RCC_CFG_SYSCLKSEL | RCC_CFG_AHBPSC |
                            RCC_CFG_APB1PSC | RCC_CFG_APB2PSC |
                            RCC_CFG_ADCPSC | RCC_CFG_CLKOUT);

    /* Reset HSEEN, HSECFDEN and PLLEN bits */
    BIT_CLEAR(RCC->CTRL, RCC_CTRL_HSEEN | RCC_CTRL_HSECFDEN |
                             RCC_CTRL_PLLEN);

    /* Reset HSEBYPS bit */
    BIT_CLEAR(RCC->CTRL, RCC_CTRL_HSEBYPS);

    /* Reset PLLRC, PLLHSEPSC, PLLMUL, USBPSC and PLLRANGE bits */
    BIT_CLEAR(RCC->CFG, RCC_CFG_PLLRC | RCC_CFG_PLLHSEPSC |
                            RCC_CFG_PLLMULT | RCC_CFG_USBPSC | RCC_CFG_PLLRANGE);

    /* Reset USB768B, CLKOUT[3], HSICAL_KEY[7:0] */
    BIT_CLEAR(RCC->MISC, 0x010100FF);

    /* Disable all interrupts and clear pending bits  */
    RCC->CLKINT = RCC_CLKINT_LSISTBLFC | RCC_CLKINT_LSESTBLFC |
                  RCC_CLKINT_HSISTBLFC | RCC_CLKINT_HSESTBLFC |
                  RCC_CLKINT_PLLSTBLFC | RCC_CLKINT_HSECFDFC;

    /* Configure the System clock frequency, HCLK, PCLK2 and PCLK1 prescalers */
    /* Configure the Flash Latency cycles and enable prefetch buffer */
    SystemCoreClockSet();
    SystemCoreClockUpdate();
#ifdef VECT_TAB_SRAM
    SCB->VTOR = SRAM_BASE | VECT_TAB_OFFSET; /* Vector Table Relocation in Internal SRAM. */
#else
    SCB->VTOR = FLASH_BASE | VECT_TAB_OFFSET; /* Vector Table Relocation in Internal FLASH. */
#endif
}