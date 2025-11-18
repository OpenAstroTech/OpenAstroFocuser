#include <zephyr/ztest.h>

#include <Moonlite.hpp>

namespace
{

class TestHandler : public moonlite::Handler
{
public:
	void stop() override
	{
		stop_called = true;
	}

	uint16_t getCurrentPosition() override
	{
		return current_position;
	}

	void setCurrentPosition(uint16_t position) override
	{
		set_current_position_value = position;
	}

	uint16_t getNewPosition() override
	{
		return new_position;
	}

	void setNewPosition(uint16_t position) override
	{
		new_position = position;
	}

	void goToNewPosition() override
	{
		go_called = true;
	}

	bool isHalfStep() override
	{
		return half_step;
	}

	void setHalfStep(bool enabled) override
	{
		half_step = enabled;
	}

	bool isMoving() override
	{
		return moving;
	}

	std::string getFirmwareVersion() override
	{
		return firmware_version;
	}

	uint8_t getSpeed() override
	{
		return speed;
	}

	void setSpeed(uint8_t value) override
	{
		speed = value;
	}

	uint16_t getTemperature() override
	{
		return temperature;
	}

	uint8_t getTemperatureCoefficientRaw() override
	{
		return temperature_coefficient;
	}

	bool stop_called{false};
	bool go_called{false};
	bool half_step{false};
	bool moving{false};
	uint16_t current_position{0x1234};
	uint16_t new_position{0x2345};
	uint16_t set_current_position_value{0xFFFF};
	uint8_t speed{0x22};
	uint16_t temperature{0x3456};
	uint8_t temperature_coefficient{0x77};
	std::string firmware_version{"FW"};
};

bool feed_frame(moonlite::Parser &parser, const char *frame, std::string &response)
{
	response.clear();
	bool completed = false;
	for (const char *p = frame; *p != '\0'; ++p)
	{
		completed = parser.feed(*p, response);
	}
	return completed;
}

} // namespace

ZTEST(moonlite_helpers, expected_payload_lengths)
{
	zassert_equal(moonlite::expectedPayloadLength(moonlite::CommandType::set_current_position), 4,
		"SP payload length");
	zassert_equal(moonlite::expectedPayloadLength(moonlite::CommandType::set_speed), 2,
		"SD payload length");
	zassert_equal(moonlite::expectedPayloadLength(moonlite::CommandType::stop), 0,
		"FQ payload length");
}

ZTEST(moonlite_helpers, opcode_and_hex_helpers)
{
	zassert_equal(moonlite::strToCommandType("GP"), moonlite::CommandType::get_current_position,
		"GP opcode decode");
	zassert_equal(moonlite::strToCommandType("SD"), moonlite::CommandType::set_speed,
		"SD opcode decode");
	zassert_equal(moonlite::strToCommandType("XX"), moonlite::CommandType::unrecognized,
		"Unknown opcode decode");

	zassert_true(moonlite::hex2(0xAB) == "AB", "hex2 formatting");
	zassert_true(moonlite::hex4(0x0C3D) == "0C3D", "hex4 formatting");
	zassert_equal(moonlite::parseHex2("ab"), 0xAB, "parseHex2 lowercase");
	zassert_equal(moonlite::parseHex4("7fff"), 0x7FFF, "parseHex4 lowercase");
}

ZTEST(moonlite_parser, handles_query_commands)
{
	TestHandler handler;
	handler.half_step = true;
	handler.moving = true;
	handler.speed = 0x3C;
	handler.firmware_version = "FW123";
	moonlite::Parser parser(handler);
	std::string response;

	zassert_true(feed_frame(parser, ":GP#", response), "GP frame completion");
	zassert_equal(response, std::string("1234#"), "GP response");

	handler.new_position = 0x7654;
	zassert_true(feed_frame(parser, ":GN#", response), "GN frame completion");
	zassert_equal(response, std::string("7654#"), "GN response");

	zassert_true(feed_frame(parser, ":GH#", response), "GH frame completion");
	zassert_equal(response, std::string("FF#"), "GH response");

	zassert_true(feed_frame(parser, ":GI#", response), "GI frame completion");
	zassert_equal(response, std::string("01#"), "GI response");

	zassert_true(feed_frame(parser, ":GV#", response), "GV frame completion");
	zassert_equal(response, std::string("FW123"), "GV response");

	zassert_true(feed_frame(parser, ":GD#", response), "GD frame completion");
	zassert_equal(response, std::string("3C#"), "GD response");

	handler.temperature = 0x1111;
	zassert_true(feed_frame(parser, ":GT#", response), "GT frame completion");
	zassert_equal(response, std::string("1111#"), "GT response");

	handler.temperature_coefficient = 0xEF;
	zassert_true(feed_frame(parser, ":GC#", response), "GC frame completion");
	zassert_equal(response, std::string("EF#"), "GC response");
}

ZTEST(moonlite_parser, handles_state_changing_commands)
{
	TestHandler handler;
	moonlite::Parser parser(handler);
	std::string response;

	zassert_true(feed_frame(parser, ":SPBEEF#", response), "SP frame completion");
	zassert_equal(handler.set_current_position_value, 0xBEEF, "SP payload applied");
	zassert_true(response.empty(), "SP has no response");

	zassert_true(feed_frame(parser, ":SNA0A0#", response), "SN frame completion");
	zassert_equal(handler.new_position, 0xA0A0, "SN payload applied");

	zassert_true(feed_frame(parser, ":FG#", response), "FG frame completion");
	zassert_true(handler.go_called, "FG invoked go");

	zassert_true(feed_frame(parser, ":SF#", response), "SF frame completion");
	zassert_false(handler.half_step, "SF disables half-step");

	zassert_true(feed_frame(parser, ":SH#", response), "SH frame completion");
	zassert_true(handler.half_step, "SH enables half-step");

	zassert_true(feed_frame(parser, ":SD33#", response), "SD frame completion");
	zassert_equal(handler.speed, 0x33, "SD payload applied");

	zassert_true(feed_frame(parser, ":FQ#", response), "FQ frame completion");
	zassert_true(handler.stop_called, "FQ stopped focuser");
}

ZTEST(moonlite_parser, rejects_invalid_payload)
{
	TestHandler handler;
	moonlite::Parser parser(handler);
	std::string response;

	handler.set_current_position_value = 0xCAFE;
	zassert_true(feed_frame(parser, ":SP12G#", response), "Invalid frame still completes");
	zassert_equal(handler.set_current_position_value, 0xCAFE, "Invalid payload ignored");
	zassert_true(response.empty(), "Invalid frame has no response");
}

ZTEST_SUITE(moonlite_helpers, NULL, NULL, NULL, NULL, NULL);
ZTEST_SUITE(moonlite_parser, NULL, NULL, NULL, NULL, NULL);
