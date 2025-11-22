# Moonlite Serial Protocol

This document summarizes the serial protocol understood by Moonlite-compatible focus controllers, based on the reference implementation in this project.

## Frame Format

Every transaction uses an ASCII frame:

```
:<opcode><payload>#
```

- `:` marks the beginning of the frame.
- `<opcode>` is a two-character command identifier (upper-case letters).
- `<payload>` is an optional ASCII-encoded value whose meaning depends on the command.
- `#` terminates the frame.

Controller responses use the same framing convention. Unless noted otherwise, commands that report data reply with a frame that mirrors the request structure.

## Numeric Encoding

Numeric payloads are sent as fixed-width uppercase hexadecimal strings:

- Positions (`PPPP`) are four hex digits representing an absolute step count.
- Speed values (`SS`) are two hex digits that scale the 500 microsecond base delay between motor steps.
- Temperature readings (`TTTT`) are four hex digits (implementation placeholder in the current firmware).

## Command Reference

| Opcode | Request Description | Payload | Response | Notes |
| --- | --- | --- | --- | --- |
| `FQ` | Stop motion and release the motor driver | none | none | Emergency stop for any active move |
| `GP` | Get current stored absolute position | none | `PPPP#` | Returns the last position registered with the controller |
| `SP` | Set current stored absolute position | `PPPP` | none | Updates internal coordinate without moving the motor |
| `GN` | Get pending target position | none | `PPPP#` | Reads the staged target set by `SN` |
| `SN` | Stage new target position | `PPPP` | none | Stores a target move without starting motion |
| `FG` | Execute staged move | none | none | Begins motion toward the last value supplied via `SN` |
| `GH` | Check half-step status | none | `FF#` or `00#` | `FF#` means half-step mode active; `00#` means full-step |
| `SF` | Switch to full-step mode | none | none | Sets driver microstepping to full-step |
| `SH` | Switch to half-step mode | none | none | Sets driver microstepping to half-step |
| `GI` | Check motion status | none | `01#` or `00#` | `01#` indicates the focuser is moving |
| `GV` | Read firmware version | none | implementation-defined | Example: `v1.0.0#` |
| `GD` | Get speed multiplier | none | `SS#` | `SS` scales the 500 microsecond base delay |
| `SD` | Set speed multiplier | `SS` | none | Larger values slow the move by increasing inter-step delay |
| `GT` | Get temperature reading | none | `TTTT#` | Placeholder response (`0000#`) unless hardware reports temperature |

## Error Handling

Unrecognized opcodes yield a controller response determined by the implementation; the firmware in this project reports them as `CommandType::unrecognized`. Clients should treat any unexpected response as a protocol error.

## Implementation Notes

- Commands that manipulate positions (`SN`, `FG`, `SP`) work with absolute coordinates; relative moves must be calculated client-side.
- Switching microstep modes does not retroactively adjust stored positions. Ensure the host software accounts for step size changes.
- Moves initiated with `FG` run asynchronously. Poll `GI` to observe completion or time out on the host side.
