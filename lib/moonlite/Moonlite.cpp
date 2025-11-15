#include "Moonlite.hpp"

namespace moonlite
{

int expectedPayloadLength(CommandType cmd)
{
  switch (cmd)
  {
  case CommandType::set_speed:
    return 2; // SS
  case CommandType::set_current_position:
  case CommandType::set_new_position:
  case CommandType::get_current_position:
  case CommandType::get_new_position:
  case CommandType::go_to_new_position:
  case CommandType::check_if_half_step:
  case CommandType::set_full_step:
  case CommandType::set_half_step:
  case CommandType::check_if_moving:
  case CommandType::get_firmware_version:
  case CommandType::get_speed:
  case CommandType::get_temperature:
  case CommandType::get_temperature_coefficient:
  case CommandType::stop:
    break;
  default:
    break;
  }

  if (cmd == CommandType::set_current_position || cmd == CommandType::set_new_position)
  {
    return 4; // PPPP
  }

  return 0;
}

CommandType strToCommandType(const char *buffer)
{
  if (buffer == nullptr)
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

std::string hex2(uint8_t v)
{
  char buf[3];
  static const char *digits = "0123456789ABCDEF";
  buf[0] = digits[(v >> 4) & 0x0F];
  buf[1] = digits[v & 0x0F];
  buf[2] = '\0';
  return std::string(buf);
}

std::string hex4(uint16_t v)
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

uint16_t parseHex4(const std::string &s)
{
  unsigned v = 0;
  for (char c : s)
  {
    v <<= 4;
    if (c >= '0' && c <= '9')
    {
      v |= (c - '0');
    }
    else if (c >= 'A' && c <= 'F')
    {
      v |= (c - 'A' + 10);
    }
    else if (c >= 'a' && c <= 'f')
    {
      v |= (c - 'a' + 10);
    }
  }
  return static_cast<uint16_t>(v & 0xFFFF);
}

uint8_t parseHex2(const std::string &s)
{
  unsigned v = 0;
  for (char c : s)
  {
    v <<= 4;
    if (c >= '0' && c <= '9')
    {
      v |= (c - '0');
    }
    else if (c >= 'A' && c <= 'F')
    {
      v |= (c - 'A' + 10);
    }
    else if (c >= 'a' && c <= 'f')
    {
      v |= (c - 'a' + 10);
    }
  }
  return static_cast<uint8_t>(v & 0xFF);
}

Parser::Parser(Handler &handler) : _handler(&handler)
{
  reset();
}

bool Parser::feed(char c, std::string &outResponse)
{
  outResponse.clear();

  if (c == ':')
  {
    reset();
    _state = State::ReadingOpcode;
    return false;
  }

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
      if (_buf.size() > 16)
      {
        reset();
        return false;
      }
      return false;
    }

    const int expected = expectedPayloadLength(_cmd);
    if (_cmd == CommandType::unrecognized)
    {
      reset();
      return true;
    }

    if (expected >= 0)
    {
      if (static_cast<int>(_buf.size()) != expected)
      {
        reset();
        return true;
      }

      if (expected > 0)
      {
        for (char ch : _buf)
        {
          if (!isHexChar(ch))
          {
            reset();
            return true;
          }
        }
      }
    }

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

void Parser::reset()
{
  _buf.clear();
  _cmd = CommandType::unrecognized;
  _state = State::Idle;
}

bool Parser::isHexChar(char c)
{
  return (c >= '0' && c <= '9') || (c >= 'A' && c <= 'F') || (c >= 'a' && c <= 'f');
}

std::string Parser::handleCommand(CommandType cmd, const std::string &payload)
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

} // namespace moonlite
