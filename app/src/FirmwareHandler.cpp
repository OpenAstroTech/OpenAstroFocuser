#include "FirmwareHandler.hpp"

#include <zephyr/device.h>
#include <zephyr/drivers/stepper.h>
#include <zephyr/logging/log.h>

#include <errno.h>

LOG_MODULE_DECLARE(focuser, CONFIG_APP_LOG_LEVEL);

namespace
{

	constexpr const char kFirmwareVersion[] = "10";

	uint32_t compute_step_period_us(uint8_t multiplier)
	{
		uint32_t m = (multiplier == 0U) ? 1U : static_cast<uint32_t>(multiplier);
		uint32_t steps_per_second = 2000U / m;
		if (steps_per_second < 100U)
		{
			steps_per_second = 100U;
		}
		if (steps_per_second == 0U)
		{
			steps_per_second = 100U;
		}

		return Z_HZ_us / steps_per_second;
	}

} // namespace

FirmwareHandler::FirmwareHandler(const struct device *stepper_dev,
								 const struct device *stepper_drv_dev)
	: m_stepper(stepper_dev), m_stepper_drv(stepper_drv_dev)
{
	(void)set_stepper_driver_enabled(true);
}

int FirmwareHandler::initialise()
{
	initialise_state();
	if (m_stepper == nullptr)
	{
		LOG_ERR("Stepper controller device is null");
		return -ENODEV;
	}

	int ret = set_stepper_driver_enabled(true);
	if (ret != 0)
	{
		return ret;
	}

	ret = stepper_set_reference_position(m_stepper, 0);
	if (ret != 0)
	{
		LOG_ERR("Failed to set stepper reference position (%d)", ret);
		return ret;
	}

	uint64_t interval_ns = 0;
	{
		MutexLock lock(m_state.lock);
		interval_ns = m_state.step_interval_ns;
	}
	return apply_step_interval(interval_ns);
}

void FirmwareHandler::update_timing_locked()
{
	const uint32_t period_us = compute_step_period_us(m_state.speed_multiplier);
	m_state.step_interval_ns = static_cast<uint64_t>(period_us) * 1000ULL;
	LOG_DBG("Computed step timing: period=%u us", period_us);
}

void FirmwareHandler::initialise_state()
{
	k_mutex_init(&m_state.lock);
	k_sem_init(&m_state.move_sem, 0, K_SEM_MAX_LIMIT);
	m_state.move_request = false;
	m_state.cancel_move = false;
	m_state.staged_position = 0U;
	m_state.desired_position = 0U;
	m_state.speed_multiplier = 1U;
	m_state.half_step = false;
	m_state.temperature_coeff_times2 = 0;
	update_timing_locked();
}

void FirmwareHandler::motion_loop()
{
	while (true)
	{
		k_sem_take(&m_state.move_sem, K_FOREVER);

		while (true)
		{
			bool should_cancel = false;
			bool have_move = false;
			uint16_t target = 0U;

			{
				MutexLock lock(m_state.lock);
				if (m_state.cancel_move)
				{
					should_cancel = true;
					m_state.cancel_move = false;
				}
				else if (m_state.move_request)
				{
					target = m_state.desired_position;
					m_state.move_request = false;
					have_move = true;
					LOG_DBG("Starting motion toward 0x%04x (%u)", target, target);
				}
			}

			if (should_cancel)
			{
				(void)stepper_stop(m_stepper);
				const uint16_t actual16 = static_cast<uint16_t>(read_actual_position() & 0xFFFF);
				MutexLock lock(m_state.lock);
				m_state.desired_position = actual16;
				break;
			}

			if (!have_move)
			{
				break;
			}

			execute_move(target);
		}
	}
}

void FirmwareHandler::stop()
{
	const uint16_t actual16 = static_cast<uint16_t>(read_actual_position() & 0xFFFF);
	{
		MutexLock lock(m_state.lock);
		m_state.cancel_move = true;
		m_state.move_request = false;
		m_state.desired_position = actual16;
	}
	(void)stepper_stop(m_stepper);
	(void)set_stepper_driver_enabled(false);
	k_sem_give(&m_state.move_sem);
	LOG_INF("stop()");
}

uint16_t FirmwareHandler::getCurrentPosition()
{
	const int32_t actual = read_actual_position();
	const uint16_t pos = static_cast<uint16_t>(actual & 0xFFFF);
	{
		MutexLock lock(m_state.lock);
		m_state.desired_position = pos;
	}
	LOG_DBG("getCurrentPosition -> 0x%04x (%u)", pos, pos);
	return pos;
}

void FirmwareHandler::setCurrentPosition(uint16_t position)
{
	int ret = stepper_set_reference_position(m_stepper, static_cast<int32_t>(position));
	if (ret != 0)
	{
		LOG_ERR("Failed to set reference position (%d)", ret);
	}

	MutexLock lock(m_state.lock);
	m_state.staged_position = position;
	m_state.desired_position = position;
	m_state.move_request = false;
	m_state.cancel_move = false;
	LOG_INF("setCurrentPosition 0x%04x (%u)", position, position);
}

uint16_t FirmwareHandler::getNewPosition()
{
	MutexLock lock(m_state.lock);
	LOG_DBG("getNewPosition -> 0x%04x (%u)", m_state.staged_position, m_state.staged_position);
	return m_state.staged_position;
}

void FirmwareHandler::setNewPosition(uint16_t position)
{
	MutexLock lock(m_state.lock);
	LOG_INF("setNewPosition 0x%04x (%u) (was 0x%04x)", position, position, m_state.staged_position);
	m_state.staged_position = position;
}

void FirmwareHandler::goToNewPosition()
{
	uint16_t target;
	{
		MutexLock lock(m_state.lock);
		m_state.desired_position = m_state.staged_position;
		m_state.move_request = true;
		m_state.cancel_move = false;
		target = m_state.staged_position;
	}
	k_sem_give(&m_state.move_sem);
	LOG_INF("goToNewPosition target=0x%04x (%u)", target, target);
}

bool FirmwareHandler::isHalfStep()
{
	MutexLock lock(m_state.lock);
	LOG_DBG("isHalfStep -> %s", m_state.half_step ? "true" : "false");
	return m_state.half_step;
}

void FirmwareHandler::setHalfStep(bool enabled)
{
	MutexLock lock(m_state.lock);
	LOG_INF("setHalfStep %s (was %s)", enabled ? "true" : "false",
			m_state.half_step ? "true" : "false");
	m_state.half_step = enabled;
}

bool FirmwareHandler::isMoving()
{
	bool moving = false;
	int ret = stepper_is_moving(m_stepper, &moving);
	if (ret != 0)
	{
		LOG_WRN("stepper_is_moving failed (%d)", ret);
		return false;
	}
	LOG_DBG("isMoving -> %s", moving ? "true" : "false");
	return moving;
}

std::string FirmwareHandler::getFirmwareVersion()
{
	LOG_DBG("getFirmwareVersion -> %s", kFirmwareVersion);
	return std::string(kFirmwareVersion);
}

uint8_t FirmwareHandler::getSpeed()
{
	MutexLock lock(m_state.lock);
	LOG_DBG("getSpeed -> 0x%02x (%u)", m_state.speed_multiplier, m_state.speed_multiplier);
	return m_state.speed_multiplier;
}

void FirmwareHandler::setSpeed(uint8_t speed)
{
	if (speed == 0U)
	{
		speed = 1U;
	}
	uint64_t interval_ns = 0;
	{
		MutexLock lock(m_state.lock);
		LOG_INF("setSpeed 0x%02x (%u) (was 0x%02x)", speed, speed, m_state.speed_multiplier);
		m_state.speed_multiplier = speed;
		update_timing_locked();
		interval_ns = m_state.step_interval_ns;
	}
	(void)apply_step_interval(interval_ns);
}

uint16_t FirmwareHandler::getTemperature()
{
	LOG_DBG("getTemperature -> 0x0000 (0)");
	return 0x0000;
}

uint8_t FirmwareHandler::getTemperatureCoefficientRaw()
{
	MutexLock lock(m_state.lock);
	LOG_DBG("getTemperatureCoefficientRaw -> 0x%02x (%d -> %.1f)",
			static_cast<uint8_t>(m_state.temperature_coeff_times2),
			static_cast<int>(m_state.temperature_coeff_times2),
			static_cast<double>(m_state.temperature_coeff_times2) / 2.0);
	return static_cast<uint8_t>(m_state.temperature_coeff_times2);
}

void FirmwareHandler::execute_move(uint16_t target)
{
	uint64_t interval_ns = 0;
	{
		MutexLock lock(m_state.lock);
		interval_ns = m_state.step_interval_ns;
	}

	if (set_stepper_driver_enabled(true) != 0)
	{
		return;
	}

	if (apply_step_interval(interval_ns) != 0)
	{
		return;
	}

	int ret = stepper_move_to(m_stepper, static_cast<int32_t>(target));
	if (ret != 0)
	{
		LOG_ERR("Failed to start move to 0x%04x (%d)", target, ret);
		return;
	}

	while (true)
	{
		bool moving = false;
		ret = stepper_is_moving(m_stepper, &moving);
		if (ret != 0)
		{
			LOG_ERR("stepper_is_moving failed (%d)", ret);
			break;
		}
		if (!moving)
		{
			break;
		}

		bool should_cancel = false;
		{
			MutexLock lock(m_state.lock);
			should_cancel = m_state.cancel_move;
			if (should_cancel)
			{
				m_state.cancel_move = false;
			}
		}

		if (should_cancel)
		{
			LOG_DBG("Stopping active motion per cancel request");
			(void)stepper_stop(m_stepper);
			break;
		}

		k_msleep(5);
	}

	const int32_t actual = read_actual_position();
	bool pending_move = false;
	const uint16_t actual16 = static_cast<uint16_t>(actual & 0xFFFF);
	{
		MutexLock lock(m_state.lock);
		m_state.desired_position = actual16;
		pending_move = m_state.move_request;
	}
	if (!pending_move)
	{
		(void)set_stepper_driver_enabled(false);
	}
	LOG_DBG("Motion complete -> 0x%04x (%d)", static_cast<uint16_t>(actual & 0xFFFF), actual);
}

int FirmwareHandler::apply_step_interval(uint64_t interval_ns)
{
	if (interval_ns == 0U)
	{
		return -EINVAL;
	}

	const int ret = stepper_set_microstep_interval(m_stepper, interval_ns);
	if (ret != 0)
	{
		LOG_ERR("Failed to set step interval (%d)", ret);
	}
	return ret;
}

int32_t FirmwareHandler::read_actual_position()
{
	int32_t actual = 0;
	int ret = stepper_get_actual_position(m_stepper, &actual);
	if (ret != 0)
	{
		LOG_WRN("Failed to query actual position (%d)", ret);
		uint16_t fallback = 0U;
		{
			MutexLock lock(m_state.lock);
			fallback = m_state.desired_position;
		}
		return static_cast<int32_t>(fallback);
	}
	return actual;
}

int FirmwareHandler::set_stepper_driver_enabled(bool enable)
{
	if (m_stepper_drv == nullptr)
	{
		return 0;
	}

	const int ret = enable ? stepper_drv_enable(m_stepper_drv) : stepper_drv_disable(m_stepper_drv);
	if ((ret != 0) && (ret != -EALREADY))
	{
		if (enable)
		{
			LOG_ERR("Failed to enable stepper driver (%d)", ret);
		}
		else
		{
			LOG_WRN("Failed to disable stepper driver (%d)", ret);
		}
		return ret;
	}

	return 0;
}
