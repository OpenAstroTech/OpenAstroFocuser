/*
 * Moonlite-compatible focuser firmware running on Zephyr.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <Moonlite.hpp>

#include <app_version.h>

#include <errno.h>
#include <cstddef>
#include <cstdint>
#include <string>

#include "FirmwareHandler.hpp"

LOG_MODULE_REGISTER(focuser, CONFIG_APP_LOG_LEVEL);

#define MOTION_THREAD_STACK_SIZE 2048
#define MOTION_THREAD_PRIORITY K_PRIO_PREEMPT(4)
#define SERIAL_THREAD_STACK_SIZE 2048
#define SERIAL_THREAD_PRIORITY K_PRIO_PREEMPT(5)

#define FOCUSER_NODE DT_PATH(zephyr_user)
#define STEPPER_ALIAS DT_ALIAS(stepper)
#define STEPPER_DRV_ALIAS DT_ALIAS(stepper_drv)

#if !DT_NODE_EXISTS(FOCUSER_NODE)
#error "zephyr,user node must provide focuser pin assignments"
#endif

#if !DT_NODE_HAS_PROP(FOCUSER_NODE, moonlite_uart)
#error "zephyr,user node requires moonlite_uart phandle"
#endif

#if !DT_NODE_HAS_STATUS(STEPPER_ALIAS, okay)
#error "stepper alias must reference an enabled stepper controller"
#endif

#if !DT_NODE_HAS_STATUS(STEPPER_DRV_ALIAS, okay)
#error "stepper-drv alias must reference an enabled stepper driver"
#endif

#if !DT_HAS_CHOSEN(zephyr_console)
#error "Console device is required for Moonlite serial protocol"
#endif

namespace
{

	const struct device *const g_console = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	const struct device *const g_proto_uart = DEVICE_DT_GET(DT_PHANDLE(FOCUSER_NODE, moonlite_uart));
	const struct device *const g_stepper = DEVICE_DT_GET(STEPPER_ALIAS);
	const struct device *const g_stepper_drv = DEVICE_DT_GET(STEPPER_DRV_ALIAS);

	FirmwareHandler g_handler(g_stepper, g_stepper_drv);
	moonlite::Parser g_parser(g_handler);

	K_THREAD_STACK_DEFINE(motion_stack, MOTION_THREAD_STACK_SIZE);
	K_THREAD_STACK_DEFINE(serial_stack, SERIAL_THREAD_STACK_SIZE);
	struct k_thread motion_thread_data;
	struct k_thread serial_thread_data;

	void motion_thread(void *, void *, void *)
	{
		g_handler.motion_loop();
	}
	constexpr size_t kMaxLoggedFrameLen = 80U;

	void serial_thread(void *, void *, void *)
	{
		std::string response;
		std::string frame_log;
		bool frame_overflow = false;

		while (true)
		{
			unsigned char byte;
			const int rc = uart_poll_in(g_proto_uart, &byte);
			if (rc == 0)
			{
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

				if (g_parser.feed(c, response))
				{
					if (!frame_log.empty())
					{
						if (frame_overflow)
						{
							LOG_INF("Moonlite RX %s... (truncated)", frame_log.c_str());
						}
						else
						{
							LOG_INF("Moonlite RX %s", frame_log.c_str());
						}
					}
					else
					{
						LOG_INF("Moonlite RX <unframed>");
					}

					if (!response.empty())
					{
						LOG_INF("Moonlite TX %s", response.c_str());
						for (char ch : response)
						{
							uart_poll_out(g_proto_uart, ch);
						}
					}
					else
					{
						LOG_DBG("Moonlite command produced no response");
					}

					frame_log.clear();
					frame_overflow = false;
					response.clear();
				}
			}
			else
			{
				k_sleep(K_MSEC(1));
			}
		}
	}

} // namespace

int main(void)
{
	LOG_INF("Moonlite focuser firmware %s", APP_VERSION_STRING);

	if (!device_is_ready(g_console))
	{
		LOG_ERR("Console device not ready");
		return -ENODEV;
	}

	if (!device_is_ready(g_proto_uart))
	{
		LOG_ERR("Moonlite UART device not ready");
		return -ENODEV;
	}

	if (!device_is_ready(g_stepper))
	{
		LOG_ERR("Stepper controller device not ready");
		return -ENODEV;
	}

	if (!device_is_ready(g_stepper_drv))
	{
		LOG_ERR("Stepper driver device not ready");
		return -ENODEV;
	}

	int ret = g_handler.initialise();
	if (ret != 0)
	{
		return ret;
	}

	k_thread_create(&motion_thread_data, motion_stack, K_THREAD_STACK_SIZEOF(motion_stack),
					&motion_thread, nullptr, nullptr, nullptr, MOTION_THREAD_PRIORITY,
					0, K_NO_WAIT);
	k_thread_name_set(&motion_thread_data, "motion");

	k_thread_create(&serial_thread_data, serial_stack, K_THREAD_STACK_SIZEOF(serial_stack),
					&serial_thread, nullptr, nullptr, nullptr, SERIAL_THREAD_PRIORITY,
					0, K_NO_WAIT);
	k_thread_name_set(&serial_thread_data, "serial");

	LOG_INF("Moonlite focuser ready: UART 9600 8N1");

	return 0;
}
