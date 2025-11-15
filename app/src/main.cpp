/*
 * Moonlite-compatible focuser firmware running on Zephyr.
 */

#include <zephyr/kernel.h>
#include <zephyr/device.h>
#include <zephyr/drivers/gpio.h>
#include <zephyr/drivers/uart.h>
#include <zephyr/logging/log.h>
#include <zephyr/sys/util.h>

#include <Moonlite.h>

#include <app_version.h>

#include <errno.h>
#include <cstddef>
#include <cstdint>
#include <string>

LOG_MODULE_REGISTER(focuser, CONFIG_APP_LOG_LEVEL);

#define MOTION_THREAD_STACK_SIZE 2048
#define MOTION_THREAD_PRIORITY K_PRIO_PREEMPT(4)
#define SERIAL_THREAD_STACK_SIZE 2048
#define SERIAL_THREAD_PRIORITY K_PRIO_PREEMPT(5)

#define FOCUSER_NODE DT_PATH(zephyr_user)

#if !DT_NODE_EXISTS(FOCUSER_NODE)
#error "zephyr,user node must provide focuser pin assignments"
#endif

#if !DT_NODE_HAS_PROP(FOCUSER_NODE, focuser_enable_gpios)
#error "zephyr,user node requires focuser-enable-gpios property"
#endif

#if !DT_NODE_HAS_PROP(FOCUSER_NODE, focuser_step_gpios)
#error "zephyr,user node requires focuser-step-gpios property"
#endif

#if !DT_NODE_HAS_PROP(FOCUSER_NODE, focuser_dir_gpios)
#error "zephyr,user node requires focuser-dir-gpios property"
#endif

#if !DT_NODE_HAS_PROP(FOCUSER_NODE, moonlite_uart)
#error "zephyr,user node requires moonlite_uart phandle"
#endif

#if !DT_HAS_CHOSEN(zephyr_console)
#error "Console device is required for Moonlite serial protocol"
#endif

namespace
{

	constexpr const char kFirmwareVersion[] = "10";

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

	FocuserState g_state;

	const struct gpio_dt_spec g_enable_spec = GPIO_DT_SPEC_GET(FOCUSER_NODE, focuser_enable_gpios);
	const struct gpio_dt_spec g_step_spec = GPIO_DT_SPEC_GET(FOCUSER_NODE, focuser_step_gpios);
	const struct gpio_dt_spec g_dir_spec = GPIO_DT_SPEC_GET(FOCUSER_NODE, focuser_dir_gpios);

	const struct device *const g_console = DEVICE_DT_GET(DT_CHOSEN(zephyr_console));
	const struct device *const g_proto_uart = DEVICE_DT_GET(DT_PHANDLE(FOCUSER_NODE, moonlite_uart));

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

	void update_timing_locked(FocuserState &state)
	{
		state.step_period_us = compute_step_period_us(state.speed_multiplier);
		state.pulse_high_us = compute_pulse_high_us(state.step_period_us);
		LOG_DBG("Computed step timing: period=%u us, pulse=%u us", state.step_period_us,
				state.pulse_high_us);
	}

	class FirmwareHandler final : public moonlite::Handler
	{
	public:
		explicit FirmwareHandler(FocuserState &state) : m_state(state) {}

		void stop() override
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

		uint16_t getCurrentPosition() override
		{
			MutexLock lock(m_state.lock);
			uint16_t pos = static_cast<uint16_t>(m_state.current_position & 0xFFFF);
			LOG_DBG("getCurrentPosition -> 0x%04x (%u)", pos, pos);
			return pos;
		}

		void setCurrentPosition(uint16_t position) override
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

		uint16_t getNewPosition() override
		{
			MutexLock lock(m_state.lock);
			LOG_DBG("getNewPosition -> 0x%04x (%u)", m_state.staged_position, m_state.staged_position);
			return m_state.staged_position;
		}

		void setNewPosition(uint16_t position) override
		{
			MutexLock lock(m_state.lock);
			LOG_INF("setNewPosition 0x%04x (%u) (was 0x%04x)", position, position,
					m_state.staged_position);
			m_state.staged_position = position;
		}

		void goToNewPosition() override
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

		bool isHalfStep() override
		{
			MutexLock lock(m_state.lock);
			LOG_DBG("isHalfStep -> %s", m_state.half_step ? "true" : "false");
			return m_state.half_step;
		}

		void setHalfStep(bool enabled) override
		{
			MutexLock lock(m_state.lock);
			LOG_INF("setHalfStep %s (was %s)", enabled ? "true" : "false",
					m_state.half_step ? "true" : "false");
			m_state.half_step = enabled;
		}

		bool isMoving() override
		{
			MutexLock lock(m_state.lock);
			LOG_DBG("isMoving -> %s", m_state.moving ? "true" : "false");
			return m_state.moving;
		}

		std::string getFirmwareVersion() override
		{
			LOG_DBG("getFirmwareVersion -> %s", kFirmwareVersion);
			return std::string(kFirmwareVersion);
		}

		uint8_t getSpeed() override
		{
			MutexLock lock(m_state.lock);
			LOG_DBG("getSpeed -> 0x%02x (%u)", m_state.speed_multiplier, m_state.speed_multiplier);
			return m_state.speed_multiplier;
		}

		void setSpeed(uint8_t speed) override
		{
			if (speed == 0U)
			{
				speed = 1U;
			}
			{
				MutexLock lock(m_state.lock);
				LOG_INF("setSpeed 0x%02x (%u) (was 0x%02x)", speed, speed, m_state.speed_multiplier);
				m_state.speed_multiplier = speed;
				update_timing_locked(m_state);
			}
		}

		uint16_t getTemperature() override
		{
			LOG_DBG("getTemperature -> 0x0000 (0)");
			return 0x0000;
		}

		uint8_t getTemperatureCoefficientRaw() override
		{
			MutexLock lock(m_state.lock);
			LOG_DBG("getTemperatureCoefficientRaw -> 0x%02x (%d -> %.1f)",
					static_cast<uint8_t>(m_state.temperature_coeff_times2),
					static_cast<int>(m_state.temperature_coeff_times2),
					static_cast<double>(m_state.temperature_coeff_times2) / 2.0);
			return static_cast<uint8_t>(m_state.temperature_coeff_times2);
		}

	private:
		FocuserState &m_state;
	};

	FirmwareHandler g_handler(g_state);
	moonlite::Parser g_parser(g_handler);

	K_THREAD_STACK_DEFINE(motion_stack, MOTION_THREAD_STACK_SIZE);
	K_THREAD_STACK_DEFINE(serial_stack, SERIAL_THREAD_STACK_SIZE);
	struct k_thread motion_thread_data;
	struct k_thread serial_thread_data;

	void motion_thread(void *, void *, void *)
	{
		while (true)
		{
			k_sem_take(&g_state.move_sem, K_FOREVER);

			while (true)
			{
				bool should_step = false;
				int direction = 0;
				uint32_t period_us = 0U;
				uint32_t pulse_high_us = 0U;

				{
					MutexLock lock(g_state.lock);

					if (g_state.cancel_move)
					{
						LOG_DBG("Motion cancelled");
						g_state.cancel_move = false;
						g_state.moving = false;
						g_state.steps_remaining = 0U;
						g_state.desired_position = static_cast<uint16_t>(g_state.current_position & 0xFFFF);
						break;
					}

					if (g_state.move_request)
					{
						const int32_t delta = static_cast<int32_t>(g_state.desired_position) -
											  g_state.current_position;
						if (delta == 0)
						{
							g_state.moving = false;
							g_state.steps_remaining = 0U;
							g_state.desired_position = static_cast<uint16_t>(g_state.current_position & 0xFFFF);
							LOG_DBG("Target equals current -> no motion");
						}
						else
						{
							g_state.direction = (delta > 0) ? 1 : -1;
							gpio_pin_set_dt(&g_state.dir, (g_state.direction > 0) ? 1 : 0);
							uint32_t magnitude = static_cast<uint32_t>((delta > 0) ? delta : -delta);
							g_state.steps_remaining = magnitude;
							g_state.moving = true;
							LOG_DBG("Starting motion: steps=%u dir=%d", magnitude, g_state.direction);
						}
						g_state.move_request = false;
					}

					if (!g_state.moving || g_state.steps_remaining == 0U)
					{
						g_state.moving = false;
						if (g_state.move_request)
						{
							continue;
						}
						break;
					}

					should_step = true;
					direction = g_state.direction;
					period_us = g_state.step_period_us;
					pulse_high_us = g_state.pulse_high_us;
				}

				if (!should_step)
				{
					continue;
				}

				gpio_pin_set_dt(&g_state.step, 1);
				k_busy_wait(pulse_high_us);
				gpio_pin_set_dt(&g_state.step, 0);
				if (period_us > pulse_high_us)
				{
					k_busy_wait(period_us - pulse_high_us);
				}
				k_yield();

				{
					MutexLock lock(g_state.lock);
					if (g_state.cancel_move)
					{
						g_state.cancel_move = false;
						g_state.moving = false;
						g_state.steps_remaining = 0U;
						g_state.desired_position = static_cast<uint16_t>(g_state.current_position & 0xFFFF);
						break;
					}

					if (g_state.steps_remaining > 0U)
					{
						g_state.steps_remaining--;
						g_state.current_position += direction;
					}

					if (g_state.steps_remaining == 0U)
					{
						g_state.moving = false;
						g_state.desired_position = static_cast<uint16_t>(g_state.current_position & 0xFFFF);
						if (!g_state.move_request)
						{
							break;
						}
					}
				}
			}
		}
	}

	constexpr size_t kMaxLoggedFrameLen = 80U;

	void serial_thread(void *, void *, void *)
	{
		std::string response;
		std::string frame_log;
		bool frame_overflow = false;

		while (true)
		{
			unsigned char byte;
			const int rc = uart_poll_in(g_proto_uart, &byte);
			if (rc == 0)
			{
				const char c = static_cast<char>(byte);

				if (c == ':')
				{
					frame_log.clear();
					frame_overflow = false;
					frame_log.push_back(c);
				}
				else if (!frame_log.empty())
				{
					if (frame_log.size() < kMaxLoggedFrameLen)
					{
						frame_log.push_back(c);
					}
					else
					{
						frame_overflow = true;
					}
				}

				if (g_parser.feed(c, response))
				{
					if (!frame_log.empty())
					{
						if (frame_overflow)
						{
							LOG_INF("Moonlite RX %s... (truncated)", frame_log.c_str());
						}
						else
						{
							LOG_INF("Moonlite RX %s", frame_log.c_str());
						}
					}
					else
					{
						LOG_INF("Moonlite RX <unframed>");
					}

					if (!response.empty())
					{
						LOG_INF("Moonlite TX %s", response.c_str());
						for (char ch : response)
						{
							uart_poll_out(g_proto_uart, ch);
						}
					}
					else
					{
						LOG_DBG("Moonlite command produced no response");
					}

					frame_log.clear();
					frame_overflow = false;
					response.clear();
				}
			}
			else
			{
				k_sleep(K_MSEC(1));
			}
		}
	}

	int configure_stepper_pins()
	{
		if (!device_is_ready(g_enable_spec.port) || !device_is_ready(g_step_spec.port) ||
			!device_is_ready(g_dir_spec.port))
		{
			LOG_ERR("Stepper GPIO controller not ready");
			return -ENODEV;
		}

		int ret = gpio_pin_configure_dt(&g_enable_spec, GPIO_OUTPUT_INACTIVE);
		if (ret != 0)
		{
			LOG_ERR("Failed to configure enable pin (%d)", ret);
			return ret;
		}

		ret = gpio_pin_configure_dt(&g_step_spec, GPIO_OUTPUT_INACTIVE);
		if (ret != 0)
		{
			LOG_ERR("Failed to configure step pin (%d)", ret);
			return ret;
		}

		ret = gpio_pin_configure_dt(&g_dir_spec, GPIO_OUTPUT_INACTIVE);
		if (ret != 0)
		{
			LOG_ERR("Failed to configure dir pin (%d)", ret);
			return ret;
		}

		/* Enable driver (active low) and set idle state. */
		gpio_pin_set_dt(&g_enable_spec, 1);
		gpio_pin_set_dt(&g_step_spec, 0);
		gpio_pin_set_dt(&g_dir_spec, 0);

		return 0;
	}

	void initialise_state()
	{
		g_state.enable = g_enable_spec;
		g_state.step = g_step_spec;
		g_state.dir = g_dir_spec;
		k_mutex_init(&g_state.lock);
		k_sem_init(&g_state.move_sem, 0, K_SEM_MAX_LIMIT);
		g_state.move_request = false;
		g_state.cancel_move = false;
		g_state.moving = false;
		g_state.direction = 1;
		g_state.steps_remaining = 0U;
		g_state.current_position = 0;
		g_state.staged_position = 0U;
		g_state.desired_position = 0U;
		g_state.speed_multiplier = 1U;
		g_state.half_step = false;
		g_state.temperature_coeff_times2 = 0;
		update_timing_locked(g_state);
	}

} // namespace

int main(void)
{
	LOG_INF("Moonlite focuser firmware %s", APP_VERSION_STRING);

	if (!device_is_ready(g_console))
	{
		LOG_ERR("Console device not ready");
		return -ENODEV;
	}

	if (!device_is_ready(g_proto_uart))
	{
		LOG_ERR("Moonlite UART device not ready");
		return -ENODEV;
	}

	initialise_state();

	int ret = configure_stepper_pins();
	if (ret != 0)
	{
		return ret;
	}

	k_thread_create(&motion_thread_data, motion_stack, K_THREAD_STACK_SIZEOF(motion_stack),
					&motion_thread, nullptr, nullptr, nullptr, MOTION_THREAD_PRIORITY,
					0, K_NO_WAIT);
	k_thread_name_set(&motion_thread_data, "focuser_motion");

	k_thread_create(&serial_thread_data, serial_stack, K_THREAD_STACK_SIZEOF(serial_stack),
					&serial_thread, nullptr, nullptr, nullptr, SERIAL_THREAD_PRIORITY,
					0, K_NO_WAIT);
	k_thread_name_set(&serial_thread_data, "focuser_serial");

	LOG_INF("Moonlite focuser ready: UART 9600 8N1");

	return 0;
}
