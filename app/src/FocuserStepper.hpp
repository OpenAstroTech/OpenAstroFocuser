#pragma once

#include <cstdint>

// Abstracts the underlying stepper controller so Focuser logic can be reused
// outside Zephyr or with mocks in tests. Methods mirror the Zephyr stepper API
// and should return the same errno-style values.
class FocuserStepper
{
public:
    virtual ~FocuserStepper() = default;

    // Returns true when both the stepper controller and driver can be used.
    virtual bool is_ready() const = 0;

    // Re-bases the reported position so the physical location matches firmware state.
    virtual int set_reference_position(int32_t position) = 0;

    // Updates the microstep interval in nanoseconds; smaller values move faster.
    virtual int set_microstep_interval(uint64_t interval_ns) = 0;

    // Begins motion toward the requested target position.
    virtual int move_to(int32_t target) = 0;

    // Queries whether the controller is currently moving.
    virtual int is_moving(bool &moving) = 0;

    // Immediately stops any active motion.
    virtual int stop() = 0;

    // Reads the actual position reported by the controller.
    virtual int get_actual_position(int32_t &position) = 0;

    // Enables or disables the external stepper driver if present.
    virtual int enable_driver(bool enable) = 0;
};
