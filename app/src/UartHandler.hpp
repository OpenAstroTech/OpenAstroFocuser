#pragma once

#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/kernel.h>

#include <cstddef>
#include <cstdint>
#include <string>

class UartHandler {
public:
	explicit UartHandler(const struct device *uart);

	int init();
	bool read_byte(std::uint8_t &byte, k_timeout_t timeout);
	void write(const std::string &data);
	void write_char(char ch);

private:
	static void uart_isr(const struct device *dev, void *user_data);
	void push_rx_bytes(const std::uint8_t *data, int length);

	static constexpr std::size_t kRxQueueDepth = 128;
	static constexpr std::size_t kRxBurstSize = 8;

	const struct device *m_uart;
	struct k_msgq m_rx_queue;
	alignas(4) std::uint8_t m_rx_queue_storage[kRxQueueDepth];
	bool m_initialized;
};
