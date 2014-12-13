//
// Copyright(C) 2005-2014 Simon Howard
// Copyright(C) 2014 Night Dive Studios, Inc.
//
// This program is free software; you can redistribute it and/or
// modify it under the terms of the GNU General Public License
// as published by the Free Software Foundation; either version 2
// of the License, or (at your option) any later version.
//
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// DESCRIPTION:
//       SDL Joystick code.
//


#include "SDL.h"

#include <stdlib.h>
#include <stdio.h>
#include <string.h>

#include "doomtype.h"
#include "d_event.h"
#include "i_joystick.h"
#include "i_system.h"
#include "i_video.h"

#include "m_config.h"
#include "m_misc.h"
#include "fe_gamepad.h"

// [SVE] svillarreal
#ifdef _USE_STEAM_
#include "steamService.h"
#endif

// [SVE]: Track whether or not we're seeing joystick events.
boolean i_seejoysticks;

// When an axis is within the dead zone, it is set to zero.
// This is 5% of the full range:

#define DEAD_ZONE (32768 / 3)

float joystick_sensitivity = 0.005f;
float joystick_threshold = 10.0f;

static SDL_GameController *joystick = NULL;

// Configuration variables:

// Standard default.cfg Joystick enable/disable

static int usejoystick = 1;
static boolean joystickInit; // true if SDL subsystem init'd

// Joystick to use, as an SDL joystick index:

static int joystick_index = -1;

// Swap left and right sticks for left-handed users.

static int joystick_southpaw = 0;

// Which joystick axis to use for horizontal movement, and whether to
// invert the direction:

static int joystick_x_invert = 0;

// Which joystick axis to use for vertical movement, and whether to
// invert the direction:

static int joystick_y_invert = 0;

// Which joystick axis to use for strafing?

static int joystick_strafe_invert = 0;

// [SVE] svillarreal - joystick axis for looking

static int joystick_look_invert = 0;

// [SVE] svillarreal

static int joystick_oldbuttons = 0;

//
// I_CloseJoystickDevice
//
// haleyjd 20141020: [SVE] separated from I_ShutdownJoystick
//
static void I_CloseJoystickDevice(void)
{
    if(joystick != NULL)
	{
		SDL_GameControllerClose(joystick);
        joystick = NULL;
    }
}

//
// I_ShutdownJoystick
//

void I_ShutdownJoystick(void)
{
    I_CloseJoystickDevice();
    if(joystickInit)
    {
		SDL_QuitSubSystem(SDL_INIT_GAMECONTROLLER);
    }
}

#if 0
static boolean IsValidAxis(int axis)
{
    int num_axes;

    if(axis < 0)
    {
        return true;
    }

    if(IS_BUTTON_AXIS(axis))
    {
        return true;
    }

    if(IS_HAT_AXIS(axis))
    {
        return HAT_AXIS_HAT(axis) < SDL_JoystickNumHats(joystick);
    }

    num_axes = SDL_JoystickNumAxes(joystick);

    return axis < num_axes;
}
#endif

//
// I_InitJoystick
//

void I_InitJoystick(void)
{
    if(!usejoystick)
    {
        return;
    }

    // init subsystem
	if(SDL_Init(SDL_INIT_GAMECONTROLLER) != 0)
    {
        return;
    }

    joystickInit = true; // subsystem initialized
    I_AtExit(I_ShutdownJoystick, true);

    // auto open first device if one is available and nothing is configured
    if(joystick_index == -1 && SDL_NumJoysticks() >= 1)
    {
        joystick_index = 0;
    }

    // open initial device if valid
    I_ActivateJoystickDevice(joystick_index);

    // [SVE] svillarreal - just pick whatever profile is available if
    // no configs are present
    if(extra_config_fresh)
    {
        FE_AutoApplyPadProfile();
    }
}

//
// I_JoystickAllowed
//

boolean I_JoystickAllowed(void)
{
    return usejoystick;
}

//
// I_QueryNumJoysticks
//
// haleyjd 20141020: [SVE] Get number of joysticks
//

int I_QueryNumJoysticks(void)
{
    return SDL_NumJoysticks();
}

//
// I_QueryJoystickName
//
// haleyjd 20141020: [SVE] Get device name at index
//

const char *I_QueryJoystickName(int index)
{
    if(index >= 0 && index < SDL_NumJoysticks())
    {
		return SDL_GameControllerNameForIndex(index);
    }

    return NULL;
}

//
// I_QueryActiveJoystickNum
//

int I_QueryActiveJoystickNum(void)
{
    return joystick_index;
}

//
// I_QueryActiveJoystickName
//

const char *I_QueryActiveJoystickName(void)
{
    return I_QueryJoystickName(joystick_index);
}

//
// I_ActivateJoystickDevice
//
// haleyjd 20141020: [SVE] Activate a selected joystick device.
//

void I_ActivateJoystickDevice(int index)
{
    // close any currently open device
    I_CloseJoystickDevice();

    // set new index
    joystick_index = index;

    // validate
    if(joystick_index < 0 || joystick_index >= SDL_NumJoysticks())
    {
        return;
    }

    // open the device
	if(!(joystick = SDL_GameControllerOpen(joystick_index)))
    {
        return;
    }

    // allow event polling
    SDL_JoystickEventState(SDL_ENABLE);
}

// Get the state of the given button.
static int GetAxisState(int axis, int invert);

static int ReadButtonState(int button)
{
	int axis;
	int sign;

    if(!joystick)
    {
        return 0;
	}

	if(button < 0 || button >= JOY_NUM_PBUTTONS)
    {
        return 0;
	}

	if(button < JOY_NUM_BUTTONS)
	{
		return SDL_GameControllerGetButton(joystick, button);
	} else if(button <= JOY_PBUTTON_LOOK_RIGHT) {
		axis = (button - JOY_PBUTTON_MOVE_UP) / 2;
		sign = ((button - JOY_PBUTTON_MOVE_UP) % 2) * 2 - 1;

		return GetAxisState(axis, 0) >= sign * DEAD_ZONE;
	} else {
		axis = button - JOY_PBUTTON_LEFTTRIGGER + 4;

		return GetAxisState(axis, 0) >= DEAD_ZONE;
	}
}

//
// I_GetJoystickEventID
//
// [SVE] svillarreal - Scans for a joystick event and
// remaps it as a button ID
//

int I_GetJoystickEventID(void)
{
    int i;
    int axis;
    int greatest_absaxisvalue =  0;
    int greatest_axis         = -1;
    int greatest_axissign     =  0;

    if(!joystick)
    {
        return -1;
    }

    // check for button presses
	for(i = 0; i < JOY_NUM_PBUTTONS; ++i)
    {
        if(ReadButtonState(i))
        {
            return i;
        }
    }

    // check for axis movement
	for(i = 0; i < JOY_NUM_AXIS; ++i)
	{
		int axisNdx = i;
		if (joystick_southpaw)
			axisNdx = i ^ 2; // Swap left and right

		axis = SDL_GameControllerGetAxis(joystick, axisNdx);

        if(axis > DEAD_ZONE*2 || axis < -DEAD_ZONE*2)
        {
            int absaxis = abs(axis);
            if(absaxis > greatest_absaxisvalue)
            {
                greatest_absaxisvalue = absaxis;
				greatest_axis         = axisNdx;
                greatest_axissign     = (axis > 0) ? 1 : -1;
            }
        }
    }

	return 0;
}

//
// I_GetJoystickAxisID
//
// [SVE] svillarreal - Returns the invoked joystick axis
//

int I_GetJoystickAxisID(int *axisvalue)
{
    int i;
    int axis;
    int greatest_absaxisvalue =  0;
    int greatest_axis         = -1;

    if(!joystick)
    {
        return -1;
    }

    // check for axis movement
	for(i = 0; i < JOY_NUM_AXIS; ++i)
    {
		axis = SDL_GameControllerGetAxis(joystick, i);

        if(axis > DEAD_ZONE || axis < -DEAD_ZONE)
        {
            int absaxis = abs(axis);
            if(absaxis > greatest_absaxisvalue)
            {
                greatest_absaxisvalue = absaxis;
				greatest_axis         = joystick_southpaw ? i ^ 2 : i;
            }
        }
    }

    if(axisvalue)
        *axisvalue = greatest_absaxisvalue;
    return greatest_axis;
}

// Get a bitmask of all currently-pressed buttons

static int GetButtonsState(void)
{
	int i;
    int result;

    result = 0;

	for(i = 0; i < JOY_NUM_PBUTTONS; ++i)
    {
        if(ReadButtonState(i))
        {
            result |= 1 << i;
        }
    }

    return result;
}

// Read the state of an axis, inverting if necessary.

static int GetAxisState(int axis, int invert)
{
    int result;

    // Axis -1 means disabled.

	if(axis < 0 || axis >= JOY_NUM_AXIS)
		return 0;

	if(joystick_southpaw)
		axis ^= 2;

	result = SDL_GameControllerGetAxis(joystick, axis);

	if (result < DEAD_ZONE && result > -DEAD_ZONE)
		result = 0;

	return invert ? -result : result;
}

//
// I_JoystickGetButtons
//
// haleyjd 20141104: [SVE] return button state directly
//
int I_JoystickGetButtons(void)
{
    int bits;
    int data;
    int ret = 0;

    if(!joystick)
        return 0;
    
    data = GetButtonsState();        

    if(data)
    {
        bits = data;

        bits &= ~joystick_oldbuttons;
        ret = bits;
    }

    joystick_oldbuttons = data;
    return ret;
}

//
// I_JoystickButtonEvent
//

static void I_JoystickButtonEvent(void)
{
    int bits;
    int data;
    int i;
    event_t ev;

    data = GetButtonsState();

    if(data)
    {
        bits = data;

        // check for button press
        bits &= ~joystick_oldbuttons;
		for(i = 0; i < JOY_NUM_PBUTTONS; ++i)
        {
            if(bits & (1 << i))
            {
                ev.type = ev_joybtndown;
                ev.data1 = i;
                D_PostEvent(&ev);
            }
        }
    }

    bits = joystick_oldbuttons;
    joystick_oldbuttons = data;

    // check for button release
    bits &= ~joystick_oldbuttons;
	for(i = 0; i < JOY_NUM_PBUTTONS; ++i)
    {
        if(bits & (1 << i))
        {
            ev.type = ev_joybtnup;
            ev.data1 = i;
            D_PostEvent(&ev);
            i_seejoysticks = true;
            i_seemouses    = false;
        }
    }
}

//
// I_JoystickGetAxes
//
// haleyjd 20141104: [SVE] return axis state directly
//
void I_JoystickGetAxes(int *x_axis, int *y_axis, int *s_axis, int *l_axis)
{
    *x_axis = 0;
    *y_axis = 0;
    *s_axis = 0;
    *l_axis = 0;

    if(joystick)
    {
        if(joystick_sensitivity < 0.001f)
        {
            joystick_sensitivity = 0.001f;
        }

        if(joystick_threshold < 1.0f)
        {
            joystick_threshold = 1.0f;
        }

		*x_axis = GetAxisState(JOY_AXIS_LOOK_X, joystick_x_invert);
		*y_axis = GetAxisState(JOY_AXIS_MOVE_Y, joystick_y_invert);
		*s_axis = GetAxisState(JOY_AXIS_MOVE_X, joystick_strafe_invert);
		*l_axis = GetAxisState(JOY_AXIS_LOOK_Y, joystick_look_invert);
    }
}

//
// I_UpdateJoystick
//

void I_UpdateJoystick(void)
{
    //
    // [SVE] svillarreal
    // clamp cvar values
    //
    if(joystick_sensitivity < 0.001f)
    {
        joystick_sensitivity = 0.001f;
    }

    if(joystick_threshold < 1.0f)
    {
        joystick_threshold = 1.0f;
    }

    // [SVE] svillarreal
#ifdef _USE_STEAM_
    if(I_SteamOverlayActive())
        return;
#endif

    if(joystick != NULL)
    {
		int x = GetAxisState(JOY_AXIS_LOOK_X, joystick_x_invert);
		int y = GetAxisState(JOY_AXIS_MOVE_Y, joystick_y_invert);
		int s = GetAxisState(JOY_AXIS_MOVE_X, joystick_strafe_invert);
		int l = GetAxisState(JOY_AXIS_LOOK_Y, joystick_look_invert);

        if(x || y || s || l)
        {
            event_t ev;

            ev.type = ev_joystick;
            ev.data1 = l;
            ev.data2 = x;
            ev.data3 = y;
            ev.data4 = s;

            D_PostEvent(&ev);
            i_seejoysticks = true; // [SVE]
            i_seemouses    = false;
        }

        I_JoystickButtonEvent();
    }
}

void I_BindJoystickVariables(void)
{
    M_BindVariable("use_joystick",          &usejoystick);
    M_BindVariable("joystick_index",        &joystick_index);
	M_BindVariable("joystick_southpaw",     &joystick_southpaw);
	M_BindVariable("joystick_invert",  &joystick_look_invert);

    M_BindVariable("joystick_sensitivity",   &joystick_sensitivity);
    M_BindVariable("joystick_threshold",     &joystick_threshold);
}
