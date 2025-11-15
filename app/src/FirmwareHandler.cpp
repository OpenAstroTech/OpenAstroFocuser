#include "FirmwareHandler.hpp"

#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
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
	return 1000000U / steps_per_second;
}

uint32_t compute_pulse_high_us(uint32_t period_us)
{
	if (period_us == 0U)
	{
		return 10U;
	}
	uint32_t high = period_us / 2U;
	if (high < 10U)
	{
		high = 10U;
	}
	if (high >= period_us)
	{
		high = (period_us > 1U) ? period_us - 1U : 1U;
	}
	return high;
}

} // namespace

FirmwareHandler::FirmwareHandler(const gpio_dt_spec &enable_spec, const gpio_dt_spec &step_spec,
			       const gpio_dt_spec &dir_spec)
	: m_enable_spec(enable_spec), m_step_spec(step_spec), m_dir_spec(dir_spec)
{
}

int FirmwareHandler::initialise()
{
	initialise_state();
	return configure_stepper_pins();
}

void FirmwareHandler::update_timing_locked()
{
	m_state.step_period_us = compute_step_period_us(m_state.speed_multiplier);
	m_state.pulse_high_us = compute_pulse_high_us(m_state.step_period_us);
	LOG_DBG("Computed step timing: period=%u us, pulse=%u us", m_state.step_period_us,
		m_state.pulse_high_us);
}

int FirmwareHandler::configure_stepper_pins()
{
	if (!device_is_ready(m_enable_spec.port) || !device_is_ready(m_step_spec.port) ||
		!device_is_ready(m_dir_spec.port))
	{
		LOG_ERR("Stepper GPIO controller not ready");
		return -ENODEV;
	}

	int ret = gpio_pin_configure_dt(&m_enable_spec, GPIO_OUTPUT_INACTIVE);
	if (ret != 0)
	{
		LOG_ERR("Failed to configure enable pin (%d)", ret);
		return ret;
	}

	ret = gpio_pin_configure_dt(&m_step_spec, GPIO_OUTPUT_INACTIVE);
	if (ret != 0)
	{
		LOG_ERR("Failed to configure step pin (%d)", ret);
		return ret;
	}

	ret = gpio_pin_configure_dt(&m_dir_spec, GPIO_OUTPUT_INACTIVE);
	if (ret != 0)
	{
		LOG_ERR("Failed to configure dir pin (%d)", ret);
		return ret;
	}

	/* Enable driver (active low) and set idle state. */
	gpio_pin_set_dt(&m_enable_spec, 1);
	gpio_pin_set_dt(&m_step_spec, 0);
	gpio_pin_set_dt(&m_dir_spec, 0);

	return 0;
}

void FirmwareHandler::initialise_state()
{
	m_state.enable = m_enable_spec;
	m_state.step = m_step_spec;
	m_state.dir = m_dir_spec;
	k_mutex_init(&m_state.lock);
	k_sem_init(&m_state.move_sem, 0, K_SEM_MAX_LIMIT);
	m_state.move_request = false;
	m_state.cancel_move = false;
	m_state.moving = false;
	m_state.direction = 1;
	m_state.steps_remaining = 0U;
	m_state.current_position = 0;
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
			bool should_step = false;
			int direction = 0;
			uint32_t period_us = 0U;
			uint32_t pulse_high_us = 0U;

			{
				MutexLock lock(m_state.lock);

				if (m_state.cancel_move)
				{
					LOG_DBG("Motion cancelled");
					m_state.cancel_move = false;
					m_state.moving = false;
					m_state.steps_remaining = 0U;
					m_state.desired_position =
						static_cast<uint16_t>(m_state.current_position & 0xFFFF);
					break;
				}

				if (m_state.move_request)
				{
					const int32_t delta = static_cast<int32_t>(m_state.desired_position) -
						m_state.current_position;
					if (delta == 0)
					{
						m_state.moving = false;
						m_state.steps_remaining = 0U;
						m_state.desired_position =
							static_cast<uint16_t>(m_state.current_position & 0xFFFF);
						LOG_DBG("Target equals current -> no motion");
					}
					else
					{
						m_state.direction = (delta > 0) ? 1 : -1;
						gpio_pin_set_dt(&m_state.dir, (m_state.direction > 0) ? 1 : 0);
						uint32_t magnitude = static_cast<uint32_t>((delta > 0) ? delta : -delta);
						m_state.steps_remaining = magnitude;
						m_state.moving = true;
						LOG_DBG("Starting motion: steps=%u dir=%d", magnitude, m_state.direction);
					}
					m_state.move_request = false;
				}

				if (!m_state.moving || m_state.steps_remaining == 0U)
				{
					m_state.moving = false;
					if (m_state.move_request)
					{
						continue;
					}
					break;
				}

				should_step = true;
				direction = m_state.direction;
				period_us = m_state.step_period_us;
				pulse_high_us = m_state.pulse_high_us;
			}

			if (!should_step)
			{
				continue;
			}

			gpio_pin_set_dt(&m_state.step, 1);
			k_busy_wait(pulse_high_us);
			gpio_pin_set_dt(&m_state.step, 0);
			if (period_us > pulse_high_us)
			{
				k_busy_wait(period_us - pulse_high_us);
			}
			k_yield();

			{
				MutexLock lock(m_state.lock);
				if (m_state.cancel_move)
				{
					m_state.cancel_move = false;
					m_state.moving = false;
					m_state.steps_remaining = 0U;
					m_state.desired_position =
						static_cast<uint16_t>(m_state.current_position & 0xFFFF);
					break;
				}

				if (m_state.steps_remaining > 0U)
				{
					m_state.steps_remaining--;
					m_state.current_position += direction;
				}

				if (m_state.steps_remaining == 0U)
				{
					m_state.moving = false;
					m_state.desired_position =
						static_cast<uint16_t>(m_state.current_position & 0xFFFF);
					if (!m_state.move_request)
					{
						break;
					}
				}
			}
		}
	}
}

void FirmwareHandler::stop()
{
	{
		MutexLock lock(m_state.lock);
		m_state.cancel_move = true;
		m_state.move_request = false;
		m_state.moving = false;
		m_state.steps_remaining = 0U;
		m_state.desired_position = static_cast<uint16_t>(m_state.current_position & 0xFFFF);
	}
	k_sem_give(&m_state.move_sem);
	LOG_INF("stop()");
}

uint16_t FirmwareHandler::getCurrentPosition()
{
	MutexLock lock(m_state.lock);
	uint16_t pos = static_cast<uint16_t>(m_state.current_position & 0xFFFF);
	LOG_DBG("getCurrentPosition -> 0x%04x (%u)", pos, pos);
	return pos;
}

void FirmwareHandler::setCurrentPosition(uint16_t position)
{
	MutexLock lock(m_state.lock);
	m_state.current_position = static_cast<int32_t>(position);
	m_state.staged_position = position;
	m_state.desired_position = position;
	m_state.steps_remaining = 0U;
	m_state.move_request = false;
	m_state.cancel_move = false;
	m_state.moving = false;
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
	MutexLock lock(m_state.lock);
	LOG_DBG("isMoving -> %s", m_state.moving ? "true" : "false");
	return m_state.moving;
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
	{
		MutexLock lock(m_state.lock);
		LOG_INF("setSpeed 0x%02x (%u) (was 0x%02x)", speed, speed, m_state.speed_multiplier);
		m_state.speed_multiplier = speed;
		update_timing_locked();
	}
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
