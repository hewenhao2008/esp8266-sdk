#include "uart.h"

//#define DBG printf
#define DBG (void)
// if you get lots of rx_overruns, increase this (or read the data quicker!)
#define UART_RX_QUEUE_SIZE 32

xQueueHandle  uart_rx_queue;
volatile uint16_t uart_rx_overruns;
volatile uint16_t uart_rx_bytes;

/*
 * UART rx Interrupt routine
 */
static void 
uart_isr(void)
{
	uint8_t temp;
	signed portBASE_TYPE ret;
	portBASE_TYPE xHigherPriorityTaskWoken = pdFALSE;
	if (UART_RXFIFO_FULL_INT_ST != (READ_PERI_REG(UART_INT_ST(UART0)) & UART_RXFIFO_FULL_INT_ST))
	{
		return;
	}
	WRITE_PERI_REG(UART_INT_CLR(UART0), UART_RXFIFO_FULL_INT_CLR);

    while (READ_PERI_REG(UART_STATUS(UART0)) & (UART_RXFIFO_CNT << UART_RXFIFO_CNT_S)) {
		temp = READ_PERI_REG(UART_FIFO(UART0)) & 0xFF;
		ret = xQueueSendToBackFromISR
                    (
                        uart_rx_queue,
						&temp,
                        &xHigherPriorityTaskWoken
                    );
		if (ret != pdTRUE)
		{
			uart_rx_overruns++;
		} 
		else
		{
			uart_rx_bytes++;
		}
	}
	portEND_SWITCHING_ISR( xHigherPriorityTaskWoken );
}

/*
 * Get a char from the RX buffer. Wait up to timeout ms for data
 * -ve timeout value means wait forever
 * return the char, or -1 on error
 */
int ICACHE_FLASH_ATTR 
uart_getchar_ms(int timeout)
{
	portBASE_TYPE ticks;
	unsigned char ch;
	if (timeout < 0)
	{
		ticks = portMAX_DELAY;
	}
	else
	{
		ticks = timeout / portTICK_RATE_MS; 
	}
	DBG("ticks=%d\r\n", ticks);
	if ( xQueueReceive(uart_rx_queue, &ch, ticks) != pdTRUE)
	{
		DBG("no data\r\n");
		return -1;
	}
	DBG("returning %d\r\n", ch);
	return (int)ch;
}


char * ICACHE_FLASH_ATTR
uart_gets(char *buf, int len)
{
	int i=1; // need to save one char for null
	char *p=buf;
	int ch;
	while (i<len)
	{
		ch = uart_getchar();
		if (ch == -1) return NULL;
		if (ch == 0x03) return NULL;
		if (ch == '\r' || ch == '\n') 
		{
			*p='\0';
			return buf;	
		}
		*p=(char)ch;
		os_putc(*p);
		p++;
		i++;
	}
	*p='\0';
	return buf;
}

/* 
 * Return the number of characters available to read
 */
int ICACHE_FLASH_ATTR
uart_rx_available(void)
{
	return uxQueueMessagesWaiting(uart_rx_queue);
}

void ICACHE_FLASH_ATTR 
uart_set_baud(int uart, int baud)
{
	uart_div_modify(uart, UART_CLK_FREQ / (baud));
}

/*
 * Initialise the uart receiver
 */
void ICACHE_FLASH_ATTR
uart_rx_init(void)
{ 
	uart_rx_queue = xQueueCreate( 
		UART_RX_QUEUE_SIZE,
		sizeof(char)
	);
	uart_rx_overruns=0;
	uart_rx_bytes=0;
	/* _xt_isr_mask seems to cause Exception 20 ? */
	//_xt_isr_mask(1<<ETS_UART_INUM);
	_xt_isr_attach(ETS_UART_INUM, uart_isr);
	_xt_isr_unmask(1<<ETS_UART_INUM);
}



// vim: ts=4 sw=4 noexpandtab