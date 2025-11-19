#include "UartThread.hpp"

#include <zephyr/logging/log.h>

#include <cstdint>
#include <string>

#include "Focuser.hpp"
#include "UartHandler.hpp"

LOG_MODULE_REGISTER(uart_thread, CONFIG_APP_LOG_LEVEL);

namespace
{
K_THREAD_STACK_DEFINE(serial_stack, config::kSerialThread.stack_size);
}

UartThread::UartThread(Focuser &focuser, UartHandler &uart_handler)
	: m_parser(focuser), m_uart_handler(uart_handler)
{
}

void UartThread::start(int priority)
{
	k_thread_create(&m_thread, serial_stack, K_THREAD_STACK_SIZEOF(serial_stack),
			thread_entry, this, nullptr, nullptr, priority, 0, K_NO_WAIT);
	k_thread_name_set(&m_thread, "serial");
}

void UartThread::thread_entry(void *arg1, void *, void *)
{
	auto *self = static_cast<UartThread *>(arg1);
	if (self == nullptr)
	{
		return;
	}

	self->run();
}

void UartThread::run()
{
	std::string response;
	std::string frame_log;
	bool frame_overflow = false;

	while (true)
	{
		std::uint8_t byte;
		if (!m_uart_handler.read_byte(byte, K_FOREVER))
		{
			continue;
		}

		const char c = static_cast<char>(byte);

		if (c == ':')
		{
			frame_log.clear();
			frame_overflow = false;
			frame_log.push_back(c);
		}
		else if (!frame_log.empty())
		{
			if (frame_log.size() < kMaxLoggedFrameLen)
			{
				frame_log.push_back(c);
			}
			else
			{
				frame_overflow = true;
			}
		}

		if (m_parser.feed(c, response))
		{
			if (!frame_log.empty())
			{
				if (frame_overflow)
				{
					LOG_INF("RX %s... (truncated)", frame_log.c_str());
				}
				else
				{
					LOG_INF("RX %s", frame_log.c_str());
				}
			}
			else
			{
				LOG_INF("RX <unframed>");
			}

			if (!response.empty())
			{
				LOG_INF("TX %s", response.c_str());
				m_uart_handler.write(response);
			}
			else
			{
				LOG_DBG("command produced no response");
			}

			frame_log.clear();
			frame_overflow = false;
			response.clear();
		}
	}
}
