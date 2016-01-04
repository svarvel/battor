#include "common.h"

#include "error.h"
#include "blink.h"
#include "control.h"
#include "interrupt.h"
#include "samples.h"
#include "drivers.h"
#include "params.h"

int main() //{{{
{
	clock_set_crystal();
	interrupt_init();
	drivers_init();
	params_init();

	// setup an LED to blink while running, start with yellow to indicate not ready yet 
	blink_init(1000, LED_YELLOW_bm); 

	g_control_mode = 0;

	// main loop for interrupt bottom halves 
	while (1) 
	{
#ifdef GPIO_MAIN_LOOP
			gpio_toggle(&PORTE, (1<<GPIO_MAIN_LOOP));
#endif

		// turn off the CPU between interrupts 
		set_sleep_mode(SLEEP_SMODE_IDLE_gc); 
		sleep_enable();
		sleep_cpu();
		sleep_disable();

		// general ms timer
		if (interrupt_is_set(INTERRUPT_TIMER_MS))
		{
			blink_ms_timer_update();
			interrupt_clear(INTERRUPT_TIMER_MS);
		}

		// UART receive
		if (interrupt_is_set(INTERRUPT_UART_RX))
		{
			control_got_uart_bytes();
			interrupt_clear(INTERRUPT_UART_RX);
		}

		// ADCB DMA (channel 2)
		if (interrupt_is_set(INTERRUPT_DMA_CH2))
		{
			// put samples on FIFO
			samples_ringbuf_write(g_adcb0, SAMPLES_LEN);
			interrupt_clear(INTERRUPT_DMA_CH2);
		}

		// other ADCB DMA (channel 3, double buffered)
		if (interrupt_is_set(INTERRUPT_DMA_CH3))
		{
			// calibration finished, setup normal measurement operation
			if (!g_samples_calibrated)
				samples_end_calibration();

			// put samples on FIFO
			samples_ringbuf_write(g_adcb1, SAMPLES_LEN);
			interrupt_clear(INTERRUPT_DMA_CH3);
		}

		if (g_control_mode == CONTROL_MODE_STREAM)
		{
			samples_uart_write();

#ifdef GPIO_SAMPLE_WRITE_DONE
			gpio_toggle(&PORTE, (1<<GPIO_SAMPLE_WRITE_DONE));
#endif
		}

		if (g_control_mode == CONTROL_MODE_STORE)
		{
			samples_store_write();
		}

		//if (g_control_mode == CONTROL_MODE_READ_FILE && g_control_read_ready)
		//{
		//	if (samples_store_read_next(g_adcb0) > 0)
		//		samples_uart_write(g_adcb0, SAMPLES_LEN);
		//	else
		//		samples_uart_write(NULL, 0);
		//	g_control_read_ready = 0;
		//}
	}

	return 0;
} //}}}
