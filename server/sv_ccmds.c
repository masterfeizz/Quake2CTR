/*
Copyright (C) 1997-2001 Id Software, Inc.

This program is free software; you can redistribute it and/or
modify it under the terms of the GNU General Public License
as published by the Free Software Foundation; either version 2
of the License, or (at your option) any later version.

This program is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  

See the GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with this program; if not, write to the Free Software
Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.

*/

#include "server.h"

/*
===============================================================================

OPERATOR CONSOLE ONLY COMMANDS

These commands can only be entered from stdin or by a remote operator datagram
===============================================================================
*/

/*
====================
SV_SetMaster_f

Specify a list of master servers
====================
*/
void SV_SetMaster_f (void)
{
	int		i, slot;

	// only dedicated servers send heartbeats
	if (!dedicated->intValue)
	{
		Com_Printf ("Only dedicated servers use masters.\n");
		return;
	}

	// make sure the server is listed public
	Cvar_Set ("public", "1");

	for (i=1 ; i<MAX_MASTERS ; i++)
	{
		memset (&master_adr[i], 0, sizeof(master_adr[i]));
	}

	slot = 1;		// slot 0 will always contain the id master

	for (i=1 ; i<Cmd_Argc() ; i++)
	{
		if (slot == MAX_MASTERS)
		{
			break;
		}

		if (!NET_StringToAdr (Cmd_Argv(i), &master_adr[i]))
		{
			Com_Printf ("Bad address: %s\n", Cmd_Argv(i));
			continue;
		}
		if (master_adr[slot].port == 0)
			master_adr[slot].port = BigShort (PORT_MASTER);

		Com_Printf ("Master server at %s\n", NET_AdrToString (master_adr[slot]));

		Com_Printf ("Sending a ping.\n");

		Netchan_OutOfBandPrint (NS_SERVER, master_adr[slot], "ping");

		slot++;
	}

	svs.last_heartbeat = -9999999;
}



/*
==================
SV_SetPlayer

Sets sv_client and sv_player to the player with idnum Cmd_Argv(1)
==================
*/
qboolean SV_SetPlayer (void)
{
	client_t	*cl;
	int			i;
	int			idnum;
	char		*s;

	if (Cmd_Argc() < 2)
	{
		return false;
	}

	s = Cmd_Argv(1);

	// numeric values are just slot numbers
	if ((s[0] >= '0') && (s[0] <= '9'))
	{
		idnum = atoi(Cmd_Argv(1));
		if ((idnum < 0) || (idnum >= maxclients->value))
		{
			Com_Printf ("Bad client slot: %i\n", idnum);
			return false;
		}

		sv_client = &svs.clients[idnum];
		sv_player = sv_client->edict;
		if (!sv_client->state)
		{
			Com_Printf ("Client %i is not active\n", idnum);
			return false;
		}
		return true;
	}

	// check for a name match
	for (i=0,cl=svs.clients ; i<maxclients->value; i++,cl++)
	{
		if (!cl->state)
		{
			continue;
		}
		if (!strcmp(cl->name, s))
		{
			sv_client = cl;
			sv_player = sv_client->edict;
			return true;
		}
	}

	Com_Printf ("Userid %s is not on the server\n", s);
	return false;
}


/*
===============================================================================

SAVEGAME FILES

===============================================================================
*/

/*
=====================
SV_WipeSavegame

Delete save/<XXX>/
=====================
*/
void SV_WipeSavegame (char *savename)
{
	char	name[MAX_OSPATH];
	char	*s;

	Com_DPrintf(DEVELOPER_MSG_SAVE, "SV_WipeSaveGame(%s)\n", savename);

	Com_sprintf (name, sizeof(name), "%s/save/%s/server.ssv", FS_Gamedir (), savename);
	remove (name);
	Com_sprintf (name, sizeof(name), "%s/save/%s/game.ssv", FS_Gamedir (), savename);
	remove (name);

	Com_sprintf (name, sizeof(name), "%s/save/%s/*.sav", FS_Gamedir (), savename);
	s = Sys_FindFirst( name, 0, 0 );
	while (s)
	{
		remove (s);
		s = Sys_FindNext( 0, 0 );
	}
	Sys_FindClose ();
	Com_sprintf (name, sizeof(name), "%s/save/%s/*.sv2", FS_Gamedir (), savename);
	s = Sys_FindFirst(name, 0, 0 );
	while (s)
	{
		remove (s);
		s = Sys_FindNext( 0, 0 );
	}
	Sys_FindClose ();
}


/*
================
CopyFile
================
*/
void CopyFile (char *src, char *dst)
{
	FILE	*f1, *f2;
	int		l;
	byte	buffer[65536];

	Com_DPrintf(DEVELOPER_MSG_IO, "CopyFile (%s, %s)\n", src, dst);

	f1 = fopen (src, "rb");
	if (!f1)
		return;
	f2 = fopen (dst, "wb");
	if (!f2)
	{
		fclose (f1);
		return;
	}

	while (1)
	{
		l = fread (buffer, 1, sizeof(buffer), f1);
		if (!l)
			break;
		fwrite (buffer, 1, l, f2);
	}

	fclose (f1);
	fclose (f2);
}


/*
================
SV_CopySaveGame
================
*/
void SV_CopySaveGame (char *src, char *dst)
{
	char	name[MAX_OSPATH], name2[MAX_OSPATH];
	int		l, len;
	char	*found;

	Com_DPrintf(DEVELOPER_MSG_SAVE, "SV_CopySaveGame(%s, %s)\n", src, dst);

	SV_WipeSavegame (dst);

	// copy the savegame over
	Com_sprintf (name, sizeof(name), "%s/save/%s/server.ssv", FS_Gamedir(), src);
	Com_sprintf (name2, sizeof(name2), "%s/save/%s/server.ssv", FS_Gamedir(), dst);
	FS_CreatePath (name2);
	CopyFile (name, name2);

	Com_sprintf (name, sizeof(name), "%s/save/%s/game.ssv", FS_Gamedir(), src);
	Com_sprintf (name2, sizeof(name2), "%s/save/%s/game.ssv", FS_Gamedir(), dst);
	CopyFile (name, name2);

	Com_sprintf (name, sizeof(name), "%s/save/%s/", FS_Gamedir(), src);
	len = strlen(name);
	Com_sprintf (name, sizeof(name), "%s/save/%s/*.sav", FS_Gamedir(), src);
	found = Sys_FindFirst(name, 0, 0 );
	while (found)
	{
	//	strncpy (name+len, found+len);
		Q_strncpyz (name+len, found+len, sizeof(name)-len);
		Com_sprintf (name2, sizeof(name2), "%s/save/%s/%s", FS_Gamedir(), dst, found+len);
		CopyFile (name, name2);

		// change sav to sv2
		l = strlen(name);
	//	strncpy (name+l-3, "sv2");
		Q_strncpyz (name+l-3, "sv2", sizeof(name)-l+3);
		l = strlen(name2);
	//	strncpy (name2+l-3, "sv2");
		Q_strncpyz (name2+l-3, "sv2", sizeof(name2)-l+3);
		CopyFile (name, name2);

		found = Sys_FindNext( 0, 0 );
	}
	Sys_FindClose ();
}


/*
==============
SV_WriteLevelFile

==============
*/
void SV_WriteLevelFile (void)
{
	char	name[MAX_OSPATH];
	FILE	*f;

	Com_DPrintf(DEVELOPER_MSG_SAVE, "SV_WriteLevelFile()\n");

	Com_sprintf (name, sizeof(name), "%s/save/doscursv/%s.sv2", FS_Gamedir(), sv.name);
	f = fopen(name, "wb");
	if (!f)
	{
		Com_Printf ("Failed to open %s\n", name);
		return;
	}
	fwrite (sv.configstrings, sizeof(sv.configstrings), 1, f);
	CM_WritePortalState (f);
	fclose (f);

	Com_sprintf (name, sizeof(name), "%s/save/doscursv/%s.sav", FS_Gamedir(), sv.name);
	ge->WriteLevel (name);
}

/*
==============
SV_ReadLevelFile

==============
*/
void SV_ReadLevelFile (void)
{
	char	name[MAX_OSPATH];
	FILE	*f;

	Com_DPrintf(DEVELOPER_MSG_SAVE, "SV_ReadLevelFile()\n");

	Com_sprintf (name, sizeof(name), "%s/save/doscursv/%s.sv2", FS_Gamedir(), sv.name);
	f = fopen(name, "rb");
	if (!f)
	{
		Com_Printf ("Failed to open %s\n", name);
		return;
	}
	FS_Read (sv.configstrings, sizeof(sv.configstrings), f);
	CM_ReadPortalState (f);
	fclose (f);

	Com_sprintf (name, sizeof(name), "%s/save/doscursv/%s.sav", FS_Gamedir(), sv.name);
	ge->ReadLevel (name);
}

/*
==============
SV_WriteServerFile

==============
*/
void SV_WriteServerFile (qboolean autosave)
{
	FILE	*f;
	cvar_t	*var;
	char	fileName[MAX_OSPATH], varName[128], string[128];
	char	comment[32];
	time_t	aclock;
	struct tm	*newtime;

	Com_DPrintf(DEVELOPER_MSG_SAVE, "SV_WriteServerFile(%s)\n", autosave ? "true" : "false");

	Com_sprintf (fileName, sizeof(fileName), "%s/save/doscursv/server.ssv", FS_Gamedir());
	f = fopen (fileName, "wb");
	if (!f)
	{
		Com_Printf ("Couldn't write %s\n", fileName);
		return;
	}
	// write the comment field
	memset (comment, 0, sizeof(comment));

	if (!autosave)
	{
		time (&aclock);
		newtime = localtime (&aclock);
		Com_sprintf (comment,sizeof(comment), "%2i:%i%i %2i/%2i  ", newtime->tm_hour
			, newtime->tm_min/10, newtime->tm_min%10,
			newtime->tm_mon+1, newtime->tm_mday);
		strncat (comment, sv.configstrings[CS_NAME], sizeof(comment)-1-strlen(comment) );
	}
	else
	{	// autosaved
		Com_sprintf (comment, sizeof(comment), "ENTERING %s", sv.configstrings[CS_NAME]);
	}

	fwrite (comment, 1, sizeof(comment), f);

	// write the mapcmd
	fwrite (svs.mapcmd, 1, sizeof(svs.mapcmd), f);

	// write all CVAR_LATCH cvars
	// these will be things like coop, skill, deathmatch, etc
	for (var = cvar_vars ; var ; var=var->next)
	{
		if (!(var->flags & CVAR_LATCH))
			continue;
		if (strlen(var->name) >= sizeof(varName)-1
			|| strlen(var->string) >= sizeof(string)-1)
		{
			Com_Printf ("Cvar too long: %s = %s\n", var->name, var->string);
			continue;
		}
		memset (varName, 0, sizeof(varName));
		memset (string, 0, sizeof(string));
	//	strncpy (varName, var->name);
	//	strncpy (string, var->string);
		Q_strncpyz (varName, var->name, sizeof(varName));
		Q_strncpyz (string, var->string, sizeof(string));
		fwrite (varName, 1, sizeof(varName), f);
		fwrite (string, 1, sizeof(string), f);
	}

	fclose (f);

	// write game state
	Com_sprintf (fileName, sizeof(fileName), "%s/save/doscursv/game.ssv", FS_Gamedir());
	ge->WriteGame (fileName, autosave);
}

/*
==============
SV_ReadServerFile

==============
*/
void SV_ReadServerFile (void)
{
	FILE	*f;
	char	fileName[MAX_OSPATH], varName[128], string[128];
	char	comment[32];
	char	mapcmd[MAX_TOKEN_CHARS];

	Com_DPrintf(DEVELOPER_MSG_SAVE, "SV_ReadServerFile()\n");

	Com_sprintf (fileName, sizeof(fileName), "%s/save/doscursv/server.ssv", FS_Gamedir());
	f = fopen (fileName, "rb");
	if (!f)
	{
		Com_Printf ("Couldn't read %s\n", fileName);
		return;
	}
	// read the comment field
	FS_Read (comment, sizeof(comment), f);

	// read the mapcmd
	FS_Read (mapcmd, sizeof(mapcmd), f);

	// read all CVAR_LATCH cvars
	// these will be things like coop, skill, deathmatch, etc
	while (1)
	{
		if (!fread (varName, 1, sizeof(varName), f))
			break;
		FS_Read (string, sizeof(string), f);
		Com_DPrintf(DEVELOPER_MSG_STANDARD, "Set %s = %s\n", varName, string);
		Cvar_ForceSet (varName, string);
	}

	fclose (f);

	// start a new game fresh with new cvars
	SV_InitGame ();

//	strncpy (svs.mapcmd, mapcmd);
	Q_strncpyz (svs.mapcmd, mapcmd, sizeof(svs.mapcmd));

	// read game state
	Com_sprintf (fileName, sizeof(fileName), "%s/save/doscursv/game.ssv", FS_Gamedir());
	ge->ReadGame (fileName);
}


//=========================================================




/*
==================
SV_DemoMap_f

Puts the server in demo mode on a specific map/cinematic
==================
*/
void SV_DemoMap_f (void)
{
	SV_Map (true, Cmd_Argv(1), false );
}

/* FS: Check to see if this is a cinematic in the gamemap <string> */
qboolean SV_NotCinematic (char *map)
{
	char mapTemp[MAX_OSPATH];
	char *ch;

	Q_strncpyz(mapTemp, map, sizeof(mapTemp));
	/* FS: Not sure if DOS actually needs this, but I guess it doesn't hurt to be safe */
	Q_strlwr(mapTemp);

	if(strstr(mapTemp, ".cin") || strstr(mapTemp, ".pcx"))
	{
//		Com_Printf("Found cin\n");
		ch = strchr(mapTemp, '+');
		if (ch && (strstr(ch, ".cin") || strstr(ch, ".pcx")) ) /* FS: Stepped forward and it's level+cinematic */
		{
//			Com_Printf("Found cinematic at the end: %s\n", ch);
			return true;
		}
		else
		{
//			Com_Printf("Cinematic at the beginning, skipping autosave\n");
			return false;
		}
	}

	return true;
}

/*
==================
SV_GameMap_f

Saves the state of the map just being exited and goes to a new map.

If the initial character of the map string is '*', the next map is
in a new unit, so the current savegame directory is cleared of
map files.

Example:

*inter.cin+jail

Clears the archived maps, plays the inter.cin cinematic, then
goes to map jail.bsp.
==================
*/
void SV_GameMap_f (void)
{
	char		*map;
	int			i;
	client_t	*cl;
	qboolean	*savedInuse;

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("USAGE: gamemap <map>\n");
		return;
	}

	Com_DPrintf(DEVELOPER_MSG_SAVE, "SV_GameMap(%s)\n", Cmd_Argv(1));

	FS_CreatePath (va("%s/save/doscursv/", FS_Gamedir()));

	// check for clearing the current savegame
	map = Cmd_Argv(1);
	if (map[0] == '*')
	{
		// wipe all the *.sav files
		SV_WipeSavegame ("doscursv");
	}
	else
	{	// save the map just exited
		if (sv.state == ss_game)
		{
			// clear all the client inuse flags before saving so that
			// when the level is re-entered, the clients will spawn
			// at spawn points instead of occupying body shells
			savedInuse = malloc(maxclients->value * sizeof(qboolean));
			for (i=0,cl=svs.clients ; i<maxclients->value; i++,cl++)
			{
				savedInuse[i] = cl->edict->inuse;
				cl->edict->inuse = false;
			}

			SV_WriteLevelFile ();

			// we must restore these for clients to transfer over correctly
			for (i=0,cl=svs.clients ; i<maxclients->value; i++,cl++)
				cl->edict->inuse = savedInuse[i];
			free (savedInuse);
		}
	}

	// start up the next map
	SV_Map (false, Cmd_Argv(1), false );

	// archive server state
	strncpy (svs.mapcmd, Cmd_Argv(1), sizeof(svs.mapcmd)-1);

	// copy off the level to the autosave slot
	// Knightmare- don't do this in deathmatch or for cinematics
	if ( !dedicated->value && !Cvar_VariableValue("deathmatch") && SV_NotCinematic(map)) /* FS: Made it a function because it didn't work */
	{
		SV_WriteServerFile (true);
		SV_CopySaveGame ("doscursv", "dossv0");
	}
}

/*
==================
SV_Map_f

Goes directly to a given map without any savegame archiving.
For development work
==================
*/
void SV_Map_f (void)
{
	char	*map;
	char	expanded[MAX_QPATH];

	if (Cmd_Argc() != 2)
	{
		Com_Printf("USAGE: map <mapname>\n");
		return;
	}

	// if not a pcx, demo, or cinematic, check to make sure the level exists
	map = Cmd_Argv(1);

	if (!strchr(map, '.') && !strchr(map, '$') && (*map != '*'))
	{
		Com_sprintf (expanded, sizeof(expanded), "maps/%s.bsp", map);

		if (FS_LoadFile (expanded, NULL) == -1)
		{
			Com_Printf ("Can't find %s\n", expanded);
			return;
		}
	}

	sv.state = ss_dead;		// don't save current level when changing
	SV_WipeSavegame("doscursv");
	SV_GameMap_f ();
}

/*
=====================================================================

  SAVEGAMES

=====================================================================
*/


/*
==============
SV_Loadgame_f

==============
*/
void SV_Loadgame_f (void)
{
	char	name[MAX_OSPATH];
	FILE	*f;
	char	*dir;

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("USAGE: loadgame <directory>\n");
		return;
	}

	Com_Printf ("Loading game...\n");

	dir = Cmd_Argv(1);
	if (strstr(dir, "..") || strchr(dir, '/') || strchr(dir, '\\'))
	{
		Com_Printf ("Bad savedir.\n");
	}

	// make sure the server.ssv file exists
	Com_sprintf (name, sizeof(name), "%s/save/%s/server.ssv", FS_Gamedir(), Cmd_Argv(1));
	f = fopen (name, "rb");
	if (!f)
	{
		Com_Printf ("No such savegame: %s\n", name);
		return;
	}
	fclose (f);

	SV_CopySaveGame (Cmd_Argv(1), "doscursv");

	SV_ReadServerFile ();

	// go to the map
	sv.state = ss_dead;		// don't save current level when changing
	SV_Map (false, svs.mapcmd, true);
}



/*
==============
SV_Savegame_f

==============
*/
extern char	fs_gamedir[MAX_OSPATH];

void SV_Savegame_f (void)
{
	char	*dir;

	if (sv.state != ss_game)
	{
		Com_Printf ("You must be in a game to save.\n");
		return;
	}

	// Knightmare- fs_gamedir may be getting messed up, causing it to occasinally save in the root dir,
	// thus leading to a hang on game loads, so we reset it here.
	if (!fs_gamedir[0])
	{
		if (fs_gamedirvar->string[0])
			Com_sprintf (fs_gamedir, sizeof(fs_gamedir), "%s/%s", fs_basedir->string, fs_gamedirvar->string);
	}

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("USAGE: savegame <directory>\n");
		return;
	}

	if (Cvar_VariableValueInt("deathmatch"))
	{
		Com_Printf ("Can't savegame in a deathmatch\n");
		return;
	}

	if (!strcmp (Cmd_Argv(1), "doscursv"))
	{
		Com_Printf ("Can't save to 'doscursv'\n");
		return;
	}

	if (maxclients->intValue == 1 && svs.clients[0].edict->client->ps.stats[STAT_HEALTH] <= 0)
	{
		Com_Printf ("\nCan't savegame while dead!\n");
		return;
	}

	dir = Cmd_Argv(1);
	if (strstr(dir, "..") || strchr(dir, '/') || strchr(dir, '\\'))
	{
		Com_Printf ("Bad savedir.\n");
	}

	Com_Printf ("Saving game...\n");

	// archive current level, including all client edicts.
	// when the level is reloaded, they will be shells awaiting
	// a connecting client
	SV_WriteLevelFile ();

	// save server state
	SV_WriteServerFile (false);

	// copy it off
	SV_CopySaveGame ("doscursv", dir);

	Com_Printf ("Done.\n");
}

//===============================================================

/*
==================
SV_Kick_f

Kick a user off of the server
==================
*/
void SV_Kick_f (void)
{
	if (!svs.initialized)
	{
		Com_Printf ("No server running.\n");
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("Usage: kick <userid>\n");
		return;
	}

	if (!SV_SetPlayer ())
	{
		return;
	}

	if ((sv_client->state == cs_spawned) && *sv_client->name)
	{
	SV_BroadcastPrintf (PRINT_HIGH, "%s was kicked\n", sv_client->name);
	}
	// print directly, because the dropped client won't get the
	// SV_BroadcastPrintf message
	SV_ClientPrintf (sv_client, PRINT_HIGH, "You were kicked from the game\n");
	SV_DropClient (sv_client);
	sv_client->lastmessage = svs.realtime;	// min case there is a funny zombie
}


/*
================
SV_Status_f
================
*/
void SV_Status_f (void)
{
	int			i, j, l;
	client_t	*cl;
	char		*s;
	int			ping;
	if (!svs.clients)
	{
		Com_Printf ("No server running.\n");
		return;
	}
	Com_Printf ("map              : %s\n", sv.name);

	Com_Printf ("num score ping name            lastmsg address               qport \n");
	Com_Printf ("--- ----- ---- --------------- ------- --------------------- ------\n");
	for (i=0,cl=svs.clients ; i<maxclients->value; i++,cl++)
	{
		if (!cl->state)
		{
			continue;
		}
		Com_Printf("%2i ", i);
		Com_Printf ("%5i ", cl->edict->client->ps.stats[STAT_FRAGS]);

		if (cl->state == cs_connected)
		{
			Com_Printf ("CNCT ");
		}
		else if (cl->state == cs_zombie)
		{
			Com_Printf ("ZMBI ");
		}
		else
		{
			ping = cl->ping < 9999 ? cl->ping : 9999;
			Com_Printf ("%4i ", ping);
		}

		Com_Printf ("%s", cl->name);
		l = 16 - strlen(cl->name);
		for (j=0 ; j<l ; j++)
		{
			Com_Printf (" ");
		}

		Com_Printf ("%7i ", svs.realtime - cl->lastmessage );

		s = NET_AdrToString ( cl->netchan.remote_address);
		Com_Printf ("%s", s);
		l = 22 - strlen(s);
		for (j=0 ; j<l ; j++)
		{
			Com_Printf (" ");
		}
		
		Com_Printf ("%5i", cl->netchan.qport);

		Com_Printf ("\n");
	}
	Com_Printf ("\n");
}

/*
==================
SV_ConSay_f
==================
*/
void SV_ConSay_f(void)
{
	client_t *client;
	int		j;
	char	*p;
	char	text[1024];

	if (Cmd_Argc () < 2)
	{
		return;
	}

	if (!svs.initialized)
	{
		Com_Printf("No server running.\n");
		return;
	}

//	strncpy (text, "console: ");
	Q_strncpyz (text, "console: ", sizeof(text));
	p = Cmd_Args();

	if (*p == '"')
	{
		p++;
		p[strlen(p)-1] = 0;
	}

//	strncat(text, p);
	Q_strncatz(text, p, sizeof(text));

	for (j = 0, client = svs.clients; j < maxclients->value; j++, client++)
	{
		if (client->state != cs_spawned)
		{
			continue;
		}
		SV_ClientPrintf(client, PRINT_CHAT, "%s\n", text);
	}
}


/*
==================
SV_Heartbeat_f
==================
*/
void SV_Heartbeat_f (void)
{
	svs.last_heartbeat = -9999999;
}


/*
===========
SV_Serverinfo_f

  Examine or change the serverinfo string
===========
*/
void SV_Serverinfo_f (void)
{
	Com_Printf ("Server info settings:\n");
	Info_Print (Cvar_Serverinfo());
}


/*
===========
SV_DumpUser_f

Examine all a users info strings
===========
*/
void SV_DumpUser_f (void)
{
	if (!svs.initialized)
	{
		Com_Printf("No server running.\n");
		return;
	}

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("Usage: info <userid>\n");
		return;
	}

	if (!SV_SetPlayer ())
	{
		return;
	}

	Com_Printf ("userinfo\n");
	Com_Printf ("--------\n");
	Info_Print (sv_client->userinfo);

}

/* FS: From KMQ2 */
/*
===================
SV_StartMod
===================
*/
void SV_StartMod (char *mod)
{
	char cfgExecString[MAX_QPATH];

	// killserver, start mod, unbind keys, exec configs, and start demos
	Cbuf_AddText ("killserver\n");
	Cbuf_AddText (va("game %s\n", mod));
	Cbuf_AddText ("unbindall\n");
	Cbuf_AddText ("exec default.cfg\n");

	Com_sprintf(cfgExecString, sizeof(cfgExecString), "exec %s\n", cfg_default->string);
	Cbuf_AddText(cfgExecString);

	Cbuf_AddText ("exec autoexec.cfg\n");
	Cbuf_AddText ("d1\n");
}

/*
===================
SV_ChangeGame_f

switch to a different mod
===================
*/
void SV_ChangeGame_f (void)
{
	if (Cmd_Argc() < 2)
	{
		Com_Printf ("changegame <gamedir> : change game directory\n");
		return;
	}
	SV_StartMod (Cmd_Argv(1));
}

/*
==============
SV_ServerRecord_f

Begins server demo recording.  Every entity and every message will be
recorded, but no playerinfo will be stored.  Primarily for demo merging.
==============
*/
void SV_ServerRecord_f (void)
{
	char	name[MAX_OSPATH];
	byte buf_data[32768];
	sizebuf_t	buf;
	int		len;
	int		i;

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("serverrecord <demoname>\n");
		return;
	}

	if (svs.demofile)
	{
		Com_Printf ("Already recording.\n");
		return;
	}

	if (sv.state != ss_game)
	{
		Com_Printf ("You must be in a level to record.\n");
		return;
	}

	if (strstr(Cmd_Argv(1), "..") || 
		strchr(Cmd_Argv(1), '/') || 
		strchr(Cmd_Argv(1), '\\'))
	{
		Com_Printf("Illegal filename.\n");
		return;
	}
	//
	// open the demo file
	//
	Com_sprintf (name, sizeof(name), "%s/demos/%s.dm2", FS_Gamedir(), Cmd_Argv(1));

	Com_Printf ("recording to %s.\n", name);
	FS_CreatePath (name);
	svs.demofile = fopen (name, "wb");
	if (!svs.demofile)
	{
		Com_Printf ("ERROR: couldn't open.\n");
		return;
	}

	// setup a buffer to catch all multicasts
	SZ_Init (&svs.demo_multicast, svs.demo_multicast_buf, sizeof(svs.demo_multicast_buf));

	//
	// write a single giant fake message with all the startup info
	//
	SZ_Init (&buf, buf_data, sizeof(buf_data));

	//
	// serverdata needs to go over for all types of servers
	// to make sure the protocol is right, and to set the gamedir
	//
	// send the serverdata
	MSG_WriteByte (&buf, svc_serverdata);
	MSG_WriteLong (&buf, PROTOCOL_VERSION);
	MSG_WriteLong (&buf, svs.spawncount);
	// 2 means server demo
	MSG_WriteByte (&buf, 2);	// demos are always attract loops
	MSG_WriteString(&buf, (char *)Cvar_VariableString("gamedir"));
	MSG_WriteShort (&buf, -1);
	// send full levelname
	MSG_WriteString (&buf, sv.configstrings[CS_NAME]);

	for (i=0 ; i<MAX_CONFIGSTRINGS ; i++)
	{
		if (sv.configstrings[i][0])
		{
			MSG_WriteByte (&buf, svc_configstring);
			MSG_WriteShort (&buf, i);
			MSG_WriteString (&buf, sv.configstrings[i]);

			if (buf.cursize + 67 >= buf.maxsize)
			{
				Com_Printf("not enough buffer space available.\n");
				fclose(svs.demofile);
				svs.demofile = NULL;
				return;
			}
		}
	}

	// write it to the demo file
	Com_DPrintf(DEVELOPER_MSG_NET, "signon message length: %i\n", buf.cursize);
	len = LittleLong (buf.cursize);
	fwrite (&len, 4, 1, svs.demofile);
	fwrite (buf.data, buf.cursize, 1, svs.demofile);

	// the rest of the demo file will be individual frames
}


/*
==============
SV_ServerStop_f

Ends server demo recording
==============
*/
void SV_ServerStop_f (void)
{
	if (!svs.demofile)
	{
		Com_Printf ("Not doing a serverrecord.\n");
		return;
	}
	fclose (svs.demofile);
	svs.demofile = NULL;
	Com_Printf ("Recording completed.\n");
}


/*
===============
SV_KillServer_f

Kick everyone off, possibly in preparation for a new game

===============
*/
void SV_KillServer_f (void)
{
	if (!svs.initialized)
	{
		return;
	}
	SV_Shutdown ("Server was killed.\n", false);
	NET_Config ( false );	// close network sockets
}

/*
===============
SV_ServerCommand_f

Let the game dll handle a command
===============
*/
void SV_ServerCommand_f (void)
{
	if (!ge)
	{
		Com_Printf ("No game loaded.\n");
		return;
	}

	ge->ServerCommand();
}

char *SV_MapName()
{
	if( !svs.initialized )
	{
		return NULL;
	}

	return sv.name;
}

/* FS: Dump Entities to *,ent file for server-side modding */
void SV_DumpEntities_f (void)
{
    FILE    *f;
    char    fileName[MAX_OSPATH];
	extern	char	entBSP[MAX_MAP_ENTSTRING];

	if (!svs.initialized)
	{
		Com_Printf ("No server running.\n");
		return;
	}

    Com_sprintf (fileName, sizeof(fileName), "%s/maps/%s.ent", FS_Gamedir(), SV_MapName());
	FS_CreatePath(fileName); /* FS: Create the /maps dir if it's not there */

    f = fopen (fileName, "w");

    if (!f)
    {
        Com_Printf("SV_DumpEntities_f: Couldn't write %s.\n", fileName);
    }
    else
    {
        Com_Printf("SV_DumpEntities_f: Dumping Entities to %s.\n", fileName);
        fputs(entBSP, f);
        fclose(f);
    }
}

//===========================================================

/*
==================
SV_InitOperatorCommands
==================
*/
void SV_InitOperatorCommands (void)
{
	Cmd_AddCommand ("heartbeat", SV_Heartbeat_f);
	Cmd_AddCommand ("kick", SV_Kick_f);
	Cmd_AddCommand ("status", SV_Status_f);
	Cmd_AddCommand ("serverinfo", SV_Serverinfo_f);
	Cmd_AddCommand ("dumpuser", SV_DumpUser_f);

	Cmd_AddCommand ("changegame", SV_ChangeGame_f); // Knightmare added

	Cmd_AddCommand ("map", SV_Map_f);
	Cmd_AddCommand ("demomap", SV_DemoMap_f);
	Cmd_AddCommand ("gamemap", SV_GameMap_f);
	Cmd_AddCommand ("setmaster", SV_SetMaster_f);

	if (dedicated->intValue)
	{
		Cmd_AddCommand ("say", SV_ConSay_f);
	}

	Cmd_AddCommand ("serverrecord", SV_ServerRecord_f);
	Cmd_AddCommand ("serverstop", SV_ServerStop_f);

	Cmd_AddCommand ("save", SV_Savegame_f);
	Cmd_AddCommand ("load", SV_Loadgame_f);

	Cmd_AddCommand ("killserver", SV_KillServer_f);

	Cmd_AddCommand ("sv", SV_ServerCommand_f);
	Cmd_AddCommand ("sv_dumpentities", SV_DumpEntities_f); /* FS */
}

