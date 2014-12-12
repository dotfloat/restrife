//
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
//    Configuration and netgame negotiation frontend for
//    Strife: Veteran Edition
//
// AUTHORS:
//    James Haley
//

#include "SDL.h"

#include <stdlib.h>
#include <string.h>

#include "doomtype.h"
#include "hu_lib.h"
#include "i_joystick.h"
#include "i_video.h"
#include "m_config.h"
#include "m_menu.h"
#include "m_misc.h"
#include "sounds.h"
#include "s_sound.h"

#include "fe_frontend.h"
#include "fe_graphics.h"
#include "fe_menuengine.h"
#include "fe_gamepad.h"

static void FE_updateGamepadMenu(void);

//=============================================================================
//
// Device Selection Menu
//

static void FE_DrawGamepadDevMenu(void)
{
    char buf[256];
    const char *devName = I_QueryActiveJoystickName();
    if(!devName)
        devName = "None";

    M_snprintf(buf, sizeof(buf), "Current Device: %s", devName);
    M_DialogDimMsg(8, 180, buf, true);
    HUlib_drawYellowText(8, 180, buf, true);
}

static femenuitem_t gamepadDevItems[16];

static femenu_t gamepadDevMenu = 
{
    gamepadDevItems,
    arrlen(gamepadDevItems),
    40,
    30,
    4,
    "Gamepad Devices",
    FE_BG_RSKULL,
    FE_DrawGamepadDevMenu,
    FE_CURSOR_LASER,
    0,
    false,
    false
};

//
// Rebuild the gamepad device selection menu
//
static void FE_RebuildGamepadDevMenu(void)
{
    int i;
    int numPads = I_QueryNumJoysticks();

    for(i = 0; i < arrlen(gamepadDevItems); i++)
    {
        femenuitem_t *item = &gamepadDevItems[i];

        if(item->description && *item->description)
        {
            free((void *)item->description);
            item->description = "";
        }
        if(item->verb && *item->verb)
        {
            free((void *)item->verb);
            item->verb = "";
        }
        item->type = FE_MITEM_END;
    }

    if(!numPads)
    {
        gamepadDevItems[0].type        = FE_MITEM_CMD;
        gamepadDevItems[0].description = M_Strdup("No gamepads available.");
        gamepadDevItems[0].verb        = M_Strdup("back");
    }
    else
    {
        if(numPads > 15)
            numPads = 15; // sorry, *ONLY* 15 gamepads :P

        for(i = 0; i < numPads; i++)
        {
            femenuitem_t *item = &gamepadDevItems[i];
            const char   *name = I_QueryJoystickName(i);
            char *end;
            int trunc = 0;

            item->type        = FE_MITEM_CMD;
            item->description = M_Strdup((name && *name) ? name : "Unknown Device");
            item->verb        = M_Strdup("selgamepad");
            item->data        = i;

            end = (char *)(item->description + strlen(item->description) - 1);
            while(end != item->description &&
                M_StringWidth(item->description) > SCREENWIDTH - 80)
            {
                *end-- = '\0';
                ++trunc;
            }
            if(trunc >= 3)
                strcat((char *)item->description, "...");
        }
    }

    if(gamepadDevItems[gamepadDevMenu.itemon].type == FE_MITEM_END)
        gamepadDevMenu.itemon = 0;
}

// profile to apply if user selects to do so
static int fe_padprofile;

// "selgamepad" command
void FE_CmdSelGamepad(void)
{
    int dev = gamepadDevItems[gamepadDevMenu.itemon].data;

    if(I_JoystickAllowed())
    {
        I_ActivateJoystickDevice(dev);
        S_StartSound(NULL, sfx_swtchn);

        // update gamepad menu with regard to profile state
        FE_updateGamepadMenu();
    }
}

// "gamepaddev" command
void FE_CmdGamepadDev(void)
{
    if(I_JoystickAllowed())
    {
        FE_RebuildGamepadDevMenu();
        FE_PushMenu(&gamepadDevMenu);
    }
    else
        S_StartSound(NULL, sfx_oof);
}

//=============================================================================
//
// Axis Configuration Menu
//

static femenuitem_t gamepadAxisItems[] =
{
    { FE_MITEM_ABIND,  "Move Forward/Back",  "joystick_y_axis"        },
    { FE_MITEM_ABIND,  "Turn Left/Right",    "joystick_x_axis"        },
    { FE_MITEM_ABIND,  "Strafe Left/Right",  "joystick_strafe_axis"   },
    { FE_MITEM_ABIND,  "Look Up/Down",       "joystick_look_axis"     },
    { FE_MITEM_TOGGLE, "Invert Move Axis",   "joystick_y_invert"      },
    { FE_MITEM_TOGGLE, "Invert Turn Axis",   "joystick_x_invert"      },
    { FE_MITEM_TOGGLE, "Invert Strafe Axis", "joystick_strafe_invert" },
    { FE_MITEM_TOGGLE, "Invert Look Axis",   "joystick_look_invert"   },
    { FE_MITEM_END, "", "" }
};

static femenu_t gamepadAxisMenu =
{
    gamepadAxisItems,
    arrlen(gamepadAxisItems),
    65,
    40,
    4,
    "Gamepad Axes",
    FE_BG_RSKULL,
    NULL,
    FE_CURSOR_LASER,
    0,
    false,
    false
};

// "gpaxes" command
void FE_CmdGPAxes(void)
{
    FE_PushMenu(&gamepadAxisMenu);
}

//=============================================================================
//
// Automap Buttons Menu
//

static femenuitem_t gamepadAutomapItems[] =
{
    { FE_MITEM_JBIND, "Toggle Map View", "joybmap_toggle"    },
    { FE_MITEM_JBIND, "Scroll North",    "joybmap_north"     },
    { FE_MITEM_JBIND, "Scroll South",    "joybmap_south"     },
    { FE_MITEM_JBIND, "Scroll East",     "joybmap_east"      },
    { FE_MITEM_JBIND, "Scroll West",     "joybmap_west"      },
    { FE_MITEM_JBIND, "Zoom In",         "joybmap_zoomin"    },
    { FE_MITEM_JBIND, "Zoom Out",        "joybmap_zoomout"   },
    { FE_MITEM_JBIND, "Follow Mode",     "joybmap_follow"    },
    { FE_MITEM_JBIND, "Mark Spot",       "joybmap_mark"      },
    { FE_MITEM_JBIND, "Clear Last Mark", "joybmap_clearmark" },
    { FE_MITEM_END, "", "" }
};

static femenu_t gamepadAutomapMenu =
{
    gamepadAutomapItems,
    arrlen(gamepadAutomapItems),
    70,
    40,
    4,
    "Gamepad Automap",
    FE_BG_RSKULL,
    NULL,
    FE_CURSOR_LASER,
    0,
    false,
    false
};

// "gpautomap" command
void FE_CmdGPAutomap(void)
{
    FE_PushMenu(&gamepadAutomapMenu);
}

//=============================================================================
//
// Menu Buttons
//

static femenuitem_t gamepadMenuButtonItems[] =
{
    { FE_MITEM_JBIND, "Toggle Menus",   "joyb_menu_activate" },
    { FE_MITEM_JBIND, "Previous Item",  "joyb_menu_up"       },
    { FE_MITEM_JBIND, "Next Item",      "joyb_menu_down"     },
    { FE_MITEM_JBIND, "Decrease Value", "joyb_menu_left"     },
    { FE_MITEM_JBIND, "Increase Value", "joyb_menu_right"    },
    { FE_MITEM_JBIND, "Go Back",        "joyb_menu_back"     },
    { FE_MITEM_JBIND, "Activate Item",  "joyb_menu_forward"  },
    { FE_MITEM_JBIND, "Answer Yes",     "joyb_menu_confirm"  },
    { FE_MITEM_JBIND, "Answer No",      "joyb_menu_abort"    },
    { FE_MITEM_END, "", "" }
};

static femenu_t gamepadMenuButtonsMenu =
{
    gamepadMenuButtonItems,
    arrlen(gamepadMenuButtonItems),
    70,
    40,
    4,
    "Gamepad Menu",
    FE_BG_RSKULL,
    NULL,
    FE_CURSOR_LASER,
    0,
    false,
    false
};

// "gpmenus" command
void FE_CmdGPMenus(void)
{
    FE_PushMenu(&gamepadMenuButtonsMenu);
}

//=============================================================================
//
// Movement Menu
//

static femenuitem_t gamepadMovementItems[] =
{
    { FE_MITEM_JBIND, "Strafe Left",     "joyb_strafeleft"  },
    { FE_MITEM_JBIND, "Strafe Right",    "joyb_straferight" },
    { FE_MITEM_JBIND, "Strafe On",       "joyb_strafe"      },
    { FE_MITEM_JBIND, "Attack",          "joyb_fire"        },
    { FE_MITEM_JBIND, "Use / Activate",  "joyb_use"         },
    { FE_MITEM_JBIND, "Run",             "joyb_speed"       },
    { FE_MITEM_JBIND, "Jump",            "joyb_jump"        },
    { FE_MITEM_JBIND, "Previous Weapon", "joyb_prevweapon"  },
    { FE_MITEM_JBIND, "Next Weapon",     "joyb_nextweapon"  },
    { FE_MITEM_END, "", "" }
};

static femenu_t gamepadMovementMenu =
{
    gamepadMovementItems,
    arrlen(gamepadMovementItems),
    70,
    40,
    4,
    "Gamepad Movement",
    FE_BG_RSKULL,
    NULL,
    FE_CURSOR_LASER,
    0,
    false,
    false
};

// "gpmovement" command
void FE_CmdGPMovement(void)
{
    FE_PushMenu(&gamepadMovementMenu);
}

//=============================================================================
// 
// Inventory Buttons
//

static femenuitem_t gamepadInvItems[] =
{
    { FE_MITEM_JBIND, "Scroll Left",       "joyb_invleft"   },
    { FE_MITEM_JBIND, "Scroll Right",      "joyb_invright"  },
    { FE_MITEM_JBIND, "Use Current Item",  "joyb_invuse"    },
    { FE_MITEM_JBIND, "Drop Current Item", "joyb_invdrop"   },
    { FE_MITEM_JBIND, "View Status",       "joyb_invpop"    },
    { FE_MITEM_JBIND, "View Mission",      "joyb_mission"   },
    { FE_MITEM_JBIND, "View Keys",         "joyb_invkey"    },
    { FE_MITEM_END, "", "" }
};

static femenu_t gamepadInvMenu =
{
    gamepadInvItems,
    arrlen(gamepadInvItems),
    70,
    40,
    4,
    "Gamepad Inventory",
    FE_BG_RSKULL,
    NULL,
    FE_CURSOR_LASER,
    0,
    false,
    false
};

// "gpinv" command
void FE_CmdGPInv(void)
{
    FE_PushMenu(&gamepadInvMenu);
}

//=============================================================================
//
// Axis Binding
//

// improbably high number
#define FE_IGNORE_LIMIT 100 

// current ABIND item
static femenuitem_t *fe_joyaitem;

// time to wait before input
static int fe_joyawaittics;

// time to wait before aborting binding
static int fe_joyatimeout;

// axes to ignore due to noise
static byte fe_ignoreaxes[FE_IGNORE_LIMIT];

void FE_JoyAxisResponder(void)
{
    // ignoring input?
    if(fe_joyawaittics)
    {
        int i = I_GetJoystickAxisID(NULL);

        if(i >= 0 && i < FE_IGNORE_LIMIT)
            fe_ignoreaxes[i] = 1;
    }
    else
    {
        char buf[33];
        int i = I_GetJoystickAxisID(NULL);
        int curVal = M_GetIntVariable(fe_joyaitem->verb);

        if(i >= 0 && i < FE_IGNORE_LIMIT && fe_ignoreaxes[i])
            return;

        if(i >= 0)
        {
            if(i != curVal)
            {
                // set new value
                M_SetVariable(fe_joyaitem->verb, M_Itoa(i, buf, 10));
            }
            else
            {
                // clear binding
                M_SetVariable(fe_joyaitem->verb, "-1");
            }
            frontend_state = FE_STATE_MAINMENU;
            S_StartSound(NULL, sfx_swtchn);
        }
    }
}

void FE_JoyAxisTicker(void)
{
    // count down wait timer
    if(fe_joyawaittics > 0)
    {
        --fe_joyawaittics;
        return;
    }

    // time out?
    if(fe_joyatimeout > 0)
    {
        --fe_joyatimeout;
        if(fe_joyatimeout == 0)
        {
            frontend_state = FE_STATE_MAINMENU;
            S_StartSound(NULL, sfx_mtalht);
        }
    }
}

//
// Start process of binding a gamepad axis
//
void FE_JoyAxisBindStart(femenuitem_t *item)
{
    frontend_state  = FE_STATE_JAINPUT;
    fe_joyawaittics = 3 * frontend_fpslimit / 2;
    fe_joyatimeout  = frontend_fpslimit * 5;
    fe_joyaitem     = item;

    memset(fe_ignoreaxes, 0, sizeof(fe_ignoreaxes)); 
}

//
// Draw joy axis input state
//
void FE_JoyAxisBindDrawer(void)
{
    FE_DrawBox(64, 84, 192, 32);
    
    if(fe_joyawaittics)
    {
        FE_WriteYellowTextCentered(90,  "Release all sticks/pads.");
        FE_WriteYellowTextCentered(102, "(Please wait...)");
    }
    else
    {
        FE_WriteYellowTextCentered(90,  "Push a stick or pad.");
        FE_WriteYellowTextCentered(102, "(Wait to cancel)");
    }
}


//=============================================================================
//
// Button Binding
//


// current JBIND item
static femenuitem_t *fe_joybitem;

// time to wait before input
static int fe_joybwaittics;

// time to wait before aborting binding
static int fe_joybtimeout;

// buttons to ignore due to noise
static byte fe_ignorebuttons[FE_IGNORE_LIMIT];

void FE_JoyBindResponder(void)
{
    // ignoring input?
    if(fe_joybwaittics)
    {
        int i = I_GetJoystickEventID();

        if(i >= 0 && i < FE_IGNORE_LIMIT)
            fe_ignorebuttons[i] = 1;
    }
    else
    {
        char buf[33];
        int i = I_GetJoystickEventID();
        int curVal = M_GetIntVariable(fe_joybitem->verb);

        if(i >= 0 && i < FE_IGNORE_LIMIT && fe_ignorebuttons[i])
            return;

        if(i >= 0)
        {
            if(i != curVal)
            {
                // set new value
                M_SetVariable(fe_joybitem->verb, M_Itoa(i, buf, 10));
            }
            else
            {
                // clear binding
                M_SetVariable(fe_joybitem->verb, "-1");
            }
            frontend_state = FE_STATE_MAINMENU;
            S_StartSound(NULL, sfx_swtchn);
        }
    }
}

void FE_JoyBindTicker(void)
{
    // count down wait timer
    if(fe_joybwaittics > 0)
    {
        --fe_joybwaittics;
        return;
    }

    // time out?
    if(fe_joybtimeout > 0)
    {
        --fe_joybtimeout;
        if(fe_joybtimeout == 0)
        {
            frontend_state = FE_STATE_MAINMENU;
            S_StartSound(NULL, sfx_mtalht);
        }
    }
}

//
// Start process of binding a gamepad button
//
void FE_JoyBindStart(femenuitem_t *item)
{
    frontend_state  = FE_STATE_JBINPUT;
    fe_joybwaittics = 3 * frontend_fpslimit / 2;
    fe_joybtimeout  = frontend_fpslimit * 5;
    fe_joybitem     = item;

    memset(fe_ignorebuttons, 0, sizeof(fe_ignorebuttons)); 
}

//
// Draw joy button input state
//
void FE_JoyBindDrawer(void)
{
    FE_DrawBox(64, 84, 192, 32);
    
    if(fe_joybwaittics)
    {
        FE_WriteYellowTextCentered(90,  "Release all buttons.");
        FE_WriteYellowTextCentered(102, "(Please wait...)");
    }
    else
    {
        FE_WriteYellowTextCentered(90,  "Press a button.");
        FE_WriteYellowTextCentered(102, "(Wait to cancel)");
    }
}

//=============================================================================
//
// Gamepad Mapping DB
//
// This blows goats.
// - haleyjd 20141027
//

// Look up configuration variable for index into a gamepad profile
static const char *feVarForGPProfileIdx[FE_JOYPROF_MAX] =
{
    // axes
    "joystick_y_axis",   "joystick_x_axis",   "joystick_strafe_axis",   "joystick_look_axis",

    // inversion
    "joystick_y_invert", "joystick_x_invert", "joystick_strafe_invert", "joystick_look_invert",

    // map keys
    "joybmap_toggle",   "joybmap_north",   "joybmap_south",   "joybmap_east", 
    "joybmap_west",     "joybmap_zoomin",  "joybmap_zoomout", "joybmap_follow", 
    "joybmap_mark",     "joybmap_clearmark",

    // menu keys
    "joyb_menu_activate", "joyb_menu_up",      "joyb_menu_down",    "joyb_menu_left", 
    "joyb_menu_right",    "joyb_menu_back",    "joyb_menu_forward", "joyb_menu_confirm", 
    "joyb_menu_abort",

    // game keys
    "joyb_strafeleft", "joyb_straferight", "joyb_strafe",     "joyb_fire",       
    "joyb_use",        "joyb_speed",       "joyb_jump",       "joyb_centerview", 
    "joyb_prevweapon", "joyb_nextweapon",  "joyb_invleft",    "joyb_invright",    
    "joyb_invuse",     "joyb_invdrop",     "joyb_invpop",     "joyb_mission",    
    "joyb_invkey"
};

typedef struct fepad_s
{
    const char  *name;
    const char **buttons;
    size_t       numbuttons;
    const char **axes;
    size_t       numaxes;
    const int   *profile;
} fepad_t;

#if defined(WIN32) || defined(_WIN32)
// Windows Pad Mappings

static const char *x360Buttons[] =
{
    "A", "B", "X", "Y", "L", "R", "BACK", "START", "L STICK", "R STICK"
};

static const char *x360Axes[] =
{
    "Left X", "Left Y", "Trigger", "Right Y", "Right X"
};

static const int x360Profile[FE_JOYPROF_MAX] =
{
     1,  4,  0,  3, // axes
     0,  0,  0,  0, // invert

     6, 21, 16, 15, // map
    20, 22, 17,  9, 
    -1, -1,  

     7, 25, 27, 28, // menus
    26,  1,  0,  0, 
     1,            
    
    -1, -1, -1, 22, // gameplay
     0, -1,  5,  9, 
     2,  3, 28, 26, 
     1, 27, 25,  4, 
    -1
};

static const char *afterglowButtons[] =
{
    "SQUARE", "X", "O", "TRIANGLE", "L1", "R1", "L2", "R2", "SELECT", "START", "L STICK", "R STICK", "GUIDE"
};

static const char *afterglowAxes[] =
{
    "Left X", "Left Y", "Right X", "Right Y"
};

static const int afterglowProfile[FE_JOYPROF_MAX] =
{
     1,  0,  2,  3, // axes
     0,  0,  0,  0, // invert

     8, 21, 16, 15, // map
    20,  6,  7, 11,
    -1, -1,

     9, -1, -1, -1, // menus
    -1,  1,  2,  2,
     1,

    -1, -1, -1,  7, // gameplay
     1, -1,  5, 11,
     0,  3, 28, 26, 
     2, 27, 25,  4,
    -1
};

static const char *F710Buttons[] =
{
    "X", "A", "B", "Y", "L", "R", "LT", "RT", "BACK", "START", "L STICK", "R STICK"
};

static const char *F710Axes[] =
{
    "Left X", "Left Y", "Right X", "Right Y"
};

static const int F710Profile[] =
{
     1,  2,  0,  3, // axes
     0,  0,  0,  0, // invert

     8, 21, 16, 15, // map
    20,  6,  7, 11, 
    -1, -1,  

     9, -1, -1, -1, // menus
    -1,  2,  1,  1, 
     2,            
    
    -1, -1, -1,  7, // gameplay
     1, -1,  5, 10, 
     0,  3, 28, 26, 
     2, 27, 25,  4, 
    -1
};

static const char *PS3DualShockButtons[] =
{
    "SQUARE", "X", "O", "TRIANGLE", "L2", "R2", "L1", "R1", "START", "SELECT", "L STICK", "R STICK", "GUIDE"
};

static const char *PS3DualShockAxes[] =
{
    "Left X", "Left Y", "Right X", "Right Y"
};

static const char *PS4Buttons[] =
{
    "SQUARE", "X", "O", "TRIANGLE", "L1", "R1", "6", "7", "SHARE", "OPTIONS", "L STICK", "R STICK", "GUIDE"    
};

static const char *PS4Axes[] =
{
    "Left X", "Left Y", "Right X", "Left Trigger", "Right Trigger", "Right Y"
};

static const char *RumblePadButtons[] =
{
    "1", "2", "3", "4", "L1", "R1", "L2", "R2", "9", "10", "L STICK", "R STICK"
};

static const char *RumblePadAxes[] =
{
    "Left X", "Left Y", "Right X", "Right Y"
};

static const char *OUYAButtons[] =
{
    "O", "U", "Y", "A", "L1", "R1", "L STICK", "R STICK", "UP", "DOWN", "LEFT", "RIGHT", "LT", "RT", 
    "HOME", "HOME-ALT"
};

static const char *OUYAAxes[] =
{
    "Left X", "Left Y", "2", "Right X", "Right Y"
};

static const char *RetroPortButtons[] =
{
    "Y", "B", "SELECT", "START", "X", "A", "L", "R"
};

static const char *RetroPortAxes[] =
{
    "X", "Y"
};

static const char *GamepadProButtons[] =
{
    "RED", "YELLOW", "GREEN", "BLUE", "L1", "R1", "L2", "R2", "SELECT", "START"
};

static const char *GamepadProAxes[] =
{
    "X", "Y"
};

static fepad_t pads[] =
{
    { 
        "XBOX 360",
        x360Buttons, arrlen(x360Buttons),
        x360Axes,    arrlen(x360Axes),
        x360Profile
    },
    { 
        "Afterglow PS3",
        afterglowButtons, arrlen(afterglowButtons),
        afterglowAxes,    arrlen(afterglowAxes),
        afterglowProfile
    },
    { 
        // F310/F510 allegedly the same
        "Logitech Dual Action",
        F710Buttons, arrlen(F710Buttons),
        F710Axes,    arrlen(F710Axes),
        F710Profile
    }, 
    { 
        // Cover also for safety
        "F710",
        F710Buttons, arrlen(F710Buttons),
        F710Axes,    arrlen(F710Axes),
        F710Profile
    }, 
    {
        "DualShock",
        PS3DualShockButtons, arrlen(PS3DualShockButtons),
        PS3DualShockAxes,    arrlen(PS3DualShockAxes),
        NULL
    },
    { 
        "PS4",
        PS4Buttons, arrlen(PS4Buttons),
        PS4Axes,    arrlen(PS4Axes),
        NULL
    },
    { 
        "RumblePad 2",
        RumblePadButtons, arrlen(RumblePadButtons),
        RumblePadAxes,    arrlen(RumblePadAxes),
        NULL
    },
    { 
        "OUYA",
        OUYAButtons, arrlen(OUYAButtons),
        OUYAAxes,    arrlen(OUYAAxes),
        NULL
    },
    { 
        "Super RetroPort",
        RetroPortButtons, arrlen(RetroPortButtons),
        RetroPortAxes,    arrlen(RetroPortAxes),
        NULL
    },
    {
        "GamePad Pro",
        GamepadProButtons, arrlen(GamepadProButtons),  
        GamepadProAxes,    arrlen(GamepadProAxes),
        NULL
    }
};
#elif defined(__linux__)
// Linux Pad Mappings

static const char *x360Buttons[] =
{
    "A", "B", "X", "Y", "L", "R", "BACK", "START", "GUIDE", "L STICK", "R STICK"
};

static const char *x360Axes[] =
{
    "Left X", "Left Y", "L Trigger", "Right Y", "Right X", "R Trigger"
};

static const int x360Profile[FE_JOYPROF_MAX] =
{
     1,  4,  0,  3, // axes     LY RX LX RY
     0,  0,  0,  0, // invert 

     6, 21, 16, 15, // map      BACK A1- A1+ A0+
    20, 29, 17, 10, //          A0-  A6+ A2+ RSTICK
    -1, -1,

     7, -1, -1, -1, // menus    START
    -1,  1,  0,  0, //          - B A A
     1,             //          B
    
    -1, -1, -1, 29, // gameplay - -  -  A6+
     0, -1,  5, 10, //          A -  R  RSTICK
     2,  3, 28, 26, //          X Y  HL HR
     1, 27, 25,  4, //          B HD HU L
    -1
};

static fepad_t pads[] =
{
    { 
        "X-Box 360",
        x360Buttons, arrlen(x360Buttons),
        x360Axes,    arrlen(x360Axes),
        x360Profile
    },
    {
        "Xbox 360", // wireless version
        x360Buttons, arrlen(x360Buttons),
        x360Axes,    arrlen(x360Axes),
        x360Profile
    },
};
#else
// No platform
static const char *noPadButtons[] =
{
    "0"
};

static fepad_t pads[] =
{
    { "!$@NotAValidPadDontMatchMe@$!", noPadButtons, arrlen(noPadButtons) }
};
#endif

const char *FE_ButtonNameForNum(int button)
{
    static char tempName[33];
    size_t i;
    const char *name;

    if(button == -1)
        return "None";

    name = I_QueryActiveJoystickName();
    if(name)
    {
        for(i = 0; i < arrlen(pads); i++)
        {
            if(strstr(name, pads[i].name))
            {
                if(button >= 0 && button < pads[i].numbuttons)
                    return pads[i].buttons[button];
            }
        }
    }

    if(button >= NUM_VIRTUAL_BUTTONS)
    {
        static const char *axisUp[5]   = { "A0+", "A1+", "A2+", "A3+", "A4+" };
        static const char *axisDown[5] = { "A0-", "A1-", "A2-", "A3-", "A4-" };
        static const char *hatPos[4]   = { "HAT UP", "HAT RIGHT", "HAT DOWN", "HAT LEFT" };
        static const char *axis6UD[2]  = { "A6+", "A6-" };
        if(button >= NUM_VIRTUAL_BUTTONS && button < NUM_VIRTUAL_BUTTONS + 5)
        {
            return axisUp[button - NUM_VIRTUAL_BUTTONS];
        }
        if(button >= NUM_VIRTUAL_BUTTONS + 5 && button < NUM_VIRTUAL_BUTTONS + 10)
        {
            return axisDown[button - (NUM_VIRTUAL_BUTTONS + 5)];
        }
        if(button >= NUM_VIRTUAL_BUTTONS + 10 && button < NUM_VIRTUAL_BUTTONS + 14)
        {
            return hatPos[button - (NUM_VIRTUAL_BUTTONS + 10)];
        }
        if(button >= NUM_VIRTUAL_BUTTONS + 14 && button < NUM_VIRTUAL_BUTTONS + 16)
        {
            return axis6UD[button - (NUM_VIRTUAL_BUTTONS + 14)];
        }
    }

    M_Itoa(button, tempName, 10);
    return tempName;
}

const char *FE_AxisNameForNum(int axis)
{
    static char tempName[33];
    size_t i;
    const char *name;

    if(axis == -1)
        return "None";

    name = I_QueryActiveJoystickName();
    if(name)
    {
        for(i = 0; i < arrlen(pads); i++)
        {
            if(strstr(name, pads[i].name))
            {
                if(axis >= 0 && axis < pads[i].numaxes)
                    return pads[i].axes[axis];
            }
        }
    }

    M_Itoa(axis, tempName, 10);
    return tempName;
}

//
// Clear the binding values of all gamepad variables, in preparation for applying
// a built-in controller profile.
//
static void FE_clearAllGamepadVars(void)
{
    int i;

    for(i = 0; i < FE_JOYPROF_MAX; i++)
    {
        if(i >= FE_JOYPROF_Y_INVERT && i <= FE_JOYPROF_LOOK_INVERT)
            M_SetVariable(feVarForGPProfileIdx[i], "0");
        else
            M_SetVariable(feVarForGPProfileIdx[i], "-1");
    }
}

static void FE_applyGamepadProfile(const int *profile)
{
    int i;
    char buf[33];
    
    FE_clearAllGamepadVars();

    for(i = 0; i < FE_JOYPROF_MAX; i++)
        M_SetVariable(feVarForGPProfileIdx[i], M_Itoa(profile[i], buf, 10));
}

//
// Check if the currently selected gamepad has a button binding profile.
// Returns profile # if so, otherwise -1.
//
int FE_PadHasProfile(void)
{
    const char *name = I_QueryActiveJoystickName();
    if(name)
    {
        int i;
        for(i = 0; i < arrlen(pads); i++)
        {
            if(strstr(name, pads[i].name))
            {
                if(pads[i].profile)
                    return i;  // pad exists and has profile
                else
                    return -1; // pad exists, but no profile
            }
        }
    }

    return -1; // pad does not exist
}

//
// Apply the profile as returned by FE_PadHasProfile
//
void FE_ApplyPadProfile(int index)
{
    if(pads[index].profile)
    {
        FE_applyGamepadProfile(pads[index].profile);
        S_StartSound(NULL, sfx_swtchn);
    }
}

//
// FE_AutoApplyPadProfile
//
// svillarreal: Auto-apply a profile if starting the game from scratch
// (ie. no configs present)
//

void FE_AutoApplyPadProfile(void)
{
    int index = FE_PadHasProfile();

    if(index >= 0 && pads[index].profile)
        FE_applyGamepadProfile(pads[index].profile);
}

//=============================================================================
//
// Main Gamepad Menu
//

static femenuitem_t gamepadMenuItems[] =
{
    { FE_MITEM_CMD, "Select Device", "gamepaddev", FE_FONT_BIG    },
    { FE_MITEM_CMD, "Apply Profile", "gpprofile",  FE_FONT_BIG, 0 },
    { FE_MITEM_CMD, "Axes",          "gpaxes",     FE_FONT_BIG    },
    { FE_MITEM_CMD, "Automap",       "gpautomap",  FE_FONT_BIG    },
    { FE_MITEM_CMD, "Inventory",     "gpinv",      FE_FONT_BIG    },
    { FE_MITEM_CMD, "Menus",         "gpmenus",    FE_FONT_BIG    },
    { FE_MITEM_CMD, "Movement",      "gpmovement", FE_FONT_BIG    },
    { FE_MITEM_END, "", "" }
};

static femenu_t gamepadMenu =
{
    gamepadMenuItems,
    arrlen(gamepadMenuItems),
    80,
    40,
    4,
    "Gamepad Options",
    FE_BG_RSKULL,
    NULL,
    FE_CURSOR_SIGIL,
    0,
    false,
    false
};

// "gamepad" command
void FE_CmdGamepad(void)
{
    FE_PushMenu(&gamepadMenu);
}

static void FE_updateGamepadMenu(void)
{
    // check for device profile
    if((fe_padprofile = FE_PadHasProfile()) >= 0)
        gamepadMenuItems[1].type = FE_MITEM_CMD;
    else
        gamepadMenuItems[1].type = FE_MITEM_GAP;
}

// "gpprofile" command
void FE_CmdGPProfile(void)
{
    if(fe_padprofile >= 0)
        FE_ApplyPadProfile(fe_padprofile);
}

// EOF

