#pragma once

#include <zephyr/kernel.h>
#include <zephyr/drivers/gpio.h>

#include <Moonlite.hpp>

#include <cstdint>
#include <string>

class FirmwareHandler final : public moonlite::Handler
{
public:
    FirmwareHandler(const gpio_dt_spec &enable_spec,
		    const gpio_dt_spec &step_spec,
		    const gpio_dt_spec &dir_spec);

	int initialise();
	void motion_loop();

	void stop() override;
	uint16_t getCurrentPosition() override;
	void setCurrentPosition(uint16_t position) override;
	uint16_t getNewPosition() override;
	void setNewPosition(uint16_t position) override;
	void goToNewPosition() override;
	bool isHalfStep() override;
	void setHalfStep(bool enabled) override;
	bool isMoving() override;
	std::string getFirmwareVersion() override;
	uint8_t getSpeed() override;
	void setSpeed(uint8_t speed) override;
	uint16_t getTemperature() override;
	uint8_t getTemperatureCoefficientRaw() override;

private:
	struct FocuserState
	{
		gpio_dt_spec enable{};
		gpio_dt_spec step{};
		gpio_dt_spec dir{};
		k_mutex lock{};
		k_sem move_sem{};
		bool move_request{false};
		bool cancel_move{false};
		bool moving{false};
		int direction{1};
		uint32_t steps_remaining{0U};
		uint32_t step_period_us{500U};
		uint32_t pulse_high_us{250U};
		int32_t current_position{0};
		uint16_t staged_position{0U};
		uint16_t desired_position{0U};
		uint8_t speed_multiplier{1U};
		bool half_step{false};
		int8_t temperature_coeff_times2{0};
	};

	class MutexLock
	{
	public:
		explicit MutexLock(k_mutex &mutex) : m_mutex(mutex)
		{
			k_mutex_lock(&m_mutex, K_FOREVER);
		}

		~MutexLock()
		{
			k_mutex_unlock(&m_mutex);
		}

	private:
		k_mutex &m_mutex;
	};

	void update_timing_locked();
	int configure_stepper_pins();
	void initialise_state();

	FocuserState m_state{};
	gpio_dt_spec m_enable_spec{};
	gpio_dt_spec m_step_spec{};
	gpio_dt_spec m_dir_spec{};
};
