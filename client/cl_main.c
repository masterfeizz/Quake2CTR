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
// cl_main.c  -- client main loop

#include "client.h"

#ifdef GAMESPY
#include "../client/gspy.h"
#endif

cvar_t	*cl_3dcam;
cvar_t	*cl_3dcam_angle;
cvar_t	*cl_3dcam_dist;

cvar_t	*freelook;

cvar_t	*adr0;
cvar_t	*adr1;
cvar_t	*adr2;
cvar_t	*adr3;
cvar_t	*adr4;
cvar_t	*adr5;
cvar_t	*adr6;
cvar_t	*adr7;
cvar_t	*adr8;

cvar_t	*cl_stereo_separation;
cvar_t	*cl_stereo;

cvar_t	*rcon_client_password;
cvar_t	*rcon_address;

cvar_t	*cl_noskins;
cvar_t	*cl_footsteps;
cvar_t	*cl_timeout;
cvar_t	*cl_predict;
//cvar_t	*cl_minfps;
cvar_t	*cl_maxfps;

// Knightmare
#ifdef CLIENT_SPLIT_NETFRAME
cvar_t	*cl_async;
cvar_t	*net_maxfps;
cvar_t	*r_maxfps;
#endif

cvar_t	*cl_gun;
// Knightmare- whether to try to play OGGs instead of CD tracks
cvar_t	*cl_ogg_music;
cvar_t	*cl_rogue_music; // whether to play Rogue tracks
cvar_t	*cl_xatrix_music; // whether to play Xatrix tracks

cvar_t	*cl_wav_music;

cvar_t	*cl_add_particles;
cvar_t	*cl_add_lights;
cvar_t	*cl_add_entities;
cvar_t	*cl_add_blend;

cvar_t	*cl_shownet;
cvar_t	*cl_showmiss;
cvar_t	*cl_showclamp;

cvar_t	*cl_paused;
cvar_t	*cl_timedemo;

cvar_t	*lookspring;
cvar_t	*lookstrafe;
cvar_t	*sensitivity;

cvar_t	*m_pitch;
cvar_t	*m_yaw;
cvar_t	*m_forward;
cvar_t	*m_side;

cvar_t	*cl_lightlevel;

//
// userinfo
//
cvar_t	*info_password;
cvar_t	*info_spectator;
cvar_t	*name;
cvar_t	*skin;
cvar_t	*rate;
cvar_t	*fov;
cvar_t	*fov_adapt;
cvar_t	*msg;
cvar_t	*hand;
cvar_t	*gender;
cvar_t	*gender_auto;

cvar_t	*cl_vwep;
cvar_t	*console_old_complete; /* FS: Old style command completing */
cvar_t	*cl_autorepeat_allkeys; /* FS: So I can autorepeat whatever I want, hoss. */
cvar_t	*cl_sleep; /* Knightmare: Added */
cvar_t	*cl_stufftext_check; /* FS: Added */

#ifdef GAMESPY
/* FS: Gamespy CVARs */
static cvar_t	*cl_master_server_ip;
static cvar_t	*cl_master_server_port;
static cvar_t	*cl_master_server_queries;
static cvar_t	*cl_master_server_timeout;
static cvar_t	*cl_master_server_retries;
static cvar_t	*cl_master_server_optout;
/*static */cvar_t	*s_gamespy_sounds;
#endif

client_static_t	cls;
client_state_t	cl;

centity_t		cl_entities[MAX_EDICTS];

entity_state_t	cl_parse_entities[MAX_PARSE_ENTITIES];

extern	cvar_t *allow_download;
extern	cvar_t *allow_download_players;
extern	cvar_t *allow_download_models;
extern	cvar_t *allow_download_sounds;
extern	cvar_t *allow_download_maps;


#ifdef GAMESPY
/* FS: For Gamespy */
static	gspyexport_t	*gspye = NULL;
static	GServerList	serverlist = NULL;
static	int		gspyCur;
gamespyBrowser_t browserList[MAX_SERVERS]; /* FS: Browser list for active servers */
gamespyBrowser_t browserListAll[MAX_SERVERS]; /* FS: Browser list for ALL servers */

static void GameSpy_Async_Think(void);
static void ListCallBack(GServerList lst, int msg, void *instance, void *param1, void *param2);
static void CL_Gspystop_f (void);
       void CL_PingNetServers_f (void);
static void CL_PrintBrowserList_f (void);
static void CL_LoadGameSpy (void);
#endif

#ifdef __DJGPP__
void Sys_Memory_Stats_f (void); /* FS: Added */
#endif

/*
==========================
ClampCvar
==========================
*/
float ClampCvar (float min, float max, float value)
{
	if ( value < min ) return min;
	if ( value > max ) return max;
	return value;
}

//======================================================================


/*
====================
CL_WriteDemoMessage

Dumps the current net message, prefixed by the length
====================
*/
void CL_WriteDemoMessage (void)
{
	int		len, swlen;

	// the first eight bytes are just packet sequencing stuff
	len = net_message.cursize-8;
	swlen = LittleLong(len);
	fwrite (&swlen, 4, 1, cls.demofile);
	fwrite (net_message.data+8,	len, 1, cls.demofile);
}


/*
====================
CL_Stop_f

stop recording a demo
====================
*/
void CL_Stop_f (void)
{
	int		len;

	if (!cls.demorecording)
	{
		Com_Printf ("Not recording a demo.\n");
		return;
	}

// finish up
	len = -1;
	fwrite (&len, 4, 1, cls.demofile);
	fclose (cls.demofile);
	cls.demofile = NULL;
	cls.demorecording = false;
	Com_Printf ("Stopped demo.\n");
}

/*
====================
CL_Record_f

record <demoname>

Begins recording a demo from the current position
====================
*/
void CL_Record_f (void)
{
	char	name[MAX_OSPATH];
	byte	buf_data[MAX_MSGLEN];
	sizebuf_t	buf;
	int		i;
	int		len;
	entity_state_t	*ent;
	entity_state_t	nullstate;

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("record <demoname>\n");
		return;
	}

	if (cls.demorecording)
	{
		Com_Printf ("Already recording.\n");
		return;
	}

	if (cls.state != ca_active)
	{
		Com_Printf ("You must be in a level to record.\n");
		return;
	}

	//
	// open the demo file
	//
	Com_sprintf (name, sizeof(name), "%s/demos/%s.dm2", FS_Gamedir(), Cmd_Argv(1));

	Com_Printf ("recording to %s.\n", name);
	FS_CreatePath (name);
	cls.demofile = fopen (name, "wb");
	if (!cls.demofile)
	{
		Com_Printf ("ERROR: couldn't open.\n");
		return;
	}
	cls.demorecording = true;

	// don't start saving messages until a non-delta compressed message is received
	cls.demowaiting = true;

	//
	// write out messages to hold the startup information
	//
	SZ_Init (&buf, buf_data, sizeof(buf_data));

	// send the serverdata
	MSG_WriteByte (&buf, svc_serverdata);
	MSG_WriteLong (&buf, PROTOCOL_VERSION);
	MSG_WriteLong (&buf, 0x10000 + cl.servercount);
	MSG_WriteByte (&buf, 1);	// demos are always attract loops
	MSG_WriteString (&buf, cl.gamedir);
	MSG_WriteShort (&buf, cl.playernum);

	MSG_WriteString (&buf, cl.configstrings[CS_NAME]);

	// configstrings
	for (i=0 ; i<MAX_CONFIGSTRINGS ; i++)
	{
		if (cl.configstrings[i][0])
		{
			if (buf.cursize + strlen (cl.configstrings[i]) + 32 > buf.maxsize)
			{	// write it out
				len = LittleLong (buf.cursize);
				fwrite (&len, 4, 1, cls.demofile);
				fwrite (buf.data, buf.cursize, 1, cls.demofile);
				buf.cursize = 0;
			}

			MSG_WriteByte (&buf, svc_configstring);
			MSG_WriteShort (&buf, i);
			MSG_WriteString (&buf, cl.configstrings[i]);
		}
	}

	// baselines
	memset (&nullstate, 0, sizeof(nullstate));
	for (i=0; i<MAX_EDICTS ; i++)
	{
		ent = &cl_entities[i].baseline;
		if (!ent->modelindex)
			continue;

		if (buf.cursize + 64 > buf.maxsize)
		{	// write it out
			len = LittleLong (buf.cursize);
			fwrite (&len, 4, 1, cls.demofile);
			fwrite (buf.data, buf.cursize, 1, cls.demofile);
			buf.cursize = 0;
		}

		MSG_WriteByte (&buf, svc_spawnbaseline);		
		MSG_WriteDeltaEntity (&nullstate, &cl_entities[i].baseline, &buf, true, true);
	}

	MSG_WriteByte (&buf, svc_stufftext);
	MSG_WriteString (&buf, "precache\n");

	// write it to the demo file

	len = LittleLong (buf.cursize);
	fwrite (&len, 4, 1, cls.demofile);
	fwrite (buf.data, buf.cursize, 1, cls.demofile);

	// the rest of the demo file will be individual frames
}

//======================================================================

/*
===================
Cmd_ForwardToServer

adds the current command line as a clc_stringcmd to the client message.
things like godmode, noclip, etc, are commands directed to the server,
so when they are typed in at the console, they will need to be forwarded.
===================
*/
void Cmd_ForwardToServer (void)
{
	char	*cmd;

	cmd = Cmd_Argv(0);
	if (cls.state <= ca_connected || *cmd == '-' || *cmd == '+')
	{
		Com_Printf ("Unknown command \"%s\"\n", cmd);
		return;
	}

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	SZ_Print (&cls.netchan.message, cmd);
	if (Cmd_Argc() > 1)
	{
		SZ_Print (&cls.netchan.message, " ");
		SZ_Print (&cls.netchan.message, Cmd_Args());
	}
	cls.forcePacket = true;
}

void CL_Setenv_f( void )
{
	int argc = Cmd_Argc();

	if ( argc > 2 )
	{
		char buffer[1000];
		int i;

		strcpy( buffer, Cmd_Argv(1) );
		strcat( buffer, "=" );

		for ( i = 2; i < argc; i++ )
		{
			strcat( buffer, Cmd_Argv( i ) );
			strcat( buffer, " " );
		}

		putenv( buffer );
	}
	else if ( argc == 2 )
	{
		char *env = getenv( Cmd_Argv(1) );

		if ( env )
		{
			Com_Printf( "%s=%s\n", Cmd_Argv(1), env );
		}
		else
		{
			Com_Printf( "%s undefined\n", Cmd_Argv(1) );
		}
	}
}


/*
==================
CL_ForwardToServer_f
==================
*/
void CL_ForwardToServer_f (void)
{
	if (cls.state != ca_connected && cls.state != ca_active)
	{
		Com_Printf ("Can't \"%s\", not connected\n", Cmd_Argv(0));
		return;
	}
	
	// don't forward the first argument
	if (Cmd_Argc() > 1)
	{
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, Cmd_Args());
		cls.forcePacket = true;
	}
}


/*
==================
CL_Pause_f
==================
*/
void CL_Pause_f (void)
{
	// never pause in multiplayer
	if (Cvar_VariableValue ("maxclients") > 1 || !Com_ServerState ())
	{
		Cvar_SetValue ("paused", 0);
		return;
	}

	Cvar_SetValue ("paused", !cl_paused->value);
}

/*
==================
CL_Quit_f
==================
*/
void CL_Quit_f (void)
{
	CL_Disconnect ();
	Com_Quit ();
}

/*
================
CL_Drop

Called after an ERR_DROP was thrown
================
*/
void CL_Drop (void)
{
	if (cls.state == ca_uninitialized)
		return;
	if (cls.state == ca_disconnected)
		return;

	CL_Disconnect ();

	// drop loading plaque unless this is the initial game start
	if (cls.disable_servercount != -1)
		SCR_EndLoadingPlaque ();	// get rid of loading plaque
}


/*
=======================
CL_SendConnectPacket

We have gotten a challenge from the server, so try and
connect.
======================
*/
void CL_SendConnectPacket (void)
{
	netadr_t	adr;
	int		port;

	if (!NET_StringToAdr (cls.servername, &adr))
	{
		Com_Printf ("Bad server address\n");
		cls.connect_time = 0;
		return;
	}
	if (adr.port == 0)
		adr.port = BigShort (PORT_SERVER);

	port = Cvar_VariableValue ("qport");
	userinfo_modified = false;

	Netchan_OutOfBandPrint (NS_CLIENT, adr, "connect %i %i %i \"%s\"\n",
		PROTOCOL_VERSION, port, cls.challenge, Cvar_Userinfo() );
}

/*
=================
CL_CheckForResend

Resend a connect message if the last one has timed out
=================
*/
void CL_CheckForResend (void)
{
	netadr_t	adr;

	// if the local server is running and we aren't
	// then connect
	if (cls.state == ca_disconnected && Com_ServerState() )
	{
		cls.state = ca_connecting;
		strncpy (cls.servername, "localhost", sizeof(cls.servername)-1);
		// we don't need a challenge on the localhost
		CL_SendConnectPacket ();
		return;
//		cls.connect_time = -99999;	// CL_CheckForResend() will fire immediately
	}

	// resend if we haven't gotten a reply yet
	if (cls.state != ca_connecting)
		return;

	if (cls.realtime - cls.connect_time < 3000)
		return;

	if (!NET_StringToAdr (cls.servername, &adr))
	{
		Com_Printf ("Bad server address\n");
		cls.state = ca_disconnected;
		return;
	}
	if (adr.port == 0)
		adr.port = BigShort (PORT_SERVER);

	cls.connect_time = cls.realtime;	// for retransmit requests

	Com_Printf ("Connecting to %s...\n", cls.servername);

	Netchan_OutOfBandPrint (NS_CLIENT, adr, "getchallenge\n");
}


/*
================
CL_Connect_f

================
*/
void CL_Connect_f (void)
{
	char	*server;

	if (Cmd_Argc() != 2)
	{
		Com_Printf ("usage: connect <server>\n");
		return;	
	}
	
	if (Com_ServerState ())
	{	// if running a local server, kill it and reissue
		SV_Shutdown ("Server quit\n", false);
	}
	else
	{
		CL_Disconnect ();
	}

	server = Cmd_Argv (1);

	NET_Config (true);		// allow remote

	CL_Disconnect ();

	cls.state = ca_connecting;
	strncpy (cls.servername, server, sizeof(cls.servername)-1);
	cls.connect_time = -99999;	// CL_CheckForResend() will fire immediately
}


/*
=====================
CL_Rcon_f

  Send the rest of the command line over as
  an unconnected command.
=====================
*/
void CL_Rcon_f (void)
{
	char	message[1024];
	int		i;
	netadr_t	to;

	if (!rcon_client_password->string)
	{
		Com_Printf ("You must set 'rcon_password' before\n"
					"issuing an rcon command.\n");
		return;
	}

	message[0] = (char)255;
	message[1] = (char)255;
	message[2] = (char)255;
	message[3] = (char)255;
	message[4] = 0;

	NET_Config (true);		// allow remote

	strcat (message, "rcon ");

	strcat (message, rcon_client_password->string);
	strcat (message, " ");

	for (i=1 ; i<Cmd_Argc() ; i++)
	{
		strcat (message, Cmd_Argv(i));
		strcat (message, " ");
	}

	if (cls.state >= ca_connected)
		to = cls.netchan.remote_address;
	else
	{
		if (!strlen(rcon_address->string))
		{
			Com_Printf ("You must either be connected,\n"
						"or set the 'rcon_address' cvar\n"
						"to issue rcon commands\n");

			return;
		}
		NET_StringToAdr (rcon_address->string, &to);
		if (to.port == 0)
			to.port = BigShort (PORT_SERVER);
	}
	
	NET_SendPacket (NS_CLIENT, strlen(message)+1, message, to);
}


/*
=====================
CL_ClearState

=====================
*/
void CL_ClearState (void)
{
	S_StopAllSounds ();
	CL_ClearEffects ();
	CL_ClearTEnts ();

// wipe the entire cl structure
	memset (&cl, 0, sizeof(cl));
	memset (&cl_entities, 0, sizeof(cl_entities));

	SZ_Clear (&cls.netchan.message);
}

/*
=====================
CL_Disconnect

Goes from a connected state to full screen console state
Sends a disconnect message to the server
This is also called on Com_Error, so it shouldn't cause any errors
=====================
*/
void CL_Disconnect (void)
{
	byte	final[32];

	if (cls.state == ca_disconnected)
		return;

	if (cl_timedemo && cl_timedemo->value)
	{
		int	time;
		
		time = Sys_Milliseconds () - cl.timedemo_start;
		if (time > 0)
			Com_Printf ("%i frames, %3.1f seconds: %3.1f fps\n", cl.timedemo_frames,
			time/1000.0, cl.timedemo_frames*1000.0 / time);
	}

	VectorClear (cl.refdef.blend);
	re.CinematicSetPalette(NULL);

	M_ForceMenuOff ();

	cls.connect_time = 0;

	SCR_StopCinematic ();

	if (cls.demorecording)
		CL_Stop_f ();

	// send a disconnect message to the server
	final[0] = clc_stringcmd;
	strcpy ((char *)final+1, "disconnect");
	Netchan_Transmit (&cls.netchan, strlen((char *)final), final);
	Netchan_Transmit (&cls.netchan, strlen((char *)final), final);
	Netchan_Transmit (&cls.netchan, strlen((char *)final), final);

	CL_ClearState ();

	// stop download
	if (cls.download) {
		fclose(cls.download);
		cls.download = NULL;
	}

#ifdef USE_CURL /* HTTP downloading from R1Q2 */
	CL_CancelHTTPDownloads (true);
	cls.downloadReferer[0] = 0;
	cls.downloadname[0] = 0;
	cls.downloadposition = 0;
	cls.downloadrate = 0.0f;
	CL_Download_Reset_KBps_counter();
#endif

	cls.state = ca_disconnected;
}

void CL_Disconnect_f (void)
{
	Com_Error (ERR_DROP, "Disconnected from server");
}

/*
====================
CL_Packet_f

packet <destination> <contents>

Contents allows \n escape character
====================
*/
void CL_Packet_f (void)
{
	char	send[2048];
	int		i, l;
	char	*in, *out;
	netadr_t	adr;

	if (Cmd_Argc() != 3)
	{
		Com_Printf ("packet <destination> <contents>\n");
		return;
	}

	NET_Config (true);		// allow remote

	if (!NET_StringToAdr (Cmd_Argv(1), &adr))
	{
		Com_Printf ("Bad address\n");
		return;
	}
	if (!adr.port)
		adr.port = BigShort (PORT_SERVER);

	in = Cmd_Argv(2);
	out = send+4;
	send[0] = send[1] = send[2] = send[3] = (char)0xff;

	l = strlen (in);
	for (i=0 ; i<l ; i++)
	{
		if (in[i] == '\\' && in[i+1] == 'n')
		{
			*out++ = '\n';
			i++;
		}
		else
			*out++ = in[i];
	}
	*out = 0;

	NET_SendPacket (NS_CLIENT, out-send, send, adr);
}

/*
=================
CL_Changing_f

Just sent as a hint to the client that they should
drop to full console
=================
*/
void CL_Changing_f (void)
{
	//ZOID
	//if we are downloading, we don't change!  This so we don't suddenly stop downloading a map
	if (cls.download)
		return;

	SCR_BeginLoadingPlaque ();
	cls.state = ca_connected;	// not active anymore, but not disconnected
	Com_Printf ("\nChanging map...\n");

#ifdef USE_CURL
	if(cls.downloadServerRetry[0] != 0) /* FS: Added because Whale's WOD HTTP server rejects you after a lot of 404s.  Then you lose HTTP until a hard reconnect. */
	{
		CL_SetHTTPServer(cls.downloadServerRetry);
	}
#endif
}


/*
=================
CL_Reconnect_f

The server is changing levels
=================
*/
void CL_Reconnect_f (void)
{
	//ZOID
	//if we are downloading, we don't change!  This so we don't suddenly stop downloading a map
	if (cls.download)
		return;

	S_StopAllSounds ();
	if (cls.state == ca_connected) {
		Com_Printf ("reconnecting...\n");
		cls.state = ca_connected;
		MSG_WriteChar (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "new");		
		cls.forcePacket = true;
		return;
	}

	if (*cls.servername) {
		if (cls.state >= ca_connected) {
			CL_Disconnect();
			cls.connect_time = cls.realtime - 1500;
		} else
			cls.connect_time = -99999; // fire immediately

		cls.state = ca_connecting;
		Com_Printf ("reconnecting...\n");
	}
}

/*
=================
CL_ParseStatusMessage

Handle a reply from a ping
=================
*/
void CL_ParseStatusMessage (void)
{
	char	*s;

	s = MSG_ReadString(&net_message);

	Com_Printf ("%s\n", s);
	M_AddToServerList (net_from, s);
}


/*
=================
CL_PingServers_f
=================
*/
void CL_PingServers_f (void)
{
	int			i;
	netadr_t	adr;
	char		name[32];
	char		*adrstring;
	cvar_t		*noudp;
	cvar_t		*noipx;

	NET_Config (true);		// allow remote

	// send a broadcast packet
	Com_Printf ("pinging broadcast...\n");

	noudp = Cvar_Get ("noudp", "0", CVAR_NOSET);
	if (!noudp->intValue)
	{
		adr.type = NA_BROADCAST;
		adr.port = BigShort(PORT_SERVER);
		Netchan_OutOfBandPrint (NS_CLIENT, adr, va("info %i", PROTOCOL_VERSION));
	}

	noipx = Cvar_Get ("noipx", "0", CVAR_NOSET);
	if (!noipx->intValue)
	{
		adr.type = NA_BROADCAST_IPX;
		adr.port = BigShort(PORT_SERVER);
		Netchan_OutOfBandPrint (NS_CLIENT, adr, va("info %i", PROTOCOL_VERSION));
	}

	// send a packet to each address book entry
	for (i=0 ; i<16 ; i++)
	{
		Com_sprintf (name, sizeof(name), "adr%i", i);
		adrstring = Cvar_VariableString (name);
		if (!adrstring || !adrstring[0])
			continue;

		Com_Printf ("pinging %s...\n", adrstring);
		if (!NET_StringToAdr (adrstring, &adr))
		{
			Com_Printf ("Bad address: %s\n", adrstring);
			continue;
		}
		if (!adr.port)
			adr.port = BigShort(PORT_SERVER);
		Netchan_OutOfBandPrint (NS_CLIENT, adr, va("info %i", PROTOCOL_VERSION));
	}
}


/*
=================
CL_Skins_f

Load or download any custom player skins and models
=================
*/
void CL_Skins_f (void)
{
	int		i;

	for (i=0 ; i<MAX_CLIENTS ; i++)
	{
		if (!cl.configstrings[CS_PLAYERSKINS+i][0])
			continue;
		Com_Printf ("client %i: %s\n", i, cl.configstrings[CS_PLAYERSKINS+i]); 
		SCR_UpdateScreen ();
		Sys_SendKeyEvents ();	// pump message loop
		CL_ParseClientinfo (i);
	}
}


/*
=================
CL_ConnectionlessPacket

Responses to broadcasts, etc
=================
*/
void CL_ConnectionlessPacket (void)
{
	char	*s;
	char	*c;
	// HTTP downloading from R1Q2
	char	*buff, *p;
	int		i;
	// end HTTP downloading from R1Q2

	MSG_BeginReading (&net_message);
	MSG_ReadLong (&net_message);	// skip the -1

	s = MSG_ReadStringLine (&net_message);

	Cmd_TokenizeString (s, false);

	c = Cmd_Argv(0);

	Com_Printf ("%s: %s\n", NET_AdrToString (net_from), c);

	// server connection
	if (!strcmp(c, "client_connect"))
	{
		if (cls.state == ca_connected)
		{
			Com_Printf ("Dup connect received.  Ignored.\n");
			return;
		}
		Netchan_Setup (NS_CLIENT, &cls.netchan, net_from, cls.quakePort);
		// HTTP downloading from R1Q2
		buff = NET_AdrToString(cls.netchan.remote_address);
		for (i = 1; i < Cmd_Argc(); i++)
		{
			p = Cmd_Argv(i);
			if ( !strncmp (p, "dlserver=", 9) )
			{
#ifdef USE_CURL
				p += 9;
				Com_sprintf (cls.downloadReferer, sizeof(cls.downloadReferer), "quake2://%s", buff);
				CL_SetHTTPServer (p);
				if ( cls.downloadServer[0] )
					Com_Printf ("HTTP downloading enabled, URL: %s\n", cls.downloadServer);
#else
				Com_Printf ("HTTP downloading supported by server but this client was built without libcurl.\n");
#endif	/* USE_CURL */
			}
		}
		// end HTTP downloading from R1Q2
		MSG_WriteChar (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, "new");	
		cls.state = ca_connected;
		cls.forcePacket = true;
		return;
	}

	// server responding to a status broadcast
	if (!strcmp(c, "info"))
	{
		CL_ParseStatusMessage ();
		return;
	}

	// remote command from gui front end
	if (!strcmp(c, "cmd"))
	{
		if (!NET_IsLocalAddress(net_from))
		{
			Com_Printf ("Command packet from remote host.  Ignored.\n");
			return;
		}
		Sys_AppActivate ();
		s = MSG_ReadString (&net_message);
		Cbuf_AddText (s);
		Cbuf_AddText ("\n");
		return;
	}
	// print command from somewhere
	if (!strcmp(c, "print"))
	{
		s = MSG_ReadString (&net_message);
		Com_Printf ("%s", s);
		return;
	}

	// ping from somewhere
	if (!strcmp(c, "ping"))
	{
		Netchan_OutOfBandPrint (NS_CLIENT, net_from, "ack");
		return;
	}

	// challenge from the server we are connecting to
	if (!strcmp(c, "challenge"))
	{
		cls.challenge = atoi(Cmd_Argv(1));
		CL_SendConnectPacket ();
		return;
	}

	// echo request from server
	if (!strcmp(c, "echo"))
	{
		Netchan_OutOfBandPrint (NS_CLIENT, net_from, "%s", Cmd_Argv(1) );
		return;
	}

	Com_Printf ("Unknown command.\n");
}


/*
=================
CL_DumpPackets

A vain attempt to help bad TCP stacks that cause problems
when they overflow
=================
*/
void CL_DumpPackets (void)
{
	while (NET_GetPacket (NS_CLIENT, &net_from, &net_message))
	{
		Com_Printf ("dumnping a packet\n");
	}
}

/*
=================
CL_ReadPackets
=================
*/
void CL_ReadPackets (void)
{
	while (NET_GetPacket (NS_CLIENT, &net_from, &net_message))
	{
//		Com_Printf ("packet\n");
		//
		// remote command packet
		//
		if (*(int *)net_message.data == -1)
		{
			CL_ConnectionlessPacket ();
			continue;
		}

		if (cls.state == ca_disconnected || cls.state == ca_connecting)
			continue;		// dump it if not connected

		if (net_message.cursize < 8)
		{
			Com_Printf ("%s: Runt packet\n",NET_AdrToString(net_from));
			continue;
		}

		//
		// packet from server
		//
		if (!NET_CompareAdr (net_from, cls.netchan.remote_address))
		{
			Com_DPrintf(DEVELOPER_MSG_NET, "%s:sequenced packet without connection\n"
				,NET_AdrToString(net_from));
			continue;
		}
		if (!Netchan_Process(&cls.netchan, &net_message))
			continue;		// wasn't accepted for some reason
		CL_ParseServerMessage ();
	}

	//
	// check timeout
	//
	if (cls.state >= ca_connected
	 && cls.realtime - cls.netchan.last_received > cl_timeout->value*1000)
	{
		if (++cl.timeoutcount > 5)	// timeoutcount saves debugger
		{
			Com_Printf ("\nServer connection timed out.\n");
			CL_Disconnect ();
			return;
		}
	}
	else
		cl.timeoutcount = 0;
	
}


//=============================================================================

/*
==============
CL_FixUpGender_f
==============
*/
void CL_FixUpGender(void)
{
	char *p;
	char sk[80];

	if (gender_auto->intValue) {

		if (gender->modified) {
			// was set directly, don't override the user
			gender->modified = false;
			return;
		}

		strncpy(sk, skin->string, sizeof(sk) - 1);
		if ((p = strchr(sk, '/')) != NULL)
			*p = 0;
		if (Q_stricmp(sk, "male") == 0 || Q_stricmp(sk, "cyborg") == 0)
			Cvar_Set ("gender", "male");
		else if (Q_stricmp(sk, "female") == 0 || Q_stricmp(sk, "crackhor") == 0)
			Cvar_Set ("gender", "female");
		else
			Cvar_Set ("gender", "none");
		gender->modified = false;
	}
}

/*
==============
CL_Userinfo_f
==============
*/
void CL_Userinfo_f (void)
{
	Com_Printf ("User info settings:\n");
	Info_Print (Cvar_Userinfo());
}

/*
=================
CL_Snd_Restart_f

Restart the sound subsystem so it can pick up
new parameters and flush all sounds
=================
*/
void CL_Snd_Restart_f (void)
{
	S_StopAllSounds(); /* FS: Clear GUS Buffer */
	S_Shutdown ();
	S_Init ();
	CL_RegisterSounds ();
}

int precache_check; // for autodownload of precache items
int precache_spawncount;
int precache_tex;
int precache_model_skin;
int precache_pak; // Knightmare

byte *precache_model; // used for skin checking in alias models

#define PLAYER_MULT 5

// ENV_CNT is map load, ENV_CNT+1 is first env map
#define ENV_CNT (CS_PLAYERSKINS + MAX_CLIENTS * PLAYER_MULT)
#define TEXTURE_CNT (ENV_CNT+13)

static const char *env_suf[6] = {"rt", "bk", "lf", "ft", "up", "dn"};

/*
=================
CL_ResetPrecacheCheck
=================
*/
void CL_ResetPrecacheCheck (void)
{
//	precache_start_time = Sys_Milliseconds();

	precache_check = CS_MODELS;
//	precache_spawncount = atoi(Cmd_Argv(1));
	precache_model = 0;
	precache_model_skin = 0;
	precache_pak = 0;	// Knightmare added
}

void CL_RequestNextDownload (void)
{
	unsigned	map_checksum;		// for detecting cheater maps
	char fn[MAX_OSPATH];
	dmdl_t *pheader;
	dsprite_t	*spriteheader;
	char		*skinname;
	qboolean	localServer = false;	// Knightmare added

	if (cls.state != ca_connected)
		return;

	// Knightmare 11/17/13- new download check
	if ( Com_ServerState() )	// no downloading from local server!
		localServer = true;

	// skip to loading map if downloads disabled or on local server
	if ( (localServer || !allow_download->intValue) && (precache_check < ENV_CNT) )
		precache_check = ENV_CNT;

//ZOID
	if (precache_check == CS_MODELS) { // confirm map
		precache_check = CS_MODELS+2; // 0 isn't used
		if (allow_download_maps->intValue)
			if (!CL_CheckOrDownloadFile(cl.configstrings[CS_MODELS+1]))
				return; // started a download
	}
	if (precache_check >= CS_MODELS && precache_check < CS_MODELS+MAX_MODELS) {
		if (allow_download_models->intValue) {
			while (precache_check < CS_MODELS+MAX_MODELS &&
				cl.configstrings[precache_check][0]) {
				if (cl.configstrings[precache_check][0] == '*' ||
					cl.configstrings[precache_check][0] == '#') {
					precache_check++;
					continue;
				}
				if (precache_model_skin == 0) {
					if (!CL_CheckOrDownloadFile(cl.configstrings[precache_check])) {
						precache_model_skin = 1;
						return; // started a download
					}
					precache_model_skin = 1;
				}
#ifdef USE_CURL /* HTTP downloading from R1Q2 */
				// pending downloads (models), let's wait here before we can check skins.
				if ( CL_PendingHTTPDownloads() )
					return;
#endif
				// checking for skins in the model
				if (!precache_model)
				{
					FS_LoadFile (cl.configstrings[precache_check], (void **)&precache_model);
					if (!precache_model) {
						precache_model_skin = 0;
						precache_check++;
						continue; // couldn't load it
					}
					if (LittleLong(*(unsigned *)precache_model) != IDALIASHEADER)
					{	// is it a sprite?
						if (LittleLong(*(unsigned *)precache_model) != IDSPRITEHEADER)
						{	// not a recognized model
							FS_FreeFile(precache_model);
							precache_model = 0;
							precache_model_skin = 0;
							precache_check++;
							continue;
						}
						else
						{	// get sprite header
							spriteheader = (dsprite_t *)precache_model;
							if (LittleLong (spriteheader->version != SPRITE_VERSION))
							{	// not a recognized sprite
								FS_FreeFile(precache_model);
								precache_model = 0;
								precache_check++;
								precache_model_skin = 0;
								continue; // couldn't load it
							}
						}
					}
					else
					{	// get md2 header
						pheader = (dmdl_t *)precache_model;
						if (LittleLong (pheader->version) != ALIAS_VERSION)
						{	// not a recognized md2
							FS_FreeFile(precache_model);
							precache_model = 0;
						precache_check++;
						precache_model_skin = 0;
						continue; // couldn't load it
						}
					}
				/*	if (LittleLong(*(unsigned *)precache_model) != IDALIASHEADER)
					{	// not an alias model
						FS_FreeFile(precache_model);
						precache_model = 0;
						precache_model_skin = 0;
						precache_check++;
						continue;
					}
					pheader = (dmdl_t *)precache_model;
					if (LittleLong (pheader->version) != ALIAS_VERSION) {
						precache_check++;
						precache_model_skin = 0;
						continue; // couldn't load it
					}*/
				}

				if (LittleLong(*(unsigned *)precache_model) == IDALIASHEADER) // md2
				{
					pheader = (dmdl_t *)precache_model;

					while (precache_model_skin - 1 < LittleLong(pheader->num_skins))
					{
						skinname = (char *)precache_model + LittleLong(pheader->ofs_skins) + 
									(precache_model_skin - 1)*MAX_SKINNAME;
						// r1ch: spam warning for models that are broken
						if (strchr (skinname, '\\'))
							Com_Printf ("Warning, model %s with incorrectly linked skin: %s\n", cl.configstrings[precache_check], skinname);
						else if (strlen(skinname) > MAX_SKINNAME-1)
							Com_Error (ERR_DROP, "Model %s has too long a skin path: %s", cl.configstrings[precache_check], skinname);

						if (!CL_CheckOrDownloadFile(skinname))
						{
							precache_model_skin++;
							return; // started a download
						}
						precache_model_skin++;
					}
				}
				else // sprite
				{
					spriteheader = (dsprite_t *)precache_model;
					while (precache_model_skin - 1 < LittleLong(spriteheader->numframes))
					{
						skinname = spriteheader->frames[(precache_model_skin - 1)].name;

						// r1ch: spam warning for models that are broken
						if (strchr (skinname, '\\'))
							Com_Printf ("Warning, model %s with incorrectly linked skin: %s\n", cl.configstrings[precache_check], skinname);

						if (!CL_CheckOrDownloadFile(skinname))
						{
							precache_model_skin++;
							return; // started a download
						}
						precache_model_skin++;
					}
				}

				if (precache_model) { 
					FS_FreeFile(precache_model);
					precache_model = 0;
				}
				precache_model_skin = 0;
				precache_check++;
			}
		}
		precache_check = CS_SOUNDS;
	}
	if (precache_check >= CS_SOUNDS && precache_check < CS_SOUNDS+MAX_SOUNDS) { 
		if (allow_download_sounds->intValue) {
			if (precache_check == CS_SOUNDS)
				precache_check++; // zero is blank
			while (precache_check < CS_SOUNDS+MAX_SOUNDS &&
				cl.configstrings[precache_check][0]) {
				if (cl.configstrings[precache_check][0] == '*') {
					precache_check++;
					continue;
				}
				Com_sprintf(fn, sizeof(fn), "sound/%s", cl.configstrings[precache_check++]);
				if (!CL_CheckOrDownloadFile(fn))
					return; // started a download
			}
		}
		precache_check = CS_IMAGES;
	}
	if (precache_check >= CS_IMAGES && precache_check < CS_IMAGES+MAX_IMAGES) {
		if (precache_check == CS_IMAGES)
			precache_check++; // zero is blank
		while (precache_check < CS_IMAGES+MAX_IMAGES &&
			cl.configstrings[precache_check][0]) {
			Com_sprintf(fn, sizeof(fn), "pics/%s.pcx", cl.configstrings[precache_check++]);
			if (!CL_CheckOrDownloadFile(fn))
				return; // started a download
		}
		precache_check = CS_PLAYERSKINS;
	}
	// skins are special, since a player has three things to download:
	// model, weapon model and skin
	// so precache_check is now *3
	if (precache_check >= CS_PLAYERSKINS && precache_check < CS_PLAYERSKINS + MAX_CLIENTS * PLAYER_MULT) {
		if (allow_download_players->intValue) {
			while (precache_check < CS_PLAYERSKINS + MAX_CLIENTS * PLAYER_MULT) {
				int i, n;
				char model[MAX_QPATH], skin[MAX_QPATH], *p;

				i = (precache_check - CS_PLAYERSKINS)/PLAYER_MULT;
				n = (precache_check - CS_PLAYERSKINS)%PLAYER_MULT;

				if (i >= cl.maxclients) /* FS: From R1Q2 */
				{
					precache_check = ENV_CNT;
					continue;
				}

				if (!cl.configstrings[CS_PLAYERSKINS+i][0]) {
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
					continue;
				}

				if ((p = strchr(cl.configstrings[CS_PLAYERSKINS+i], '\\')) != NULL)
					p++;
				else
					p = cl.configstrings[CS_PLAYERSKINS+i];
				strcpy(model, p);
				p = strchr(model, '/');
				if (!p)
					p = strchr(model, '\\');
				if (p) {
					*p++ = 0;
					strcpy(skin, p);
				} else
					*skin = 0;

				switch (n) {
				case 0: // model
					Com_sprintf(fn, sizeof(fn), "players/%s/tris.md2", model);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 1;
						return; // started a download
					}
					n++;
					/*FALL THROUGH*/

				case 1: // weapon model
					Com_sprintf(fn, sizeof(fn), "players/%s/weapon.md2", model);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 2;
						return; // started a download
					}
					n++;
					/*FALL THROUGH*/

				case 2: // weapon skin
					Com_sprintf(fn, sizeof(fn), "players/%s/weapon.pcx", model);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 3;
						return; // started a download
					}
					n++;
					/*FALL THROUGH*/

				case 3: // skin
					Com_sprintf(fn, sizeof(fn), "players/%s/%s.pcx", model, skin);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 4;
						return; // started a download
					}
					n++;
					/*FALL THROUGH*/

				case 4: // skin_i
					Com_sprintf(fn, sizeof(fn), "players/%s/%s_i.pcx", model, skin);
					if (!CL_CheckOrDownloadFile(fn)) {
						precache_check = CS_PLAYERSKINS + i * PLAYER_MULT + 5;
						return; // started a download
					}
					// move on to next model
					precache_check = CS_PLAYERSKINS + (i + 1) * PLAYER_MULT;
				}
			}
		}
		// precache phase completed
		precache_check = ENV_CNT;
	}
#ifdef USE_CURL /* HTTP downloading from R1Q2 */
	// pending downloads (possibly the map), let's wait here.
	if ( CL_PendingHTTPDownloads() )
		return;
#endif
	if (precache_check == ENV_CNT)
	{
		if (localServer)	// if on local server, skip checking textures
			precache_check = TEXTURE_CNT+999;
		else
			precache_check = ENV_CNT + 1;

		CM_LoadMap (cl.configstrings[CS_MODELS+1], true, &map_checksum);

		if (map_checksum != atoi(cl.configstrings[CS_MAPCHECKSUM])) {
			Com_Error (ERR_DROP, "Local map version differs from server: %i != '%s'\n",
				map_checksum, cl.configstrings[CS_MAPCHECKSUM]);
			return;
		}
	}

	if (precache_check > ENV_CNT && precache_check < TEXTURE_CNT) {
		if (allow_download->value && allow_download_maps->value) {
			while (precache_check < TEXTURE_CNT) {
				int n = precache_check++ - ENV_CNT - 1;

				if (n & 1)
					Com_sprintf(fn, sizeof(fn), "env/%s%s.pcx", 
						cl.configstrings[CS_SKY], env_suf[n/2]);
				else
					Com_sprintf(fn, sizeof(fn), "env/%s%s.tga", 
						cl.configstrings[CS_SKY], env_suf[n/2]);
				if (!CL_CheckOrDownloadFile(fn))
					return; // started a download
			}
		}
		precache_check = TEXTURE_CNT;
	}

	if (precache_check == TEXTURE_CNT) {
		precache_check = TEXTURE_CNT+1;
		precache_tex = 0;
	}

	// confirm existance of textures, download any that don't exist
	if (precache_check == TEXTURE_CNT+1) {
		// from qcommon/cmodel.c
		extern int			numtexinfo;
		extern mapsurface_t	map_surfaces[];

		if (allow_download->value && allow_download_maps->value) {
			while (precache_tex < numtexinfo) {
				char fn[MAX_OSPATH];
				sprintf(fn, "textures/%s.wal", map_surfaces[precache_tex++].rname);
				if (!CL_CheckOrDownloadFile(fn))
					return; // started a download
			}
		}
		precache_check = TEXTURE_CNT+999;
	}
#ifdef USE_CURL /* HTTP downloading from R1Q2 */
	// pending downloads (possibly textures), let's wait here.
	if ( CL_PendingHTTPDownloads() )
		return;
#endif
//ZOID
	CL_RegisterSounds ();
	CL_PrepRefresh ();

	MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
	MSG_WriteString (&cls.netchan.message, va("begin %i\n", precache_spawncount) );
	cls.forcePacket = true;
}

/*
=================
CL_Precache_f

The server will send this command right
before allowing the client into the server
=================
*/
void CL_Precache_f (void)
{
	//Yet another hack to let old demos work
	//the old precache sequence
	if (Cmd_Argc() < 2) {
		unsigned	map_checksum;		// for detecting cheater maps

		CM_LoadMap (cl.configstrings[CS_MODELS+1], true, &map_checksum);
		CL_RegisterSounds ();
		CL_PrepRefresh ();
		return;
	}

	precache_check = CS_MODELS;
	precache_spawncount = atoi(Cmd_Argv(1));
	precache_model = 0;
	precache_model_skin = 0;
	precache_pak = 0;	// Knightmare added

#ifdef USE_CURL /* HTTP downloading from R1Q2 */
	CL_HTTP_ResetMapAbort ();	// Knightmare- reset the map abort flag
#endif

	CL_RequestNextDownload();
}

/*
=================
CL_InitLocal
=================
*/
void CL_InitLocal (void)
{
	cls.state = ca_disconnected;
	cls.realtime = Sys_Milliseconds ();

	CL_InitInput ();

	adr0 = Cvar_Get( "adr0", "", CVAR_ARCHIVE );
	adr1 = Cvar_Get( "adr1", "", CVAR_ARCHIVE );
	adr2 = Cvar_Get( "adr2", "", CVAR_ARCHIVE );
	adr3 = Cvar_Get( "adr3", "", CVAR_ARCHIVE );
	adr4 = Cvar_Get( "adr4", "", CVAR_ARCHIVE );
	adr5 = Cvar_Get( "adr5", "", CVAR_ARCHIVE );
	adr6 = Cvar_Get( "adr6", "", CVAR_ARCHIVE );
	adr7 = Cvar_Get( "adr7", "", CVAR_ARCHIVE );
	adr8 = Cvar_Get( "adr8", "", CVAR_ARCHIVE );

//
// register our variables
//
	cl_stereo_separation = Cvar_Get( "cl_stereo_separation", "0.4", CVAR_ARCHIVE );
	Cvar_SetDescription("cl_stereo_separation" ,"Stereo separation when used with cl_stereo 1.");
	cl_stereo = Cvar_Get( "cl_stereo", "0", 0 );
	Cvar_SetDescription("cl_stereo", "Stereo mode for 3D Glasses.");

	cl_add_blend = Cvar_Get ("cl_blend", "1", 0);
	cl_add_lights = Cvar_Get ("cl_lights", "1", 0);
	cl_add_particles = Cvar_Get ("cl_particles", "1", 0);
	cl_add_entities = Cvar_Get ("cl_entities", "1", 0);
	cl_gun = Cvar_Get ("cl_gun", "1", 0);
	Cvar_SetDescription("cl_gun", "Set to 0 to disable rendering of the gun model.  Useful for screenshots.");
	cl_footsteps = Cvar_Get ("cl_footsteps", "1", 0);
	Cvar_SetDescription("cl_footsetps", "Play footstep sounds from player.");
	cl_noskins = Cvar_Get ("cl_noskins", "0", 0);
	Cvar_SetDescription("cl_noskins", "All player skins are Male/Grunt");
	cl_predict = Cvar_Get ("cl_predict", "1", 0);
	Cvar_SetDescription("cl_predict", "Client-side movement prediction.  Recommended to leave enabled.");
//	cl_minfps = Cvar_Get ("cl_minfps", "5", 0);
	cl_maxfps = Cvar_Get ("cl_maxfps", "65", CVAR_ARCHIVE);
	Cvar_SetDescription("cl_maxfps", "Maximum number of frames to render ahead when cl_async (asynchronous frames) is set to 0.");

#ifdef CLIENT_SPLIT_NETFRAME
	cl_async = Cvar_Get ("cl_async", "0", CVAR_ARCHIVE);
	Cvar_SetDescription("cl_async", "Asynchronous Frame rendering.  Network frames and renderer frames are separated.  Uses r_maxfps and net_maxfps cvars.");
	net_maxfps = Cvar_Get ("net_maxfps", "60", CVAR_ARCHIVE);
	Cvar_SetDescription("net_maxfps", "FPS limit for network frames when used with cl_async 1.");
	r_maxfps = Cvar_Get ("r_maxfps", "125", CVAR_ARCHIVE);
	Cvar_SetDescription("r_maxfps", "FPS limit for renderer frames when used with cl_async 1.");
#endif

	// Knightmare- whether to try to play OGGs instead of CD tracks
	cl_ogg_music = Cvar_Get ("cl_ogg_music", "1", CVAR_ARCHIVE);
	Cvar_SetDescription("cl_ogg_music", "Whether to try to play OGG Vorbis files instead of CD audio tracks.");
	cl_rogue_music = Cvar_Get ("cl_rogue_music", "0", CVAR_ARCHIVE);
	cl_xatrix_music = Cvar_Get ("cl_xatrix_music", "0", CVAR_ARCHIVE);

	cl_wav_music = Cvar_Get ("cl_wav_music", "1", CVAR_ARCHIVE);
	Cvar_SetDescription("cl_wav_music", "Whether to try to play WAV files instead of CD audio tracks.");

	cl_3dcam = Cvar_Get ("cl_3dcam", "0", CVAR_ARCHIVE);
	Cvar_SetDescription("cl_3dcam", "Enables 3rd person camera.  Fine tune with cl_3dcam_angle and cl_3dcam_dist.");
	cl_3dcam_angle = Cvar_Get ("cl_3dcam_angle", "30", CVAR_ARCHIVE);
	Cvar_SetDescription("cl_3dcam_angle", "Angle for the 3rd person camera.");
	cl_3dcam_dist = Cvar_Get ("cl_3dcam_dist", "50", CVAR_ARCHIVE);
	Cvar_SetDescription("cl_3dcam_dist", "Distance for the 3rd person camera.");

	cl_upspeed = Cvar_Get ("cl_upspeed", "200", 0);
	cl_forwardspeed = Cvar_Get ("cl_forwardspeed", "200", 0);
	cl_sidespeed = Cvar_Get ("cl_sidespeed", "200", 0);
	cl_yawspeed = Cvar_Get ("cl_yawspeed", "140", 0);
	cl_pitchspeed = Cvar_Get ("cl_pitchspeed", "150", 0);
	cl_anglespeedkey = Cvar_Get ("cl_anglespeedkey", "1.5", 0);

	cl_run = Cvar_Get ("cl_run", "0", CVAR_ARCHIVE);
	Cvar_SetDescription("cl_run", "Set to 1 for always run movement.");
	freelook = Cvar_Get( "freelook", "0", CVAR_ARCHIVE );
	Cvar_SetDescription("freelook", "If enabled mouse will be used for looking around instead of moving.");
	lookspring = Cvar_Get ("lookspring", "0", CVAR_ARCHIVE);
	lookstrafe = Cvar_Get ("lookstrafe", "0", CVAR_ARCHIVE);
	sensitivity = Cvar_Get ("sensitivity", "3", CVAR_ARCHIVE);
	Cvar_SetDescription("sensitivity", "Mouse sensitivity.");

	m_pitch = Cvar_Get ("m_pitch", "0.022", CVAR_ARCHIVE);
	m_yaw = Cvar_Get ("m_yaw", "0.022", 0);
	m_forward = Cvar_Get ("m_forward", "1", 0);
	m_side = Cvar_Get ("m_side", "1", 0);

	cl_shownet = Cvar_Get ("cl_shownet", "0", 0);
	Cvar_SetDescription("cl_shownet", "Shows verbose output about server packets.  1 will show current message sizes.  2 will show svc_xxx packets as they are parsed.  3 will show verbose information about delta encoding from packet entities.");
	cl_showmiss = Cvar_Get ("cl_showmiss", "0", 0);
	Cvar_SetDescription("cl_showmiss", "Shows misses on movement prediction.");
	cl_showclamp = Cvar_Get ("showclamp", "0", 0);
	Cvar_SetDescription("showclamp", "Shows time skews from clients timer versus the servers timer.");
	cl_timeout = Cvar_Get ("cl_timeout", "120", 0);
	Cvar_SetDescription("cl_timeout", "Timeout (in seconds) for connecting to servers.");
	cl_paused = Cvar_Get ("paused", "0", 0);
	Cvar_SetDescription("paused", "If enabled in Single Player then the game is currently paused.  Only works in multiplayer if cheats are enabled.");
	cl_timedemo = Cvar_Get ("timedemo", "0", 0);
	Cvar_SetDescription("timedemo", "Set to 1 for timing playback of demos.  Useful for bencmarking.");

	rcon_client_password = Cvar_Get ("rcon_password", "", 0);
	rcon_address = Cvar_Get ("rcon_address", "", 0);

	cl_lightlevel = Cvar_Get ("r_lightlevel", "0", 0);

	//
	// userinfo
	//
	info_password = Cvar_Get ("password", "", CVAR_USERINFO);
	info_spectator = Cvar_Get ("spectator", "0", CVAR_USERINFO);
	name = Cvar_Get ("name", "unnamed", CVAR_USERINFO | CVAR_ARCHIVE);
	skin = Cvar_Get ("skin", "male/grunt", CVAR_USERINFO | CVAR_ARCHIVE);
	rate = Cvar_Get ("rate", "25000", CVAR_USERINFO | CVAR_ARCHIVE);	// FIXME
	msg = Cvar_Get ("msg", "1", CVAR_USERINFO | CVAR_ARCHIVE);
	hand = Cvar_Get ("hand", "0", CVAR_USERINFO | CVAR_ARCHIVE);
	fov = Cvar_Get ("fov", "90", CVAR_USERINFO | CVAR_ARCHIVE);
	fov_adapt = Cvar_Get ("fov_adapt", "1", CVAR_ARCHIVE);
	Cvar_SetDescription("fov_adapt", "Hor+ style field of view (FOV) scaling: Useful for widescreen resolutions. If enabled your FOV will be scaled automatically according to the resolution.");
	gender = Cvar_Get ("gender", "male", CVAR_USERINFO | CVAR_ARCHIVE);
	Cvar_SetDescription("gender", "Player gender.");
	gender_auto = Cvar_Get ("gender_auto", "1", CVAR_ARCHIVE);
	Cvar_SetDescription("gender_auto", "Automatically fix the player gender if it is modified or a userinfo variable is changed.");
	gender->modified = false; // clear this so we know when user sets it manually

	cl_vwep = Cvar_Get ("cl_vwep", "1", CVAR_ARCHIVE);

	/* FS: New stuff */
	console_old_complete = Cvar_Get("console_old_complete", "0", CVAR_ARCHIVE); /* FS: Old style command completing */
	cl_autorepeat_allkeys = Cvar_Get("cl_autorepeat_allkeys", "0", CVAR_ARCHIVE); /* FS: Because I want to autorepeat whatever I want, hoss */
	Cvar_SetDescription("cl_autorepeat_allkeys", "Allow to autorepeat any key, not just Backspace, Pause, PgUp, and PgDn keys.");
	cl_stufftext_check = Cvar_Get ("cl_stufftext_check", "0", 0);
	Cvar_SetDescription("cl_stufftext_check", "Check malicious stufftexts from servers and their admins.  Must explicitly be enabled and will not be saved.  Values higher than 1 will ignore connect stufftexts from WallFly, etc.");

	/* Knightmare: Added */
	cl_sleep = Cvar_Get("cl_sleep", "0", CVAR_ARCHIVE);
	Cvar_SetDescription("cl_sleep", "Reduce CPU usage by issuing sleep commands between extra frames.");

#ifdef GAMESPY
	/* FS: For gamespy */
	cl_master_server_ip = Cvar_Get("cl_master_server_ip", CL_MASTER_ADDR, CVAR_ARCHIVE);
	Cvar_SetDescription("cl_master_server_ip", "Master server IP to use with the gamespy browser.");
	cl_master_server_port = Cvar_Get("cl_master_server_port", CL_MASTER_PORT, CVAR_ARCHIVE);
	Cvar_SetDescription("cl_master_server_port", "Master server port to use with the gamespy browser.");
	cl_master_server_queries = Cvar_Get("cl_master_server_queries", "10", CVAR_ARCHIVE);
	Cvar_SetDescription("cl_master_server_queries", "Maximum number of query (ping) requests to use per loop with the gamespy browser.");
	cl_master_server_timeout = Cvar_Get("cl_master_server_timeout", "3000", CVAR_ARCHIVE);
	Cvar_SetDescription("cl_master_server_timeout", "Timeout (in milliseconds) to give up on pinging a server.");
	cl_master_server_retries = Cvar_Get("cl_master_server_retries", "20", CVAR_ARCHIVE);
	Cvar_SetDescription("cl_master_server_retries", "Number of retries to attempt for receiving the server list.  Formula is 50ms + 10ms for each retry.");
	cl_master_server_optout = Cvar_Get("cl_master_server_optout", "0", CVAR_ARCHIVE);
	Cvar_SetDescription("cl_master_server_optout", "Opt-out of sending your Quake 2 Username in GameSpy list requests.");
	s_gamespy_sounds = Cvar_Get("s_gamespysounds", "0", CVAR_ARCHIVE);
	Cvar_SetDescription("s_gamespysounds", "Play the complete.wav and abort.wav from GameSpy3D if it exists in sounds/gamespy.");
#endif

#ifdef USE_CURL /* HTTP downloading from R1Q2 */
	cl_http_proxy = Cvar_Get ("cl_http_proxy", "", 0);
	cl_http_filelists = Cvar_Get ("cl_http_filelists", "1", 0);
	cl_http_downloads = Cvar_Get ("cl_http_downloads", "1", CVAR_ARCHIVE);
	Cvar_SetDescription("cl_http_downloads", "Enable HTTP downloading.");
	cl_http_max_connections = Cvar_Get ("cl_http_max_connections", "4", 0);
	Cvar_SetDescription("cl_http_max_connections", "Maximum number of connections to open up during HTTP downloading.  Currently limited to 4.");
#endif
	//
	// register our commands
	//
	Cmd_AddCommand ("cmd", CL_ForwardToServer_f);
	Cmd_AddCommand ("pause", CL_Pause_f);
	Cmd_AddCommand ("pingservers", CL_PingServers_f);
	Cmd_AddCommand ("skins", CL_Skins_f);

	Cmd_AddCommand ("userinfo", CL_Userinfo_f);
	Cmd_AddCommand ("snd_restart", CL_Snd_Restart_f);

	Cmd_AddCommand ("changing", CL_Changing_f);
	Cmd_AddCommand ("disconnect", CL_Disconnect_f);

	Cmd_AddCommand ("record", CL_Record_f);
	Cmd_AddCommand ("stop", CL_Stop_f);

	Cmd_AddCommand ("quit", CL_Quit_f);

	Cmd_AddCommand ("connect", CL_Connect_f);
	Cmd_AddCommand ("reconnect", CL_Reconnect_f);

	Cmd_AddCommand ("rcon", CL_Rcon_f);

 	Cmd_AddCommand ("packet", CL_Packet_f); // this is dangerous to leave in

	Cmd_AddCommand ("setenv", CL_Setenv_f );

	Cmd_AddCommand ("precache", CL_Precache_f);

	Cmd_AddCommand ("download", CL_Download_f);

	Cmd_AddCommand ("writeconfig", CL_WriteConfig_f);	// Knightmare- added writeconfig command

	//
	// forward to server commands
	//
	// the only thing this does is allow command completion
	// to work -- all unknown commands are automatically
	// forwarded to the server
	Cmd_AddCommand ("wave", NULL);
	Cmd_AddCommand ("inven", NULL);
	Cmd_AddCommand ("kill", NULL);
	Cmd_AddCommand ("use", NULL);
	Cmd_AddCommand ("drop", NULL);
	Cmd_AddCommand ("say", NULL);
	Cmd_AddCommand ("say_team", NULL);
	Cmd_AddCommand ("info", NULL);
	Cmd_AddCommand ("prog", NULL);
	Cmd_AddCommand ("give", NULL);
	Cmd_AddCommand ("god", NULL);
	Cmd_AddCommand ("notarget", NULL);
	Cmd_AddCommand ("noclip", NULL);
	Cmd_AddCommand ("invuse", NULL);
	Cmd_AddCommand ("invprev", NULL);
	Cmd_AddCommand ("invnext", NULL);
	Cmd_AddCommand ("invdrop", NULL);
	Cmd_AddCommand ("weapnext", NULL);
	Cmd_AddCommand ("weapprev", NULL);

#ifdef GAMESPY
	Cmd_AddCommand ("gspystop", CL_Gspystop_f);
	Cmd_AddCommand ("slist2", CL_PingNetServers_f);
	Cmd_AddCommand ("srelist", CL_PrintBrowserList_f);

	memset(&browserList, 0, sizeof(browserList));
	memset(&browserListAll, 0, sizeof(browserListAll));

	CL_LoadGameSpy ();
#endif /* GAMESPY */

#ifdef __DJGPP__
	Cmd_AddCommand ("memstats", Sys_Memory_Stats_f); /* FS: Added to keep track of memory usage in DOS */
#endif
}


/*
===============
CL_WriteConfiguration

Writes key bindings and archived cvars to config.cfg
===============
*/
// Knightmare- this now takes cfgname as a parameter
void CL_WriteConfiguration (char *cfgName)
{
	FILE	*f;
	char	path[MAX_QPATH];

	if (cls.state == ca_uninitialized)
		return;

	if(!(cfgName) || (cfgName[0] == 0)) /* FS: Sanity check */
	{
#ifdef _3DS
		Com_sprintf (path, sizeof(path),"%s/q2ctr.cfg", FS_Gamedir());
#else
		Com_sprintf (path, sizeof(path),"%s/q2dos.cfg", FS_Gamedir());
#endif
	}
	else
	{
		Com_sprintf (path, sizeof(path),"%s/%s.cfg", FS_Gamedir(), cfgName);
	}

	f = fopen (path, "w");
	if (!f)
	{
		Com_Printf ("Couldn't write %s.cfg.\n", cfgName);
		return;
	}

	fprintf (f, "// This file is generated by Q2DOS, do not modify.\n");
	fprintf (f, "// Use autoexec.cfg for adding custom settings.\n");
	Key_WriteBindings (f);
	fclose (f);

	Cvar_WriteVariables (path);
}

// Knightmare added
/*
===============
CL_WriteConfig_f

===============
*/
void CL_WriteConfig_f (void)
{
	char cfgName[MAX_QPATH];

	if ((Cmd_Argc() == 1) || (Cmd_Argc() == 2))
	{
		if (Cmd_Argc() == 1)
			COM_StripExtension(cfg_default->string, cfgName);
		else
			strncpy (cfgName, Cmd_Argv(1), sizeof(cfgName));

		CL_WriteConfiguration (cfgName);
		Com_Printf ("Wrote config file %s/%s.cfg.\n", FS_Gamedir(), cfgName);
	}
	else
		Com_Printf ("Usage: writeconfig <name>\n");
}

//============================================================================

/*
==================
CL_FixCvarCheats

==================
*/

typedef struct
{
	char	*name;
	char	*value;
	cvar_t	*var;
} cheatvar_t;

cheatvar_t	cheatvars[] = {
	{"timescale", "1"},
	{"timedemo", "0"},
	{"r_drawworld", "1"},
	{"cl_testlights", "0"},
	{"r_fullbright", "0"},
	{"r_drawflat", "0"},
	{"paused", "0"},
	{"fixedtime", "0"},
	{"sw_draworder", "0"},
	{"gl_lightmap", "0"},
	{"gl_saturatelighting", "0"},
	{NULL, NULL}
};

int		numcheatvars;

void CL_FixCvarCheats (void)
{
	int			i;
	cheatvar_t	*var;

	if ( !strcmp(cl.configstrings[CS_MAXCLIENTS], "1") 
		|| !cl.configstrings[CS_MAXCLIENTS][0] || cl.attractloop) /* FS: Allow timedemo from deathmatch demos to work */
		return;		// single player can cheat

	// find all the cvars if we haven't done it yet
	if (!numcheatvars)
	{
		while (cheatvars[numcheatvars].name)
		{
			cheatvars[numcheatvars].var = Cvar_Get (cheatvars[numcheatvars].name,
					cheatvars[numcheatvars].value, 0);
			numcheatvars++;
		}
	}

	// make sure they are all set to the proper values
	for (i = 0, var = cheatvars; i < numcheatvars; i++, var++)
	{
		if (strcmp (var->var->string, var->value) != 0)
		{
			Cvar_Set (var->name, var->value);
		}
	}
}

#ifdef CLIENT_SPLIT_NETFRAME
/*
==================
CL_RefreshInputs
==================
*/
static void CL_RefreshInputs (void)
{
	// fetch results from server
	CL_ReadPackets ();

	// get new key events
	Sys_SendKeyEvents ();

	// allow mice or other external controllers to add commands
	IN_Commands ();

	// process console commands
	Cbuf_Execute ();

	// fix any cheating cvars
	CL_FixCvarCheats ();

	// fetch results from server
//	CL_ReadPackets ();

	// Update usercmd state
	if (cls.state > ca_connecting)
	{
		CL_RefreshCmd ();
	}
	else
	{
		CL_RefreshMove ();
	}
}

/*
==================
CL_SendCommand_Async
==================
*/
static void CL_SendCommand_Async (void)
{
	// send intentions now
	CL_SendCmd_Async ();

	// resend a connection request if necessary
	CL_CheckForResend ();
}


/*
==================
CL_Frame_Async
==================
*/
#define FRAMETIME_MAX 0.5 /* was 0.2 */
void CL_Frame_Async (int msec)
{
	static int	packetDelta = 0;
	static int	renderDelta = 0;
	static int	miscDelta = 0;
	static int  lasttimecalled;
	qboolean	packetFrame = true;
	qboolean	renderFrame = true;
	qboolean	miscFrame = true;

	// Don't allow setting maxfps too low or too high
	if (net_maxfps->value < 10)
	{
		Cvar_SetValue("net_maxfps", 10);
	}

	if (net_maxfps->value > 90)
	{
		Cvar_SetValue("net_maxfps", 90);
	}

	if (r_maxfps->value < 10)
	{
		Cvar_SetValue("r_maxfps", 10);
	}

	if (r_maxfps->value > 1000)
	{
		Cvar_SetValue("r_maxfps", 1000);
	}

	packetDelta += msec;
	renderDelta += msec;
	miscDelta += msec;

	// decide the simulation time
	cls.netFrameTime = packetDelta * 0.001f;
	cls.renderFrameTime = renderDelta * 0.001f;
	cl.time += msec;
	cls.realtime = curtime;

	// Don't extrapolate too far ahead
	if (cls.netFrameTime > FRAMETIME_MAX)
	{
		cls.netFrameTime = FRAMETIME_MAX;
	}

	if (cls.renderFrameTime > FRAMETIME_MAX)
	{
		cls.renderFrameTime = FRAMETIME_MAX;
	}

	// If in the debugger last frame, don't timeout
	if (msec > 5000)
	{
		cls.netchan.last_received = Sys_Milliseconds ();
	}

	if (!cl_timedemo->intValue)
	{	// Don't flood packets out while connecting
		if (cls.state == ca_connected && packetDelta < 100)
		{
			packetFrame = false;
		}

		if (packetDelta < 1000.0 / net_maxfps->intValue)
		{
			packetFrame = false;
		}
		else if (cls.netFrameTime == cls.renderFrameTime)
		{
			packetFrame = false;
		}

		if (renderDelta < 1000.0 / r_maxfps->value)
		{
			renderFrame = false;
		}

		// Stuff that only needs to run at 10FPS
		if (miscDelta < 1000.0 / 10)
		{
			miscFrame = false;
		}
		
		if (!packetFrame && !renderFrame && !cls.forcePacket && !userinfo_modified)
		{
			// Pooy's CPU usage fix
			if (cl_sleep->intValue)
			{
				int temptime = min( (1000.0 / net_maxfps->intValue - packetDelta), (1000.0 / r_maxfps->intValue - renderDelta) );
				if (temptime > 1)
				{
					Sys_Sleep (1);
				}
			} // end CPU usage fix
			return;
		}
		
	}
	else if (msec < 1)	// don't exceed 1000 fps in timedemo mode (fixes hang)
	{
		return;
	}

#ifdef USE_CURL /* HTTP downloading from R1Q2 */
	if (cls.state == ca_connected)
	{
		// downloads run full speed when connecting
		CL_RunHTTPDownloads ();
	}
#endif

	// Update the inputs (keyboard, mouse, console)
	if (packetFrame || renderFrame)
	{
		CL_RefreshInputs ();
	}

	if (cls.forcePacket || userinfo_modified)
	{
		packetFrame = true;
		cls.forcePacket = false;
	}

	// Send a new command message to the server
	if (packetFrame)
	{
		packetDelta = 0;
		CL_SendCommand_Async ();

#ifdef USE_CURL /* HTTP downloading from R1Q2 */
		// downloads run less often in game
		CL_RunHTTPDownloads ();
#endif
	}

	if (renderFrame)
	{
		renderDelta = 0;

		if (miscFrame)
		{
			miscDelta = 0;

			if (cls.spamTime && (cls.spamTime < cls.realtime)) /* FS: From R1Q2 */
			{
				char versionStr[256];

				if(vidref_val == VIDREF_GL)
					Com_sprintf(versionStr, sizeof(versionStr), "say Q2DOS with 3DFX Voodoo %4.2f %s %s %s\n", VERSION, CPUSTRING, __DATE__, BUILDSTRING);
				else
					Com_sprintf(versionStr, sizeof(versionStr), "say Q2DOS %4.2f %s %s %s\n", VERSION, CPUSTRING, __DATE__, BUILDSTRING);
				Cbuf_AddText(versionStr);
				cls.lastSpamTime = cls.realtime;
				cls.spamTime = 0.0f;
			}

			// Let the mouse activate or deactivate
			IN_Frame ();

			// Allow rendering DLL change
			VID_CheckChanges ();
		}
		// Predict all unacknowledged movements
		CL_PredictMovement ();

		if (!cl.refresh_prepped && cls.state == ca_active)
		{
			CL_PrepRefresh ();
		}

		// update the screen
		if (host_speeds->intValue)
		{
			time_before_ref = Sys_Milliseconds ();
		}

		SCR_UpdateScreen ();

		if (host_speeds->intValue)
		{
			time_after_ref = Sys_Milliseconds ();
		}

		// Update audio
		S_Update (cl.refdef.vieworg, cl.v_forward, cl.v_right, cl.v_up);

		if (miscFrame)
		{
			CDAudio_Update();
		}

		// Advance local effects for next frame
		CL_RunDLights ();
		CL_RunLightStyles ();
		SCR_RunCinematic ();
		SCR_RunConsole ();

		cls.framecount++;

		if (log_stats->intValue)
		{
			if (cls.state == ca_active)
			{
				if (!lasttimecalled)
				{
					lasttimecalled = Sys_Milliseconds();

					if (log_stats_file)
					{
						fprintf( log_stats_file, "0\n" );
					}
				}
				else
				{
					int now = Sys_Milliseconds();

					if (log_stats_file)
					{
						fprintf( log_stats_file, "%i\n", now - lasttimecalled );
					}

					lasttimecalled = now;
				}
			}
		}
	}
}
#endif /* CLIENT_SPLIT_NETFRAME */


//============================================================================

/*
==================
CL_SendCommand

==================
*/
void CL_SendCommand (void)
{
	// get new key events
	Sys_SendKeyEvents ();

	// allow mice or other external controllers to add commands
	IN_Commands ();

	// process console commands
	Cbuf_Execute ();

	// fix any cheating cvars
	CL_FixCvarCheats ();

	// send intentions now
	CL_SendCmd ();

	// resend a connection request if necessary
	CL_CheckForResend ();
}


/*
==================
CL_Frame

==================
*/
void CL_Frame (int msec)
{
	static int	extratime;
	static int  lasttimecalled;
	float	fps;

	if (dedicated->intValue)
	{
		return;
	}

#ifdef GAMESPY
	GameSpy_Async_Think(); /* FS: Update the query loop if we're pinging some servers */
#endif

#ifdef CLIENT_SPLIT_NETFRAME
	if (cl_async->intValue && !cl_timedemo->intValue)
	{
		CL_Frame_Async (msec);
		return;
	}
#endif

	extratime += msec;

	if (cl_maxfps->intValue)
	{
		fps = bound(5, cl_maxfps->intValue, 1000); /* FS: Don't go under 5 */
	}
	else
	{
		fps = bound(5, cl_maxfps->intValue, 72); /* FS: Default to 72hz if nothing is set */
	}

	if (!cl_timedemo->intValue)
	{
		if (cls.state == ca_connected && extratime < 100)
		{
			return;			// don't flood packets out while connecting
		}

		if (extratime < 1000.0/fps)
		{
			// Knightmare- added Pooy's CPU usage fix
			if (cl_sleep->intValue)
			{
				int temptime = 1000 / cl_maxfps->intValue - extratime;

				if (temptime > 1)
				{
					Sys_Sleep (1);
				}
			} // end CPU usage fix
			return;			// framerate is too high
		}
	}

	// let the mouse activate or deactivate
	IN_Frame ();

	// decide the simulation time
	cls.netFrameTime = extratime/1000.0;
	cl.time += extratime;
	cls.realtime = curtime;

	extratime = 0;

	if (cls.netFrameTime > (1.0 / 5)) /* FS: 5 -- Minfps */
	{
		cls.netFrameTime = (1.0 / 5);
	}

	cls.renderFrameTime = cls.netFrameTime;

	// if in the debugger last frame, don't timeout
	if (msec > 5000)
	{
		cls.netchan.last_received = Sys_Milliseconds ();
	}

#ifdef USE_CURL /* HTTP downloading from R1Q2 */
	CL_RunHTTPDownloads ();
#endif

	// fetch results from server
	CL_ReadPackets ();

	// send a new command message to the server
	CL_SendCommand ();

	if (cls.spamTime && (cls.spamTime < cls.realtime)) /* FS: From R1Q2 */
	{
		char versionStr[256];

		if(vidref_val == VIDREF_GL)
			Com_sprintf(versionStr, sizeof(versionStr), "say Q2DOS with 3DFX Voodoo %4.2f %s %s %s\n", VERSION, CPUSTRING, __DATE__, BUILDSTRING);
		else
			Com_sprintf(versionStr, sizeof(versionStr), "say Q2DOS %4.2f %s %s %s\n", VERSION, CPUSTRING, __DATE__, BUILDSTRING);
		Cbuf_AddText(versionStr);
		cls.lastSpamTime = cls.realtime;
		cls.spamTime = 0;
	}

	// predict all unacknowledged movements
	CL_PredictMovement ();

	// allow rendering DLL change
	VID_CheckChanges ();

	if (!cl.refresh_prepped && cls.state == ca_active)
	{
		CL_PrepRefresh ();
	}

	// update the screen
	if (host_speeds->intValue)
	{
		time_before_ref = Sys_Milliseconds ();
	}

	SCR_UpdateScreen ();

	if (host_speeds->intValue)
	{
		time_after_ref = Sys_Milliseconds ();
	}

	// update audio
	S_Update (cl.refdef.vieworg, cl.v_forward, cl.v_right, cl.v_up);
	
	CDAudio_Update();

	// advance local effects for next frame
	CL_RunDLights ();
	CL_RunLightStyles ();
	SCR_RunCinematic ();
	SCR_RunConsole ();

	cls.framecount++;

	if ( log_stats->intValue)
	{
		if ( cls.state == ca_active )
		{
			if ( !lasttimecalled )
			{
				lasttimecalled = Sys_Milliseconds();

				if ( log_stats_file )
				{
					fprintf( log_stats_file, "0\n" );
				}
			}
			else
			{
				int now = Sys_Milliseconds();

				if ( log_stats_file )
				{
					fprintf( log_stats_file, "%d\n", (int)(now - lasttimecalled) );
				}

				lasttimecalled = now;
			}
		}
	}
}


//============================================================================

/*
====================
CL_Init
====================
*/
void CL_Init (void)
{
	if (dedicated->intValue)
	{
		return;		// nothing running on the client
	}

	// all archived variables will now be loaded

	Con_Init ();
	VID_Init ();
	S_Init ();	// sound must be initialized after window is created

	V_Init ();

	net_message.data = net_message_buffer;
	net_message.maxsize = sizeof(net_message_buffer);

	M_Init ();
	
	SCR_Init ();
	cls.disable_screen = true;	// don't draw yet

	CDAudio_Init ();
	CL_InitLocal ();
	IN_Init ();

#ifdef USE_CURL /* HTTP downloading from R1Q2 */
	CL_InitHTTPDownloads ();
#endif

//	Cbuf_AddText ("exec autoexec.cfg\n");
	FS_ExecAutoexec ();
	Cbuf_Execute ();

}


/*
===============
CL_Shutdown

FIXME: this is a callback from Sys_Quit and Com_Error.  It would be better
to run quit through here before the final handoff to the sys code.
===============
*/
void CL_Shutdown(void)
{
	static qboolean isdown = false;
	char	cfgName[MAX_QPATH];

	if (isdown)
	{
		printf ("recursive shutdown\n");
		return;
	}
	isdown = true;

#ifdef USE_CURL /* HTTP downloading from R1Q2 */
	CL_HTTP_Cleanup (true);
#endif

	/* FS: Changed config to q2dos for WriteConfig */
	COM_StripExtension(cfg_default->string, cfgName);
	CL_WriteConfiguration (cfgName);	// Knightmare- changed to take config name as a parameter

	CDAudio_Shutdown ();
	S_Shutdown();
	IN_Shutdown ();
	VID_Shutdown();
}

/* FS: Gamespy Server Browser */
#ifdef GAMESPY
extern void Update_Gamespy_Menu (void);

static void CL_Gamespy_Check_Error(GServerList lst, int error)
{
	if (error != GE_NOERROR) /* FS: Grab the error code */
	{
		Com_Printf("\x02GameSpy Error: ");
		Com_Printf("%s.\n", gspye->ServerListErrorDesc(lst, error));
		if(cls.state == ca_disconnected)
			NET_Config(false);
	}
}

static void GameSpy_Async_Think(void)
{
	int error;

	if(!serverlist)
		return;

	if(gspye->ServerListState(serverlist) == sl_idle && cls.gamespyupdate)
	{
		if (cls.key_dest != key_menu) /* FS: Only print this from an slist2 command, not the server browser. */
		{
			Com_Printf("Found %i active servers out of %i in %i seconds.\n", gspyCur, cls.gamespytotalservers, (((int)Sys_Milliseconds()-cls.gamespystarttime) / 1000) );
		}
		else
		{
			Update_Gamespy_Menu();
		}
		cls.gamespyupdate = 0;
		cls.gamespypercent = 0;
		gspye->ServerListClear(serverlist);
		gspye->ServerListFree(serverlist);
		serverlist = NULL; /* FS: This is on purpose so future ctrl+c's won't try to close empty serverlists */
		if(cls.state == ca_disconnected)
			NET_Config(false);
	}
	else
	{
		error = gspye->ServerListThink(serverlist);
		CL_Gamespy_Check_Error(serverlist, error);
	}
}

static void CL_Gspystop_f (void)
{
	if(serverlist != NULL && cls.gamespyupdate) /* FS: Immediately abort gspy scans */
	{
		Com_Printf("\x02Server scan aborted!\n");
		S_GamespySound ("gamespy/abort.wav");
		gspye->ServerListHalt(serverlist);
	}
}

static void CL_PrintBrowserList_f (void)
{
	int i;
	int num_active_servers = 0;
	qboolean showAll = false;

	if(Cmd_Argc() > 1)
	{
		showAll = true;
	}

	for (i = 0; i < MAX_SERVERS; i++)
	{
		if(browserList[i].hostname[0] != 0)
		{
			if (browserList[i].curPlayers > 0)
			{
				Com_Printf("%02d:  %s:%d [%d] %s ", num_active_servers+1, browserList[i].ip, browserList[i].port, browserList[i].ping, browserList[i].hostname);
				Com_Printf("\x02%d", browserList[i].curPlayers); /* FS: Show the current players number in the green font */
				Com_Printf("/%d %s\n", browserList[i].maxPlayers, browserList[i].mapname);
				num_active_servers++;
			}
		}
		else /* FS: if theres nothing there the rest of the list is old garbage, bye. */
		{
			break;
		}
	}

	if (showAll)
	{
		int skip = 0;

		for (i = 0; i < MAX_SERVERS; i++)
		{
			if(browserListAll[i].hostname[0] != 0)
			{
				Com_Printf("%02d:  %s:%d [%d] %s %d/%d %s\n", (i+num_active_servers+1)-(skip), browserListAll[i].ip, browserListAll[i].port, browserListAll[i].ping, browserListAll[i].hostname, browserListAll[i].curPlayers, browserListAll[i].maxPlayers, browserListAll[i].mapname);
			}
			else /* FS: The next one could be 0 if we skipped over it previously in GameSpy_Sort_By_Ping.  So increment the number of skips counter so the server number shows sequentially */
			{
				skip++;
				continue;
			}
		}
	}
}

static void GameSpy_Sort_By_Ping(GServerList lst)
{
	int i;
	gspyCur = 0;

	for (i = 0; i < cls.gamespytotalservers; i++)
	{
		GServer server = gspye->ServerListGetServer(lst, i);
		if (server)
		{
			if(gspye->ServerGetIntValue(server, "numplayers", 0) <= 0)
				continue;

			if(i == MAX_SERVERS)
				break;

			Q_strncpyz(browserList[gspyCur].ip, gspye->ServerGetAddress(server), sizeof(browserList[gspyCur].ip));
			browserList[gspyCur].port = gspye->ServerGetQueryPort(server);
			browserList[gspyCur].ping = gspye->ServerGetPing(server);
			Q_strncpyz(browserList[gspyCur].hostname, gspye->ServerGetStringValue(server, "hostname","(NONE)"), sizeof(browserList[gspyCur].hostname));
			Q_strncpyz(browserList[gspyCur].mapname, gspye->ServerGetStringValue(server,"mapname","(NO MAP)"), sizeof(browserList[gspyCur].mapname));
			browserList[gspyCur].curPlayers = gspye->ServerGetIntValue(server,"numplayers",0);
			browserList[gspyCur].maxPlayers = gspye->ServerGetIntValue(server,"maxclients",0);

			gspyCur++;
		}
	}

	for (i = 0; i < cls.gamespytotalservers; i++)
	{
		GServer server = gspye->ServerListGetServer(lst, i);

		if (server)
		{
			if(gspye->ServerGetIntValue(server, "numplayers", 0) > 0) /* FS: We already added this so skip it */
			{
				continue;
			}

			if (strncmp(gspye->ServerGetStringValue(server, "hostname","(NONE)"), "(NONE)", 6) == 0) /* FS: A server that timed-out or we aborted early */
			{
				continue;
			}

			if(i == MAX_SERVERS)
				break;

			Q_strncpyz(browserListAll[i].ip, gspye->ServerGetAddress(server), sizeof(browserListAll[i].ip));
			browserListAll[i].port = gspye->ServerGetQueryPort(server);
			browserListAll[i].ping = gspye->ServerGetPing(server);
			Q_strncpyz(browserListAll[i].hostname, gspye->ServerGetStringValue(server, "hostname","(NONE)"), sizeof(browserListAll[i].hostname));
			Q_strncpyz(browserListAll[i].mapname, gspye->ServerGetStringValue(server,"mapname","(NO MAP)"), sizeof(browserListAll[i].mapname));
			browserListAll[i].curPlayers = gspye->ServerGetIntValue(server,"numplayers",0);
			browserListAll[i].maxPlayers = gspye->ServerGetIntValue(server,"maxclients",0);
		}
	}
}

static void ListCallBack(GServerList lst, int msg, void *instance, void *param1, void *param2)
{
	GServer server;
	int numplayers;

	if (msg == LIST_PROGRESS)
	{
		server = (GServer)param1;
		numplayers = gspye->ServerGetIntValue(server,"numplayers",0);

		if(numplayers > 0) /* FS: Only show populated servers */
		{
			if (cls.key_dest != key_menu) /* FS: Only print this from an slist2 command, not the server browser. */
			{
				Com_Printf("%s:%d [%d] %s ", gspye->ServerGetAddress(server), gspye->ServerGetQueryPort(server), gspye->ServerGetPing(server), gspye->ServerGetStringValue(server, "hostname","(NONE)"));
				Com_Printf("\x02%d", numplayers); /* FS: Show the current players number in the green font */
				Com_Printf("/%d %s\n", gspye->ServerGetIntValue(server,"maxclients",0), gspye->ServerGetStringValue(server,"mapname","(NO MAP)"));
			}
		}
		else if (cls.gamespyupdate == SHOW_ALL_SERVERS)
		{
			if (cls.key_dest != key_menu) /* FS: Only print this from an slist2 command, not the server browser. */
			{
				Com_Printf("%s:%d [%d] %s %d/%d %s\n", gspye->ServerGetAddress(server), gspye->ServerGetQueryPort(server), gspye->ServerGetPing(server), gspye->ServerGetStringValue(server, "hostname","(NONE)"), gspye->ServerGetIntValue(server,"numplayers",0), gspye->ServerGetIntValue(server,"maxclients",0), gspye->ServerGetStringValue(server,"mapname","(NO MAP)"));
			}
		}

		if(param2)
		{
			cls.gamespypercent = (int) (intptr_t)param2;
		}
	}
	else if (msg == LIST_STATECHANGED)
	{
		switch(gspye->ServerListState(lst))
		{
			case sl_idle:
				gspye->ServerListSort(lst, true, "ping", cm_int);
				GameSpy_Sort_By_Ping(lst);
				break;
			default:
				break;
		}
	}
}

void CL_PingNetServers_f (void)
{
	char goa_secret_key[7];
	int error;
	int allocatedSockets;

	if(!gspye)
	{
		Com_Printf("Error: GameSpy DLL not loaded!\n");
		return;
	}

	if(cls.gamespyupdate)
	{
		Com_Printf("Error: Already querying the GameSpy Master!\n");
		return;
	}

	NET_Config(true); /* FS: Retrieve a MOTD.  This is reset again later if we error out, abort, or if the server completes as long as we are disconnected */

	gspyCur = 0;
	memset(&browserList, 0, sizeof(browserList));
	memset(&browserListAll, 0, sizeof(browserListAll));

	goa_secret_key[0] = 'r';
	goa_secret_key[1] = 't';
	goa_secret_key[2] = 'W';
	goa_secret_key[3] = '0';
	goa_secret_key[4] = 'x';
	goa_secret_key[5] = 'g';
	goa_secret_key[6] = '\0'; /* FS: Gamespy requires a null terminator at the end of the secret key */

	if ((Cmd_Argc() == 1) || (cls.key_dest == key_menu))
	{
		cls.gamespyupdate = SHOW_POPULATED_SERVERS;;
		Com_Printf("\x02Grabbing populated server list from GameSpy master. . .\n");
	}
	else
	{
		cls.gamespyupdate = SHOW_ALL_SERVERS;
		Com_Printf("\x02Grabbing all servers from GameSpy master. . .\n");
	}

	cls.gamespypercent = 0;
	cls.gamespystarttime = (int)Sys_Milliseconds();
	cls.gamespytotalservers = 0;

	allocatedSockets = bound(5, cl_master_server_queries->value, 40);

	SCR_UpdateScreen(); /* FS: Force an update so the percentage bar shows some progress */

	serverlist = gspye->ServerListNew("quake2","quake2",goa_secret_key,allocatedSockets,ListCallBack,GSPYCALLBACK_FUNCTION,NULL);
	error = gspye->ServerListUpdate(serverlist,true); /* FS: Use Async now! */
	CL_Gamespy_Check_Error(serverlist, error);
}

static void CL_Gamespy_Update_Num_Servers(int numServers)
{
	cls.gamespytotalservers = numServers;
}

static void CL_LoadGameSpy (void)
{
	gspyimport_t	import;

	import.print = Com_Printf;
	import.dprint = Com_DPrintf;
	import.error = Sys_Error;
	import.cvar = Cvar_Get;
	import.cvar_set = Cvar_Set;
	import.cvar_forceset = Cvar_ForceSet;
	import.net_strerror = NET_ErrorString;
	import.run_keyevents = Sys_SendKeyEvents;
	import.play_sound = S_GamespySound;
	import.update_numservers = CL_Gamespy_Update_Num_Servers;

	gspye = (gspyexport_t *) Sys_GetGameSpyAPI (&import);
	if (!gspye)
	{
		Com_Printf ("failed to load GameSpy DLL\n");
		return;
	}
	if (gspye->api_version != GAMESPY_API_VERSION)
	{
		Com_Printf("Error: Unsupported GameSpy DLL version %i (need %i)\n", gspye->api_version, GAMESPY_API_VERSION);
		Sys_UnloadGameSpy();
		gspye = NULL;
		return;
	}

	/* all good */
	gspye->Init ();
}
#endif /* GAMESPY */

//=============================================================================

// Download speed counter

typedef struct {
	int		prevTime;
	int		bytesRead;
	int		byteCount;
	float	timeCount;
	float	prevTimeCount;
	float	startTime;
} dlSpeedInfo_t;

dlSpeedInfo_t	dlSpeedInfo;

/*
=====================
CL_Download_Reset_KBps_counter
=====================
*/
void CL_Download_Reset_KBps_counter (void)
{
	dlSpeedInfo.timeCount = dlSpeedInfo.prevTime = dlSpeedInfo.prevTimeCount = dlSpeedInfo.bytesRead = dlSpeedInfo.byteCount = 0;
	dlSpeedInfo.startTime = (float)cls.realtime;
	cls.downloadrate = 0;
}

/*
=====================
CL_Download_Calculate_KBps
=====================
*/
void CL_Download_Calculate_KBps (int byteDistance, int totalSize)
{
	float	timeDistance = (float)(cls.realtime - dlSpeedInfo.prevTime);
	float	totalTime = (dlSpeedInfo.timeCount - dlSpeedInfo.startTime) / 1000.0f;

	dlSpeedInfo.timeCount += timeDistance;
	dlSpeedInfo.byteCount += byteDistance;
	dlSpeedInfo.bytesRead += byteDistance;

//	Com_DPrintf(DEVELOPER_MSG_NET, "Time distance: %fs\n", timeDistance);
//	Com_DPrintf(DEVELOPER_MSG_NET, "Byte distance: %i\nByteCount: %i\nTotal: %i\n", byteDistance, dlSpeedInfo.byteCount, totalSize);
//	Com_DPrintf(DEVELOPER_MSG_NET, "Total time counted: %3.2fs\n", totalTime );

	if (totalTime >= 1.0f)
	{
		cls.downloadrate = (float)dlSpeedInfo.byteCount / 1024.0f;
		Com_DPrintf (DEVELOPER_MSG_NET, "Rate: %4.2fKB/s, Downloaded %4.2fKB of %4.2fKB\n", cls.downloadrate, (float)dlSpeedInfo.bytesRead/1024.0, (float)totalSize/1024.0);
		dlSpeedInfo.byteCount = 0;
		dlSpeedInfo.startTime = (float)cls.realtime;
	}
	dlSpeedInfo.prevTime = cls.realtime;
}

