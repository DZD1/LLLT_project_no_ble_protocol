#include "usart.h"
#include "stdio.h"

int fputc(int ch, FILE *f)
{
	USART_SendData(USART1, (uint8_t) ch);

	while (USART_GetFlagStatus(USART1, USART_FLAG_TXC) == RESET) {}	
   
    return ch;
}
void Usart_Init(void)
{
	USART_InitType USART_InitStructure;
    GPIO_InitType GPIO_InitStructure;
    
    /* System Clocks Configuration */
    /* Enable GPIO clock */
    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_GPIOA|RCC_APB2_PERIPH_GPIOB|RCC_APB2_PERIPH_AFIO, ENABLE);
	
    /* Enable USARTx Clock */
    RCC_EnableAPB2PeriphClk(RCC_APB2_PERIPH_USART1, ENABLE);
 
    /* Initialize GPIO_InitStructure */
    GPIO_InitStruct(&GPIO_InitStructure);    
    //pb6:tx
    /* Configure USARTx Tx as alternate function push-pull */
    GPIO_InitStructure.Pin            = GPIO_PIN_6;
    GPIO_InitStructure.GPIO_Mode      = GPIO_MODE_AF_PP;
    GPIO_InitStructure.GPIO_Alternate = GPIO_AF4_USART1;
    GPIO_InitPeripheral(GPIOB, &GPIO_InitStructure);   
    //pb7:rx
    /* Configure USARTx Rx as alternate function push-pull */
    GPIO_InitStructure.Pin            = GPIO_PIN_15;
    GPIO_InitStructure.GPIO_Alternate = GPIO_AF4_USART1;
    GPIO_InitPeripheral(GPIOA, &GPIO_InitStructure); 
  
    /* USARTy and USARTz configuration ------------------------------------------------------*/
    USART_InitStructure.BaudRate            = 115200;
    USART_InitStructure.WordLength          = USART_WL_8B;
    USART_InitStructure.StopBits            = USART_STPB_1;
    USART_InitStructure.Parity              = USART_PE_NO;
    USART_InitStructure.HardwareFlowControl = USART_HFCTRL_NONE;
    USART_InitStructure.Mode                = USART_MODE_RX | USART_MODE_TX;
 
    /* Configure USARTx */
    USART_Init(USART1, &USART_InitStructure);
    /* Enable the USARTx */
    USART_Enable(USART1, ENABLE);
}
