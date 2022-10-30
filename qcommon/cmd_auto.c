/*
Copyright 2021 Frank Sapone

Redistribution and use in source and binary forms, with or without modification, are permitted provided that the following conditions are met:

1. Redistributions of source code must retain the above copyright notice, this list of conditions and the following disclaimer.

2. Redistributions in binary form must reproduce the above copyright notice, this list of conditions and the following disclaimer in the documentation and/or other materials provided with the distribution.

3. Neither the name of the copyright holder nor the names of its contributors may be used to endorse or promote products derived from this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
// cmd_autocomplete.c -- Quake engine console autocomplete by [HCI]Mara'akate

#include "qcommon.h"
#include "cmd.h"
#include <ctype.h> // tolower()a

#define RETRY_INITIAL	0
#define RETRY_ONCE		1
#define RETRY_MULTIPLE	2

typedef struct autocomplete_cvar_s
{
	char		*name;
	struct autocomplete_cvar_s *next;
} autocomplete_cvar_t;

static char *prevPartial = NULL;
static autocomplete_cvar_t *auto_cvars = NULL;
static cmd_function_t **sorted_cmds = NULL;
static cmdalias_t **sorted_alias = NULL;
static cvar_t **sorted_cvars = NULL;
static int cmdcount = 0;
static int aliascount = 0;
static int cvarcount = 0;

void Cmd_RemoveAutoComplete (void);
qboolean Sort_Possible_Strtolower (const char *partial, const char *complete);

extern cmdalias_t	*cmd_alias;
extern cmd_function_t	*cmd_functions;

static int GetCmdCount (void)
{
	int i = 0;
	cmd_function_t* cmd;

	for (cmd = cmd_functions; cmd; cmd = cmd->next)
	{
		i++;
	}

	return i;
}

static int GetAliasCount (void)
{
	int i = 0;
	cmdalias_t* alias;

	for (alias = cmd_alias; alias; alias = alias->next)
	{
		i++;
	}

	return i;
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

static int cmpr_cmds (const void *a, const void *b)
{
	cmd_function_t *aa = *(cmd_function_t **)a;
	cmd_function_t *bb = *(cmd_function_t **)b;

	return strcmp(aa->name, bb->name);
}

static int cmpr_aliases (const void *a, const void *b)
{
	cmdalias_t *aa = *(cmdalias_t **)a;
	cmdalias_t *bb = *(cmdalias_t **)b;

	return strcmp(aa->name, bb->name);
}

static int cmpr_cvars (const void *a, const void *b)
{
	cvar_t *aa = *(cvar_t **)a;
	cvar_t *bb = *(cvar_t **)b;

	return strcmp(aa->name, bb->name);
}

static void QSort_Autocomplete (void)
{
	cmd_function_t *cmds = cmd_functions;
	cmdalias_t *aliases = cmd_alias;
	cvar_t *cvars = cvar_vars;
	int i;

	/* Commands. */
	cmdcount = GetCmdCount();
	sorted_cmds = (cmd_function_t **)calloc(1, sizeof(cmd_function_t*)*cmdcount);
	if (!sorted_cmds)
	{
		Com_Error(ERR_FATAL, "QSort_Autocomplete: Failed to allocate memory.");
		return;
	}

	for (i = 0; i < cmdcount; i++)
	{
		sorted_cmds[i] = cmds;
		cmds = cmds->next;
	}

	qsort(sorted_cmds, cmdcount, sizeof(cmd_function_t*), &cmpr_cmds);

	/* Aliases. */
	aliascount = GetAliasCount();
	sorted_alias = (cmdalias_t **)calloc(1, sizeof(cmdalias_t*)*aliascount);
	if (!sorted_alias)
	{
		Com_Error(ERR_FATAL, "QSort_Autocomplete: Failed to allocate memory.");
		return;
	}

	for (i = 0; i < aliascount; i++)
	{
		sorted_alias[i] = aliases;
		aliases = aliases->next;
	}

	qsort(sorted_alias, aliascount, sizeof(cvar_t*), &cmpr_aliases);

	/* CVARs. */
	cvarcount = GetCVARCount();
	sorted_cvars = (cvar_t **)calloc(1, sizeof(cvar_t*)*cvarcount);
	if (!sorted_cvars)
	{
		Com_Error(ERR_FATAL, "QSort_Autocomplete: Failed to allocate memory.");
		return;
	}

	for (i = 0; i < cvarcount; i++)
	{
		sorted_cvars[i] = cvars;
		cvars = cvars->next;
	}

	qsort(sorted_cvars, cvarcount, sizeof(cvar_t*), &cmpr_cvars);
}

char *Sort_Possible_Cmds (char *partial, qboolean backwards)
{
	cmd_function_t	*cmd;
	cvar_t			*cvar;
	int				len, i;
	cmdalias_t		*a;
	int	foundExactCount = 0;
	int foundPartialCount = 0;
	int retryPartialFlag = RETRY_INITIAL;

	len = (int)strlen(partial);

	if (!len)
		return NULL;

	if (partial[len - 1] == ' ') /* FS: Remove the space for auto-complete for comparision */
		partial[len - 1] = '\0';

	if (prevPartial)
	{
		autocomplete_cvar_t *vars = NULL;

		if (!auto_cvars || (strcmp(prevPartial, partial)))
		{
			Cmd_RemoveAutoComplete();
		}
		else
		{
			free(prevPartial);
			prevPartial = NULL;

			for (vars = auto_cvars; vars; vars = vars->next)
			{
				if (vars->next && vars->next->name)
				{
					autocomplete_cvar_t *ptrList;
					const char *ptrName;

					if (!backwards)
					{
						ptrList = vars;
						ptrName = vars->next->name;
					}
					else
					{
						/* FS: Return the "previous" one in the list since this is adding in descending sequence and I'm too lazy to redo this with head, current, and tail. */
						ptrList = vars->next;
						ptrName = vars->name;
					}

					if (!strcmp(partial, ptrName))
					{
						prevPartial = strdup(ptrList->name);
						if (!prevPartial)
						{
							Com_Error(ERR_FATAL, "Sort_Possible_Cmds: Failed to allocate memory.");
							return NULL;
						}

						return ptrList->name;
					}
				}
				else
				{
					return partial;
				}
			}
		}
	}

	Cmd_RemoveAutoComplete();

	QSort_Autocomplete();

	prevPartial = strdup(partial);
	if (!prevPartial)
	{
		Com_Error(ERR_FATAL, "Sort_Possible_Cmds: Failed to allocate memory.");
		return NULL;
	}

	// check for exact match
	foundExactCount = 0;

	for (i = 0; i < cmdcount; i++)
	{
		cmd = sorted_cmds[i];
		if (!cmd)
		{
			break;
		}

		if (!strcmp (partial, cmd->name))
		{
			autocomplete_cvar_t *var = (autocomplete_cvar_t *)calloc(1, sizeof(autocomplete_cvar_t));
			if (!var)
			{
				Com_Error(ERR_FATAL, "Sort_Possible_Cmds: Failed to allocate memory.");
				return NULL;
			}

			var->name = strdup(cmd->name);
			if (!var->name)
			{
				free(var);
				Com_Error(ERR_FATAL, "Sort_Possible_Cmds: Failed to allocate memory.");
				return NULL;
			}

			var->next = auto_cvars;
			auto_cvars = var;

			foundExactCount++;
		}
	}

	for (i = 0; i < aliascount; i++)
	{
		a = sorted_alias[i];
		if (!a)
		{
			break;
		}

		if (!strcmp (partial, a->name))
		{
			autocomplete_cvar_t *var = (autocomplete_cvar_t *)calloc(1, sizeof(autocomplete_cvar_t));
			if (!var)
			{
				Com_Error(ERR_FATAL, "Sort_Possible_Cmds: Failed to allocate memory.");
				return NULL;
			}

			var->name = strdup(a->name);
			if (!var->name)
			{
				free(var);
				Com_Error(ERR_FATAL, "Sort_Possible_Cmds: Failed to allocate memory.");
				return NULL;
			}

			var->next = auto_cvars;
			auto_cvars = var;

			foundExactCount++;
		}
	}

	for (i = 0; i < cvarcount; i++)
	{
		cvar = sorted_cvars[i];
		if (!cvar)
		{
			break;
		}

		if (!strcmp (partial, cvar->name))
		{
			autocomplete_cvar_t *var = (autocomplete_cvar_t *)calloc(1, sizeof(autocomplete_cvar_t));
			if (!var)
			{
				Com_Error(ERR_FATAL, "Sort_Possible_Cmds: Failed to allocate memory.");
				return NULL;
			}

			var->name = strdup(cvar->name);
			if (!var->name)
			{
				free(var);
				Com_Error(ERR_FATAL, "Sort_Possible_Cmds: Failed to allocate memory.");
				return NULL;
			}

			var->next = auto_cvars;
			auto_cvars = var;

			foundExactCount++;
		}
	}

	// check for partial match
retryPartial:
	foundPartialCount = 0;

	for (i = 0; i < cmdcount; i++)
	{
		cmd = sorted_cmds[i];
		if (!cmd)
		{
			break;
		}

		if (Sort_Possible_Strtolower(partial, cmd->name))
		{
			autocomplete_cvar_t *var = (autocomplete_cvar_t *)calloc(1, sizeof(autocomplete_cvar_t));
			if (!var)
			{
				Com_Error(ERR_FATAL, "Sort_Possible_Cmds: Failed to allocate memory.");
				return NULL;
			}

			foundPartialCount++;

			var->name = strdup(cmd->name);
			if (!var->name)
			{
				free(var);
				Com_Error(ERR_FATAL, "Sort_Possible_Cmds: Failed to allocate memory.");
				return NULL;
			}

			var->next = auto_cvars;
			auto_cvars = var;

			if (retryPartialFlag == RETRY_MULTIPLE)
				Com_Printf("  %s [C]\n", cmd->name);
			else if (retryPartialFlag == RETRY_ONCE)
				return cmd->name;
		}
	}

	for (i = 0; i < aliascount; i++)
	{
		a = sorted_alias[i];
		if (!a)
		{
			break;
		}

		if (Sort_Possible_Strtolower(partial, a->name))
		{
			autocomplete_cvar_t *var = (autocomplete_cvar_t *)calloc(1, sizeof(autocomplete_cvar_t));
			if (!var)
			{
				Com_Error(ERR_FATAL, "Sort_Possible_Cmds: Failed to allocate memory.");
				return NULL;
			}

			foundPartialCount++;

			var->name = strdup(a->name);
			if (!var->name)
			{
				free(var);
				Com_Error(ERR_FATAL, "Sort_Possible_Cmds: Failed to allocate memory.");
				return NULL;
			}

			var->next = auto_cvars;
			auto_cvars = var;

			if (retryPartialFlag == RETRY_MULTIPLE)
				Com_Printf("  %s [A]\n", a->name);
			else if (retryPartialFlag == RETRY_ONCE)
				return a->name;
		}
	}

	for (i = 0; i < cvarcount; i++)
	{
		cvar = sorted_cvars[i];
		if (!cvar)
		{
			break;
		}

		if (Sort_Possible_Strtolower(partial, cvar->name))
		{
			autocomplete_cvar_t *var = (autocomplete_cvar_t *)calloc(1, sizeof(autocomplete_cvar_t));
			if (!var)
			{
				Com_Error(ERR_FATAL, "Sort_Possible_Cmds: Failed to allocate memory.");
				return NULL;
			}

			foundPartialCount++;

			var->name = strdup(cvar->name);
			if (!var->name)
			{
				free(var);
				Com_Error(ERR_FATAL, "Sort_Possible_Cmds: Failed to allocate memory.");
				return NULL;
			}

			var->next = auto_cvars;
			auto_cvars = var;

			if (retryPartialFlag == RETRY_MULTIPLE)
				Com_Printf("  %s [V]\n", cvar->name);
			else if (retryPartialFlag == RETRY_ONCE)
				return cvar->name;
		}
	}

	if (foundPartialCount == 1)
	{
		retryPartialFlag = RETRY_ONCE;
		goto retryPartial;
	}
	else if (foundPartialCount == 0)
	{
		return NULL;
	}
	else if (retryPartialFlag == RETRY_INITIAL)
	{
		retryPartialFlag = RETRY_MULTIPLE;
		Com_Printf("Listing matches for '%s'...\n", partial);
		goto retryPartial;
	}
	else if (foundExactCount + foundPartialCount > 0)
	{
		autocomplete_cvar_t *vars = NULL;

		Com_Printf("Found %d matches.\n", foundExactCount + foundPartialCount);

		/* FS: Walk through the whole thing to get to the first one in the list */
		for (vars = auto_cvars; vars; vars = vars->next)
		{
			if (!vars->next)
				break;
		}

		if (!vars || !vars->name)
		{
			if (prevPartial)
			{
				free(prevPartial);
				prevPartial = NULL;
			}
			return NULL;
		}

		if (prevPartial)
		{
			free(prevPartial);
			prevPartial = NULL;
		}

		prevPartial = strdup(vars->name);
		if (!prevPartial)
		{
			Com_Error(ERR_FATAL, "Sort_Possible_Cmds: Failed to allocate memory.");
			return NULL;
		}

		return vars->name;
	}

	return NULL;
}

/* FS: This is needed because cmds, aliases, and cvars are case sensitive. */
qboolean Sort_Possible_Strtolower (const char *partial, const char *complete)
{
	int partialLength = 0;
	int x = 0;

	partialLength = (int)strlen(partial);

	while (x < partialLength)
	{
		if (tolower(partial[x]) != tolower(complete[x]))
			return false;
		x++;
	}

	return true;
}

void Cmd_RemoveAutoComplete (void)
{
	autocomplete_cvar_t *vars = auto_cvars;

	if (prevPartial)
	{
		free(prevPartial);
		prevPartial = NULL;
	}

	if (sorted_cmds)
	{
		free(sorted_cmds);
		sorted_cmds = NULL;
	}

	if (sorted_alias)
	{
		free(sorted_alias);
		sorted_alias = NULL;
	}

	if (sorted_cvars)
	{
		free(sorted_cvars);
		sorted_cvars = NULL;
	}

	if (vars)
	{
		do
		{
			autocomplete_cvar_t *Next;
			Next = vars->next;
			if (vars->name)
			{
				free(vars->name);
				vars->name = NULL;
			}

			free(vars);
			vars = Next;
		} while (vars);
	}

	auto_cvars = NULL;
	cmdcount = aliascount = cvarcount = 0;
}
