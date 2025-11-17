/*
 * Moonlite-compatible focuser firmware running on Zephyr.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <Moonlite.hpp>

#include <app_version.h>

#include <errno.h>

#include "Configuration.hpp"
#include "Focuser.hpp"
#include "UartHandler.hpp"
#include "UartThread.hpp"

LOG_MODULE_REGISTER(focuser, CONFIG_APP_LOG_LEVEL);

namespace
{

	const struct device *const g_console = config::console_device();
	const struct device *const g_uart_handler_dev = config::uart_handler_device();
	const struct device *const g_stepper = config::stepper_device();
	const struct device *const g_stepper_drv = config::stepper_driver_device();

	Focuser g_focuser(g_stepper, g_stepper_drv);
	moonlite::Parser g_parser(g_focuser);
	UartHandler g_uart_handler(g_uart_handler_dev);
	UartThread g_uart_thread(g_parser, g_uart_handler);

	K_THREAD_STACK_DEFINE(focuser_stack, config::kFocuserThread.stack_size);
	struct k_thread focuser_thread_data;

	void focuser_thread(void *, void *, void *)
	{
		g_focuser.loop();
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

	if (!device_is_ready(g_uart_handler_dev))
	{
		LOG_ERR("UART handler device not ready");
		return -ENODEV;
	}

	int ret = g_uart_handler.init();
	if (ret != 0)
	{
		return ret;
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

	ret = g_focuser.initialise();
	if (ret != 0)
	{
		return ret;
	}

	k_thread_create(&focuser_thread_data, focuser_stack, K_THREAD_STACK_SIZEOF(focuser_stack),
				&focuser_thread, nullptr, nullptr, nullptr, config::kFocuserThread.priority,
					0, K_NO_WAIT);
	k_thread_name_set(&focuser_thread_data, "focuser");

	g_uart_thread.start(config::kSerialThread.priority);

	LOG_INF("Moonlite focuser ready: UART 9600 8N1");

	return 0;
}
