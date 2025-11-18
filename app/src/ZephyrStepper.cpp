#include "ZephyrStepper.hpp"

#include <zephyr/device.h>
#include <zephyr/drivers/stepper.h>

#include <errno.h>

ZephyrFocuserStepper::ZephyrFocuserStepper(const struct device *stepper_dev,
					 const struct device *stepper_drv_dev)
	: m_stepper(stepper_dev), m_stepper_drv(stepper_drv_dev)
{
}

bool ZephyrFocuserStepper::is_ready() const
{
	if ((m_stepper == nullptr) || !device_is_ready(m_stepper))
	{
		return false;
	}

	if ((m_stepper_drv != nullptr) && !device_is_ready(m_stepper_drv))
	{
		return false;
	}

	return true;
}

int ZephyrFocuserStepper::set_reference_position(int32_t position)
{
	if (m_stepper == nullptr)
	{
		return -ENODEV;
	}

	return stepper_set_reference_position(m_stepper, position);
}

int ZephyrFocuserStepper::set_microstep_interval(uint64_t interval_ns)
{
	if (m_stepper == nullptr)
	{
		return -ENODEV;
	}

	return stepper_set_microstep_interval(m_stepper, interval_ns);
}

int ZephyrFocuserStepper::move_to(int32_t target)
{
	if (m_stepper == nullptr)
	{
		return -ENODEV;
	}

	return stepper_move_to(m_stepper, target);
}

int ZephyrFocuserStepper::is_moving(bool &moving)
{
	if (m_stepper == nullptr)
	{
		return -ENODEV;
	}

	return stepper_is_moving(m_stepper, &moving);
}

int ZephyrFocuserStepper::stop()
{
	if (m_stepper == nullptr)
	{
		return -ENODEV;
	}

	return stepper_stop(m_stepper);
}

int ZephyrFocuserStepper::get_actual_position(int32_t &position)
{
	if (m_stepper == nullptr)
	{
		return -ENODEV;
	}

	return stepper_get_actual_position(m_stepper, &position);
}

int ZephyrFocuserStepper::enable_driver(bool enable)
{
	if (m_stepper_drv == nullptr)
	{
		return 0;
	}

	return enable ? stepper_drv_enable(m_stepper_drv) : stepper_drv_disable(m_stepper_drv);
}
