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

#include "SDL.h"

#ifdef _USE_STEAM_
#include "steamService.h"
#endif

#include "z_zone.h"
#include "doomdef.h"
#include "doomstat.h"
#include "doomtype.h"
#include "hu_lib.h"
#include "m_menu.h"
#include "m_misc.h"
#include "sounds.h"
#include "s_sound.h"
#include "v_video.h"
#include "w_wad.h"

#include "fe_commands.h"
#include "fe_graphics.h"
#include "fe_frontend.h"
#include "fe_menuengine.h"
#include "fe_menus.h"
#include "fe_multiplayer.h"

#include "net_steamworks.h"

void (*feModalLoopFunc)(void);

//=============================================================================
//
// DM vars
//

// backup vars
static int backup_deathmatch;
static int backup_fastparm;
static int backup_nomonsters;
static int backup_randomparm;
static int backup_respawnparm;
static int backup_startmap;
static int backup_startskill;
static int backup_timelimit;

//
// Back up all deathmatch related variables when entering a multiplayer lobby;
// that way, if the player backs out and plays a single player game, they're not
// suddenly in altdeath with no monsters.
//
static void FE_BackupDMVars(void)
{
    backup_deathmatch  = deathmatch;
    backup_fastparm    = fastparm;
    backup_nomonsters  = nomonsters;
    backup_randomparm  = randomparm;
    backup_respawnparm = respawnparm;
    backup_startmap    = startmap;
    backup_startskill  = startskill;
    backup_timelimit   = timelimit;
}

//
// Restore all deathmatch variables to the values they had going into the frontend.
// Note this doesn't preclude the use of the corresponding command line parameters,
// where such action is possible; it in fact enables it.
//
static void FE_RestoreDMVars(void)
{
    deathmatch  = backup_deathmatch;
    fastparm    = backup_fastparm;
    nomonsters  = backup_nomonsters;
    randomparm  = backup_randomparm;
    respawnparm = backup_respawnparm;
    startmap    = backup_startmap;
    startskill  = backup_startskill;
    timelimit   = backup_timelimit;
}

//
// Set all deathmatch variables to the best defaults.
//
static void FE_DMSensibleDefaults(boolean backup)
{
    if(backup)
        FE_BackupDMVars();
    deathmatch  = 2;
    fastparm    = false;
    nomonsters  = true;
    randomparm  = false;
    respawnparm = false;
    startmap    = 1;
    startskill  = 2;
    timelimit   = 0;
}

//=============================================================================
//
// Utils
//

//
// Sanitize a Steamworks API-derived string for display in the frontend.
// The passed string MUST be mutable.
//
#ifdef _USE_STEAM_
static void FE_SanitizeString(char *in, int limit, boolean useyfont)
{
    unsigned char *end;
    unsigned char *c = (unsigned char *)in;
    size_t len = strlen(in);

    if(!len)
        return;
    end = in + len - 1;

    // Steam strings are UTF-8, and the Doom engine is a strictly ASCII
    // affair, so prune any apparent control chars or extended ASCII.
    while(*c)
    {
        if(*c < 0x20 || *c >= 0x7f)
            *c = ' ';
        ++c;
    }

    // Steam strings can be as long as 256 chars depending on the context,
    // so limit to a displayable length depending on the font
    if(useyfont)
    {
        while(HUlib_yellowTextWidth(in) > limit && end != in)
            *end-- = '\0';
    }
    else
    {
        while(M_StringWidth(in) > limit && end != in)
            *end-- = '\0';
    }
}
#endif

//=============================================================================
//
// Teams (for Capture the Chalice)
//

static const char *ctcteamvals[] =
{
    "Auto",
    "Blue",
    "Red"
};

void FE_IncrementTeam(void)
{
    if(++ctcprefteam == PREF_TEAM_MAX)
        ctcprefteam = PREF_TEAM_AUTO;
    S_StartSound(NULL, sfx_swtchn);

#ifdef _USE_STEAM_
    // broadcast to the lobby
    I_SteamLobbyChangeTeam(ctcteamvals[ctcprefteam]);
#endif
}

void FE_DecrementTeam(void)
{
    if(ctcprefteam == PREF_TEAM_AUTO)
        ctcprefteam = PREF_TEAM_RED;
    else
        --ctcprefteam;
    S_StartSound(NULL, sfx_swtchn);

#ifdef _USE_STEAM_
    // broadcast to the lobby
    I_SteamLobbyChangeTeam(ctcteamvals[ctcprefteam]);
#endif
}

const char *FE_GetLobbyTeam(void)
{
    return ctcteamvals[ctcprefteam];
}

//=============================================================================
//
// Lobby Listener
//
// We can accept lobby invitations while the frontend is running at startup.
//

//
// Turn on the listener
//
void FE_EnableLobbyListener(void)
{
#ifdef _USE_STEAM_
    I_SteamBeginLobbyJoinListener();
#endif
}

//
// Disable the listener
//
void FE_DisableLobbyListener(void)
{
#ifdef _USE_STEAM_
    I_SteamDestroyLobbyJoinListener();
#endif
}

//
// Check for lobby join
//
boolean FE_CheckLobbyListener(void)
{
#ifdef _USE_STEAM_
    return !!I_SteamLobbyJoinRequested();
#else
    return false;
#endif
}

//=============================================================================
//
// Lobby Create (Server)
//

static boolean feInLobby;
static boolean feLobbyCreateFailed;
static int     feLobbyErrMsgTimer;

//
// Returns 0 if waiting, 1 if connected, -1 if failed.
//
static int FE_CheckLobbyCreated(void)
{
#ifdef _USE_STEAM_
    switch(I_SteamLobbyPollState())
    {
    case I_LOBBY_STATE_CREATING:
        return 0;
    case I_LOBBY_STATE_INLOBBY:
        return 1;
    default: // create failed, or a different (unknown?) state
        I_SteamLobbyCleanUp();
        return -1;
    }
#else
    return -1;
#endif
}

//
// Lobby Create modal loop
//
static void FE_LobbyCreateTick(void)
{
    if(feLobbyCreateFailed)
    {
        if(--feLobbyErrMsgTimer == 0)
        {
            frontend_modalmsg = NULL;
            feModalLoopFunc   = NULL;
            frontend_state    = FE_STATE_MAINMENU;
            FE_EnableLobbyListener(); // can re-enable listener
        }
        return;
    }

    switch(FE_CheckLobbyCreated())
    {
    case 1: // successful connection
        frontend_modalmsg = NULL;
        feModalLoopFunc   = NULL;
        frontend_state    = FE_STATE_MAINMENU;
        feInLobby         = true;
        FE_DMSensibleDefaults(true);
        FE_ExecCmd("lobbysrv"); // go to lobby server menu
        break;
    case 0: // still waiting
        break;
    case -1: // failed
        frontend_modalmsg   = "Lobby creation failed.";
        feLobbyCreateFailed = true;
        feLobbyErrMsgTimer  = 2*frontend_fpslimit;
        break;
    }
}

//
// Create a multiplayer game lobby.
//
void FE_CreateLobby(int publicLobby)
{
    if(feInLobby)
        return;

    // ensure listener is disabled
    FE_DisableLobbyListener();

#ifdef _USE_STEAM_
    I_SteamCreateNewLobby(publicLobby);
#endif
    frontend_state      = FE_STATE_LOBBYCREATE;
    frontend_modalmsg   = "Creating lobby...";
    feInLobby           = false;
    feLobbyCreateFailed = false;
    feModalLoopFunc     = FE_LobbyCreateTick;
}

//
// Leave lobby
//
void FE_LeaveLobby(void)
{
    if(!feInLobby)
        return;

    FE_EnableLobbyListener(); // turn listener back on now.

#ifdef _USE_STEAM_
    I_SteamLeaveLobby();    
#endif
    feInLobby = false;
    FE_RestoreDMVars(); // don't make game settings stick to single player
    FE_PopMenu(true);   // force exit lobby menu
}

//
// Get ready state
//
boolean FE_GetLobbyReady(void)
{
#ifdef _USE_STEAM_
    return !!I_SteamLobbyGetReady();
#else
    return false;
#endif
}

//
// Toggle ready state
//
void FE_ToggleLobbyReady(void)
{
#ifdef _USE_STEAM_
    I_SteamLobbyChangeReady();
#endif
    S_StartSound(NULL, sfx_swtchn);
}

//
// Check for upgrade from client to server (user has become owner)
//
void FE_CheckForLobbyUpgrade(void)
{
    if(!(feInLobby && FE_InClientLobby()))
        return;

#ifdef _USE_STEAM_
    if(I_SteamLobbyUserIsOwner())
    {
        I_SteamLobbyUpdateName();     // update lobby name
        FE_PopMenu(true);             // exit out of client menu
        FE_DMSensibleDefaults(false); // set DM defaults w/o backup
        FE_ExecCmd("lobbysrv");       // go to lobby server menu
    }
#endif
}

//
// Lobby Menu Commands
//

// "newpublobby" command
void FE_CmdNewPublicLobby(void)
{
    FE_CreateLobby(1);
}

// "newprivlobby" command
void FE_CmdNewPrivateLobby(void)
{
    FE_CreateLobby(0);
}

// "leavelobby" command
void FE_CmdLeaveLobby(void)
{
    FE_LeaveLobby();
}

// "invite" command
void FE_CmdInvite(void)
{
#ifdef _USE_STEAM_
    I_SteamLobbyInviteFriends();
#endif
}

//=============================================================================
//
// Lobby Join (Client)
//

static boolean feLobbyJoinFailed;

//
// Returns 0 if waiting, 1 if connected, -1 if failed.
//
static int FE_CheckLobbyJoined(void)
{
#ifdef _USE_STEAM_
    switch(I_SteamLobbyPollState())
    {
    case I_LOBBY_STATE_JOINING:
        return 0;
    case I_LOBBY_STATE_INLOBBY:
        return 1;
    default: // join failed, or a different (unknown?) state
        I_SteamLobbyCleanUp();
        return -1;
    }
#else
    return -1;
#endif
}


//
// Lobby join modal loop
//
static void FE_LobbyJoinTick(void)
{
    if(feLobbyJoinFailed)
    {
        if(--feLobbyErrMsgTimer == 0)
        {
            frontend_modalmsg = NULL;
            feModalLoopFunc   = NULL;
            frontend_state    = FE_STATE_MAINMENU;
            FE_EnableLobbyListener(); // can re-enable listener
        }
        return;
    }

    switch(FE_CheckLobbyJoined())
    {
    case 1: // successful connection
        frontend_modalmsg = NULL;
        feModalLoopFunc   = NULL;
        frontend_state    = FE_STATE_MAINMENU;
        feInLobby         = true;
#ifdef _USE_STEAM_
        if(I_SteamLobbyUserIsOwner()) // user became owner when joining?
        {
            FE_DMSensibleDefaults(true);
            FE_ExecCmd("lobbysrv"); // go to lobby server menu
        }
        else
#endif
        {

            FE_BackupDMVars();
            FE_ExecCmd("lobbyclient"); // go to lobby client menu
        }
        break;
    case 0: // still waiting
        break;
    case -1: // failed
        frontend_modalmsg  = "Lobby join failed.";
        feLobbyJoinFailed  = true;
        feLobbyErrMsgTimer = 2*frontend_fpslimit;
        break;
    }
}

//
// FE_JoinLobbyFromInvite
//
// We received and accepted an invite to a lobby from another player. Execute
// the join now.
//
void FE_JoinLobbyFromInvite(void)
{
    if(feInLobby)
        return;

#ifdef _USE_STEAM_
    I_SteamJoinLobbyFromInvite();
#endif
    FE_DisableLobbyListener(); // turn off listener now.
    frontend_state    = FE_STATE_LOBBYJOIN;
    frontend_modalmsg = "Joining lobby...";
    feInLobby         = false;
    feLobbyJoinFailed = false;
    feModalLoopFunc   = FE_LobbyJoinTick;
}

//
// FE_JoinLobbyFromConnectStr
//
// We were started with the +connect_lobby command; pass the lobby ID to Steam
// and join it.
//
void FE_JoinLobbyFromConnectStr(const char *lobbyID)
{
    if(feInLobby)
        return;

#ifdef _USE_STEAM_
    I_SteamJoinLobbyFromStartup(lobbyID);
#endif
    FE_DisableLobbyListener(); // turn off listener if still on
    frontend_state    = FE_STATE_LOBBYJOIN;
    frontend_modalmsg = "Joining lobby...";
    feInLobby         = false;
    feLobbyJoinFailed = false;
    feModalLoopFunc   = FE_LobbyJoinTick;
}

//=============================================================================
//
// Lobby User List
//

static char    dashlinebuf[128];
static boolean dashlineBuilt;

//
// Draw a separating line of dashes in the yellow font.
//
#ifdef _USE_STEAM_
static void FE_DrawDashLine(int x, int y, int width)
{
    if(!dashlineBuilt)
    {
        int idx = 0;
        while(HUlib_yellowTextWidth(dashlinebuf) < width && idx < 127)
            dashlinebuf[idx++] = '-';
        dashlineBuilt = true;
    }

    HUlib_drawYellowText(x, y, dashlinebuf, true);
}
#endif

//
// Draw the lobby user list with ready states.
//
void FE_DrawLobbyUserList(int x, int y)
{
#ifdef _USE_STEAM_
    int i;
    int numUsers = I_SteamLobbyGetNumMembers();
    static int stateWidth;

    if(!stateWidth)
        stateWidth = HUlib_yellowTextWidth("Player State");

    HUlib_drawYellowText(x, y, "Players in Lobby", true);
    HUlib_drawYellowText(x+120, y, "Player State", true);
    y += 8;
    FE_DrawDashLine(x, y, 120 + stateWidth);
    y += 8;
    for(i = 0; i < numUsers; i++)
    {
        char *name    = M_Strdup(I_SteamLobbyGetUserNameAt(i));
        char *team    = M_Strdup(I_SteamLobbyGetTeamAt(i));
        boolean ready = I_SteamLobbyGetUserReadyAt(i);
        char *patch;
        boolean autoteam = false;
       
        FE_SanitizeString(name, 110, true);

        if(!strcasecmp(team, "Blue"))
            patch = "STCOLOR8";
        else if(!strcasecmp(team, "Red"))
            patch = "STCOLOR2";
        else
        {
            patch = "STCOLOR1";
            autoteam = true;
        }

        V_DrawPatch(x - 10, y - 1, W_CacheLumpName(patch, PU_CACHE));
        if(autoteam)
            HUlib_drawYellowText(x - 9, y, "?", false);
        HUlib_drawYellowText(x, y, name, true);
        HUlib_drawYellowText(x+120, y, ready ? "Ready" : "Not Ready", true);
        y += 8;

        free(name);
        free(team);
    }
#endif
}

//=============================================================================
//
// Lobby Manager/List
//

static femenuitem_t lobbyMenuItems[] =
{
    { FE_MITEM_CMD, "", "" },
    { FE_MITEM_END, "", "" },
    { FE_MITEM_END, "", "" },
    { FE_MITEM_END, "", "" },
    { FE_MITEM_END, "", "" },
    { FE_MITEM_END, "", "" },
    { FE_MITEM_END, "", "" },
    { FE_MITEM_END, "", "" },
    { FE_MITEM_END, "", "" },
    { FE_MITEM_END, "", "" },
    { FE_MITEM_END, "", "" },
    { FE_MITEM_END, "", "" },
    { FE_MITEM_END, "", "" },
    { FE_MITEM_END, "", "" }
};

static femenu_t lobbyMenu =
{
    lobbyMenuItems,
    2,
    60,
    24,
    2,
    "Lobbies",
    FE_BG_TSKULL,
    NULL,
    FE_CURSOR_LASER,
    0,
    false,
    false
};

//
// Draw a FE_MITEM_LOBBY menu item.
//
void FE_DrawLobbyItem(femenuitem_t *item, int x, int y)
{
#ifdef _USE_STEAM_
    char *name;

    if(I_SteamLobbyMgrInRequest())
        return;

    name = M_Strdup(I_SteamLobbyMgrGetNameAt((unsigned int)(item->data)));
    FE_SanitizeString(name, SCREENWIDTH - 20 - lobbyMenu.x, false);

    item->x = x;
    item->y = y;
    item->w = M_StringWidth(name);
    item->h = 12;

    M_WriteText(x, y, name);

    free(name);
#endif
}

//
// Rebuild the lobby menu
//
static void FE_RebuildLobbyMenu(void)
{
    unsigned int i;
    unsigned int numLobbies = 0;

#ifdef _USE_STEAM_
    numLobbies = I_SteamLobbyMgrGetCount();

    if(I_SteamLobbyMgrInRequest())
    {
        lobbyMenuItems[0].description = "Searching...";
        lobbyMenuItems[0].type        = FE_MITEM_CMD;
        lobbyMenuItems[0].verb        = "";
        lobbyMenuItems[0].data        = 0;
        lobbyMenu.numitems            = 2;
        lobbyMenu.itemon              = 0;
        for(i = 1; i < arrlen(lobbyMenuItems); i++)
            lobbyMenuItems[i].type = FE_MITEM_END;
    }
    else
#endif
    if(numLobbies == 0)
    {
        lobbyMenuItems[0].description = "No lobbies available.";
        lobbyMenuItems[0].type        = FE_MITEM_CMD;
        lobbyMenuItems[0].verb        = "";
        lobbyMenuItems[0].data        = 0;
        lobbyMenuItems[1].description = "Refresh...";
        lobbyMenuItems[1].type        = FE_MITEM_CMD;
        lobbyMenuItems[1].verb        = "lobbyrefresh";
        lobbyMenuItems[1].data        = 0;
        lobbyMenu.numitems            = 3;
        lobbyMenu.itemon              = 1;
        for(i = 2; i < arrlen(lobbyMenuItems); i++)
            lobbyMenuItems[i].type = FE_MITEM_END;        
    }
    else
    {
        unsigned int numMenuLobbies = numLobbies < 12 ? numLobbies : 12;
        for(i = 0; i < numMenuLobbies; i++)
        {
            lobbyMenuItems[i].description = "";
            lobbyMenuItems[i].type        = FE_MITEM_LOBBY;
            lobbyMenuItems[i].verb        = "joinlobby";
            lobbyMenuItems[i].data        = (int)i;
        }
        lobbyMenuItems[numMenuLobbies + 1].description = "Refresh...";
        lobbyMenuItems[numMenuLobbies + 1].type        = FE_MITEM_CMD;
        lobbyMenuItems[numMenuLobbies + 1].verb        = "lobbyrefresh";
        lobbyMenuItems[numMenuLobbies + 1].data        = 0;
        lobbyMenu.numitems = numMenuLobbies + 2;
        for(i = numMenuLobbies + 2; i < arrlen(lobbyMenuItems); i++)
            lobbyMenuItems[i].type = FE_MITEM_END;
        if(lobbyMenu.itemon >= lobbyMenu.numitems - 2)
            lobbyMenu.itemon = lobbyMenu.numitems - 2;
    }
}

//
// Lobby refresh modal loop
//
static void FE_LobbyRefreshTick(void)
{
#ifdef _USE_STEAM_
    if(!I_SteamLobbyMgrInRequest())
#endif
    {
        frontend_modalmsg = NULL;
        feModalLoopFunc   = NULL;
        frontend_state    = FE_STATE_MAINMENU;
    }
    FE_RebuildLobbyMenu();
}

//
// "joinlobby" command
//
void FE_CmdJoinLobby(void)
{
    unsigned int lobbyNum;
    femenuitem_t *item = &lobbyMenuItems[lobbyMenu.itemon];

    // not sure how you got here...
    if(item->type != FE_MITEM_LOBBY || strcmp(item->verb, "joinlobby"))
        return;

    lobbyNum = (unsigned int)(item->data);

#ifdef _USE_STEAM_
    I_SteamLobbyMgrJoinLobbyAt(lobbyNum);
#endif
    FE_DisableLobbyListener(); // turn off listener if still on
    frontend_state    = FE_STATE_LOBBYJOIN;
    frontend_modalmsg = "Joining lobby...";
    feInLobby         = false;
    feLobbyJoinFailed = false;
    feModalLoopFunc   = FE_LobbyJoinTick;
}

//
// "lobbyrefresh" command
//
void FE_CmdLobbyRefresh(void)
{
#ifdef _USE_STEAM_
    I_SteamLobbyMgrRefresh();
#endif
    frontend_state    = FE_STATE_REFRESH;
    frontend_modalmsg = "Refreshing list...";
    feModalLoopFunc   = FE_LobbyRefreshTick;
    FE_RebuildLobbyMenu();
}

//
// "lobbylist" command
//
void FE_CmdLobbyList(void)
{
    FE_PushMenu(&lobbyMenu);
    FE_CmdLobbyRefresh();
}

//=============================================================================
//
// Start Netgame
//

void FE_ClientCheckForGameStart(void)
{
    // not in lobby, or already starting?
    if(!feInLobby)
        return;

#ifdef _USE_STEAM_
    if (net_SteamGame)
        return;

    // not for server
    if(I_SteamLobbyUserIsOwner())
        return;

    if(I_SteamLobbyPollState() == I_LOBBY_STATE_GAMESTARTING)
    {
        net_SteamGame     = true;
        net_SteamNodeType = NET_STEAM_CLIENT;
        net_SteamNumNodes = I_SteamLobbyGetNumMembers();
        I_SteamClientGetServerFromLobby();   // record server
        net_SteamServerID = M_Strdup(I_SteamClientGetServerAddr());
        frontend_state = FE_STATE_GAMESTART; // netgame is starting
        FE_ExecCmd("go"); // exit frontend
    }
#endif
}

// "startgame" command, run by the server user to start the game
void FE_CmdStartGame(void)
{
    if(!feInLobby)
        return;

#ifdef _USE_STEAM_
    if (net_SteamGame)
        return;

    net_SteamGame     = true;
    net_SteamNodeType = NET_STEAM_SERVER;
    net_SteamNumNodes = I_SteamServerGetClientsFromLobby(); // record clients from lobby
    I_SteamLobbyStartGame();             // notify lobby of game start
    frontend_state = FE_STATE_GAMESTART; // netgame is starting
    FE_ExecCmd("go");                    // exit frontend
#endif
}

// EOF

