#pragma once

#include <zephyr/device.h>
#include <zephyr/kernel.h>
#include <zephyr/sys/util.h>

#include <cstddef>

namespace config
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

	struct ThreadConfig
	{
		std::size_t stack_size;
		int priority;
	};

	inline constexpr ThreadConfig kFocuserThread{CONFIG_FOCUSER_THREAD_STACK_SIZE, K_PRIO_PREEMPT(4)};
	inline constexpr ThreadConfig kSerialThread{CONFIG_SERIAL_THREAD_STACK_SIZE, K_PRIO_PREEMPT(5)};

} // namespace config