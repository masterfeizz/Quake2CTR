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
// cvar.c -- dynamic variable tracking

#include "qcommon.h"

cvar_t	*cvar_vars;
cvar_t	*con_show_description; /* FS */
cvar_t	*con_show_dev_flags; /* FS */
void Cvar_ParseDeveloperFlags (void); /* FS: Special stuff for showing all the dev flags */
void Cvar_Force_f (void); /* FS: Force a NOSET CVAR (within reason) */
void Cvar_Toggle_f (void); /* FS: Toggle a CVAR */
void Cvar_Reset_f (void); /* FS: Reset a CVAR to it's default value */
qboolean Cvar_Never_Reset_Cmds(char *var_name); /* FS: Tired of copying CVARs everywhere */

/*
============
Cvar_InfoValidate
============
*/
static qboolean Cvar_InfoValidate (char *s)
{
	if (strchr(s, '\\'))
		return false;
	if (strchr(s, '\"'))
		return false;
	if (strchr(s, ';'))
		return false;
	return true;
}

/*
============
Cvar_FindVar
============
*/
static cvar_t *Cvar_FindVar (char *var_name)
{
	cvar_t	*var;

	for (var=cvar_vars ; var ; var=var->next)
		if (!strcmp (var_name, var->name))
			return var;

	return NULL;
}

/*
============
Cvar_VariableValue
============
*/
float Cvar_VariableValue (char *var_name)
{
	cvar_t	*var;

	var = Cvar_FindVar (var_name);
	if (!var)
		return 0;
	return atof (var->string);
}

/*
============
Cvar_VariableValue
============
*/
int Cvar_VariableValueInt (char *var_name) /* FS */
{
	cvar_t	*var;

	var = Cvar_FindVar (var_name);
	if (!var)
		return 0;
	return var->intValue;
}

/*
============
Cvar_VariableString
============
*/
char *Cvar_VariableString (char *var_name)
{
	cvar_t *var;

	var = Cvar_FindVar (var_name);
	if (!var)
		return "";
	return var->string;
}


/*
============
Cvar_CompleteVariable
============
*/
char *Cvar_CompleteVariable (char *partial)
{
	cvar_t		*cvar;
	int			len;

	len = strlen(partial);

	if (!len)
		return NULL;

	// check exact match
	for (cvar=cvar_vars ; cvar ; cvar=cvar->next)
		if (!strcmp (partial,cvar->name))
			return cvar->name;

	// check partial match
	for (cvar=cvar_vars ; cvar ; cvar=cvar->next)
		if (!strncmp (partial,cvar->name, len))
			return cvar->name;

	return NULL;
}


/*
============
Cvar_Get

If the variable already exists, the value will not be set
The flags will be or'ed in if the variable exists.
============
*/
cvar_t *Cvar_Get (char *var_name, char *var_value, int flags)
{
	cvar_t	*var;

	if (flags & (CVAR_USERINFO | CVAR_SERVERINFO))
	{
		if (!Cvar_InfoValidate (var_name))
		{
			Com_Printf("invalid info cvar name\n");
			return NULL;
		}
	}

	var = Cvar_FindVar (var_name);
	if (var)
	{
		var->flags |= flags;
		// Knightmare- change default value if this is called again
		Z_Free(var->defaultValue);
		var->defaultValue = CopyString(var_value);
		var->defaultFlags |= flags; /* FS: Ditto */

		return var;
	}

	if (!var_value)
		return NULL;

	if (flags & (CVAR_USERINFO | CVAR_SERVERINFO))
	{
		if (!Cvar_InfoValidate (var_value))
		{
			Com_Printf("invalid info cvar value\n");
			return NULL;
		}
	}

	var = Z_Malloc (sizeof(*var));
	var->name = CopyString (var_name);
	var->string = CopyString (var_value);
	var->modified = true;
	var->value = atof (var->string);
	var->intValue = atoi(var->string); /* FS: So we don't need to cast shit all the time */
	var->defaultValue = CopyString(var_value); /* FS: Find out what it was initially */
	var->defaultFlags = flags; /* FS: Default flags for resetcvar */
	var->description = NULL; /* FS: Init it first, d'oh */

	// link the variable in
	var->next = cvar_vars;
	cvar_vars = var;

	var->flags = flags;

	return var;
}

/*
============
Cvar_Set2
============
*/
cvar_t *Cvar_Set2 (char *var_name, char *value, qboolean force)
{
	cvar_t	*var;

	var = Cvar_FindVar (var_name);
	if (!var)
	{	// create it
		return Cvar_Get (var_name, value, 0);
	}

	if (var->flags & (CVAR_USERINFO | CVAR_SERVERINFO))
	{
		if (!Cvar_InfoValidate (value))
		{
			Com_Printf("invalid info cvar value\n");
			return var;
		}
	}

	if (!force)
	{
		if (var->flags & CVAR_NOSET)
		{
			Com_Printf ("%s is write protected.\n", var_name);
			return var;
		}

		if (var->flags & CVAR_LATCH)
		{
			if (var->latched_string)
			{
				if (strcmp(value, var->latched_string) == 0)
					return var;
				Z_Free (var->latched_string);
			}
			else
			{
				if (strcmp(value, var->string) == 0)
					return var;
			}

			if (Com_ServerState())
			{
				Com_Printf ("%s will be changed for next game.\n", var_name);
				var->latched_string = CopyString(value);
			}
			else
			{
				var->string = CopyString(value);
				var->value = atof (var->string);
				var->intValue = atoi(var->string); /* FS: So we don't need to cast shit all the time */

				if (!strcmp(var->name, "game"))
				{
					char cfgExecString[MAX_QPATH];

					FS_SetGamedir (var->string);
					FS_ExecAutoexec ();

					Com_sprintf(cfgExecString, sizeof(cfgExecString), "exec %s\n", cfg_default->string);
					Cbuf_AddText (cfgExecString); /* FS: If we switch game modes dynamically load that dirs config */
				}
			}
			return var;
		}
	}
	else
	{
		if (var->latched_string)
		{
			Z_Free (var->latched_string);
			var->latched_string = NULL;
		}
	}

	if (!strcmp(value, var->string))
		return var;		// not changed

	var->modified = true;

	if (var->flags & CVAR_USERINFO)
		userinfo_modified = true;	// transmit at next oportunity

	Z_Free (var->string);	// free the old value string

	var->string = CopyString(value);
	var->value = atof (var->string);
	var->intValue = atoi(var->string); /* FS: So we don't need to cast shit all the time */

	return var;
}

/*
============
Cvar_ForceSet
============
*/
cvar_t *Cvar_ForceSet (char *var_name, char *value)
{
	return Cvar_Set2 (var_name, value, true);
}

/*
============
Cvar_Set
============
*/
cvar_t *Cvar_Set (char *var_name, char *value)
{
	return Cvar_Set2 (var_name, value, false);
}

/*
============
Cvar_FullSet
============
*/
cvar_t *Cvar_FullSet (char *var_name, char *value, int flags)
{
	cvar_t	*var;

	var = Cvar_FindVar (var_name);
	if (!var)
	{	// create it
		return Cvar_Get (var_name, value, flags);
	}

	var->modified = true;

	if (var->flags & CVAR_USERINFO)
		userinfo_modified = true;	// transmit at next oportunity

	Z_Free (var->string);	// free the old value string

	var->string = CopyString(value);
	var->value = atof (var->string);
	var->intValue = atoi(var->string); /* FS: So we don't need to cast shit all the time */
	var->flags = flags;

	return var;
}

/*
============
Cvar_SetValue
============
*/
void Cvar_SetValue (char *var_name, float value)
{
	char	val[32];

	if (value == (int)value)
		Com_sprintf (val, sizeof(val), "%i",(int)value);
	else
		Com_sprintf (val, sizeof(val), "%f",value);
	Cvar_Set (var_name, val);
}


/*
============
Cvar_GetLatchedVars

Any variables with latched values will now be updated
============
*/
void Cvar_GetLatchedVars (void)
{
	cvar_t	*var;

	for (var = cvar_vars ; var ; var = var->next)
	{
		if (!var->latched_string)
			continue;
		Z_Free (var->string);
		var->string = var->latched_string;
		var->latched_string = NULL;
		var->value = atof(var->string);
		var->intValue = atoi(var->string); /* FS: So we don't need to cast shit all the time */
		if (!strcmp(var->name, "game"))
		{
			FS_SetGamedir (var->string);
			FS_ExecAutoexec ();
		}
	}
}


/*
============
Cvar_Command

Handles variable inspection and changing from the console
============
*/
qboolean Cvar_Command (void)
{
	cvar_t			*v;

// check variables
	v = Cvar_FindVar (Cmd_Argv(0));
	if (!v)
		return false;

	if (!strcmp(v->name, "developer") && con_show_dev_flags->intValue) /* FS: Special case for showing enabled flags */
	{
		if (Cmd_Argv(1)[0] != '\0')
			Cvar_Set(developer->name, Cmd_Argv(1));
		Cvar_ParseDeveloperFlags();
		return true;
	}

// perform a variable print or set
	if (Cmd_Argc() == 1)
	{
		if ( (v->flags & CVAR_LATCH) && v->latched_string)
			Com_Printf ("\"%s\" is \"%s\", Default: \"%s\", Latched to: \"%s\"\n", v->name, v->string, v->defaultValue, v->latched_string);
		else
			Com_Printf ("\"%s\" is \"%s\", Default: \"%s\"\n", v->name, v->string, v->defaultValue);

		/* FS: cvar descriptions */
		/* FS: Always show it for con_show_description so we know what it does */
		if (v->description) {
		    if (con_show_description->intValue || v == con_show_description)
			Com_Printf("Description: %s\n", v->description);
		}

		return true;
	}

	Cvar_Set (v->name, Cmd_Argv(1));
	return true;
}


/*
============
Cvar_Set_f

Allows setting and defining of arbitrary cvars from console
============
*/
void Cvar_Set_f (void)
{
	int		c;
	int		flags;

	c = Cmd_Argc();
	if (c != 3 && c != 4)
	{
		Com_Printf ("usage: set <variable> <value> [u / s]\n");
		return;
	}

	if (c == 4)
	{
		if (!strcmp(Cmd_Argv(3), "u"))
			flags = CVAR_USERINFO;
		else if (!strcmp(Cmd_Argv(3), "s"))
			flags = CVAR_SERVERINFO;
		else
		{
			Com_Printf ("flags can only be 'u' or 's'\n");
			return;
		}
		Cvar_FullSet (Cmd_Argv(1), Cmd_Argv(2), flags);
	}
	else
		Cvar_Set (Cmd_Argv(1), Cmd_Argv(2));
}


/*
============
Cvar_WriteVariables

Appends lines containing "set variable value" for all variables
with the archive flag set to true.
============
*/
void Cvar_WriteVariables (char *path)
{
	cvar_t	*var;
	char	buffer[1024];
	FILE	*f;

	if (!path)
	{
		Com_Error(ERR_DROP, "Cvar_WriteVariables(): Null or empty path.  Aborting.\n");
		return;
	}

	f = fopen (path, "a");
	if (!f)
	{
		Com_Error(ERR_DROP, "Cvar_WriteVariables(): Failed to open %s for writing.\n", path);
		return;
	}

	for (var = cvar_vars ; var ; var = var->next)
	{
		if (var->flags & CVAR_ARCHIVE)
		{
			Com_sprintf (buffer, sizeof(buffer), "set %s \"%s\"\n", var->name, var->string);
			fprintf (f, "%s", buffer);
		}
	}
	fclose (f);
}

static int cmpr_cvars (const void *a, const void *b)
{
	cvar_t *aa = *(cvar_t **)a;
	cvar_t *bb = *(cvar_t **)b;

	return strcmp(aa->name, bb->name);
}

static int GetCVARCount (void)
{
	int i = 0;
	cvar_t* cvar;

	for (cvar = cvar_vars; cvar; cvar = cvar->next)
	{
		i++;
	}

	return i;
}

/*
============
Cvar_List_f

============
*/
void Cvar_List_f (void)
{
	cvar_t **sorted_cvars = NULL; /* FS: Sort by name. */
	cvar_t	*head = &cvar_vars[0];
	cvar_t	*var;
	const char *search_filter = NULL;
	int		i = 0, j = 0, q = 0, args = 0, search_filter_len = 0, cvar_count = 0;

	args = Cmd_Argc();

	if (args > 1) /* FS */
	{
		search_filter = Cmd_Argv(1);
		if (search_filter != NULL)
		{
			Com_Printf("Listing matches for '%s'...\n", search_filter);

			if (args > 2)
			{
				search_filter_len = strlen(search_filter);
			}
		}
	}

	cvar_count = GetCVARCount();

	sorted_cvars = (cvar_t **)malloc(sizeof(cvar_t*)*cvar_count);
	if (!sorted_cvars)
	{
		Com_Error(ERR_FATAL, "Cvar_List_f: Failed to allocate memory.");
		return;
	}

	for (q = 0; q < cvar_count; q++)
	{
		sorted_cvars[q] = cvar_vars;
		cvar_vars = cvar_vars->next;
	}

	qsort(sorted_cvars, cvar_count, sizeof(cvar_t*), &cmpr_cvars);

	for (i = 0; i < cvar_count; i++)
	{
		var = sorted_cvars[i];
		if (!var)
		{
			break;
		}

		if (search_filter) /* FS */
		{
			if (!strstr(var->name, search_filter))
				continue;

			if ((args > 2) && (strncmp(var->name, search_filter, search_filter_len)))
				continue;

			j++;
		}

		if (var->flags & CVAR_ARCHIVE)
			Com_Printf("*");
		else
			Com_Printf(" ");
		if (var->flags & CVAR_USERINFO)
			Com_Printf("U");
		else
			Com_Printf(" ");
		if (var->flags & CVAR_SERVERINFO)
			Com_Printf("S");
		else
			Com_Printf(" ");
		if (var->flags & CVAR_NOSET)
			Com_Printf("-");
		else if (var->flags & CVAR_LATCH)
			Com_Printf("L");
		else
			Com_Printf(" ");
		if (var->description)
			Com_Printf("D");
		else
			Com_Printf(" ");

		if ( (var->flags & CVAR_LATCH) && var->latched_string)
			Com_Printf("\"%s\" is \"%s\", Default: \"%s\", Latched to: \"%s\"\n", var->name, var->string, var->defaultValue, var->latched_string);
		else
			Com_Printf(" %s \"%s\", Default: \"%s\"\n", var->name, var->string, var->defaultValue);
	}

	Com_Printf("Legend: * Archive. U Userinfo. S Serverinfo. - Write Protected. L Latched. D Containts a Help Description.\n"); /* FS: Added a legend */
	Com_Printf("%d cvars\n", search_filter ? j : i);

	free(sorted_cvars);
	sorted_cvars = NULL;

	cvar_vars = (cvar_t *)head;
}


qboolean userinfo_modified;


char	*Cvar_BitInfo (int bit)
{
	static char	info[MAX_INFO_STRING];
	cvar_t	*var;

	info[0] = 0;

	for (var = cvar_vars ; var ; var = var->next)
	{
		if (var->flags & bit)
			Info_SetValueForKey (info, var->name, var->string);
	}
	return info;
}

// returns an info string containing all the CVAR_USERINFO cvars
char	*Cvar_Userinfo (void)
{
	return Cvar_BitInfo (CVAR_USERINFO);
}

// returns an info string containing all the CVAR_SERVERINFO cvars
char	*Cvar_Serverinfo (void)
{
	return Cvar_BitInfo (CVAR_SERVERINFO);
}

/*
============
Cvar_Init

Reads in all archived cvars
============
*/
void Cvar_Init (void)
{
	con_show_description = Cvar_Get("con_show_description", "1", CVAR_ARCHIVE); /* FS */
	Cvar_SetDescription("con_show_description", "Toggle descriptions for CVARs.");
	con_show_dev_flags = Cvar_Get ("con_show_dev_flags", "1", CVAR_ARCHIVE); /* FS */
	Cvar_SetDescription("con_show_dev_flags", "Show toggled developer flags when using the developer CVAR.");

	Cmd_AddCommand ("set", Cvar_Set_f);
	Cmd_AddCommand ("cvarlist", Cvar_List_f);
	Cmd_AddCommand ("togglecvar", Cvar_Toggle_f); /* FS */
	Cmd_AddCommand ("forcecvar", Cvar_Force_f); /* FS */
	Cmd_AddCommand ("resetcvar", Cvar_Reset_f); /* FS */
}

void Cvar_Shutdown (void)
{
	cvar_t *var, *next;

	for (var = cvar_vars; var; var = next)
	{
		next = var->next;

		if (var->description)
		{
			free(var->description);
			var->description = NULL;
		}

		if (var->defaultValue)
		{
			Z_Free(var->defaultValue);
			var->defaultValue = NULL;
		}

		if (var->string)
		{
			Z_Free(var->string);
			var->string = NULL;
		}

		if (var->name)
		{
			Z_Free(var->name);
			var->name = NULL;
		}

		Z_Free(var);
		var = NULL;
	}
}

static cvar_t *Cvar_IsNoset (const char *var_name) /* FS: Make sure this isn't a NOSET CVAR! */
{
	cvar_t	*var;
	
	for (var=cvar_vars ; var ; var=var->next)
	{
		if( var->name )
		{
			if (!strcmp (var_name, var->name))
			{
				if (var->flags & CVAR_NOSET)
				{
					return var;
				}
			}
		}
	}
	return NULL;
}

void Cvar_Toggle_f (void) /* FS: Added */
{
	if(Cmd_Argc() == 2 && Cmd_Argv(1))
	{
		char *cvar_name = Cmd_Argv(1);
		float cur_value;

		if (cvar_name == NULL || cvar_name[0] == '\0' || Cvar_FindVar(cvar_name) == NULL) /* FS: Check for NULL sillies */
		{
			Com_Printf("%s not found!\n", Cmd_Argv(1));
			return;
		}

		if (Cvar_IsNoset(cvar_name))
		{
			Com_Printf("%s is write protected.\n", cvar_name);
			return;
		}

	    //get the value of the cvar.
		cur_value = Cvar_VariableValue(cvar_name);

		//set it to the opposite value.
		if (cur_value != 0.0f)
			Cvar_ForceSet(cvar_name, "0");
		else
			Cvar_ForceSet(cvar_name, "1");

		Com_Printf("%s set to %0.0f\n", cvar_name, Cvar_VariableValue(cvar_name));
	}
	else
	{
		Com_Printf("USAGE: togglecvar <console variable>\n");
		return;
	}
}

void Cvar_Force_f (void) /* FS: Added */
{
	if(Cmd_Argc() == 3 && Cmd_Argv(1))
	{
		char *cvar_name = Cmd_Argv(1);
		char *cvar_value;

		if (cvar_name == NULL || cvar_name[0] == '\0' || Cvar_FindVar(cvar_name) == NULL) /* FS: Check for NULL sillies */
		{
			Com_Printf("%s not found!\n", Cmd_Argv(1));
			return;
		}

		// Knightmare 2/24/13- prevent dedicated from being changed!
		/* FS: Consolidated all of these into a special function because resetcvar bans them too */
		if (Cvar_Never_Reset_Cmds(cvar_name))
		{
			Com_Printf("Error: %s cannot be changed from console!\n", cvar_name);
			return;
		}

		cvar_value = Cmd_Argv(2);

		Cvar_ForceSet(cvar_name, cvar_value);

		Com_Printf("%s set to %s\n", cvar_name, cvar_value);
	}
	else
	{
		Com_Printf("USAGE: forcecvar <console variable> <value>\n");
		return;
	}
}

qboolean Cvar_Never_Reset_Cmds(char *var_name) /* FS: Tired of copying CVARs everywhere */
{
	if(!strcmp(var_name, "dedicated"))
		return true;
	if(!strcmp(var_name, "port"))
		return true;
	if(!strcmp(var_name, "qport"))
		return true;
	if(!strcmp(var_name, "version"))
		return true;
	if(!strcmp(var_name, "cfg_default"))
		return true;

	return false;
}

void Cvar_Reset_f (void) /* FS: Reset a CVAR to its default value */
{
	int args;
	cvar_t *var;
	char *var_name;

	args = Cmd_Argc();

	if (args != 2)
	{
		Com_Printf("usage: resetcvar <variable>.  Resets CVARs to their default values\n");
		return;
	}

	var_name = Cmd_Argv(1);

	if (Cvar_Never_Reset_Cmds(var_name))
	{
		Com_Printf("Error: you can not reset this value: %s!\n", var_name);
		return;
	}

	var = Cvar_FindVar(var_name);

	if (!var)
	{
		Com_Printf("Error: %s is not a valid CVAR!\n", var_name);
		return;
	}

	Com_Printf("Resetting %s to default value of %s, Flags Value: %i\n", var_name, var->defaultValue, var->defaultFlags);
	var->flags = var->defaultFlags;
	Cvar_ForceSet(var_name, var->defaultValue);
}

void Cvar_ParseDeveloperFlags (void) /* FS: Special stuff for showing all the dev flags */
{
	extern cvar_t	*developer;

	Com_Printf("\"%s\" is \"%s\", Default: \"%s\"\n", developer->name, developer->string, developer->defaultValue);

	if (developer->intValue)
	{
		unsigned int devFlags = 0;
		if(developer->intValue == 1)
			devFlags = 65534;
		else
			devFlags = (unsigned int)developer->intValue;
		Com_Printf("Toggled flags:\n");
		if(devFlags & DEVELOPER_MSG_STANDARD)
			Com_Printf(" * Standard messages - 2\n");
		if(devFlags & DEVELOPER_MSG_SOUND)
			Com_Printf(" * Sound messages - 4\n");
		if(devFlags & DEVELOPER_MSG_NET)
			Com_Printf(" * Network messages - 8\n");
		if(devFlags & DEVELOPER_MSG_IO)
			Com_Printf(" * File IO messages - 16\n");
		if(devFlags & DEVELOPER_MSG_GFX)
			Com_Printf(" * Graphics Renderer messages - 32\n");
		if(devFlags & DEVELOPER_MSG_GAME)
			Com_Printf(" * Game DLL messages - 64\n");
		if(devFlags & DEVELOPER_MSG_MEM)
			Com_Printf(" * Memory messages - 128\n");
		if(devFlags & DEVELOPER_MSG_SERVER)
			Com_Printf(" * Server messages - 256\n");
		if(devFlags & DEVELOPER_MSG_CD)
			Com_Printf(" * CD Audio messages - 512\n");
		if(devFlags & DEVELOPER_MSG_OGG)
			Com_Printf(" * OGG Vorbis messages - 1024\n");
		if(devFlags & DEVELOPER_MSG_PHYSICS)
			Com_Printf(" * Physics messages - 2048\n");
		if(devFlags & DEVELOPER_MSG_ENTITY)
			Com_Printf(" * Entity messages - 4096\n");
		if(devFlags & DEVELOPER_MSG_SAVE)
			Com_Printf(" * Save/Restore messages - 8192\n");
		if(devFlags & DEVELOPER_MSG_UNUSED1)
			Com_Printf(" * Currently unused - 16384\n");
		if(devFlags & DEVELOPER_MSG_UNUSED2)
			Com_Printf(" * Currently unused - 32768\n");
		if(devFlags & DEVELOPER_MSG_VERBOSE)
			Com_Printf(" * Extremely Verbose messages - 65536\n");
		if(devFlags & DEVELOPER_MSG_GAMESPY)
			Com_Printf(" * Extremely Verbose Gamespy messages - 131072\n");
	}
	else
	{
		if (developer->description && con_show_description->intValue) /* FS: Show all available flags */
			Com_Printf("Description: %s\n", developer->description);
	}
}

void Cvar_SetDescription (char *var_name, const char *description) /* FS: Set descriptions for CVARs */
{
	cvar_t	*var = Cvar_FindVar (var_name);
	if (!var) {
		Com_DPrintf(DEVELOPER_MSG_STANDARD, "Error: Can't set description for %s!\n", var_name);
		return;
	}

	if (!description) {
		Com_DPrintf(DEVELOPER_MSG_STANDARD, "NULL description for %s\n", var_name);
		return;
	}

	if (var->description)
		free(var->description);

	var->description = strdup(description);
}
