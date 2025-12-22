#include <zephyr/fff.h>
#include <zephyr/ztest.h>

#include <errno.h>

#include <cstdint>
#include <cstring>

#include <zephyr/device.h>
#include <zephyr/drivers/eeprom/eeprom_fake.h>
#include <zephyr/drivers/stepper/stepper_fake.h>
#include <zephyr/logging/log.h>

#include "Focuser.hpp"
#include "EepromPositionStore.hpp"
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
constexpr char kFirmwareVersion[] = "twister-test";

class MockPositionStore final : public PositionStore
{
public:
	bool load(uint16_t &position_out) override
	{
		++load_calls;
		if (!has_value)
		{
			return false;
		}
		position_out = value;
		return true;
	}

	void save(uint16_t position) override
	{
		++save_calls;
		last_saved = position;
	}

	bool has_value{false};
	uint16_t value{0U};
	unsigned int load_calls{0U};
	unsigned int save_calls{0U};
	uint16_t last_saved{0U};
};

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
	Focuser focuser(stepper, nullptr, kFirmwareVersion);

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
	Focuser focuser(stepper, nullptr, kFirmwareVersion);

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

ZTEST(focuser_app, test_initialise_restores_position_from_store)
{
	assert_stepper_devices_ready();
	ZephyrFocuserStepper stepper(k_stepper_controller, k_stepper_driver);
	MockPositionStore store;
	store.has_value = true;
	store.value = 0x1234;
	Focuser focuser(stepper, &store, kFirmwareVersion);

	const int ret = focuser.initialise();

	zassert_equal(ret, 0, "initialise should succeed");
	zassert_equal(store.load_calls, 1U, "should attempt to load persisted position once");
	zassert_equal(store.save_calls, 0U, "restore should not persist back");

	zassert_equal(fake_stepper_set_reference_position_fake.call_count, 2U,
		"init should set reference to 0 then restored position");
	zassert_equal(fake_stepper_set_reference_position_fake.arg1_history[0], 0,
		"first reference position should be 0");
	zassert_equal(fake_stepper_set_reference_position_fake.arg1_history[1], 0x1234,
		"second reference position should be restored value");
}

ZTEST(focuser_app, test_set_current_position_persists_via_store)
{
	assert_stepper_devices_ready();
	ZephyrFocuserStepper stepper(k_stepper_controller, k_stepper_driver);
	MockPositionStore store;
	Focuser focuser(stepper, &store, kFirmwareVersion);
	zassert_ok(focuser.initialise(), "initialise precondition");

	focuser.setCurrentPosition(0x1111);

	zassert_equal(store.save_calls, 1U, "setCurrentPosition should persist once");
	zassert_equal(store.last_saved, 0x1111, "persisted value should match set position");
}

ZTEST(focuser_app, test_set_speed_clamps_to_minimum_and_updates_interval)
{
	assert_stepper_devices_ready();
	ZephyrFocuserStepper stepper(k_stepper_controller, k_stepper_driver);
	Focuser focuser(stepper, nullptr, kFirmwareVersion);
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
	MockPositionStore store;
	Focuser focuser(stepper, &store, kFirmwareVersion);
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
	zassert_equal(store.save_calls, 1U, "stop should persist the last known position");
	zassert_equal(store.last_saved, 0x4321, "stop should persist queried position");
}

ZTEST(focuser_app, test_initialise_ignores_ealready_from_driver)
{
	assert_stepper_devices_ready();
	ZephyrFocuserStepper stepper(k_stepper_controller, k_stepper_driver);
	Focuser focuser(stepper, nullptr, kFirmwareVersion);

	int enable_results[] = {-EALREADY, -EALREADY};
	SET_RETURN_SEQ(fake_stepper_drv_enable, enable_results, 2);

	const int ret = focuser.initialise();

	zassert_equal(ret, 0, "-EALREADY responses should not fail init");
	zassert_equal(fake_stepper_drv_enable_fake.call_count, 2U,
		"two enable attempts should have occurred");
}

ZTEST(focuser_app, test_eeprom_position_store_roundtrip)
{
	static uint8_t backing_store[32];
	std::memset(backing_store, 0xFF, sizeof(backing_store));

	RESET_FAKE(fake_eeprom_read);
	RESET_FAKE(fake_eeprom_write);
	RESET_FAKE(fake_eeprom_size);

	fake_eeprom_size_fake.custom_fake = [](const struct device *) -> size_t {
		return sizeof(backing_store);
	};

	fake_eeprom_write_fake.custom_fake = [](const struct device *, off_t offset, const void *data, size_t len) -> int {
		if ((offset < 0) || (static_cast<size_t>(offset) + len > sizeof(backing_store)))
		{
			return -EINVAL;
		}
		std::memcpy(&backing_store[static_cast<size_t>(offset)], data, len);
		return 0;
	};

	fake_eeprom_read_fake.custom_fake = [](const struct device *, off_t offset, void *data, size_t len) -> int {
		if ((offset < 0) || (static_cast<size_t>(offset) + len > sizeof(backing_store)))
		{
			return -EINVAL;
		}
		std::memcpy(data, &backing_store[static_cast<size_t>(offset)], len);
		return 0;
	};

	EepromPositionStore store;
	store.save(0x2222);

	EepromPositionStore store2;
	uint16_t loaded = 0U;
	zassert_true(store2.load(loaded), "load should succeed after save");
	zassert_equal(loaded, 0x2222, "loaded value should match saved");
}

ZTEST_SUITE(focuser_app, NULL, NULL, NULL, NULL, NULL);
