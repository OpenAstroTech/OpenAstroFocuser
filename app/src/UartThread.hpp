#pragma once

#include <zephyr/kernel.h>

#include <Moonlite.hpp>

#include "Configuration.hpp"

class UartHandler;

class UartThread {
public:
	UartThread(moonlite::Parser &parser, UartHandler &uart_handler);

	void start(int priority);

private:
	static void thread_entry(void *, void *, void *);
	void run();

	static constexpr std::size_t kMaxLoggedFrameLen = 80U;

	moonlite::Parser &m_parser;
	UartHandler &m_uart_handler;
	struct k_thread m_thread;
};
