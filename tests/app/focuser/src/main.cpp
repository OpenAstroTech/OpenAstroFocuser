#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#include <errno.h>

#include <cstdint>

#include <zephyr/device.h>
#include <zephyr/drivers/stepper/stepper_fake.h>
#include <zephyr/logging/log.h>

#include "Focuser.hpp"
#include "ZephyrStepper.hpp"

#ifndef CONFIG_APP_LOG_LEVEL
#define CONFIG_APP_LOG_LEVEL LOG_LEVEL_INF
#endif

LOG_MODULE_REGISTER(focuser, CONFIG_APP_LOG_LEVEL);

DEFINE_FFF_GLOBALS;

namespace
{

const struct device *const k_stepper_controller = DEVICE_DT_GET(DT_ALIAS(stepper));
const struct device *const k_stepper_driver = DEVICE_DT_GET(DT_ALIAS(stepper_drv));

void assert_stepper_devices_ready()
{
	zassert_true(device_is_ready(k_stepper_controller), "Fake stepper controller not ready");
	zassert_true(device_is_ready(k_stepper_driver), "Fake stepper driver not ready");
}

class ReadyOverrideStepper : public FocuserStepper
{
public:
	ReadyOverrideStepper(FocuserStepper &impl, bool ready_override)
		: m_impl(impl), m_ready_override(ready_override)
	{
	}

	bool is_ready() const override
	{
		return m_ready_override && m_impl.is_ready();
	}

	int set_reference_position(int32_t position) override
	{
		return m_impl.set_reference_position(position);
	}

	int set_microstep_interval(uint64_t interval_ns) override
	{
		return m_impl.set_microstep_interval(interval_ns);
	}

	int move_to(int32_t target) override
	{
		return m_impl.move_to(target);
	}

	int is_moving(bool &moving) override
	{
		return m_impl.is_moving(moving);
	}

	int stop() override
	{
		return m_impl.stop();
	}

	int get_actual_position(int32_t &position) override
	{
		return m_impl.get_actual_position(position);
	}

	int enable_driver(bool enable) override
	{
		return m_impl.enable_driver(enable);
	}

private:
	FocuserStepper &m_impl;
	bool m_ready_override;
};

} // namespace

ZTEST(focuser_app, test_initialise_requires_ready_stepper)
{
	assert_stepper_devices_ready();
	ZephyrFocuserStepper hw_stepper(k_stepper_controller, k_stepper_driver);
	ReadyOverrideStepper stepper(hw_stepper, false);
	Focuser focuser(stepper);

	const int ret = focuser.initialise();

	zassert_equal(ret, -ENODEV, "initialise should fail when hardware is not ready");
	zassert_equal(fake_stepper_set_reference_position_fake.call_count, 0U,
		"should not set reference position on failure");
	zassert_equal(fake_stepper_set_microstep_interval_fake.call_count, 0U,
		"should not update step interval on failure");
	zassert_equal(fake_stepper_drv_enable_fake.call_count, 1U,
		"only constructor enable should run");
}

ZTEST(focuser_app, test_initialise_configures_stepper_when_ready)
{
	assert_stepper_devices_ready();
	ZephyrFocuserStepper stepper(k_stepper_controller, k_stepper_driver);
	Focuser focuser(stepper);

	const int ret = focuser.initialise();

	zassert_equal(ret, 0, "initialise should succeed when hardware is ready");
	zassert_equal(fake_stepper_set_reference_position_fake.call_count, 1U,
		"reference position should be set once");
	zassert_equal(fake_stepper_set_reference_position_fake.arg1_val, 0,
		"reference position should reset to zero");
	zassert_equal(fake_stepper_set_microstep_interval_fake.call_count, 1U,
		"step interval should be applied once");
	zassert_equal(fake_stepper_set_microstep_interval_fake.arg1_val, 500000ULL,
		"default speed uses 500us interval");
	zassert_equal(fake_stepper_drv_enable_fake.call_count, 2U,
		"constructor plus initialise enable calls expected");
	zassert_equal(fake_stepper_drv_disable_fake.call_count, 0U,
		"driver should remain enabled after init");
}

ZTEST(focuser_app, test_set_speed_clamps_to_minimum_and_updates_interval)
{
	assert_stepper_devices_ready();
	ZephyrFocuserStepper stepper(k_stepper_controller, k_stepper_driver);
	Focuser focuser(stepper);
	zassert_ok(focuser.initialise(), "initialise precondition");

	const unsigned int initial_microstep_calls =
		fake_stepper_set_microstep_interval_fake.call_count;

	focuser.setSpeed(0);
	zassert_equal(fake_stepper_set_microstep_interval_fake.call_count,
		initial_microstep_calls + 1U, "setSpeed should update timing once");
	zassert_equal(fake_stepper_set_microstep_interval_fake.arg1_val, 500000ULL,
		"speed 0 should clamp to 1x interval");
	zassert_equal(focuser.getSpeed(), 1, "speed should clamp to 1");

	focuser.setSpeed(40);
	zassert_equal(fake_stepper_set_microstep_interval_fake.call_count,
		initial_microstep_calls + 2U, "second setSpeed call should reapply interval");
	zassert_equal(fake_stepper_set_microstep_interval_fake.arg1_val, 10000000ULL,
		"high multiplier should clamp to 100 sps");
	zassert_equal(focuser.getSpeed(), 40, "speed multiplier should store requested value");
}

ZTEST(focuser_app, test_stop_stops_motion_and_disables_driver)
{
	assert_stepper_devices_ready();
	ZephyrFocuserStepper stepper(k_stepper_controller, k_stepper_driver);
	Focuser focuser(stepper);
	zassert_ok(focuser.initialise(), "initialise precondition");

	const unsigned int initial_stop_calls = fake_stepper_stop_fake.call_count;
	const unsigned int initial_get_pos_calls = fake_stepper_get_actual_position_fake.call_count;
	const unsigned int initial_disable_calls = fake_stepper_drv_disable_fake.call_count;

	fake_stepper_get_actual_position_fake.custom_fake =
		[](const struct device *, int32_t *position) {
			if (position != nullptr)
			{
				*position = 0x4321;
			}
			return 0;
		};

	focuser.stop();

	zassert_equal(fake_stepper_stop_fake.call_count, initial_stop_calls + 1U,
		"stop should halt the stepper");
	zassert_equal(fake_stepper_get_actual_position_fake.call_count,
		initial_get_pos_calls + 1U, "stop queries actual position once");
	zassert_equal(fake_stepper_drv_disable_fake.call_count, initial_disable_calls + 1U,
		"stop should disable the driver once");
}

ZTEST(focuser_app, test_initialise_ignores_ealready_from_driver)
{
	assert_stepper_devices_ready();
	ZephyrFocuserStepper stepper(k_stepper_controller, k_stepper_driver);
	Focuser focuser(stepper);

	int enable_results[] = {-EALREADY, -EALREADY};
	SET_RETURN_SEQ(fake_stepper_drv_enable, enable_results, 2);

	const int ret = focuser.initialise();

	zassert_equal(ret, 0, "-EALREADY responses should not fail init");
	zassert_equal(fake_stepper_drv_enable_fake.call_count, 2U,
		"two enable attempts should have occurred");
}

ZTEST_SUITE(focuser_app, NULL, NULL, NULL, NULL, NULL);
