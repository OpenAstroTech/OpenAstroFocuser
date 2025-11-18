#pragma once

#include "FocuserStepper.hpp"

struct device;

class ZephyrFocuserStepper final : public FocuserStepper
{
public:
	ZephyrFocuserStepper(const struct device *stepper_dev,
			    const struct device *stepper_drv_dev);

	bool is_ready() const override;
	int set_reference_position(int32_t position) override;
	int set_microstep_interval(uint64_t interval_ns) override;
	int move_to(int32_t target) override;
	int is_moving(bool &moving) override;
	int stop() override;
	int get_actual_position(int32_t &position) override;
	int enable_driver(bool enable) override;

private:
	const struct device *m_stepper;
	const struct device *m_stepper_drv;
};
