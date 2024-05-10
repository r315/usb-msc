#include <stdint.h>
#undef USE_STDPERIPH_DRIVER
#include "at32f4xx.h"
#include "core_cm4.h"


#define WEAK    __attribute__((weak))
#define ISR     __attribute__((section(".isr_vector")))
#define NORETURN   __attribute__((naked, __noreturn__, aligned(4)))


ISR void *vector_table[];
extern uint32_t _sidata, _sdata, _edata, _sbss, _ebss, _stack, _estack;

extern void  __libc_init_array();
WEAK int main(void){}

NORETURN void Reset_Handler(void)
{
    volatile uint32_t *src, *dest;

    /* Copy initialize variables with .data section values*/
    for (src = &_sidata, dest = &_sdata; dest < &_edata; src++, dest++){
        *dest = *src;
    }

    /* Clear ram remaining ram to stack top */
    dest = &_sbss;

    while (dest < &_estack)
        *dest++ = 0;

    __libc_init_array();
    
    main();

    while(1){}
}


typedef struct {
    uint32_t r0, r1, r2, r3, r12, lr, pc, psr;
}stackframe_t;

void Stack_Dump(stackframe_t *stack){
    (void)stack;
    //GPIOJ->MODER = (1 << 26);
    //HAL_GPIO_WritePin(GPIOJ, GPIO_PIN_13, GPIO_PIN_SET);

    asm volatile
    (
        "bkpt #01 \n"
        "b . \n"
    );
}

void HardFault_Handler(void){
    asm volatile
    (
        " tst lr, #4                                 \n"        // Check current stack
        " ite eq                                     \n"
        " mrseq r0, msp                              \n"        // Move msp to r0 ??
        " mrsne r0, psp                              \n"        // Move psp to r0 ??
        " ldr r1, [r0, #24]                          \n"        // Get address were exception happen ?
        " b Stack_Dump                               \n"
        //" ldr r2, dumpHandler_address                \n"
        //" bx r2                                      \n"
        //" dumpHandler_address: .word Stack_Dump     \n"
    );
}

void Default_Handler(void){
    volatile uint8_t isr_number = (SCB->ICSR & 255) - 16;
    // See position number on Table 1-3 from RM_AT32F415
    (void)isr_number;

    asm volatile
    (
        "bkpt #01 \n"
        "b .      \n"
    );
}

void NMI_Handler(void)          __attribute__ ((weak, alias("Default_Handler")));
void MemManage_Handler(void)    __attribute__ ((weak, alias("Default_Handler")));
void HardFault_Handler(void);
void BusFault_Handler(void)     __attribute__ ((weak, alias("Default_Handler")));
void UsageFault_Handler(void)   __attribute__ ((weak, alias("Default_Handler")));
void SVC_Handler(void)          __attribute__ ((weak, alias("Default_Handler")));
void DebugMon_Handler(void)     __attribute__ ((weak, alias("Default_Handler")));
void PendSV_Handler(void)       __attribute__ ((weak, alias("Default_Handler")));
void SysTick_Handler(void)      __attribute__ ((weak, alias("Default_Handler")));

void WWDG_IRQHandler(void)      __attribute__ ((weak, alias("Default_Handler")));
void PVD_IRQHandler(void)       __attribute__ ((weak, alias("Default_Handler")));
void TAMP_STAMP_IRQHandler(void)__attribute__ ((weak, alias("Default_Handler")));
void ERTC_WKUP_IRQHandler(void) __attribute__ ((weak, alias("Default_Handler")));
void FLASH_IRQHandler(void)     __attribute__ ((weak, alias("Default_Handler")));
void RCC_IRQHandler(void)       __attribute__ ((weak, alias("Default_Handler")));
void EXTI0_IRQHandler(void)     __attribute__ ((weak, alias("Default_Handler")));
void EXTI1_IRQHandler(void)     __attribute__ ((weak, alias("Default_Handler")));
void EXTI2_IRQHandler(void)     __attribute__ ((weak, alias("Default_Handler")));
void EXTI3_IRQHandler(void)     __attribute__ ((weak, alias("Default_Handler")));
void EXTI4_IRQHandler(void)     __attribute__ ((weak, alias("Default_Handler")));
void DMA1_Channel1_IRQHandler(void) __attribute__ ((weak, alias("Default_Handler")));
void DMA1_Channel2_IRQHandler(void) __attribute__ ((weak, alias("Default_Handler")));
void DMA1_Channel3_IRQHandler(void) __attribute__ ((weak, alias("Default_Handler")));
void DMA1_Channel4_IRQHandler(void) __attribute__ ((weak, alias("Default_Handler")));
void DMA1_Channel5_IRQHandler(void) __attribute__ ((weak, alias("Default_Handler")));
void DMA1_Channel6_IRQHandler(void) __attribute__ ((weak, alias("Default_Handler")));
void DMA1_Channel7_IRQHandler(void) __attribute__ ((weak, alias("Default_Handler")));
void ADC1_2_IRQHandler(void)    __attribute__ ((weak, alias("Default_Handler")));
void CAN1_TX_IRQHandler(void)   __attribute__ ((weak, alias("Default_Handler")));
void CAN1_RX0_IRQHandler (void) __attribute__ ((weak, alias("Default_Handler")));
void CAN1_RX1_IRQHandler (void) __attribute__ ((weak, alias("Default_Handler")));
void CAN1_SCE_IRQHandler (void) __attribute__ ((weak, alias("Default_Handler")));
void EXTI9_5_IRQHandler (void)  __attribute__ ((weak, alias("Default_Handler")));
void TMR1_BRK_TMR9_IRQHandler (void) __attribute__ ((weak, alias("Default_Handler")));
void TMR1_OV_TMR10_IRQHandler (void) __attribute__ ((weak, alias("Default_Handler")));
void TMR1_TRG_COM_TMR11_IRQHandler (void) __attribute__ ((weak, alias("Default_Handler")));
void TMR1_CC_IRQHandler (void)   __attribute__ ((weak, alias("Default_Handler")));
void TMR2_GLOBAL_IRQHandler (void) __attribute__ ((weak, alias("Default_Handler")));
void TMR3_GLOBAL_IRQHandler (void) __attribute__ ((weak, alias("Default_Handler")));
void TMR4_GLOBAL_IRQHandler (void) __attribute__ ((weak, alias("Default_Handler")));
void I2C1_EV_IRQHandler (void)  __attribute__ ((weak, alias("Default_Handler")));
void I2C1_ER_IRQHandler (void)  __attribute__ ((weak, alias("Default_Handler")));
void I2C2_EV_IRQHandler (void)  __attribute__ ((weak, alias("Default_Handler")));
void I2C2_ER_IRQHandler (void)  __attribute__ ((weak, alias("Default_Handler")));
void SPI1_IRQHandler (void)     __attribute__ ((weak, alias("Default_Handler")));
void SPI2_IRQHandler (void)     __attribute__ ((weak, alias("Default_Handler")));
void USART1_IRQHandler (void)   __attribute__ ((weak, alias("Default_Handler")));
void USART2_IRQHandler (void)   __attribute__ ((weak, alias("Default_Handler")));
void USART3_IRQHandler (void)   __attribute__ ((weak, alias("Default_Handler")));
void EXTI15_10_IRQHandler (void) __attribute__ ((weak, alias("Default_Handler")));
void ERTCAlarm_IRQHandler (void) __attribute__ ((weak, alias("Default_Handler")));
void OTGFS1_WKUP_IRQHandler (void) __attribute__ ((weak, alias("Default_Handler")));
void SDIO_IRQHandler (void)     __attribute__ ((weak, alias("Default_Handler")));
void TMR5_GLOBAL_IRQHandler (void)  __attribute__ ((weak, alias("Default_Handler")));
void DMA2_Channel1_IRQHandler (void) __attribute__ ((weak, alias("Default_Handler")));
void DMA2_Channel2_IRQHandler (void) __attribute__ ((weak, alias("Default_Handler")));
void DMA2_Channel3_IRQHandler (void) __attribute__ ((weak, alias("Default_Handler")));
void DMA2_Channel4_5_IRQHandler (void) __attribute__ ((weak, alias("Default_Handler")));
void OTGFS1_IRQHandler (void)   __attribute__ ((weak, alias("Default_Handler")));
void COMP1_IRQHandler (void)    __attribute__ ((weak, alias("Default_Handler")));
void COMP2_IRQHandler (void)    __attribute__ ((weak, alias("Default_Handler")));
void ACC_IRQHandler (void)      __attribute__ ((weak, alias("Default_Handler")));
void DMA2_Channel6_7_IRQHandler (void)  __attribute__ ((weak, alias("Default_Handler")));


ISR void *vector_table[] = {
    &_estack,
    Reset_Handler,
    NMI_Handler,
    HardFault_Handler,
    MemManage_Handler,
    BusFault_Handler,
    UsageFault_Handler,
    0,
    0,
    0,
    0,
    SVC_Handler,
    DebugMon_Handler,
    0,
    PendSV_Handler,
    SysTick_Handler,
  
    /* External Interrupts */
    WWDG_IRQHandler,                     /* Window Watchdog                             */
    PVD_IRQHandler,                      /* PVD through EXTI Line detect                */
    TAMP_STAMP_IRQHandler,               /* Tamper and TimeStamps through the EXTI line */
    ERTC_WKUP_IRQHandler,                /* ERTC Wakeup through the EXTI line           */
    FLASH_IRQHandler,                    /* Flash                                       */
    RCC_IRQHandler,                      /* RCC                                         */
    EXTI0_IRQHandler,                    /* EXTI Line 0                                 */
    EXTI1_IRQHandler,                    /* EXTI Line 1                                 */
    EXTI2_IRQHandler,                    /* EXTI Line 2                                 */
    EXTI3_IRQHandler,                    /* EXTI Line 3                                 */
    EXTI4_IRQHandler,                    /* EXTI Line 4                                 */
    DMA1_Channel1_IRQHandler,            /* DMA1 Channel 1                              */
    DMA1_Channel2_IRQHandler,            /* DMA1 Channel 2                              */
    DMA1_Channel3_IRQHandler,            /* DMA1 Channel 3                              */
    DMA1_Channel4_IRQHandler,            /* DMA1 Channel 4                              */
    DMA1_Channel5_IRQHandler,            /* DMA1 Channel 5                              */
    DMA1_Channel6_IRQHandler,            /* DMA1 Channel 6                              */
    DMA1_Channel7_IRQHandler,            /* DMA1 Channel 7                              */
    ADC1_2_IRQHandler,                   /* ADC1 & ADC2                                 */
    CAN1_TX_IRQHandler,                  /* CAN1 TX                                     */
    CAN1_RX0_IRQHandler,                 /* CAN1 RX0                                    */
    CAN1_RX1_IRQHandler,                 /* CAN1 RX1                                    */
    CAN1_SCE_IRQHandler,                 /* CAN1 SCE                                    */
    EXTI9_5_IRQHandler,                  /* EXTI Line [9:5]                             */
    TMR1_BRK_TMR9_IRQHandler,            /* TMR1 Break and TMR9                         */
    TMR1_OV_TMR10_IRQHandler,            /* TMR1 Update and TMR10                       */
    TMR1_TRG_COM_TMR11_IRQHandler,       /* TMR1 Trigger and Commutation and TMR11      */
    TMR1_CC_IRQHandler,                  /* TMR1 Capture Compare                        */
    TMR2_GLOBAL_IRQHandler,              /* TMR2                                        */
    TMR3_GLOBAL_IRQHandler,              /* TMR3                                        */
    TMR4_GLOBAL_IRQHandler,              /* TMR4                                        */
    I2C1_EV_IRQHandler,                  /* I2C1 Event                                  */
    I2C1_ER_IRQHandler,                  /* I2C1 Error                                  */
    I2C2_EV_IRQHandler,                  /* I2C2 Event                                  */
    I2C2_ER_IRQHandler,                  /* I2C2 Error                                  */
    SPI1_IRQHandler,                     /* SPI1                                        */
    SPI2_IRQHandler,                     /* SPI2                                        */
    USART1_IRQHandler,                   /* USART1                                      */
    USART2_IRQHandler,                   /* USART2                                      */
    USART3_IRQHandler,                   /* USART3                                      */
    EXTI15_10_IRQHandler,                /* EXTI Line [15:10]                           */
    ERTCAlarm_IRQHandler,                /* ERTC Alarm through EXTI Line                */
    OTGFS1_WKUP_IRQHandler,              /* USB Wakeup from suspend                     */
    0,                                   /* Reserved                                    */
    0,                                   /* Reserved                                    */
    0,                                   /* Reserved                                    */
    0,                                   /* Reserved                                    */
    0,                  			  	 /* Reserved                                    */
    0,                  			  	 /* Reserved                                    */
    SDIO_IRQHandler,                     /* SDIO                                        */
    TMR5_GLOBAL_IRQHandler,              /* TMR5                                        */
    0,                  			  	 /* Reserved                                    */
    0,                                   /* Reserved                                    */
    0,                                   /* Reserved                                    */
    0,                  			  	 /* Reserved                                    */
    0,                  			  	 /* Reserved                                    */
    DMA2_Channel1_IRQHandler,            /* DMA2 Channel1                               */
    DMA2_Channel2_IRQHandler,            /* DMA2 Channel2                               */
    DMA2_Channel3_IRQHandler,            /* DMA2 Channel3                               */
    DMA2_Channel4_5_IRQHandler,          /* DMA2 Channel4 & Channel5                    */
    0,                                   /* Reserved                                    */
    0,                                   /* Reserved                                    */
    0,                                   /* Reserved                                    */
    0,                                   /* Reserved                                    */
    0,                                   /* Reserved                                    */
    0,                                   /* Reserved                                    */
    0,                                   /* Reserved                                    */
    OTGFS1_IRQHandler,                   /* USBOTG                                      */
    0,                                   /* Reserved                                    */
    0,                                   /* Reserved                                    */
    COMP1_IRQHandler,                    /* COMP1                                       */
    COMP2_IRQHandler,                    /* COMP2                                       */
    ACC_IRQHandler,                      /* ACC                                         */
    0,                                   /* Reserved                                    */
    0,                                   /* Reserved                                    */
    DMA2_Channel6_7_IRQHandler,          /* DMA2 Channel6 & Channel7                    */
};
