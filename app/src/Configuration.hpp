#pragma once

#include <zephyr/device.h>
#include <zephyr/kernel.h>

#include <cstddef>

#define FOCUSER_NODE DT_PATH(zephyr_user)
#define STEPPER_ALIAS DT_ALIAS(stepper)
#define STEPPER_DRV_ALIAS DT_ALIAS(stepper_drv)

#if !DT_NODE_EXISTS(FOCUSER_NODE)
#error "zephyr,user node must provide focuser pin assignments"
#endif

#if !DT_NODE_HAS_PROP(FOCUSER_NODE, uart_handler)
#error "zephyr,user node requires uart_handler phandle"
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

namespace config
{

struct ThreadConfig
{
	std::size_t stack_size;
	int priority;
};

inline constexpr ThreadConfig kFocuserThread{CONFIG_FOCUSER_THREAD_STACK_SIZE, K_PRIO_PREEMPT(4)};
inline constexpr ThreadConfig kSerialThread{CONFIG_SERIAL_THREAD_STACK_SIZE, K_PRIO_PREEMPT(5)};

inline const struct device *console_device()
{
	return DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
}

inline const struct device *uart_handler_device()
{
	return DEVICE_DT_GET(DT_PHANDLE(FOCUSER_NODE, uart_handler));
}

inline const struct device *stepper_device()
{
	return DEVICE_DT_GET(STEPPER_ALIAS);
}

inline const struct device *stepper_driver_device()
{
	return DEVICE_DT_GET(STEPPER_DRV_ALIAS);
}

} // namespace config