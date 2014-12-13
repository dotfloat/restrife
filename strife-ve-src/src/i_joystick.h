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
//      System-specific joystick interface.
//


#ifndef __I_JOYSTICK__
#define __I_JOYSTICK__

#define JOY_BUTTON_A 0 // Cross
#define JOY_BUTTON_B 1 // Circle
#define JOY_BUTTON_X 2 // Square
#define JOY_BUTTON_Y 3 // Triangle
#define JOY_BUTTON_SELECT 4
#define JOY_BUTTON_HOME 5
#define JOY_BUTTON_START 6
#define JOY_BUTTON_MOVESTICK 7
#define JOY_BUTTON_LOOKSTICK 8
#define JOY_BUTTON_LEFTSHOULDER 9
#define JOY_BUTTON_RIGHTSHOULDER 10
#define JOY_BUTTON_DPAD_UP 11
#define JOY_BUTTON_DPAD_DOWN 12
#define JOY_BUTTON_DPAD_LEFT 13
#define JOY_BUTTON_DPAD_RIGHT 14
#define JOY_NUM_BUTTONS 15

// Pseudo-buttons
#define JOY_PBUTTON_MOVE_UP 15
#define JOY_PBUTTON_MOVE_DOWN 16
#define JOY_PBUTTON_MOVE_LEFT 17
#define JOY_PBUTTON_MOVE_RIGHT 18
#define JOY_PBUTTON_LOOK_UP 19
#define JOY_PBUTTON_LOOK_DOWN 20
#define JOY_PBUTTON_LOOK_LEFT 21
#define JOY_PBUTTON_LOOK_RIGHT 22
#define JOY_PBUTTON_LEFTTRIGGER 23
#define JOY_PBUTTON_RIGHTTRIGGER 24
#define JOY_NUM_PBUTTONS 25

#define JOY_AXIS_MOVE_X 0
#define JOY_AXIS_MOVE_Y 1
#define JOY_AXIS_LOOK_X 2
#define JOY_AXIS_LOOK_Y 3
#define JOY_AXIS_LEFTTRIGGER 4
#define JOY_AXIS_RIGHTTRIGGER 5
#define JOY_NUM_AXIS 6

#define JOY_IS_AXIS(x) ((x) >= JOY_PBUTTON_MOVE_UP && (x) <= JOY_PBUTTON_LOOK_RIGHT)
#define JOY_IS_MOVE_AXIS(x) ((x) >= JOY_PBUTTON_MOVE_UP && (x) <= JOY_PBUTTON_MOVE_RIGHT)
#define JOY_IS_LOOK_AXIS(x) ((x) >= JOY_PBUTTON_LOOK_UP && (x) <= JOY_PBUTTON_LOOK_RIGHT)

// [SVE] svillarreal
extern float joystick_sensitivity;
extern float joystick_threshold;

void I_InitJoystick(void);
void I_ShutdownJoystick(void);
void I_UpdateJoystick(void);
int  I_GetJoystickEventID(void);
int  I_GetJoystickAxisID(int *axisvalue);
void I_BindJoystickVariables(void);

// [SVE] runtime joystick selection
int  I_QueryNumJoysticks(void);
void I_ActivateJoystickDevice(int index);
int  I_QueryActiveJoystickNum(void);
const char *I_QueryJoystickName(int index);
const char *I_QueryActiveJoystickName(void);
boolean     I_JoystickAllowed(void);

// [SVE] some bullcrap
extern boolean i_seejoysticks;

// [SVE] frontend functions
int I_JoystickGetButtons(void);
void I_JoystickGetAxes(int *x_axis, int *y_axis, int *s_axis, int *l_axis);

#endif /* #ifndef __I_JOYSTICK__ */

