#pragma once

#include "Moonlite.hpp"

#if 0

#ifndef _MOONLITE_H_
#define _MOONLITE_H_

#include <inttypes.h>
#include <string.h>
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

  /** Expected request payload length (in hex characters) for each command. */
  inline int expectedPayloadLength(CommandType cmd)
  {
    switch (cmd)
    {
    case CommandType::set_speed:
      return 2; // SS
    case CommandType::set_current_position:
    case CommandType::set_new_position:
    case CommandType::get_current_position:        // request has no payload
    case CommandType::get_new_position:            // request has no payload
    case CommandType::go_to_new_position:          // request has no payload
    case CommandType::check_if_half_step:          // request has no payload
    case CommandType::set_full_step:               // request has no payload
    case CommandType::set_half_step:               // request has no payload
    case CommandType::check_if_moving:             // request has no payload
    case CommandType::get_firmware_version:        // request has no payload
    case CommandType::get_speed:                   // request has no payload
    case CommandType::get_temperature:             // request has no payload
    case CommandType::get_temperature_coefficient: // request has no payload
    case CommandType::stop:                        // request has no payload
      break;
    default:
      break;
    }
    // Only setters carry payloads; all other requests have none
    if (cmd == CommandType::set_current_position || cmd == CommandType::set_new_position)
      return 4; // PPPP
    return 0;   // most requests
  }

  /**
   * Translate a two-character Moonlite opcode to the matching enum value.
   *
   * @param buffer Pointer to at least two opcode characters.
   * @return Matching `CommandType`, or `unrecognized` if the opcode is not known.
   */
  inline CommandType strToCommandType(const char *buffer)
  {
    if (buffer == NULL)
    {
      return CommandType::unrecognized;
    }

    const uint16_t opcode = (static_cast<uint16_t>(static_cast<uint8_t>(buffer[0])) << 8) |
                            static_cast<uint8_t>(buffer[1]);

    switch (opcode)
    {
    case ('F' << 8) | 'Q':
      return CommandType::stop;
    case ('G' << 8) | 'P':
      return CommandType::get_current_position;
    case ('S' << 8) | 'P':
      return CommandType::set_current_position;
    case ('G' << 8) | 'N':
      return CommandType::get_new_position;
    case ('S' << 8) | 'N':
      return CommandType::set_new_position;
    case ('F' << 8) | 'G':
      return CommandType::go_to_new_position;
    case ('G' << 8) | 'H':
      return CommandType::check_if_half_step;
    case ('S' << 8) | 'F':
      return CommandType::set_full_step;
    case ('S' << 8) | 'H':
      return CommandType::set_half_step;
    case ('G' << 8) | 'I':
      return CommandType::check_if_moving;
    case ('G' << 8) | 'V':
      return CommandType::get_firmware_version;
    case ('G' << 8) | 'D':
      return CommandType::get_speed;
    case ('S' << 8) | 'D':
      return CommandType::set_speed;
    case ('G' << 8) | 'T':
      return CommandType::get_temperature;
    case ('G' << 8) | 'C':
      return CommandType::get_temperature_coefficient;
    default:
      break;
    }
    return CommandType::unrecognized;
  }

  /**
   * Generic device handler interface.
   *
   * Implement this interface on the firmware side to connect the Moonlite
   * protocol parser to your hardware control logic. The parser is responsible
   * for:
   *   - framing (":" prefix and "#" terminator),
   *   - opcode decoding and payload length validation,
   *   - converting hexadecimal payloads to native C++ types and back,
   *   - formatting response payloads (without the leading ':'),
   * so your implementation focuses purely on device behavior.
   *
   * Important notes:
   *   - Methods that conceptually "set" values do not return responses; the
   *     parser returns no payload for those commands.
   *   - Methods that conceptually "get" values return the current value in a
   *     native type; the parser serializes them to the protocol's hex strings.
   *   - All positions are absolute step counts (0..65535) represented as 4 hex
   *     digits on the wire (PPPP).
   *   - Speed is a delay multiplier byte (SS) used by the protocol; you can map
   *     it to your driver configuration however you prefer.
   */
  class Handler
  {
  public:
    virtual ~Handler() {}

    /**
     * Handle emergency stop command (FQ).
     * Should cancel any ongoing move and release/disable the motor driver
     * if applicable. No response payload is expected by the protocol.
     */
    virtual void stop() = 0;

    /**
     * Get the current stored absolute position (GP).
     * Return value is an absolute step count in the 0..65535 range.
     */
    virtual uint16_t getCurrentPosition() = 0;

    /**
     * Set the stored absolute position without moving the motor (SP).
     * The provided value is an absolute step count. This should adjust the
     * internal coordinate system only.
     */
    virtual void setCurrentPosition(uint16_t) = 0;

    /**
     * Get the staged/pending target position (GN).
     * Return value is the absolute step target last provided via setNewPosition.
     */
    virtual uint16_t getNewPosition() = 0;

    /**
     * Stage a new absolute target position (SN).
     * This must not start motion; it only updates the pending target.
     */
    virtual void setNewPosition(uint16_t) = 0;

    /**
     * Begin motion towards the staged target (FG).
     * This should start the move asynchronously if supported.
     */
    virtual void goToNewPosition() = 0;

    /**
     * Report whether half-step mode is active (GH).
     * Returns true to produce an "FF#" response, false for "00#".
     */
    virtual bool isHalfStep() = 0;

    /**
     * Set the microstep mode (SF/SH).
     * true => half-step (SH), false => full-step (SF).
     */
    virtual void setHalfStep(bool enabled) = 0;

    /**
     * Report whether the focuser is currently moving (GI).
     * Returns true to produce "01#", false for "00#".
     */
    virtual bool isMoving() = 0;

    /**
     * Provide a firmware version string (GV).
     * Do not include the trailing '#'; the parser appends it.
     * The string can be any implementation-defined content (e.g., "v1.0.0").
     */
    virtual std::string getFirmwareVersion() = 0; // without trailing '#'

    /**
     * Get the current speed multiplier (GD).
     * Return the protocol's SS byte which scales the base inter-step delay
     * (each unit adds 500 microseconds in the reference behavior).
     */
    virtual uint8_t getSpeed() = 0;

    /**
     * Set the speed multiplier (SD).
     * The value corresponds to the protocol's SS byte; map it to your driver
     * configuration as appropriate for your hardware.
     */
    virtual void setSpeed(uint8_t) = 0;

    /**
     * Return the temperature reading (GT).
     * Return value is implementation-defined and serialized as 4 hex digits.
     * If no sensor exists, return 0 (the parser will produce "0000#").
     */
    virtual uint16_t getTemperature() = 0; // implementation-defined; 0 if N/A

    /**
     * Get temperature coefficient raw byte (GC).
     * Returns the two's-complement encoded int8_t value multiplied by 2, as an
     * unsigned byte to preserve bit pattern on the wire. The parser will format
     * this as two uppercase hex digits (e.g., FF#, 01#, 00#).
     */
    virtual uint8_t getTemperatureCoefficientRaw() = 0;
  };

  inline std::string hex2(uint8_t v)
  {
    char buf[3];
    static const char *digits = "0123456789ABCDEF";
    buf[0] = digits[(v >> 4) & 0x0F];
    buf[1] = digits[v & 0x0F];
    buf[2] = '\0';
    return std::string(buf);
  }

  inline std::string hex4(uint16_t v)
  {
    char buf[5];
    static const char *digits = "0123456789ABCDEF";
    buf[0] = digits[(v >> 12) & 0x0F];
    buf[1] = digits[(v >> 8) & 0x0F];
    buf[2] = digits[(v >> 4) & 0x0F];
    buf[3] = digits[v & 0x0F];
    buf[4] = '\0';
    return std::string(buf);
  }

  inline uint16_t parseHex4(const std::string &s)
  {
    unsigned v = 0;
    for (char c : s)
    {
      v <<= 4;
      if (c >= '0' && c <= '9')
        v |= (c - '0');
      else if (c >= 'A' && c <= 'F')
        v |= (c - 'A' + 10);
      else if (c >= 'a' && c <= 'f')
        v |= (c - 'a' + 10);
    }
    return static_cast<uint16_t>(v & 0xFFFF);
  }

  inline uint8_t parseHex2(const std::string &s)
  {
    unsigned v = 0;
    for (char c : s)
    {
      v <<= 4;
      if (c >= '0' && c <= '9')
        v |= (c - '0');
      else if (c >= 'A' && c <= 'F')
        v |= (c - 'A' + 10);
      else if (c >= 'a' && c <= 'f')
        v |= (c - 'a' + 10);
    }
    return static_cast<uint8_t>(v & 0xFF);
  }

  /**
   * Streaming Moonlite protocol parser.
   *
   * Usage:
   *   - Construct with a command handler callback that returns the response payload (without trailing '#').
   *   - Feed incoming bytes via feed(c, outResponse).
   *   - When a full, valid frame is parsed, feed returns true and fills outResponse.
   *     The outResponse already includes the trailing '#'.
   *   - For commands that require no response, the handler may return an empty string; in that case
   *     feed will return true with outResponse empty.
   */
  class Parser
  {
  public:
    explicit Parser(Handler &handler) : _handler(&handler) { reset(); }

    // Feed a single character. Returns true when a command has been parsed (valid or not).
    // If a valid command was parsed, outResponse contains the response payload with a trailing '#'.
    // If invalid, outResponse is empty and handler is not called.
    bool feed(char c, std::string &outResponse)
    {
      outResponse.clear();

      // Start of frame always resets state.
      if (c == ':')
      {
        reset();
        _state = State::ReadingOpcode;
        return false;
      }

      // Ignore CR/LF when idle
      if (_state == State::Idle)
      {
        return false;
      }

      if (_state == State::ReadingOpcode)
      {
        _buf.push_back(c);
        if (_buf.size() == 2)
        {
          _cmd = strToCommandType(_buf.c_str());
          _buf.clear();
          _state = State::ReadingPayload;
        }
        return false;
      }

      if (_state == State::ReadingPayload)
      {
        if (c != '#')
        {
          _buf.push_back(c);
          // Optional: enforce a reasonable maximum payload length to protect memory
          if (_buf.size() > 16)
          {
            // Too long => invalidate frame
            reset();
            return false;
          }
          return false;
        }

        // End of frame reached, validate
        const int expected = expectedPayloadLength(_cmd);
        if (_cmd == CommandType::unrecognized)
        {
          reset();
          return true; // parsed (but invalid); no response
        }

        if (expected >= 0)
        {
          if (static_cast<int>(_buf.size()) != expected)
          {
            reset();
            return true; // wrong length; no response
          }
          // Validate hex payload when expected > 0
          if (expected > 0)
          {
            for (char ch : _buf)
            {
              if (!isHexChar(ch))
              {
                reset();
                return true; // invalid chars; no response
              }
            }
          }
        }

        // Valid command => invoke handler
        std::string respPayload;
        if (_handler)
        {
          respPayload = handleCommand(_cmd, _buf);
        }

        reset();

        if (!respPayload.empty())
        {
          outResponse = respPayload;
          outResponse.push_back('#');
        }
        return true;
      }

      return false;
    }

    void reset()
    {
      _buf.clear();
      _cmd = CommandType::unrecognized;
      _state = State::Idle;
    }

  private:
    enum class State : uint8_t
    {
      Idle,
      ReadingOpcode,
      ReadingPayload
    };

    static inline bool isHexChar(char c)
    {
      return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
    }

    Handler *_handler;
    State _state{State::Idle};
    CommandType _cmd{CommandType::unrecognized};
    std::string _buf; // accumulates opcode then payload

    std::string handleCommand(CommandType cmd, const std::string &payload)
    {
      switch (cmd)
      {
      case CommandType::stop:
        _handler->stop();
        return std::string();
      case CommandType::get_current_position:
        return hex4(_handler->getCurrentPosition());
      case CommandType::set_current_position:
        _handler->setCurrentPosition(parseHex4(payload));
        return std::string();
      case CommandType::get_new_position:
        return hex4(_handler->getNewPosition());
      case CommandType::set_new_position:
        _handler->setNewPosition(parseHex4(payload));
        return std::string();
      case CommandType::go_to_new_position:
        _handler->goToNewPosition();
        return std::string();
      case CommandType::check_if_half_step:
        return _handler->isHalfStep() ? std::string("FF") : std::string("00");
      case CommandType::set_full_step:
        _handler->setHalfStep(false);
        return std::string();
      case CommandType::set_half_step:
        _handler->setHalfStep(true);
        return std::string();
      case CommandType::check_if_moving:
        return _handler->isMoving() ? std::string("01") : std::string("00");
      case CommandType::get_firmware_version:
        return _handler->getFirmwareVersion();
      case CommandType::get_speed:
        return hex2(_handler->getSpeed());
      case CommandType::set_speed:
        _handler->setSpeed(parseHex2(payload));
        return std::string();
      case CommandType::get_temperature:
        return hex4(_handler->getTemperature());
      case CommandType::get_temperature_coefficient:
        return hex2(_handler->getTemperatureCoefficientRaw());
      default:
        break;
      }
      return std::string();
    }
  };

} // namespace moonlite

#endif

#endif /* Legacy Moonlite.h retained for reference */