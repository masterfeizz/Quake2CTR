/*
Copyright (C) 2017 Felipe Izzo
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

#include <sys/types.h>
#include <sys/stat.h>
#include <errno.h>
#include <stdio.h>
#include <dirent.h>
#include <unistd.h>

#include "../qcommon/qcommon.h"
#include "../client/keys.h"
#include "errno.h"

#include <3ds.h>

#define TICKS_PER_MSEC 268111.856

int __stacksize__ = 3 * 1024 * 1024;

int		curtime;
unsigned	sys_frame_time;

static byte	*membase;
static int	hunkmaxsize;
static int	cursize;

void *GetGameAPI (void *import);

void Sys_Error (char *error, ...)
{
	consoleInit(GFX_BOTTOM, NULL);

	va_list		argptr;

	printf ("Sys_Error: ");	
	va_start (argptr,error);
	vprintf (error,argptr);
	va_end (argptr);
	printf ("\n");

	while(1)
	{
		hidScanInput();
		u32 kDown = hidKeysDown();
		if (kDown & KEY_START)
				break;
	}

	gfxExit();
	exit (1);
}

void Sys_Quit (void)
{
	exit (0);
}

void Sys_UnloadGame (void)
{

} 

void *Sys_GetGameAPI (void *parms)
{
	return GetGameAPI (parms);
}


char *Sys_ConsoleInput (void)
{
	return NULL;
}

void Sys_ConsoleOutput (char *string)
{
	printf("%s",string);
}

void Sys_DefaultConfig(void)
{
	Cbuf_AddText ("bind PADUP \"invuse\"\n");
	Cbuf_AddText ("bind PADDOWN \"invdrop\"\n");
	Cbuf_AddText ("bind PADLEFT \"invprev\"\n");
	Cbuf_AddText ("bind PADRIGHT \"invnext\"\n");

	Cbuf_AddText ("bind ABUTTON \"+right\"\n");
	Cbuf_AddText ("bind BBUTTON \"+lookdown\"\n");
	Cbuf_AddText ("bind XBUTTON \"+lookup\"\n");
	Cbuf_AddText ("bind YBUTTON \"+left\"\n");
	Cbuf_AddText ("bind LTRIGGER \"+moveup\"\n");
	Cbuf_AddText ("bind RTRIGGER \"+attack\"\n");

	Cbuf_AddText ("bind T1 \"weapnext\"\n");
	Cbuf_AddText ("bind T2 \"inven\"\n");
	Cbuf_AddText ("bind T5 \"+movedown\"\n");

	Cbuf_AddText ("lookstrafe \"1.000000\"\n");
	Cbuf_AddText ("lookspring \"0.000000\"\n");
	Cbuf_AddText ("gamma \"0.700000\"\n");

	Cbuf_Execute();
}

void Sys_SetKeys(u32 keys, u32 state, u32 grab_time)
{

	if( keys & KEY_SELECT)
		Key_Event(K_ESCAPE, state, grab_time);
	if( keys & KEY_START)
		Key_Event(K_ENTER, state, grab_time);
	if( keys & KEY_DUP)
		Key_Event(K_UPARROW, state, grab_time);
	if( keys & KEY_DDOWN)
		Key_Event(K_DOWNARROW, state, grab_time);
	if( keys & KEY_DLEFT)
		Key_Event(K_LEFTARROW, state, grab_time);
	if( keys & KEY_DRIGHT)
		Key_Event(K_RIGHTARROW, state, grab_time);
	if( keys & KEY_Y)
		Key_Event(K_AUX4, state, grab_time);
	if( keys & KEY_X)
		Key_Event(K_AUX3, state, grab_time);
	if( keys & KEY_B)
		Key_Event(K_AUX2, state, grab_time);
	if( keys & KEY_A)
		Key_Event(K_AUX1, state, grab_time);
	if( keys & KEY_L)
		Key_Event(K_AUX5, state, grab_time);
	if( keys & KEY_R)
		Key_Event(K_AUX7, state, grab_time);
	if( keys & KEY_ZL)
		Key_Event(K_AUX6, state, grab_time);
	if( keys & KEY_ZR)
		Key_Event(K_AUX8, state, grab_time);
}

void Sys_SendKeyEvents (void)
{
	hidScanInput();

	u32 grab_time = Sys_Milliseconds();

	u32 kDown = hidKeysDown();
	u32 kUp   = hidKeysUp();

	if(kDown)
		Sys_SetKeys(kDown, true, grab_time);
	if(kUp)
		Sys_SetKeys(kUp,  false, grab_time);

	sys_frame_time = Sys_Milliseconds();
}


void Sys_AppActivate (void)
{
}

void Sys_CopyProtect (void)
{
}

char *Sys_GetClipboardData( void )
{
	return NULL;
}

void *Hunk_Begin (int maxsize)
{
	// reserve a huge chunk of memory, but don't commit any yet
	hunkmaxsize = maxsize;
	cursize = 0;
	membase = malloc(hunkmaxsize);

	if (membase == NULL)
		Sys_Error("unable to allocate %d bytes", hunkmaxsize);
	else
		memset (membase, 0, hunkmaxsize);

	return membase;
}

void *Hunk_Alloc (int size)
{
	byte *buf;

	// round to cacheline
	size = (size+31)&~31;

	if (cursize + size > hunkmaxsize)
		Sys_Error("Hunk_Alloc overflow");

	buf = membase + cursize;
	cursize += size;

	return buf;
}

int Hunk_End (void)
{
	byte *n;

	n = realloc(membase, cursize);

	if (n != membase)
		Sys_Error("Hunk_End:  Could not remap virtual block (%d)", errno);

	return cursize;
}

void Hunk_Free (void *base)
{
	if (base)
		free(base);
}

int Sys_Milliseconds (void)
{
	static u64	base;

	u64 time = osGetTime();
	
	if (!base)
	{
		base = time;
	}

	curtime = (int)(time - base);
	
	return curtime;
}

void Sys_Mkdir (char *path)
{
	mkdir (path, 0777);
}

void Sys_MkdirRecursive(char *path)
{
        char tmp[256];
        char *p = NULL;
        size_t len;
 
        snprintf(tmp, sizeof(tmp),"%s",path);
        len = strlen(tmp);
        if(tmp[len - 1] == '/')
                tmp[len - 1] = 0;
        for(p = tmp + 1; *p; p++)
                if(*p == '/') {
                        *p = 0;
                        Sys_Mkdir(tmp);
                        *p = '/';
                }
        Sys_Mkdir(tmp);
}

static	char	findbase[MAX_OSPATH];
static	char	findpath[MAX_OSPATH];
static	char	findpattern[MAX_OSPATH];
static	DIR		*fdir;

int glob_match(char *pattern, char *text);

static qboolean CompareAttributes(char *path, char *name,
	unsigned musthave, unsigned canthave )
{
	struct stat st;
	char fn[MAX_OSPATH];

// . and .. never match
	if (strcmp(name, ".") == 0 || strcmp(name, "..") == 0)
		return false;

	sprintf(fn, "%s/%s", path, name);
	if (stat(fn, &st) == -1)
		return false; // shouldn't happen

	if ( ( st.st_mode & S_IFDIR ) && ( canthave & SFF_SUBDIR ) )
		return false;

	if ( ( musthave & SFF_SUBDIR ) && !( st.st_mode & S_IFDIR ) )
		return false;

	return true;
}

char *Sys_FindFirst (char *path, unsigned musthave, unsigned canhave)
{
	struct dirent *d;
	char *p;

	if (fdir)
		Sys_Error ("Sys_BeginFind without close");

//	COM_FilePath (path, findbase);
	strcpy(findbase, path);

	if ((p = strrchr(findbase, '/')) != NULL) {
		*p = 0;
		strcpy(findpattern, p + 1);
	} else
		strcpy(findpattern, "*");

	if (strcmp(findpattern, "*.*") == 0)
		strcpy(findpattern, "*");
	
	if ((fdir = opendir(findbase)) == NULL)
		return NULL;
	while ((d = readdir(fdir)) != NULL) {
		if (!*findpattern || glob_match(findpattern, d->d_name)) {
//			if (*findpattern)
//				printf("%s matched %s\n", findpattern, d->d_name);
			if (CompareAttributes(findbase, d->d_name, musthave, canhave)) {
				sprintf (findpath, "%s/%s", findbase, d->d_name);
				return findpath;
			}
		}
	}
	return NULL;
}

char *Sys_FindNext (unsigned musthave, unsigned canhave)
{
	struct dirent *d;

	if (fdir == NULL)
		return NULL;
	while ((d = readdir(fdir)) != NULL) {
		if (!*findpattern || glob_match(findpattern, d->d_name)) {
//			if (*findpattern)
//				printf("%s matched %s\n", findpattern, d->d_name);
			if (CompareAttributes(findbase, d->d_name, musthave, canhave)) {
				sprintf (findpath, "%s/%s", findbase, d->d_name);
				return findpath;
			}
		}
	}
	return NULL;
}

void Sys_FindClose (void)
{
	if (fdir != NULL)
		closedir(fdir);
	fdir = NULL;
}

void	Sys_Init (void)
{
	Touch_Init();
	Touch_DrawOverlay();
}

void Sys_Sleep (unsigned msec)
{
}

//=============================================================================

int main (int argc, char **argv)
{
	int	time, oldtime, newtime;

	romfsInit();

	osSetSpeedupEnable(true);

	gfxInit(GSP_BGR8_OES,GSP_RGB565_OES,false);
	gfxSetDoubleBuffering(GFX_BOTTOM, false);
	gfxSwapBuffersGpu();
	
	uint8_t model;

	cfguInit();
	CFGU_GetSystemModel(&model);
	cfguExit();
	
	if(model != CFG_MODEL_2DS)
		gfxSetWide(true);

	pglInitEx(0x040000, 0x100000);

	Sys_MkdirRecursive("sdmc:/3ds/quake2/baseq2");
	chdir("sdmc:/3ds/quake2/");

	Qcommon_Init (argc, argv);

	oldtime = Sys_Milliseconds ();

	while (aptMainLoop())
	{
		do {
			newtime = Sys_Milliseconds ();
			time = newtime - oldtime;
		} while (time < 1);
		Qcommon_Frame (time);
		oldtime = newtime;
	}

	gfxExit();
	return 0;
}


