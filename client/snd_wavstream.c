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

/* FS: WAV streaming.  Basically a paraphrased snd_stream.c for OGG/Vorbis */

#include "client.h"
#include "snd_loc.h"

static wavinfo_t	musicWavInfo;
static bgTrack_t	s_bgTrack;

static qboolean		wav_first_init = true;	// First initialization flag
static qboolean		wav_started = false;	// Initialization flag
static bgm_status_t	trk_status;		// Status indicator

#define MAX_WAVLIST 512
static char		**wav_filelist;		// List of WAV files
static int			wav_numfiles;		// Number of WAV files
static int			wav_loopcounter;

static cvar_t		*wav_loopcount;
static cvar_t		*wav_ambient_track;

static void S_WAV_LoadFileList (void);
static void S_WAV_ParseCmd (void);


static int FGetLittleLong (FILE *f)
{
	int		v;

	fread(&v, 1, sizeof(v), f);

	return LittleLong(v);
}

static short FGetLittleShort(FILE *f)
{
	short	v;

	fread(&v, 1, sizeof(v), f);

	return LittleShort(v);
}

static int WAV_ReadChunkInfo(FILE *f, char *name)
{
	int len, r;

	name[4] = 0;

	r = fread(name, 1, 4, f);
	if (r != 4)
		return -1;

	len = FGetLittleLong(f);
	if (len < 0)
	{
		Com_Printf("WAV: Negative chunk length\n");
		return -1;
	}

	return len;
}

/*
=================
WAV_FindRIFFChunk

Returns the length of the data in the chunk, or -1 if not found
=================
*/
static int WAV_FindRIFFChunk(FILE *f, const char *chunk)
{
	char	name[5];
	int		len;

	while ((len = WAV_ReadChunkInfo(f, name)) >= 0)
	{
		/* If this is the right chunk, return */
		if (!strncmp(name, chunk, 4))
			return len;
		len = ((len + 1) & ~1);	/* pad by 2 . */

		/* Not the right chunk - skip it */
		fseek(f, len, SEEK_CUR);
	}

	return -1;
}

/*
=================
WAV_ReadRIFFHeader
=================
*/
static qboolean WAV_ReadRIFFHeader(const char *name, FILE *file, wavinfo_t *info)
{
	char dump[16];
	int value;
	int fmtlen = 0;

	if (fread(dump, 1, 12, file) < 12 ||
	    strncmp(dump, "RIFF", 4) != 0 ||
	    strncmp(&dump[8], "WAVE", 4) != 0)
	{
		Com_Printf("%s is missing RIFF/WAVE chunks\n", name);
		return false;
	}

	/* Scan for the format chunk */
	if ((fmtlen = WAV_FindRIFFChunk(file, "fmt ")) < 0)
	{
		Com_Printf("%s is missing fmt chunk\n", name);
		return false;
	}

	/* Save the parameters */
	value = FGetLittleShort(file); /* wav_format */
	if (value != WAV_FORMAT_PCM)
	{
		Com_Printf("%s is not Microsoft PCM format\n", name);
		return false;
	}

	info->channels = FGetLittleShort(file);
	info->rate = FGetLittleLong(file);
	FGetLittleLong(file);
	FGetLittleShort(file);
	value = FGetLittleShort(file); /* bits */

	if (value != 8 && value != 16)
	{
		Com_Printf("%s is not 8 or 16 bit\n", name);
		return false;
	}

	info->width = value / 8;

	/* Skip the rest of the format chunk if required */
	if (fmtlen > 16)
	{
		fmtlen -= 16;
		fseek(file, fmtlen, SEEK_CUR);
	}

	/* Scan for the data chunk */
	if ((value = WAV_FindRIFFChunk(file, "data")) < 0) /* size */
	{
		Com_Printf("%s is missing data chunk\n", name);
		return false;
	}

	info->dataofs = ftell(file);
	fseek(file, 0, SEEK_END);
	if (info->dataofs + value > ftell(file))
	{
		Com_Printf("%s data size mismatch\n", name);
		return false;
	}

	if (info->channels != 1 && info->channels != 2)
	{
		Com_Printf("Unsupported number of channels %d in %s\n",
						info->channels, name);
		return false;
	}
	info->samples = (value / info->width) / info->channels;
	if (info->samples == 0)
	{
		Com_Printf("%s has zero samples\n", name);
		return false;
	}

//	Com_Printf("Rate: %i, Width: %i, CH: %i. DataOffset: %i, Samples: %i.\n",
//		    musicWavInfo.rate, musicWavInfo.width, musicWavInfo.channels,
//		    musicWavInfo.dataofs, musicWavInfo.samples);
	fseek(file, info->dataofs, SEEK_SET);
	return true;
}

static qboolean S_OpenWAVBackgroundTrack (char *name, bgTrack_t *track)
{
	char	filename[MAX_OSPATH];
	char	*path = NULL;

//	Com_Printf("Opening background track: %s\n", name);
	do
	{
		path = FS_NextPath( path );
		Com_sprintf( filename, sizeof(filename), "%s/%s", path, name );
		if ((track->file = fopen(filename, "rb")) != NULL)
			break;
	} while ( path );

	if (!track->file)
	{
		Com_Printf("Couldn't find %s\n", name);
		return false;
	}

	return WAV_ReadRIFFHeader(name, track->file, &musicWavInfo);
}

static void S_CloseWAVBackgroundTrack(void)
{
	if(!s_bgTrack.file)
		return;

	fclose(s_bgTrack.file);
	s_bgTrack.file = NULL;
}

void S_StreamWAVBackgroundTrack(void)
{
	int		i, samples, maxSamples;
	int		read, maxRead, total;
	float	scale;
	byte	musicWavData[MAX_RAW_SAMPLES];

	if (!s_bgTrack.file || !s_musicvolume->value || !cl_wav_music->intValue)
		return;

	if (s_rawend < paintedtime)
		s_rawend = paintedtime;

	scale = (float)musicWavInfo.rate / dma.speed;
	maxSamples = sizeof(musicWavData) / musicWavInfo.channels / musicWavInfo.width;
//	Com_Printf("Scale: %f.  Max Samples: %i\n", scale, maxSamples);

	while (1)
	{
		samples = (paintedtime + MAX_RAW_SAMPLES - s_rawend) * scale;
		if (samples <= 0)
			return;
		if (samples > maxSamples)
			samples = maxSamples;
		maxRead = samples * musicWavInfo.channels * musicWavInfo.width;

		total = 0;
		while (total < maxRead)
		{
			read = fread(musicWavData, 1, maxRead, s_bgTrack.file);
			if (!read)
			{	// End of file
				if (!s_bgTrack.looping)
				{	// Close the intro track
					S_CloseWAVBackgroundTrack();

					// Open the loop track
					if (!S_OpenWAVBackgroundTrack(s_bgTrack.loopName, &s_bgTrack))
					{
						S_StopBackgroundTrack();
						return;
					}
					s_bgTrack.looping = true;
				}
				else
				{	// check if it's time to switch to the ambient track
					if ( ++wav_loopcounter >= wav_loopcount->intValue
						&& (!cl.configstrings[CS_MAXCLIENTS][0] || !strcmp(cl.configstrings[CS_MAXCLIENTS], "1")) )
					{	// Close the loop track
						S_CloseWAVBackgroundTrack();

						if (!S_OpenWAVBackgroundTrack(s_bgTrack.ambientName, &s_bgTrack))
						{
							if (!S_OpenWAVBackgroundTrack(s_bgTrack.loopName, &s_bgTrack))
							{
								S_StopBackgroundTrack();
								return;
							}
						}
						else
							s_bgTrack.ambient_looping = true;
					}
				}

				// Restart the track, skipping over the header
				fseek(s_bgTrack.file, musicWavInfo.dataofs, SEEK_SET);
			}

			total+= read;
//			Com_Printf("Read: %i, Samples: %i, Total: %i\n", read, samples, total);
		}
		if (musicWavInfo.width == 2) {
			total = samples * musicWavInfo.channels;
			for (i = 0; i < total; i++) {
				((short *)musicWavData)[i] = LittleShort( ((short *)musicWavData)[i] );
			}
		}
		S_RawSamples (samples, musicWavInfo.rate, musicWavInfo.width, musicWavInfo.channels, musicWavData, true);
	}
}

void S_UpdateWavTrack(void)
{
	// stop music if paused
	if (trk_status == BGM_PLAY)// && !cl_paused->value)
		S_StreamWAVBackgroundTrack();
}

void S_StartWAVBackgroundTrack (const char *introTrack, const char *loopTrack)
{
	if (!wav_started) // was sound_started
		return;

	// Stop any playing tracks
	S_StopBackgroundTrack();

	// Start it up
	Q_strncpyz(s_bgTrack.introName, introTrack, sizeof(s_bgTrack.introName));
	Q_strncpyz(s_bgTrack.loopName, loopTrack, sizeof(s_bgTrack.loopName));
	Q_strncpyz(s_bgTrack.ambientName, va("music/%s.wav", wav_ambient_track->string), sizeof(s_bgTrack.ambientName));

	// set a loop counter so that this track will change to the ambient track later
	wav_loopcounter = 0;

	// Open the intro track
	if (!S_OpenWAVBackgroundTrack(s_bgTrack.introName, &s_bgTrack))
	{
		S_StopBackgroundTrack();
		return;
	}

	trk_status = BGM_PLAY;

	S_StreamWAVBackgroundTrack();
}

/* FS: Called from S_StopBackgroundTrack in snd_dma.c */
void S_StopWAVBackgroundTrack (void)
{
	if (!wav_started)
		return;

	S_CloseWAVBackgroundTrack();

	trk_status = BGM_STOP;

	memset(&musicWavInfo, 0, sizeof(wavinfo_t));
	memset(&s_bgTrack, 0, sizeof(bgTrack_t));
}

void S_WAV_Init (void)
{
	if (wav_started)
		return;

	// Cvars
	wav_loopcount = Cvar_Get ("wav_loopcount", "5", CVAR_ARCHIVE);
	wav_ambient_track = Cvar_Get ("wav_ambient_track", "track11", CVAR_ARCHIVE);

	// Console commands
	Cmd_AddCommand("wav", S_WAV_ParseCmd);

	// Build list of files
	Com_Printf("Searching for WAV files...\n");
	wav_numfiles = 0;
	S_WAV_LoadFileList ();
	Com_Printf("%d WAV files found.\n", wav_numfiles);

	// Initialize variables
	if (wav_first_init) {
		trk_status = BGM_STOP;
		wav_first_init = false;
	}

	wav_started = true;
}

void S_WAV_Shutdown (void)
{
	int		i;

	if (!wav_started)
		return;

	S_StopBackgroundTrack ();

	// Free the list of files
	for (i = 0; i < wav_numfiles; i++)
		free(wav_filelist[i]);
	if (wav_numfiles > 0)
		free(wav_filelist);

	// Remove console commands
	Cmd_RemoveCommand("wav");

	wav_started = false;
}

void S_WAV_Restart (void)
{
	S_WAV_Shutdown ();
	S_WAV_Init ();
}

static void S_WAV_LoadFileList (void)
{
	char	*p, *path = NULL;
	char	**list;			// List of .ogg files
	char	findname[MAX_OSPATH];
	char	lastPath[MAX_OSPATH];	// Knightmare added
	int		i, numfiles = 0;

	wav_filelist = malloc(sizeof(char *) * MAX_WAVLIST);
	if (!wav_filelist)
	{
		Sys_Error("S_WAV_LoadFileList:  Failed to allocate memory.\n");
		return;
	}
	memset( wav_filelist, 0, sizeof( char * ) * MAX_WAVLIST );
	lastPath[0] = 0;	// Knightmare added

	// Set search path
	path = FS_NextPath(path);
	while (path)
	{
		// Knightmare- catch repeated paths
		if ( lastPath[0] != '\0' && !strcmp (path, lastPath) ) {
			path = FS_NextPath( path );
			continue;
		}

		// Get file list
		Com_sprintf( findname, sizeof(findname), "%s/music/*.wav", path );
		list = FS_ListFiles(findname, &numfiles, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM);

		// Add valid Ogg Vorbis file to the list
		for (i=0; i<numfiles && wav_numfiles<MAX_WAVLIST; i++)
		{
			if (!list || !list[i])
				continue;
			p = list[i];

			if (!strstr(p, ".wav"))
				continue;
			if (!FS_ItemInList(p, wav_numfiles, wav_filelist)) // check if already in list
			{
				wav_filelist[wav_numfiles] = strdup(p);
				wav_numfiles++;
			}
		}
		if (numfiles) // Free the file list
			FS_FreeFileList(list, numfiles);

		Q_strncpyz (lastPath, path, sizeof(lastPath));	// Knightmare- copy to lastPath
		path = FS_NextPath( path );
	}
}

static void S_WAV_PlayCmd (void)
{
	char	name[MAX_QPATH];

	if (Cmd_Argc() < 3) {
		Com_Printf("Usage: wav play {track}\n");
		return;
	}
	Com_sprintf(name, sizeof(name), "music/%s.wav", Cmd_Argv(2) );
	S_StartWAVBackgroundTrack (name, name);
}

static void S_WAV_StatusCmd (void)
{
	const char	*trackName;

	if (s_bgTrack.ambient_looping)
		trackName = s_bgTrack.ambientName;
	else if (s_bgTrack.looping)
		trackName = s_bgTrack.loopName;
	else
		trackName = s_bgTrack.introName;

	switch (trk_status)
	{
	case BGM_PLAY:
		Com_Printf("Playing file %s.\n", trackName);
		break;
	case BGM_PAUSE:
		Com_Printf("Paused file %s.\n", trackName);
		break;
	case BGM_STOP:
		Com_Printf("Stopped.\n");
		break;
	}
}

static void S_WAV_ListCmd (void)
{
	int i;

	if (wav_numfiles <= 0) {
		Com_Printf("No WAV files to list.\n");
		return;
	}

	for (i = 0; i < wav_numfiles; i++)
		Com_Printf("%d %s\n", i+1, wav_filelist[i]);

	Com_Printf("%d WAV files.\n", wav_numfiles);
}

static void S_WAV_ParseCmd (void)
{
	char	*command;

	if (Cmd_Argc() < 2) {
		Com_Printf("Usage: wav {play | pause | resume | stop | status | list}\n");
		return;
	}

	command = Cmd_Argv (1);

	if (Q_strcasecmp(command, "play") == 0) {
		S_WAV_PlayCmd ();
		return;
	}

	if (Q_strcasecmp(command, "pause") == 0) {
		if (trk_status == BGM_PLAY)
			trk_status = BGM_PAUSE;
		return;
	}

	if (Q_strcasecmp(command, "resume") == 0) {
		if (trk_status == BGM_PAUSE)
			trk_status = BGM_PLAY;
		return;
	}

	if (Q_strcasecmp(command, "stop") == 0) {
		S_StopBackgroundTrack ();
		return;
	}

	if (Q_strcasecmp(command, "status") == 0) {
		S_WAV_StatusCmd ();
		return;
	}

	if (Q_strcasecmp(command, "list") == 0) {
		S_WAV_ListCmd ();
		return;
	}

	Com_Printf("Usage: wav {play | pause | resume | stop | status | list}\n");
}
