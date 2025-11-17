/*
 * Moonlite-compatible focuser firmware running on Zephyr.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <app_version.h>

#include <errno.h>

#include "Configuration.hpp"
#include "Focuser.hpp"
#include "FocuserThread.hpp"
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
	FocuserThread g_focuser_thread(g_focuser);
	UartHandler g_uart_handler(g_uart_handler_dev);
	UartThread g_uart_thread(g_focuser, g_uart_handler);

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

	g_focuser_thread.start();

	g_uart_thread.start(config::kSerialThread.priority);

	LOG_INF("Moonlite focuser ready: UART 9600 8N1");

	return 0;
}
