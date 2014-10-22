// -*- tab-width: 4; Mode: C++; c-basic-offset: 4; indent-tabs-mode: nil -*-

#include <AP_Gear.h>
#include <AP_Relay.h>
#include <AP_Math.h>
#include <RC_Channel.h>
#include <AP_Notify.h>
#include <AP_HAL.h>

extern const AP_HAL::HAL& hal;

const AP_Param::GroupInfo AP_Gear::var_info[] PROGMEM = {

    // @Param: ENABLED
    // @DisplayName: Parachute release enabled or disabled
    // @Description: Parachute release enabled or disabled
    // @Values: 0:Disabled,1:Enabled
    // @User: Standard
    AP_GROUPINFO("ENABLED", 0, AP_Gear, _enabled, 0),

    // @Param: SERVO_ON
    // @DisplayName: Parachute Servo ON PWM value
    // @Description: Parachute Servo PWM value when parachute is released
    // @Range: 1000 2000
    // @Units: pwm
    // @Increment: 1
    // @User: Standard
    AP_GROUPINFO("SERVO_ON", 1, AP_Gear, _servo_on_pwm, AP_GEAR_SERVO_ON_PWM_DEFAULT),

    // @Param: SERVO_OFF
    // @DisplayName: Servo OFF PWM value
    // @Description: Parachute Servo PWM value when parachute is not released
    // @Range: 1000 2000
    // @Units: pwm
    // @Increment: 1
    // @User: Standard
    AP_GROUPINFO("SERVO_OFF", 2, AP_Gear, _servo_off_pwm, AP_GEAR_SERVO_OFF_PWM_DEFAULT),

    // @Param: ALT_MIN
    // @DisplayName: Altitude at which the gear is either retracted or released
    // @Description: The vehicle has to cross this height limit for at least 3 seconds for the landing gear to be activated. 0 to disable alt check.
    // @Range: 0 100
    // @Units: cm
    // @Increment: 1
    // @User: Standard
    AP_GROUPINFO("ALT_MIN", 3, AP_Gear, _alt_min, AP_GEAR_ALT_MIN_DEFAULT),

    // @Param: SPEED_MAX
    // @DisplayName: 
    // @Description: Maximum speed the vehicle has to be below of before the landing gear is released. 0 to disable speed check.
    // @Range: 0 100
    // @Units: cm/s
    // @Increment: 1
    // @User: Standard
    AP_GROUPINFO("SPEED_MAX", 4, AP_Gear, _speed_max, AP_GEAR_SPEED_MAX_DEFAULT),

    AP_GROUPEND
};

/// enabled - enable or disable parachute release
void AP_Gear::enabled(bool on_off)
{
    _enabled = on_off;

   // Log_Write_Event(_enabled?DATA_GEAR_ENABLED:DATA_GEAR_DISABLED);
}

void AP_Gear::autonomous()
{
    // exit immediately if not enabled
    if (_enabled <= 0) {
        return;
    }
    _gearmode = 2;
   // Log_Write_Event(DATA_GEAR_AUTO);
}

/// release - release landing gear
void AP_Gear::release()
{
    // exit immediately if not enabled
    if (_enabled <= 0) {
        return;
    }
    _retract = false;
    _gearmode = 1;
}

/// retract - retract landing gear
void AP_Gear::retract()
{
    // exit immediately if not enabled
    if (_enabled <= 0) {
        return;
    }
    _retract = true;
    _gearmode = 0;
}

/// update - shuts off the trigger should be called at about 10hz
void AP_Gear::update(int8_t control_mode)
{
    // exit immediately if not enabled or parachute not to be released
    if (_enabled <= 0) {
        return;
    }

    if (!RC_Channel_aux::function_assigned(RC_Channel_aux::k_landinggear)) {
        return;
    }

    if (_gearmode == 2) {

        // get current altitude in meters
        float curr_alt = _inav->get_altitude();

        // auto retract / release based on altitude
        if (_alt_min > 0) {

            if (_released) {
                // legs down

                if (curr_alt > _alt_min) {
                    // move up?

                    if (_switch_time == 0) {
                        _switch_time = hal.scheduler->millis();
                    } else if (hal.scheduler->millis() - _switch_time > AP_GEAR_AUTO_TIMEOUT_DEFAULT) {
                        _retract = true;
                    }
                } else _switch_time = 0;
            } else {
                // legs up

                if (curr_alt < _alt_min) {
                    // move down?

                    float curr_speed = _inav->get_velocity_xy();
                    // not if too fast!
                    if (curr_speed <= _speed_max || _speed_max == 0) {
                        if (_switch_time == 0) {
                            _switch_time = hal.scheduler->millis();
                        } else if (hal.scheduler->millis() - _switch_time > AP_GEAR_AUTO_TIMEOUT_DEFAULT) {
                            _retract = false;
                        }
                    } else _switch_time = 0;

                } else _switch_time = 0;
            }
        }

        // never retract when not armed
        if (AP_Notify::flags.armed <= 0) {
            _retract = false;
        }
    }

    // check for landing modes (RTL, Land)
    switch (control_mode) {
//        case 6: //RTL:
        case 9: //LAND:
            _retract = false;
            break;
    }
    
    // check for failsafe condition
    if (AP_Notify::flags.failsafe_radio > 0|| AP_Notify::flags.failsafe_battery > 0 || 
        AP_Notify::flags.failsafe_gps > 0) {
        _retract = false;
    }
    
    if (_released && _retract) {
        RC_Channel_aux::set_radio(RC_Channel_aux::k_landinggear, _servo_on_pwm);
        _released = false;
//        Log_Write_Event(DATA_GEAR_UP);
    } else if (!_released && !_retract) {
        RC_Channel_aux::set_radio(RC_Channel_aux::k_landinggear, _servo_off_pwm);
        _released = true;
//        Log_Write_Event(DATA_GEAR_DOWN);
    }
}
