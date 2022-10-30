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
// cl_parse.c  -- parse a message received from the server

#include "client.h"
#include "snd_loc.h"

char *svc_strings[256] =
{
	"svc_bad",

	"svc_muzzleflash",
	"svc_muzzlflash2",
	"svc_temp_entity",
	"svc_layout",
	"svc_inventory",

	"svc_nop",
	"svc_disconnect",
	"svc_reconnect",
	"svc_sound",
	"svc_print",
	"svc_stufftext",
	"svc_serverdata",
	"svc_configstring",
	"svc_spawnbaseline",	
	"svc_centerprint",
	"svc_download",
	"svc_playerinfo",
	"svc_packetentities",
	"svc_deltapacketentities",
	"svc_frame"
};

//=============================================================================

void CL_DownloadFileName(char *dest, int destlen, char *fn)
{
	if (strncmp(fn, "players", 7) == 0)
		Com_sprintf (dest, destlen, "%s/%s", BASEDIRNAME, fn);
	else
		Com_sprintf (dest, destlen, "%s/%s", FS_Gamedir(), fn);
}

/*
===============
CL_CheckOrDownloadFile

Returns true if the file exists, otherwise it attempts
to start a download from the server.
===============
*/
qboolean	CL_CheckOrDownloadFile (char *filename)
{
	FILE *fp;
	char	name[MAX_OSPATH];
	static char lastfilename[MAX_OSPATH] = {0};
	int	length = 0; /* FS: MSVC4 hates it elsewhere */
	char	*p = NULL; /* FS: MSVC4 hates it elsewhere */

	//r1: don't attempt same file many times
	if (!strcmp (filename, lastfilename))
		return true;

	strcpy (lastfilename, filename);

	if (strstr (filename, ".."))
	{
		Com_Printf ("Refusing to check a path with .. (%s)\n", filename);
		return true;
	}

	if (strchr (filename, ' '))
	{
		Com_Printf ("Refusing to check a path containing spaces (%s)\n", filename);
		return true;
	}

	if (strchr (filename, ':'))
	{
		Com_Printf ("Refusing to check a path containing a colon (%s)\n", filename);
		return true;
	}

	if (filename[0] == '/')
	{
		Com_Printf ("Refusing to check a path starting with / (%s)\n", filename);
		return true;
	}

	if (FS_LoadFile (filename, NULL) != -1)
	{	// it exists, no need to download
		return true;
	}
#ifdef USE_CURL /* HTTP downloading from R1Q2 */
	if (CL_QueueHTTPDownload(filename))
	{
		// We return true so that the precache check keeps feeding us more files.
		// Since we have multiple HTTP connections we want to minimize latency
		// and be constantly sending requests, not one at a time.
		return true;
	}
#endif

	strcpy (cls.downloadname, filename);

	//r1: fix \ to /
	p = cls.downloadname;
	while ((p = strchr(p, '\\')))
		p[0] = '/';

	length = (int)strlen(cls.downloadname);

	//normalize path
	p = cls.downloadname;
	while ((p = strstr (p, "./")))
	{
		memmove (p, p+2, length - (p - cls.downloadname) - 1);
		length -= 2;
	}

	//r1: verify we are giving the server a legal path
	if (cls.downloadname[length-1] == '/')
	{
		Com_Printf ("Refusing to download bad path (%s)\n", filename);
		return true;
	}

	// download to a temp name, and only rename
	// to the real name when done, so if interrupted
	// a runt file wont be left
	COM_StripExtension (cls.downloadname, cls.downloadtempname);
	strcat (cls.downloadtempname, ".tmp");

//ZOID
	// check to see if we already have a tmp for this file, if so, try to resume
	// open the file if not opened yet
	CL_DownloadFileName(name, sizeof(name), cls.downloadtempname);

//	FS_CreatePath (name);
	fp = fopen (name, "r+b");
	if (fp)
	{ // it exists
		int len;
		fseek(fp, 0, SEEK_END);
		len = ftell(fp);

		cls.download = fp;

		// give the server an offset to start the download
		Com_Printf ("Resuming %s\n", cls.downloadname);
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message, va("download \"%s\" %i", cls.downloadname, len));
	}
	else
	{
		Com_Printf ("Downloading %s\n", cls.downloadname);
		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		MSG_WriteString (&cls.netchan.message,
		va("download %s", cls.downloadname));
	}

	cls.downloadnumber++;
	cls.forcePacket = true;

	return false;
}

/*
===============
CL_Download_f

Request a download from the server
===============
*/
void	CL_Download_f (void)
{
	/* FS: R1Q2 version */
	char	*filename;

	if (Cmd_Argc() != 2) {
		Com_Printf("Usage: download <filename>\n");
		return;
	}

	if (cls.state < ca_connected)
	{
		Com_Printf ("You must be connected to use this command.\n");
		return;
	}

	//Com_sprintf(filename, sizeof(filename), "%s", Cmd_Argv(1));
	filename = Cmd_Argv(1);

	if (FS_LoadFile (filename, NULL) != -1)
	{	
		// it exists, no need to download
		Com_Printf("File already exists.\n");
		return;
	}

	CL_CheckOrDownloadFile (filename);
}

/*
======================
CL_RegisterSounds
======================
*/
void CL_RegisterSounds (void)
{
	int		i;

	S_BeginRegistration ();
	CL_RegisterTEntSounds ();
	for (i=1 ; i<MAX_SOUNDS ; i++)
	{
		if (!cl.configstrings[CS_SOUNDS+i][0])
			break;
		cl.sound_precache[i] = S_RegisterSound (cl.configstrings[CS_SOUNDS+i]);
		Sys_SendKeyEvents ();	// pump message loop
	}
	S_EndRegistration ();
}


/*
=====================
CL_ParseDownload

A download message has been received from the server
=====================
*/
void CL_ParseDownload (void)
{
	int		size, percent;
	char	name[MAX_OSPATH];
	int		r;

	// read the data
	size = MSG_ReadShort (&net_message);
	percent = MSG_ReadByte (&net_message);
	if (size < 0)
	{
		if (size == -1)
			Com_Printf ("Server does not have this file.\n");
		else
			Com_Printf ("Bad download data from server.\n");

		//r1: nuke the temp filename
		cls.downloadtempname[0] = 0;
		cls.downloadname[0] = 0;
		
		if (cls.download)
		{
			// if here, we tried to resume a file but the server said no
			fclose (cls.download);
			cls.download = NULL;
		}
		CL_RequestNextDownload ();
		return;
	}

	// open the file if not opened yet
	if (!cls.download)
	{
		CL_Download_Reset_KBps_counter ();	// Knightmare- for KB/s counter

		CL_DownloadFileName(name, sizeof(name), cls.downloadtempname);

		FS_CreatePath (name);

		cls.download = fopen (name, "wb");
		if (!cls.download)
		{
			net_message.readcount += size;
			Com_Printf ("Failed to open %s\n", cls.downloadtempname);
			CL_RequestNextDownload ();
			return;
		}
	}

	//r1: downloading something, drop to console to show status bar
	SCR_EndLoadingPlaque();

	fwrite (net_message.data + net_message.readcount, 1, size, cls.download);
	net_message.readcount += size;

	if (percent != 100)
	{
		// request next block
		CL_Download_Calculate_KBps (size, 0);	// Knightmare- for KB/s counter

		cls.downloadpercent = percent;

		MSG_WriteByte (&cls.netchan.message, clc_stringcmd);
		SZ_Print (&cls.netchan.message, "nextdl");
		cls.forcePacket = true;
	}
	else
	{
		char	oldn[MAX_OSPATH];
		char	newn[MAX_OSPATH];

//		Com_Printf ("100%%\n");

		fclose (cls.download);

		// rename the temp file to it's final name
		CL_DownloadFileName(oldn, sizeof(oldn), cls.downloadtempname);
		CL_DownloadFileName(newn, sizeof(newn), cls.downloadname);
		r = rename (oldn, newn);
		if (r)
			Com_Printf ("failed to rename.\n");

		cls.download = NULL;
		cls.downloadpercent = 0;

		// get another file if needed

		CL_RequestNextDownload ();
	}
}


/*
=====================================================================

  SERVER CONNECTING MESSAGES

=====================================================================
*/

/*
==================
CL_ParseServerData
==================
*/
void CL_ParseServerData (void)
{
	extern cvar_t	*fs_gamedirvar;
	char	*str;
	int		i;
	
	Com_DPrintf(DEVELOPER_MSG_NET, "Serverdata packet received.\n");
//
// wipe the client_state_t struct
//
	CL_ClearState ();
	cls.state = ca_connected;

// parse protocol version number
	i = MSG_ReadLong (&net_message);
	cls.serverProtocol = i;

	// BIG HACK to let demos from release work with the 3.0x patch!!!
	if (Com_ServerState() && PROTOCOL_VERSION == 34)
	{
	}
	else if (i != PROTOCOL_VERSION)
		Com_Error (ERR_DROP,"Server returned version %i, not %i", i, PROTOCOL_VERSION);

	cl.servercount = MSG_ReadLong (&net_message);
	cl.attractloop = MSG_ReadByte (&net_message);

	// game directory
	str = MSG_ReadString (&net_message);
	strncpy (cl.gamedir, str, sizeof(cl.gamedir)-1);

	// set gamedir
	if ((*str && (!fs_gamedirvar->string || !*fs_gamedirvar->string || strcmp(fs_gamedirvar->string, str) != 0)) || (!*str && (fs_gamedirvar->string || *fs_gamedirvar->string)))
		Cvar_Set("game", str);

	// parse player entity number
	cl.playernum = MSG_ReadShort (&net_message);

	// get the full level name
	str = MSG_ReadString (&net_message);

	if (cl.playernum == -1)
	{	// playing a cinematic or showing a pic, not a level
		SCR_PlayCinematic (str);
	}
	else
	{
		// seperate the printfs so the server message can have a color
		Com_Printf("\n\n\35\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\36\37\n\n");
		Com_Printf ("%c%s\n", 2, str);

		// need to prep refresh at next oportunity
		cl.refresh_prepped = false;
	}
}

/*
==================
CL_ParseBaseline
==================
*/
void CL_ParseBaseline (void)
{
	entity_state_t	*es;
	unsigned int				bits;
	int				newnum;
	entity_state_t	nullstate;

	memset (&nullstate, 0, sizeof(nullstate));

	newnum = CL_ParseEntityBits (&bits);
	es = &cl_entities[newnum].baseline;
	CL_ParseDelta (&nullstate, es, newnum, bits);
}


/*
================
CL_LoadClientinfo

================
*/

void CL_LoadClientinfo (clientinfo_t *ci, char *s)
{
	int i;
	char		*t;
	char		model_name[MAX_QPATH];
	char		skin_name[MAX_QPATH];
	char		model_filename[MAX_QPATH];
	char		skin_filename[MAX_QPATH];
	char		weapon_filename[MAX_QPATH];

	strncpy(ci->cinfo, s, sizeof(ci->cinfo));
	ci->cinfo[sizeof(ci->cinfo)-1] = 0;
	// sku avoid buffer overflow
	s = ci->cinfo;

	// isolate the player's name
	strncpy(ci->name, s, sizeof(ci->name));
	ci->name[sizeof(ci->name)-1] = 0;
	t = strchr (s, '\\');
	if (t)
	{
		ci->name[t-s] = 0;
		s = t+1;
	}

	if (cl_noskins->value || *s == 0)
	{
		Com_sprintf (model_filename, sizeof(model_filename), "players/male/tris.md2");
		Com_sprintf (weapon_filename, sizeof(weapon_filename), "players/male/weapon.md2");
		Com_sprintf (skin_filename, sizeof(skin_filename), "players/male/grunt.pcx");
		Com_sprintf (ci->iconname, sizeof(ci->iconname), "/players/male/grunt_i.pcx");
		ci->model = re.RegisterModel (model_filename);
		memset(ci->weaponmodel, 0, sizeof(ci->weaponmodel));
		ci->weaponmodel[0] = re.RegisterModel (weapon_filename);
		ci->skin = re.RegisterSkin (skin_filename);
		ci->icon = re.RegisterPic (ci->iconname);
	}
	else
	{
		// isolate the model name
		strcpy (model_name, s);
		t = strchr(model_name, '/');
		if (!t)
			t = strchr(model_name, '\\');
		if (!t)
			t = model_name;
		*t = 0;

		// isolate the skin name
		strcpy (skin_name, s + strlen(model_name) + 1);

		// model file
		Com_sprintf (model_filename, sizeof(model_filename), "players/%s/tris.md2", model_name);
		ci->model = re.RegisterModel (model_filename);
		if (!ci->model)
		{
			strcpy(model_name, "male");
			Com_sprintf (model_filename, sizeof(model_filename), "players/male/tris.md2");
			ci->model = re.RegisterModel (model_filename);
		}

		// skin file
		Com_sprintf (skin_filename, sizeof(skin_filename), "players/%s/%s.pcx", model_name, skin_name);
		ci->skin = re.RegisterSkin (skin_filename);

		// if we don't have the skin and the model wasn't male,
		// see if the male has it (this is for CTF's skins)
 		if (!ci->skin && Q_stricmp(model_name, "male"))
		{
			// change model to male
			strcpy(model_name, "male");
			Com_sprintf (model_filename, sizeof(model_filename), "players/male/tris.md2");
			ci->model = re.RegisterModel (model_filename);

			// see if the skin exists for the male model
			Com_sprintf (skin_filename, sizeof(skin_filename), "players/%s/%s.pcx", model_name, skin_name);
			ci->skin = re.RegisterSkin (skin_filename);
		}

		// if we still don't have a skin, it means that the male model didn't have
		// it, so default to grunt
		if (!ci->skin) {
			// see if the skin exists for the male model
			Com_sprintf (skin_filename, sizeof(skin_filename), "players/%s/grunt.pcx", model_name);
			ci->skin = re.RegisterSkin (skin_filename);
		}

		// weapon file
		for (i = 0; i < num_cl_weaponmodels; i++) {
			Com_sprintf (weapon_filename, sizeof(weapon_filename), "players/%s/%s", model_name, cl_weaponmodels[i]);
			ci->weaponmodel[i] = re.RegisterModel(weapon_filename);
			if (!ci->weaponmodel[i] && strcmp(model_name, "cyborg") == 0) {
				// try male
				Com_sprintf (weapon_filename, sizeof(weapon_filename), "players/male/%s", cl_weaponmodels[i]);
				ci->weaponmodel[i] = re.RegisterModel(weapon_filename);
			}
			if (!cl_vwep->intValue)
				break; // only one when vwep is off
		}

		// icon file
		Com_sprintf (ci->iconname, sizeof(ci->iconname), "/players/%s/%s_i.pcx", model_name, skin_name);
		ci->icon = re.RegisterPic (ci->iconname);
	}

	// must have loaded all data types to be valud
	if (!ci->skin || !ci->icon || !ci->model || !ci->weaponmodel[0])
	{
		ci->skin = NULL;
		ci->icon = NULL;
		ci->model = NULL;
		ci->weaponmodel[0] = NULL;
		return;
	}
}


/*
================
CL_ParseClientinfo

Load the skin, icon, and model for a client
================
*/
void CL_ParseClientinfo (int player)
{
	char			*s;
	clientinfo_t	*ci;

	s = cl.configstrings[player+CS_PLAYERSKINS];

	ci = &cl.clientinfo[player];

	CL_LoadClientinfo (ci, s);
}

// Knightmare added
qboolean FS_ModType (char *name);
/*
================
CL_MissionPackCDTrack
Returns correct OGG track number for mission packs.
This assumes that the standard Q2 CD was ripped
as track02-track11, and the Rogue CD as track12-track21.
================
*/
int CL_MissionPackCDTrack (int tracknum)
{
	if (FS_ModType("rogue") || cl_rogue_music->value)
	{
		if (tracknum >= 2 && tracknum <= 11)
			return tracknum + 10;
		else
			return tracknum;
	}
	// an out-of-order mix from Q2 and Rogue CDs
	else if (FS_ModType("xatrix") || cl_xatrix_music->value)
	{
		switch(tracknum)
		{
			case 2: return 9;	break;
			case 3: return 13;	break;
			case 4: return 14;	break;
			case 5: return 7;	break;
			case 6: return 16;	break;
			case 7: return 2;	break;
			case 8: return 15;	break;
			case 9: return 3;	break;
			case 10: return 4;	break;
			case 11: return 18; break;
			default: return tracknum; break;
		}
	}
	else
		return tracknum;
}
/*
=================
CL_PlayBackgroundTrack
=================
*/
void CL_PlayBackgroundTrack (void)
{
#define BGMUSIC_WAV (1<<0)
#define BGMUSIC_OGG (1<<1)

	char	name[MAX_QPATH], *p;
	int		track;
	int	have_extmusic;

	Com_DPrintf(DEVELOPER_MSG_CD, "CL_PlayBackgroundTrack\n");

	if (!cl.refresh_prepped)
		return;

	/* using a named audio track intead of numbered */
	if (strlen(cl.configstrings[CS_CDTRACK]) > 2)
	{
		/* check existing files with supported extensions */
		have_extmusic = 0;
		Com_sprintf (name, sizeof(name), "music/%s.", cl.configstrings[CS_CDTRACK]);
		p = name + strlen(name);
		strcpy (p, "wav");
		if (FS_LoadFile(name, NULL) != -1)
			have_extmusic |= BGMUSIC_WAV;
#ifdef OGG_SUPPORT
		strcpy (p, "ogg");
		if (FS_LoadFile(name, NULL) != -1)
			have_extmusic |= BGMUSIC_OGG;
#endif

		/* play whatever is found */
		if ((have_extmusic&BGMUSIC_WAV) && (cl_wav_music->intValue || !(have_extmusic&BGMUSIC_OGG))) {
			CDAudio_Stop();
			strcpy (p, "wav");
			S_StartWAVBackgroundTrack(name, name);
			return;
		}
#ifdef OGG_SUPPORT
		if (have_extmusic&BGMUSIC_OGG) {
			CDAudio_Stop();
			strcpy (p, "ogg");
			S_StartOGGBackgroundTrack(name, name);
			return;
		}
#endif
	}

	track = atoi(cl.configstrings[CS_CDTRACK]);
	if (track == 0)
	{	// Stop any playing track
		Com_DPrintf(DEVELOPER_MSG_CD, "CL_PlayBackgroundTrack: stopping\n");
		CDAudio_Stop();
		S_StopBackgroundTrack();
		return;
	}

	/* If an external music file exists play it, otherwise fall back to CD audio */
	have_extmusic = 0;
	Com_sprintf (name, sizeof(name), "music/track%02i.", CL_MissionPackCDTrack(track));
	p = name + strlen(name);
	strcpy (p, "wav");
	if (FS_LoadFile(name, NULL) != -1)
		have_extmusic |= BGMUSIC_WAV;
#ifdef OGG_SUPPORT
	strcpy (p, "ogg");
	if (FS_LoadFile(name, NULL) != -1)
		have_extmusic |= BGMUSIC_OGG;
#endif

	/* play whatever is found */
	if ((have_extmusic&BGMUSIC_WAV) && (cl_wav_music->intValue || !(have_extmusic&BGMUSIC_OGG))) {
		CDAudio_Stop();
		strcpy (p, "wav");
		S_StartWAVBackgroundTrack(name, name);
	}
#ifdef OGG_SUPPORT
	else if (have_extmusic&BGMUSIC_OGG) {
		CDAudio_Stop();
		strcpy (p, "ogg");
		S_StartOGGBackgroundTrack(name, name);
	}
#endif
	else {
		CDAudio_Play(track, true);
	}
}
// end Knightmare

/*
================
CL_ParseConfigString
================
*/
void CL_ParseConfigString (void)
{
	int		i, length;
	char	*s;
	char	olds[MAX_QPATH];

	i = MSG_ReadShort (&net_message);
	if (i < 0 || i >= MAX_CONFIGSTRINGS)
		Com_Error (ERR_DROP, "configstring > MAX_CONFIGSTRINGS");
	s = MSG_ReadString(&net_message);

	strncpy (olds, cl.configstrings[i], sizeof(olds));
	olds[sizeof(olds) - 1] = 0;

	// sku - avoid potentional buffer overflow vulnerability
	length = strlen(s);
	if (length > sizeof(cl.configstrings) - sizeof(cl.configstrings[0])*i - 1) {
		Com_Error(ERR_DROP, "CL_ParseConfigString: oversize configstring");
	}
	strcpy (cl.configstrings[i], s);

	// do something apropriate 

	if (i >= CS_LIGHTS && i < CS_LIGHTS+MAX_LIGHTSTYLES)
		CL_SetLightstyle (i - CS_LIGHTS);
	else if (i == CS_CDTRACK)
	{
		if (cl.refresh_prepped)
		//	CDAudio_Play (atoi(cl.configstrings[CS_CDTRACK]), true);
			CL_PlayBackgroundTrack ();	// Knightmare changed
	}
	else if (i >= CS_MODELS && i < CS_MODELS+MAX_MODELS)
	{
		if (cl.refresh_prepped)
		{
			cl.model_draw[i-CS_MODELS] = re.RegisterModel (cl.configstrings[i]);
			if (cl.configstrings[i][0] == '*')
				cl.model_clip[i-CS_MODELS] = CM_InlineModel (cl.configstrings[i]);
			else
				cl.model_clip[i-CS_MODELS] = NULL;
		}
	}
	else if (i >= CS_SOUNDS && i < CS_SOUNDS+MAX_MODELS)
	{
		if (cl.refresh_prepped)
			cl.sound_precache[i-CS_SOUNDS] = S_RegisterSound (cl.configstrings[i]);
	}
	else if (i >= CS_IMAGES && i < CS_IMAGES+MAX_MODELS)
	{
		if (cl.refresh_prepped)
			cl.image_precache[i-CS_IMAGES] = re.RegisterPic (cl.configstrings[i]);
	}
	else if (i == CS_MAXCLIENTS)
	{
		if (!cl.attractloop)
			cl.maxclients = atoi(cl.configstrings[CS_MAXCLIENTS]);
	}
	else if (i >= CS_PLAYERSKINS && i < CS_PLAYERSKINS+MAX_CLIENTS)
	{
		//r1: hack to avoid parsing non-skins from mods that overload CS_PLAYERSKINS
		//FIXME: how reliable is CS_MAXCLIENTS?
		i -= CS_PLAYERSKINS;
		if (i < cl.maxclients)
		{
			if (cl.refresh_prepped && strcmp(olds, s) != 0)
				CL_ParseClientinfo (i);
		}
		else
		{
			Com_DPrintf (DEVELOPER_MSG_NET, "CL_ParseConfigString: Ignoring out-of-range playerskin %d (%s)\n", i, s);
		}
	}
}


/*
=====================================================================

ACTION MESSAGES

=====================================================================
*/

/*
==================
CL_ParseStartSoundPacket
==================
*/
void CL_ParseStartSoundPacket(void)
{
    vec3_t  pos_v;
	float	*pos;
    int 	channel, ent;
    int 	sound_num;
    float 	volume;
    float 	attenuation;  
	int		flags;
	float	ofs;

	flags = MSG_ReadByte (&net_message);
	sound_num = MSG_ReadByte (&net_message);

    if (flags & SND_VOLUME)
		volume = MSG_ReadByte (&net_message) / 255.0;
	else
		volume = DEFAULT_SOUND_PACKET_VOLUME;
	
    if (flags & SND_ATTENUATION)
		attenuation = MSG_ReadByte (&net_message) / 64.0;
	else
		attenuation = DEFAULT_SOUND_PACKET_ATTENUATION;	

    if (flags & SND_OFFSET)
		ofs = MSG_ReadByte (&net_message) / 1000.0;
	else
		ofs = 0;

	if (flags & SND_ENT)
	{	// entity reletive
		channel = MSG_ReadShort(&net_message); 
		ent = channel>>3;
		if (ent > MAX_EDICTS)
			Com_Error (ERR_DROP,"CL_ParseStartSoundPacket: ent = %i", ent);

		channel &= 7;
	}
	else
	{
		ent = 0;
		channel = 0;
	}

	if (flags & SND_POS)
	{	// positioned in space
		MSG_ReadPos (&net_message, pos_v);
 
		pos = pos_v;
	}
	else	// use entity number
		pos = NULL;

	if (!cl.sound_precache[sound_num])
		return;

	S_StartSound (pos, ent, channel, cl.sound_precache[sound_num], volume, attenuation, ofs);
}       


void SHOWNET(char *s)
{
	if (cl_shownet->intValue >=2)
		Com_Printf ("%3i:%s\n", net_message.readcount-1, s);
}

/* FS: One time, I was playing on a xatrix server and that a player who has been
       MIA for months moved on to another xatrix server.  The admin started sending
	   down malicious stufftexts for me to connect elsewhere.  Let's stop that and other
	   potentially scary shit */
qboolean CL_MaliciousStuffText(char *s)
{
	if(!cl_stufftext_check->intValue)
		return false;

	if(!s || s[0] == 0)
		return false;

	if((cl_stufftext_check->intValue > 1) && (strstr(s, "connect "))) /* FS: Special case, because you might want to use WallFly to jump from server-to-server, but ignore malicious admin from Friends of Xatrix server. */
	{
		Com_DPrintf (DEVELOPER_MSG_NET, "Ignoring malicious stufftext: %s\n", s);
		return true;
	}

	if( (strstr(s, "fov ")) || (strstr(s, "s_mixahead ")) || (strstr(s, "rate ")) || (strstr(s, "cl_maxfps ")) || (strstr(s, "r_maxfps ")) )
	{
		Com_DPrintf (DEVELOPER_MSG_NET, "Ignoring malicious stufftext: %s\n", s);
		return true;
	}

	return false;
}

/*
=====================
CL_ParseServerMessage
=====================
*/
void CL_ParseServerMessage (void)
{
	int			cmd;
	char		*s;
	int			i;

//
// if recording demos, copy the message out
//
	if (cl_shownet->intValue == 1)
		Com_Printf ("%i ",net_message.cursize);
	else if (cl_shownet->intValue >= 2)
		Com_Printf ("------------------\n");


//
// parse the message
//
	while (1)
	{
		if (net_message.readcount > net_message.cursize)
		{
			Com_Error (ERR_DROP,"CL_ParseServerMessage: Bad server message");
			break;
		}

		cmd = MSG_ReadByte (&net_message);

		if (cmd == -1)
		{
			SHOWNET("END OF MESSAGE");
			break;
		}

		if (cl_shownet->value>=2)
		{
			if (!svc_strings[cmd])
				Com_Printf ("%3i:BAD CMD %i\n", net_message.readcount-1,cmd);
			else
				SHOWNET(svc_strings[cmd]);
		}
	
	// other commands
		switch (cmd)
		{
		default:
			Com_Error (ERR_DROP,"CL_ParseServerMessage: Illegible server message\n");
			break;
			
		case svc_nop:
//			Com_Printf ("svc_nop\n");
			break;
			
		case svc_disconnect:
			Com_Error (ERR_DISCONNECT,"Server disconnected\n");
			break;

		case svc_reconnect:
			Com_Printf ("Server disconnected, reconnecting\n");
			if (cls.download) {
				//ZOID, close download
				fclose (cls.download);
				cls.download = NULL;
			}
			cls.state = ca_connecting;
			cls.connect_time = -99999;	// CL_CheckForResend() will fire immediately
			break;

		case svc_print:
			i = MSG_ReadByte (&net_message);
			s = MSG_ReadString(&net_message);

			if (i == PRINT_CHAT)
			{
				S_StartLocalSound ("misc/talk.wav");
				con.ormask = 128;

				//r1: change !p_version to !version since p is for proxies
				if ((strstr (s, "!r1q2_version") || strstr (s, "!version")) &&
					(cls.lastSpamTime == 0 || cls.realtime > cls.lastSpamTime + 300000)) /* FS: 5 minutes */
				{
					cls.spamTime = cls.realtime + 1500; /* FS: 1.5 second delay */
				}

			}
			Com_Printf("%s", s); /* FS: !version reply */

			con.ormask = 0;
			break;
			
		case svc_centerprint:
			SCR_CenterPrint (MSG_ReadString (&net_message));
			break;
			
		case svc_stufftext:
			s = MSG_ReadString (&net_message);
			Com_DPrintf(DEVELOPER_MSG_NET, "stufftext: %s\n", s);

			if(!CL_MaliciousStuffText(s)) /* FS: See function for more info */
				Cbuf_AddText (s);
			break;
			
		case svc_serverdata:
			Cbuf_Execute ();		// make sure any stuffed commands are done
			CL_ParseServerData ();
			break;
			
		case svc_configstring:
			CL_ParseConfigString ();
			break;
			
		case svc_sound:
			CL_ParseStartSoundPacket();
			break;
			
		case svc_spawnbaseline:
			CL_ParseBaseline ();
			break;

		case svc_temp_entity:
			CL_ParseTEnt ();
			break;

		case svc_muzzleflash:
			CL_ParseMuzzleFlash ();
			break;

		case svc_muzzleflash2:
			CL_ParseMuzzleFlash2 ();
			break;

		case svc_download:
			CL_ParseDownload ();
			break;

		case svc_frame:
			CL_ParseFrame ();
			break;

		case svc_inventory:
			CL_ParseInventory ();
			break;

		case svc_layout:
			s = MSG_ReadString (&net_message);
			strncpy (cl.layout, s, sizeof(cl.layout)-1);
			break;

		case svc_playerinfo:
		case svc_packetentities:
		case svc_deltapacketentities:
			Com_Error (ERR_DROP, "Out of place frame data");
			break;
		}
	}

	CL_AddNetgraph ();

	//
	// we don't know if it is ok to save a demo message until
	// after we have parsed the frame
	//
	if (cls.demorecording && !cls.demowaiting)
		CL_WriteDemoMessage ();

}


