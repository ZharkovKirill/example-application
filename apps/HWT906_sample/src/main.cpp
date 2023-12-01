/*
 * Copyright (c) 2022 Libre Solar Technologies GmbH
 *
 * SPDX-License-Identifier: Apache-2.0
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <witmotion_protocol/witmotion_protocol.hpp>
#include <string.h>


/* change this to any other UART peripheral if desired */
#define UART_DEVICE_NODE DT_CHOSEN(zephyr_shell_uart)

#define MSG_SIZE 128

/* queue to store up to 10 messages (aligned to 4-byte boundary) */
K_MSGQ_DEFINE(uart_msgq, MSG_SIZE, 10, 4);

static const struct device *const uart_dev = DEVICE_DT_GET(UART_DEVICE_NODE);

/* receive buffer used in UART ISR callback */
static char rx_buf[MSG_SIZE];


static int rx_buf_pos;
static uint8_t hwt_data_buf[9];
uint8_t hwt_state = witmotion_protocol::state::IDLE;
static auto hwt_current_payload = witmotion_protocol::header::ANGLE;
static auto hwt_angle = witmotion_protocol::wp_angle_message();
/*
 * Read characters from UART until line end is detected. Afterwards push the
 * data to the message queue.
 */
void serial_cb(const struct device *dev, void *user_data)
{
	uint8_t c;

	if (!uart_irq_update(uart_dev)) {
		return;
	}

	if (!uart_irq_rx_ready(uart_dev)) {
		return;
	}

	/* read until FIFO empty */
	while (uart_fifo_read(uart_dev, &c, 1) == 1) {
		if ((c == '\n' || c == '\r') && rx_buf_pos > 0) {
			/* terminate string */
			rx_buf[rx_buf_pos] = '\0';

			/* if queue is full, message is silently dropped */
			k_msgq_put(&uart_msgq, &rx_buf, K_NO_WAIT);

			/* reset the buffer (it was copied to the msgq) */
			rx_buf_pos = 0;
		} else if (rx_buf_pos < (sizeof(rx_buf) - 1)) {
			rx_buf[rx_buf_pos++] = c;
		}
		/* else: characters beyond buffer size are dropped */
	}
}

void serial_cb_hwt(const struct device *dev, void *user_data)
{
	uint8_t c;

	if (!uart_irq_update(uart_dev)) {
		return;
	}

	if (!uart_irq_rx_ready(uart_dev)) {
		return;
	}
	
	/* read until FIFO empty */
	while (uart_fifo_read(uart_dev, &c, 1) == 1) {
		switch (hwt_state)
		{
			case witmotion_protocol::state::IDLE :
				if (c == witmotion_protocol::header::MESSAGE_HEADER)
				{
					hwt_state = witmotion_protocol::state::HEADER;
				}
				else
				{
					hwt_state = witmotion_protocol::state::IDLE;
				};
				break;
			
			case witmotion_protocol::state::HEADER :
				rx_buf_pos = 0;
				// TODO: add all types of header
				if (c == witmotion_protocol::header::ANGLE)
				{
					hwt_state = witmotion_protocol::state::PAYLOAD;
					hwt_current_payload = witmotion_protocol::header(c);				
				}
				else
				{
					hwt_state = witmotion_protocol::state::IDLE;
				}
				break;
			
			case witmotion_protocol::state::PAYLOAD :
				if ((hwt_current_payload == witmotion_protocol::header::ANGLE) && !(rx_buf_pos >= 9))
				{
					hwt_data_buf[rx_buf_pos++] = c;
				}
				else if (rx_buf_pos >= 9)
				{
					auto data_ptr = hwt_data_buf;
					auto wit_data_ptr = reinterpret_cast<witmotion_protocol::wp_angle_message*>(data_ptr);
					auto data = *wit_data_ptr;
					//double angle =(((hwt_data_buf[1]<<8)|hwt_data_buf[0])*180.0)/32768.0;
					double angle =(((data.RollH<<8)|data.RollL)*180.0)/32768.0;
					if (angle > 10)
					{printk("angle: %f\n", angle);};
					hwt_state = witmotion_protocol::state::IDLE;
				}
				else
				{
					hwt_state = witmotion_protocol::state::IDLE;
				}
			break;
 
		}
    
	
	
	};
}



/*
 * Print a null-terminated string character by character to the UART interface
 */
void print_uart(char *buf)
{
	int msg_len = strlen(buf);

	for (int i = 0; i < msg_len; i++) {
		uart_poll_out(uart_dev, buf[i]);
	}
}
 
int main(void)
{
	char tx_buf[MSG_SIZE];

	if (!device_is_ready(uart_dev)) {
		printk("UART device not found!");
		return 0;
	}

	/* configure interrupt and callback to receive data */
	int ret = uart_irq_callback_user_data_set(uart_dev, serial_cb_hwt, NULL);

	if (ret < 0) {
		if (ret == -ENOTSUP) {
			printk("Interrupt-driven UART API support not enabled\n");
		} else if (ret == -ENOSYS) {
			printk("UART device does not support interrupt-driven API\n");
		} else {
			printk("Error setting UART callback: %d\n", ret);
		}
		return 0;
	}
	uart_irq_rx_enable(uart_dev);
	k_timeout_t wre;
	wre.ticks  =  100;
	while (true)
	{
 		k_sleep(wre);
	};
	printk("Interrupt-driven UART API support not enabled\n");
 
	return 0;
}
