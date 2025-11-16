#pragma once

#include <cstdint>
#include <string>

namespace moonlite
{

  /**
   * Moonlite focuser serial protocol helpers.
   *
   * Every command is transmitted as a frame of the form `:<opcode><payload>#`.
   * The comments below document both the expected payload (if any) and the
   * controller response emitted by the reference implementation.
   */
  enum class CommandType : uint8_t
  {
    /**
     * `FQ`
     *   Payload: none
     *   Response: none
     *   Action: stop any ongoing move and release the motor driver.
     */
    stop,

    /**
     * `GP`
     *   Payload: none
     *   Response: `PPPP#`
     *   Action: report the currently stored absolute position as a 4-digit hex.
     */
    get_current_position,

    /**
     * `SP`
     *   Payload: `PPPP`
     *   Response: none
     *   Action: set the stored absolute position without moving the motor.
     */
    set_current_position,

    /**
     * `GN`
     *   Payload: none
     *   Response: `PPPP#`
     *   Action: read the pending target position requested by the controller.
     */
    get_new_position,

    /**
     * `SN`
     *   Payload: `PPPP`
     *   Response: none
     *   Action: stage a new absolute target position (no motion yet).
     */
    set_new_position,

    /**
     * `FG`
     *   Payload: none
     *   Response: none (motion begins)
     *   Action: execute the staged move to the last value supplied with `SN`.
     */
    go_to_new_position,

    /**
     * `GH`
     *   Payload: none
     *   Response: `FF#` when half-step, `00#` otherwise
     *   Action: report whether the controller currently uses the half-step profile.
     */
    check_if_half_step,

    /**
     * `SF`
     *   Payload: none
     *   Response: none
     *   Action: switch the driver to the configured "full step" microstep mode.
     */
    set_full_step,

    /**
     * `SH`
     *   Payload: none
     *   Response: none
     *   Action: switch the driver to the configured "half step" microstep mode.
     */
    set_half_step,

    /**
     * `GI`
     *   Payload: none
     *   Response: `01#` if moving, `00#` otherwise
     *   Action: report whether the focuser is actively moving.
     */
    check_if_moving,

    /**
     * `GV`
     *   Payload: none
     *   Response: implementation-defined string
     *   Action: read the firmware version string exposed by the controller.
     */
    get_firmware_version,

    /**
     * `GD`
     *   Payload: none
     *   Response: `SS#`
     *   Action: read the current delay-multiplier byte that controls the slew rate (500 µs * multiplier).
     */
    get_speed,

    /**
     * `SD`
     *   Payload: `SS`
     *   Response: none
     *   Action: set the delay multiplier that defines the motor slew rate (each unit adds 500 µs between steps).
     */
    set_speed,

    /**
     * `GT`
     *   Payload: none
     *   Response: `TTTT#` (currently `0000#` placeholder)
     *   Action: request the temperature sensor reading if the hardware provides it.
     */
    get_temperature,

    /**
     * `GC`
     *   Payload: none
     *   Response: `CC#` where CC is a two's-complement byte
     *   Action: read the temperature compensation coefficient encoded as int8_t*2
     */
    get_temperature_coefficient,

    /** Unrecognised command string. */
    unrecognized
  };

  /** Expected request payload length (in hex chars) for each command. */
  int expectedPayloadLength(CommandType cmd);

  /**
   * Translate a two-character Moonlite opcode to the matching enum value.
   *
   * @param buffer Pointer to at least two opcode characters.
   * @return Matching `CommandType`, or `unrecognized` when unknown.
   */
  CommandType strToCommandType(const char *buffer);

  /**
   * Generic device handler interface used by the parser to interact with your
   * focuser implementation.
   *
   * Methods that conceptually "set" values do not return responses; the parser
   * will send no payload for those commands. Methods that "get" values return
   * native types which the parser converts to the protocol's hexadecimal
   * strings. All positions are absolute step counts (0..65535) and the speed is
   * a delay multiplier byte (each unit adds 500 µs between steps).
   */
  class Handler
  {
  public:
    virtual ~Handler() {}

    /** Handle emergency stop command (FQ). */
    virtual void stop() = 0;

    /** Report the current stored absolute position (GP). */
    virtual uint16_t getCurrentPosition() = 0;

    /** Set the stored absolute position without moving the motor (SP). */
    virtual void setCurrentPosition(uint16_t) = 0;

    /** Report the pending target position (GN). */
    virtual uint16_t getNewPosition() = 0;

    /** Stage a new absolute target position without motion (SN). */
    virtual void setNewPosition(uint16_t) = 0;

    /** Begin motion toward the staged target (FG). */
    virtual void goToNewPosition() = 0;

    /** Whether half-step mode is active (GH). */
    virtual bool isHalfStep() = 0;

    /** Switch between half-step/full-step (SF/SH). */
    virtual void setHalfStep(bool enabled) = 0;

    /** Whether the focuser is currently moving (GI). */
    virtual bool isMoving() = 0;

    /** Provide firmware version string (GV). */
    virtual std::string getFirmwareVersion() = 0;

    /** Return the current speed multiplier byte (GD). */
    virtual uint8_t getSpeed() = 0;

    /** Update the speed multiplier byte (SD). */
    virtual void setSpeed(uint8_t) = 0;

    /** Provide the temperature reading, or 0 if unsupported (GT). */
    virtual uint16_t getTemperature() = 0;

    /** Return the raw temperature coefficient byte (GC). */
    virtual uint8_t getTemperatureCoefficientRaw() = 0;
  };

  /** Format a byte/word as uppercase hexadecimal strings. */
  std::string hex2(uint8_t v);
  std::string hex4(uint16_t v);

  /** Parse hexadecimal payloads emitted by the Moonlite protocol. */
  uint16_t parseHex4(const std::string &s);
  uint8_t parseHex2(const std::string &s);

  /**
   * Streaming Moonlite protocol parser.
   *
   * Usage:
   *   - Construct with a handler implementation.
   *   - Feed incoming bytes via feed().
   *   - When a full frame is parsed, feed() returns true and (optionally)
   *     populates the response string, which already includes the trailing '#'.
   */
  class Parser
  {
  public:
    explicit Parser(Handler &handler);

    /** Feed a single byte of input; returns true when a frame completes. */
    bool feed(char c, std::string &outResponse);

    /** Reset the parser state machine (used on framing errors). */
    void reset();

  private:
    enum class State : uint8_t
    {
      Idle,
      ReadingOpcode,
      ReadingPayload
    };

    static bool isHexChar(char c);

    std::string handleCommand(CommandType cmd, const std::string &payload);

    Handler *_handler;
    State _state{State::Idle};
    CommandType _cmd{CommandType::unrecognized};
    std::string _buf;
  };

} // namespace moonlite
