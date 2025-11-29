#pragma once

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include <app_version.h>

#include <cstddef>

namespace config
{
	constexpr const char version[] = STRINGIFY(APP_VERSION_MAJOR) STRINGIFY(APP_VERSION_MINOR);

	namespace devices
	{

#if !DT_HAS_CHOSEN(focuser_uart)
#error "UART device is required for Moonlite serial protocol"
#else
		constexpr auto uart = DEVICE_DT_GET(DT_CHOSEN(focuser_uart));
#endif

#if !DT_HAS_CHOSEN(focuser_stepper)
#error "Stepper device is required for Moonlite serial protocol"
#else
		constexpr auto stepper = DEVICE_DT_GET(DT_CHOSEN(focuser_stepper));
#endif

#if !DT_HAS_CHOSEN(focuser_stepper_drv)
#error "Stepper driver device is required for Moonlite serial protocol"
#else
		constexpr auto stepper_drv = DEVICE_DT_GET(DT_CHOSEN(focuser_stepper_drv));
#endif

	} // namespace devices

	namespace threads
	{
		constexpr auto focuser_priority = K_PRIO_PREEMPT(4);
		constexpr auto focuser_stack_size = K_THREAD_STACK_LEN(2048);
		inline k_thread_stack_t focuser_stack[focuser_stack_size];

		constexpr auto serial_priority = K_PRIO_PREEMPT(5);
		constexpr auto serial_stack_size = K_THREAD_STACK_LEN(2048);
		inline k_thread_stack_t serial_stack[serial_stack_size];
	} // namespace threads
} // namespace config