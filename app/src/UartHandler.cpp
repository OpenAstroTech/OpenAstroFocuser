#include "UartHandler.hpp"

#include <zephyr/logging/log.h>

#include <cerrno>
#include <cstring>

LOG_MODULE_REGISTER(uart_handler, CONFIG_APP_LOG_LEVEL);

UartHandler::UartHandler(const struct device *uart)
	: m_uart(uart), m_initialized(false)
{
	std::memset(&m_rx_queue, 0, sizeof(m_rx_queue));
}

int UartHandler::init()
{
	if (m_initialized)
	{
		return 0;
	}

	k_msgq_init(&m_rx_queue, reinterpret_cast<char *>(m_rx_queue_storage), sizeof(std::uint8_t), kRxQueueDepth);

	if (!device_is_ready(m_uart))
	{
		LOG_ERR("UART handler device not ready");
		return -ENODEV;
	}

	const int cb_rc = uart_irq_callback_user_data_set(m_uart, UartHandler::uart_isr, this);
	if (cb_rc != 0)
	{
		LOG_ERR("Failed to set UART callback (%d)", cb_rc);
		return cb_rc;
	}

	uart_irq_rx_enable(m_uart);
	m_initialized = true;
	return 0;
}

bool UartHandler::read_byte(std::uint8_t &byte, k_timeout_t timeout)
{
	if (!m_initialized)
	{
		return false;
	}

	const int rc = k_msgq_get(&m_rx_queue, &byte, timeout);
	return rc == 0;
}

void UartHandler::write(const std::string &data)
{
	for (char ch : data)
	{
		write_char(ch);
	}
}

void UartHandler::write_char(char ch)
{
	if (!m_initialized)
	{
		return;
	}

	uart_poll_out(m_uart, static_cast<unsigned char>(ch));
}

void UartHandler::push_rx_bytes(const std::uint8_t *data, int length)
{
	for (int i = 0; i < length; ++i)
	{
		const std::uint8_t byte = data[i];
		const int rc = k_msgq_put(&m_rx_queue, &byte, K_NO_WAIT);
		if (rc != 0)
		{
			LOG_WRN("UART handler RX queue full, dropping byte");
			break;
		}
	}
}

void UartHandler::uart_isr(const struct device *dev, void *user_data)
{
	auto *self = static_cast<UartHandler *>(user_data);
	if (self == nullptr)
	{
		return;
	}

	while (uart_irq_update(dev) && uart_irq_is_pending(dev))
	{
		if (uart_irq_rx_ready(dev))
		{
			std::uint8_t buffer[kRxBurstSize];
			const int bytes_read = uart_fifo_read(dev, buffer, sizeof(buffer));
			if (bytes_read > 0)
			{
				self->push_rx_bytes(buffer, bytes_read);
			}
		}
	}
}
