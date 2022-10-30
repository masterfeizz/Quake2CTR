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
#ifndef __GSPY_H
#define __GSPY_H

#include "../qcommon/qcommon.h"

/* FS: IF THIS FILE IS CHANGED IT MUST BE CHANGED IN CLIENT/GSPY.H ! */

#define	GAMESPY_API_VERSION		3

/* FS: Stuff we need to be able to link without goaceng.h */
typedef struct GServerListImplementation *GServerList;
typedef struct GServerImplementation *GServer;
typedef int GError;
typedef int gbool;
typedef enum {sl_idle, sl_listxfer, sl_lanlist, sl_querying} GServerListState;
typedef enum {cm_int, cm_float, cm_strcase, cm_stricase} GCompareMode;

#define	LIST_STATECHANGED		1
#define LIST_PROGRESS			2

#define GSPYCALLBACK_FUNCTION	1

#define GE_NOERROR		0

/*
 * these are the functions exported by the gamespy module
 */
typedef struct
{
	/* if api_version is different, the dll cannot be used */
	int		api_version;

	/* called when the library is loaded */
	void	(*Init) (void);

	/* called before the library is unloaded */
	void	(*Shutdown) (void);

	GServerList	(*ServerListNew)(char *gamename,char *enginename, char *seckey, int maxconcupdates, void *CallBackFn, int CallBackFnType, void *instance);
	void (*ServerListFree)(GServerList serverlist);
	GError (*ServerListUpdate)(GServerList serverlist, gbool async);
	GError (*ServerListThink)(GServerList serverlist);
	GError (*ServerListHalt)(GServerList serverlist);
	GError (*ServerListClear)(GServerList serverlist);
	GServer (*ServerListGetServer)(GServerList serverlist, int index);
	GServerListState (*ServerListState)(GServerList serverlist);
	char *(*ServerGetAddress)(GServer server);
	char *(*ServerListErrorDesc)(GServerList serverlist, GError error);
	void (*ServerListSort)(GServerList serverlist, gbool ascending, char *sortkey, GCompareMode comparemode);
	int (*ServerGetPing)(GServer server);
	char *(*ServerGetStringValue)(GServer server, char *key, char *sdefault);
	int (*ServerGetIntValue)(GServer server, char *key, int idefault);
	int (*ServerGetQueryPort)(GServer server);

} gspyexport_t;

/*
 * these are the functions imported by the gamespy module
 */
typedef struct
{
	void	(*print)(char *str, ...) __fp_attribute__((__format__(__printf__,1,2)));
	void	(*dprint)(unsigned int developerFlags, char *fmt, ...) __fp_attribute__((__format__(__printf__,2,3)));
	void	(*error)(char *error, ...) __fp_attribute__((__noreturn__, __format__(__printf__,1,2)));
	cvar_t	*(*cvar)(char *name, char *value, int flags);
	cvar_t	*(*cvar_set)(char *name, char *value);
	cvar_t	*(*cvar_forceset)(char *name, char *value);
	char	*(*net_strerror)(void);
	void	(*run_keyevents)(void);
	void	(*play_sound)(char *s);
	void	(*update_numservers)(int numServers);
} gspyimport_t;

/* FS: Binary stuff the DLL needs to access */
extern void S_GamespySound (char *sound);
extern char *NET_ErrorString(void);
extern void *Sys_GetGameSpyAPI(void *parms);
extern void Sys_UnloadGameSpy(void);

#endif /* __GSPY_H */
