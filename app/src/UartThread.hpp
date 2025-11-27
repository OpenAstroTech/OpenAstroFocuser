#pragma once

#include <Moonlite.hpp>

#include "Thread.hpp"

class Focuser;
class UartHandler;

class UartThread : public Thread {
public:
	UartThread(Focuser &focuser, UartHandler &uart_handler);

	void start();

private:
	void run() override;

	static constexpr std::size_t kMaxLoggedFrameLen = 80U;

	moonlite::Parser m_parser;
	UartHandler &m_uart_handler;
};
