#pragma once

#include <zephyr/kernel.h>

#include <Moonlite.hpp>

#include <cstdint>
#include <string>

#include "FocuserStepper.hpp"

class Focuser final : public moonlite::Handler
{
public:
	explicit Focuser(FocuserStepper &stepper);

	int initialise();
	void loop();

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
		k_mutex lock{};
		k_sem move_sem{};
		bool move_request{false};
		bool cancel_move{false};
		uint64_t step_interval_ns{500000U};
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
	void init();
	void move_to(uint16_t target);
	int apply_step_interval(uint64_t interval_ns);
	int32_t read_actual_position();
	int set_stepper_driver_enabled(bool enable);

	FocuserState m_state{};
	FocuserStepper &m_stepper;
};
