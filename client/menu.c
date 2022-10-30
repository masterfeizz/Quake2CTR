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
#ifdef _WIN32
#include <io.h>
#endif
#include "client.h"
#include "qmenu.h"
#include "snd_loc.h"

#ifdef __DJGPP__
extern int	havegus; /* FS: DOS GUS */
#else
static const int havegus = 0;
#endif

static int	m_main_cursor;

static menuframework_s	s_options_menu;

#define NUM_CURSOR_FRAMES 15

static char *menu_in_sound		= "misc/menu1.wav";
static char *menu_move_sound	= "misc/menu2.wav";
static char *menu_out_sound		= "misc/menu3.wav";

void M_Menu_Main_f (void);
	void M_Menu_Game_f (void);
		void M_Menu_LoadGame_f (void);
		void M_Menu_SaveGame_f (void);
		void M_Menu_PlayerConfig_f (void);
			void M_Menu_DownloadOptions_f (void);
		void M_Menu_Credits_f( void );
	void M_Menu_Multiplayer_f( void );
		void M_Menu_JoinServer_f (void);
#ifdef GAMESPY
		void M_Menu_JoinGamespyServer_f (void); /* FS: GameSpy Browser */
#endif
			void M_Menu_AddressBook_f( void );
		void M_Menu_StartServer_f (void);
			void M_Menu_DMOptions_f (void);
	void M_Menu_Video_f (void);
	void M_Menu_Options_f (void);
		void M_Menu_Extended_Options_f (void); /* FS: Extended Options unique to Q2DOS */
		void M_Menu_Keys_f (void);
	void M_Menu_Quit_f (void);

	void M_Menu_Credits( void );

qboolean	m_entersound;		// play after drawing a frame, so caching
								// won't disrupt the sound
void	(*m_drawfunc) (void);
const char *(*m_keyfunc) (int key);

//=============================================================================
/* Support Routines */

#define	MAX_MENU_DEPTH	8


typedef struct
{
	void	(*draw) (void);
	const char *(*key) (int k);
} menulayer_t;

menulayer_t	m_layers[MAX_MENU_DEPTH];
int		m_menudepth;

static void M_Banner( char *name )
{
	int w, h;

	re.DrawGetPicSize (&w, &h, name );
	re.DrawPic( viddef.width / 2 - w / 2, viddef.height / 2 - 110, name );
}

void M_PushMenu ( void (*draw) (void), const char *(*key) (int k) )
{
	int		i;

	if (Cvar_VariableValueInt("maxclients") == 1 
		&& Com_ServerState ())
		Cvar_Set ("paused", "1");

	// if this menu is already present, drop back to that level
	// to avoid stacking menus by hotkeys
	for (i=0 ; i<m_menudepth ; i++)
		if (m_layers[i].draw == draw &&
			m_layers[i].key == key)
		{
			m_menudepth = i;
		}

	if (i == m_menudepth)
	{
		if (m_menudepth >= MAX_MENU_DEPTH)
			Com_Error (ERR_FATAL, "M_PushMenu: MAX_MENU_DEPTH");
		m_layers[m_menudepth].draw = m_drawfunc;
		m_layers[m_menudepth].key = m_keyfunc;
		m_menudepth++;
	}

	m_drawfunc = draw;
	m_keyfunc = key;

	m_entersound = true;

	cls.key_dest = key_menu;
}

void M_ForceMenuOff (void)
{
	m_drawfunc = 0;
	m_keyfunc = 0;
	cls.key_dest = key_game;
	m_menudepth = 0;
	Key_ClearStates ();
	Cvar_Set ("paused", "0");
}

void M_PopMenu (void)
{
	S_StartLocalSound( menu_out_sound );
	if (m_menudepth < 1)
		Com_Error (ERR_FATAL, "M_PopMenu: depth < 1");
	m_menudepth--;

	m_drawfunc = m_layers[m_menudepth].draw;
	m_keyfunc = m_layers[m_menudepth].key;

	if (!m_menudepth)
		M_ForceMenuOff ();
}


const char *Default_MenuKey( menuframework_s *m, int key )
{
	const char *sound = NULL;
	menucommon_s *item;

	if ( m )
	{
		if ( ( item = Menu_ItemAtCursor( m ) ) != 0 )
		{
			if ( item->type == MTYPE_FIELD )
			{
				if ( Field_Key( ( menufield_s * ) item, key ) )
					return NULL;
			}
		}
	}

	switch ( key )
	{
	case K_ESCAPE:
		if(m == &s_options_menu)
		{
			char	cfgName[MAX_QPATH];
			COM_StripExtension(cfg_default->string, cfgName);
			CL_WriteConfiguration (cfgName);
		}
		M_PopMenu();
		return menu_out_sound;
	case K_KP_UPARROW:
	case K_UPARROW:
		if ( m )
		{
			m->cursor--;
			Menu_AdjustCursor( m, -1 );
			sound = menu_move_sound;
		}
		break;
	case K_TAB:
		if ( m )
		{
			m->cursor++;
			Menu_AdjustCursor( m, 1 );
			sound = menu_move_sound;
		}
		break;
	case K_KP_DOWNARROW:
	case K_DOWNARROW:
		if ( m )
		{
			m->cursor++;
			Menu_AdjustCursor( m, 1 );
			sound = menu_move_sound;
		}
		break;
	case K_KP_LEFTARROW:
	case K_LEFTARROW:
		if ( m )
		{
			Menu_SlideItem( m, -1 );
			sound = menu_move_sound;
		}
		break;
	case K_KP_RIGHTARROW:
	case K_RIGHTARROW:
		if ( m )
		{
			Menu_SlideItem( m, 1 );
			sound = menu_move_sound;
		}
		break;

	case K_MOUSE1:
	case K_MOUSE2:
	case K_MOUSE3:
	case K_JOY1:
	case K_JOY2:
	case K_JOY3:
	case K_JOY4:
	case K_AUX1:
	case K_AUX2:
	case K_AUX3:
	case K_AUX4:
	case K_AUX5:
	case K_AUX6:
	case K_AUX7:
	case K_AUX8:
	case K_AUX9:
	case K_AUX10:
	case K_AUX11:
	case K_AUX12:
	case K_AUX13:
	case K_AUX14:
	case K_AUX15:
	case K_AUX16:
	case K_AUX17:
	case K_AUX18:
	case K_AUX19:
	case K_AUX20:
	case K_AUX21:
	case K_AUX22:
	case K_AUX23:
	case K_AUX24:
	case K_AUX25:
	case K_AUX26:
	case K_AUX27:
	case K_AUX28:
	case K_AUX29:
	case K_AUX30:
	case K_AUX31:
	case K_AUX32:
		
	case K_KP_ENTER:
	case K_ENTER:
		if ( m )
			Menu_SelectItem( m );
		sound = menu_move_sound;
		break;
	}

	return sound;
}

//=============================================================================

/*
================
M_DrawCharacter

Draws one solid graphics character
cx and cy are in 320*240 coordinates, and will be centered on
higher res screens.
================
*/
void M_DrawCharacter (int cx, int cy, int num)
{
	re.DrawChar ( cx + ((viddef.width - 320)>>1), cy + ((viddef.height - 240)>>1), num);
}

void M_Print (int cx, int cy, char *str)
{
	while (*str)
	{
		M_DrawCharacter (cx, cy, (*str)+128);
		str++;
		cx += 8;
	}
}

void M_PrintWhite (int cx, int cy, char *str)
{
	while (*str)
	{
		M_DrawCharacter (cx, cy, *str);
		str++;
		cx += 8;
	}
}

void M_DrawPic (int x, int y, char *pic)
{
	re.DrawPic (x + ((viddef.width - 320)>>1), y + ((viddef.height - 240)>>1), pic);
}


/*
=============
M_DrawCursor

Draws an animating cursor with the point at
x,y.  The pic will extend to the left of x,
and both above and below y.
=============
*/
void M_DrawCursor( int x, int y, int f )
{
	char	cursorname[80];
	static qboolean cached;

	if ( !cached )
	{
		int i;

		for ( i = 0; i < NUM_CURSOR_FRAMES; i++ )
		{
			Com_sprintf( cursorname, sizeof( cursorname ), "m_cursor%d", i );

			re.RegisterPic( cursorname );
		}
		cached = true;
	}

	Com_sprintf( cursorname, sizeof(cursorname), "m_cursor%d", f );
	re.DrawPic( x, y, cursorname );
}

void M_DrawTextBox (int x, int y, int width, int lines)
{
	int		cx, cy;
	int		n;

	// draw left side
	cx = x;
	cy = y;
	M_DrawCharacter (cx, cy, 1);
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawCharacter (cx, cy, 4);
	}
	M_DrawCharacter (cx, cy+8, 7);

	// draw middle
	cx += 8;
	while (width > 0)
	{
		cy = y;
		M_DrawCharacter (cx, cy, 2);
		for (n = 0; n < lines; n++)
		{
			cy += 8;
			M_DrawCharacter (cx, cy, 5);
		}
		M_DrawCharacter (cx, cy+8, 8);
		width -= 1;
		cx += 8;
	}

	// draw right side
	cy = y;
	M_DrawCharacter (cx, cy, 3);
	for (n = 0; n < lines; n++)
	{
		cy += 8;
		M_DrawCharacter (cx, cy, 6);
	}
	M_DrawCharacter (cx, cy+8, 9);
}

		
/*
=======================================================================

MAIN MENU

=======================================================================
*/
#define	MAIN_ITEMS	5


void M_Main_Draw (void)
{
	int i;
	int w, h;
	int ystart;
	int	xoffset;
	int widest = -1;
	int totalheight = 0;
	char litname[80];
	char *names[] =
	{
		"m_main_game",
		"m_main_multiplayer",
		"m_main_options",
		"m_main_video",
		"m_main_quit",
		0
	};

	for ( i = 0; names[i] != 0; i++ )
	{
		re.DrawGetPicSize( &w, &h, names[i] );

		if ( w > widest )
			widest = w;
		totalheight += ( h + 12 );
	}

	ystart = ( viddef.height / 2 - 110 );
	xoffset = ( viddef.width - widest + 70 ) / 2;

	for ( i = 0; names[i] != 0; i++ )
	{
		if ( i != m_main_cursor )
			re.DrawPic( xoffset, ystart + i * 40 + 13, names[i] );
	}
	strcpy( litname, names[m_main_cursor] );
	strcat( litname, "_sel" );
	re.DrawPic( xoffset, ystart + m_main_cursor * 40 + 13, litname );

	M_DrawCursor( xoffset - 25, ystart + m_main_cursor * 40 + 11, (int)(cls.realtime / 100)%NUM_CURSOR_FRAMES );

	re.DrawGetPicSize( &w, &h, "m_main_plaque" );
	re.DrawPic( xoffset - 30 - w, ystart, "m_main_plaque" );

	re.DrawPic( xoffset - 30 - w, ystart + h + 5, "m_main_logo" );
}


const char *M_Main_Key (int key)
{
	const char *sound = menu_move_sound;

	switch (key)
	{
	/* FS: Keyboard shortcuts */
	case 'g':
	case 'G':
		M_Menu_Game_f ();
		break;
	case 'm':
	case 'M':
		M_Menu_Multiplayer_f();
		break;

	case 'o':
	case 'O':
		M_Menu_Options_f ();
		break;

	case 'v':
	case 'V':
		M_Menu_Video_f ();
		break;

	case 'q':
	case 'Q':
		M_Menu_Quit_f ();
		break;

	case K_ESCAPE:
		M_PopMenu ();
		break;

	case K_KP_DOWNARROW:
	case K_DOWNARROW:
		if (++m_main_cursor >= MAIN_ITEMS)
			m_main_cursor = 0;
		return sound;

	case K_KP_UPARROW:
	case K_UPARROW:
		if (--m_main_cursor < 0)
			m_main_cursor = MAIN_ITEMS - 1;
		return sound;

	case K_KP_ENTER:
	case K_ENTER:
		m_entersound = true;

		switch (m_main_cursor)
		{
		case 0:
			M_Menu_Game_f ();
			break;

		case 1:
			M_Menu_Multiplayer_f();
			break;

		case 2:
			M_Menu_Options_f ();
			break;

		case 3:
			M_Menu_Video_f ();
			break;

		case 4:
			M_Menu_Quit_f ();
			break;
		}
	}

	return NULL;
}


void M_Menu_Main_f (void)
{
	M_PushMenu (M_Main_Draw, M_Main_Key);
}

/*
=======================================================================

MULTIPLAYER MENU

=======================================================================
*/
static menuframework_s	s_multiplayer_menu;
#ifdef GAMESPY
static menuaction_s		s_join_gamespy_server_action;
#endif
static menuaction_s		s_join_network_server_action;
static menuaction_s		s_start_network_server_action;
static menuaction_s		s_player_setup_action;

static void Multiplayer_MenuDraw (void)
{
	M_Banner( "m_banner_multiplayer" );

	Menu_AdjustCursor( &s_multiplayer_menu, 1 );
	Menu_Draw( &s_multiplayer_menu );
}

static void PlayerSetupFunc( void *unused )
{
	M_Menu_PlayerConfig_f();
}

static void JoinNetworkServerFunc( void *unused )
{
	M_Menu_JoinServer_f();
}

#ifdef GAMESPY
static void JoinGamespyServerFunc( void *unused )
{
	M_Menu_JoinGamespyServer_f();
}
#endif

static void StartNetworkServerFunc( void *unused )
{
	M_Menu_StartServer_f ();
}

void Multiplayer_MenuInit( void )
{
	s_multiplayer_menu.x = viddef.width * 0.50 - 64;
	s_multiplayer_menu.nitems = 0;

#ifdef GAMESPY
	s_join_gamespy_server_action.generic.type	= MTYPE_ACTION;
	s_join_gamespy_server_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_join_gamespy_server_action.generic.x		= 0;
	s_join_gamespy_server_action.generic.y		= 0;
	s_join_gamespy_server_action.generic.name	= " join gamespy server";
	s_join_gamespy_server_action.generic.callback = JoinGamespyServerFunc;
#endif

	s_join_network_server_action.generic.type	= MTYPE_ACTION;
	s_join_network_server_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_join_network_server_action.generic.x		= 0;
	s_join_network_server_action.generic.y		= 10;
	s_join_network_server_action.generic.name	= " join network server";
	s_join_network_server_action.generic.callback = JoinNetworkServerFunc;

	s_start_network_server_action.generic.type	= MTYPE_ACTION;
	s_start_network_server_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_start_network_server_action.generic.x		= 0;
	s_start_network_server_action.generic.y		= 20;
	s_start_network_server_action.generic.name	= " start network server";
	s_start_network_server_action.generic.callback = StartNetworkServerFunc;

	s_player_setup_action.generic.type	= MTYPE_ACTION;
	s_player_setup_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_player_setup_action.generic.x		= 0;
	s_player_setup_action.generic.y		= 30;
	s_player_setup_action.generic.name	= " player setup";
	s_player_setup_action.generic.callback = PlayerSetupFunc;

#ifdef GAMESPY
	Menu_AddItem( &s_multiplayer_menu, ( void * ) &s_join_gamespy_server_action );
#endif
	Menu_AddItem( &s_multiplayer_menu, ( void * ) &s_join_network_server_action );
	Menu_AddItem( &s_multiplayer_menu, ( void * ) &s_start_network_server_action );
	Menu_AddItem( &s_multiplayer_menu, ( void * ) &s_player_setup_action );

	Menu_SetStatusBar( &s_multiplayer_menu, NULL );

	Menu_Center( &s_multiplayer_menu );
}

const char *Multiplayer_MenuKey( int key )
{
	switch(key)
	{
	/* FS: Keyboard shortcuts */
#ifdef GAMESPY
	case 'g':
	case 'G':
		M_Menu_JoinGamespyServer_f ();
		break;
#endif
	case 'n':
	case 'N':
		M_Menu_JoinServer_f();
		break;

	case 's':
	case 'S':
		M_Menu_StartServer_f ();
		break;

	case 'p':
	case 'P':
		M_Menu_PlayerConfig_f ();
		break;
	}

	return Default_MenuKey( &s_multiplayer_menu, key );
}

void M_Menu_Multiplayer_f( void )
{
	Multiplayer_MenuInit();
	M_PushMenu( Multiplayer_MenuDraw, Multiplayer_MenuKey );
}

/*
=======================================================================

KEYS MENU

=======================================================================
*/
char *bindnames[][2] =
{
{"+attack", 		"attack"},
{"weapnext", 		"next weapon"},
{"+forward", 		"walk forward"},
{"+back", 			"backpedal"},
{"+left", 			"turn left"},
{"+right", 			"turn right"},
{"+speed", 			"run"},
{"+moveleft", 		"step left"},
{"+moveright", 		"step right"},
{"+strafe", 		"sidestep"},
{"+lookup", 		"look up"},
{"+lookdown", 		"look down"},
{"centerview", 		"center view"},
{"+mlook", 			"mouse look"},
{"+klook", 			"keyboard look"},
{"+moveup",			"up / jump"},
{"+movedown",		"down / crouch"},

{"inven",			"inventory"},
{"invuse",			"use item"},
{"invdrop",			"drop item"},
{"invprev",			"prev item"},
{"invnext",			"next item"},

{"cmd help", 		"help computer" }, 
{ 0, 0 }
};

int				keys_cursor;
static int		bind_grab;

static menuframework_s	s_keys_menu;
static menuaction_s		s_keys_attack_action;
static menuaction_s		s_keys_change_weapon_action;
static menuaction_s		s_keys_walk_forward_action;
static menuaction_s		s_keys_backpedal_action;
static menuaction_s		s_keys_turn_left_action;
static menuaction_s		s_keys_turn_right_action;
static menuaction_s		s_keys_run_action;
static menuaction_s		s_keys_step_left_action;
static menuaction_s		s_keys_step_right_action;
static menuaction_s		s_keys_sidestep_action;
static menuaction_s		s_keys_look_up_action;
static menuaction_s		s_keys_look_down_action;
static menuaction_s		s_keys_center_view_action;
static menuaction_s		s_keys_mouse_look_action;
static menuaction_s		s_keys_keyboard_look_action;
static menuaction_s		s_keys_move_up_action;
static menuaction_s		s_keys_move_down_action;
static menuaction_s		s_keys_inventory_action;
static menuaction_s		s_keys_inv_use_action;
static menuaction_s		s_keys_inv_drop_action;
static menuaction_s		s_keys_inv_prev_action;
static menuaction_s		s_keys_inv_next_action;

static menuaction_s		s_keys_help_computer_action;

static void M_UnbindCommand (char *command)
{
	int		j;
	int		l;
	char	*b;

	l = strlen(command);

	for (j=0 ; j<256 ; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
			Key_SetBinding (j, "");
	}
}

static void M_FindKeysForCommand (char *command, int *twokeys)
{
	int		count;
	int		j;
	int		l;
	char	*b;

	twokeys[0] = twokeys[1] = -1;
	l = strlen(command);
	count = 0;

	for (j=0 ; j<256 ; j++)
	{
		b = keybindings[j];
		if (!b)
			continue;
		if (!strncmp (b, command, l) )
		{
			twokeys[count] = j;
			count++;
			if (count == 2)
				break;
		}
	}
}

static void KeyCursorDrawFunc( menuframework_s *menu )
{
	if ( bind_grab )
		re.DrawChar( menu->x, menu->y + menu->cursor * 9, '=' );
	else
		re.DrawChar( menu->x, menu->y + menu->cursor * 9, 12 + ( ( int ) ( Sys_Milliseconds() / 250 ) & 1 ) );
}

static void DrawKeyBindingFunc( void *self )
{
	int keys[2];
	menuaction_s *a = ( menuaction_s * ) self;

	M_FindKeysForCommand( bindnames[a->generic.localdata[0]][0], keys);
		
	if (keys[0] == -1)
	{
		Menu_DrawString( a->generic.x + a->generic.parent->x + 16, a->generic.y + a->generic.parent->y, "???" );
	}
	else
	{
		int x;
		const char *name;

		name = Key_KeynumToString (keys[0]);

		Menu_DrawString( a->generic.x + a->generic.parent->x + 16, a->generic.y + a->generic.parent->y, name );

		x = strlen(name) * 8;

		if (keys[1] != -1)
		{
			Menu_DrawString( a->generic.x + a->generic.parent->x + 24 + x, a->generic.y + a->generic.parent->y, "or" );
			Menu_DrawString( a->generic.x + a->generic.parent->x + 48 + x, a->generic.y + a->generic.parent->y, Key_KeynumToString (keys[1]) );
		}
	}
}

static void KeyBindingFunc( void *self )
{
	menuaction_s *a = ( menuaction_s * ) self;
	int keys[2];

	M_FindKeysForCommand( bindnames[a->generic.localdata[0]][0], keys );

	if (keys[1] != -1)
		M_UnbindCommand( bindnames[a->generic.localdata[0]][0]);

	bind_grab = true;

	Menu_SetStatusBar( &s_keys_menu, "press a key or button for this action" );
}

static void Keys_MenuInit( void )
{
	int y = 0;
	int i = 0;

	s_keys_menu.x = viddef.width * 0.50;
	s_keys_menu.nitems = 0;
	s_keys_menu.cursordraw = KeyCursorDrawFunc;

	s_keys_attack_action.generic.type	= MTYPE_ACTION;
	s_keys_attack_action.generic.flags  = QMF_GRAYED;
	s_keys_attack_action.generic.x		= 0;
	s_keys_attack_action.generic.y		= y;
	s_keys_attack_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_attack_action.generic.localdata[0] = i;
	s_keys_attack_action.generic.name	= bindnames[s_keys_attack_action.generic.localdata[0]][1];

	s_keys_change_weapon_action.generic.type	= MTYPE_ACTION;
	s_keys_change_weapon_action.generic.flags  = QMF_GRAYED;
	s_keys_change_weapon_action.generic.x		= 0;
	s_keys_change_weapon_action.generic.y		= y += 9;
	s_keys_change_weapon_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_change_weapon_action.generic.localdata[0] = ++i;
	s_keys_change_weapon_action.generic.name	= bindnames[s_keys_change_weapon_action.generic.localdata[0]][1];

	s_keys_walk_forward_action.generic.type	= MTYPE_ACTION;
	s_keys_walk_forward_action.generic.flags  = QMF_GRAYED;
	s_keys_walk_forward_action.generic.x		= 0;
	s_keys_walk_forward_action.generic.y		= y += 9;
	s_keys_walk_forward_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_walk_forward_action.generic.localdata[0] = ++i;
	s_keys_walk_forward_action.generic.name	= bindnames[s_keys_walk_forward_action.generic.localdata[0]][1];

	s_keys_backpedal_action.generic.type	= MTYPE_ACTION;
	s_keys_backpedal_action.generic.flags  = QMF_GRAYED;
	s_keys_backpedal_action.generic.x		= 0;
	s_keys_backpedal_action.generic.y		= y += 9;
	s_keys_backpedal_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_backpedal_action.generic.localdata[0] = ++i;
	s_keys_backpedal_action.generic.name	= bindnames[s_keys_backpedal_action.generic.localdata[0]][1];

	s_keys_turn_left_action.generic.type	= MTYPE_ACTION;
	s_keys_turn_left_action.generic.flags  = QMF_GRAYED;
	s_keys_turn_left_action.generic.x		= 0;
	s_keys_turn_left_action.generic.y		= y += 9;
	s_keys_turn_left_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_turn_left_action.generic.localdata[0] = ++i;
	s_keys_turn_left_action.generic.name	= bindnames[s_keys_turn_left_action.generic.localdata[0]][1];

	s_keys_turn_right_action.generic.type	= MTYPE_ACTION;
	s_keys_turn_right_action.generic.flags  = QMF_GRAYED;
	s_keys_turn_right_action.generic.x		= 0;
	s_keys_turn_right_action.generic.y		= y += 9;
	s_keys_turn_right_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_turn_right_action.generic.localdata[0] = ++i;
	s_keys_turn_right_action.generic.name	= bindnames[s_keys_turn_right_action.generic.localdata[0]][1];

	s_keys_run_action.generic.type	= MTYPE_ACTION;
	s_keys_run_action.generic.flags  = QMF_GRAYED;
	s_keys_run_action.generic.x		= 0;
	s_keys_run_action.generic.y		= y += 9;
	s_keys_run_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_run_action.generic.localdata[0] = ++i;
	s_keys_run_action.generic.name	= bindnames[s_keys_run_action.generic.localdata[0]][1];

	s_keys_step_left_action.generic.type	= MTYPE_ACTION;
	s_keys_step_left_action.generic.flags  = QMF_GRAYED;
	s_keys_step_left_action.generic.x		= 0;
	s_keys_step_left_action.generic.y		= y += 9;
	s_keys_step_left_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_step_left_action.generic.localdata[0] = ++i;
	s_keys_step_left_action.generic.name	= bindnames[s_keys_step_left_action.generic.localdata[0]][1];

	s_keys_step_right_action.generic.type	= MTYPE_ACTION;
	s_keys_step_right_action.generic.flags  = QMF_GRAYED;
	s_keys_step_right_action.generic.x		= 0;
	s_keys_step_right_action.generic.y		= y += 9;
	s_keys_step_right_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_step_right_action.generic.localdata[0] = ++i;
	s_keys_step_right_action.generic.name	= bindnames[s_keys_step_right_action.generic.localdata[0]][1];

	s_keys_sidestep_action.generic.type	= MTYPE_ACTION;
	s_keys_sidestep_action.generic.flags  = QMF_GRAYED;
	s_keys_sidestep_action.generic.x		= 0;
	s_keys_sidestep_action.generic.y		= y += 9;
	s_keys_sidestep_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_sidestep_action.generic.localdata[0] = ++i;
	s_keys_sidestep_action.generic.name	= bindnames[s_keys_sidestep_action.generic.localdata[0]][1];

	s_keys_look_up_action.generic.type	= MTYPE_ACTION;
	s_keys_look_up_action.generic.flags  = QMF_GRAYED;
	s_keys_look_up_action.generic.x		= 0;
	s_keys_look_up_action.generic.y		= y += 9;
	s_keys_look_up_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_look_up_action.generic.localdata[0] = ++i;
	s_keys_look_up_action.generic.name	= bindnames[s_keys_look_up_action.generic.localdata[0]][1];

	s_keys_look_down_action.generic.type	= MTYPE_ACTION;
	s_keys_look_down_action.generic.flags  = QMF_GRAYED;
	s_keys_look_down_action.generic.x		= 0;
	s_keys_look_down_action.generic.y		= y += 9;
	s_keys_look_down_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_look_down_action.generic.localdata[0] = ++i;
	s_keys_look_down_action.generic.name	= bindnames[s_keys_look_down_action.generic.localdata[0]][1];

	s_keys_center_view_action.generic.type	= MTYPE_ACTION;
	s_keys_center_view_action.generic.flags  = QMF_GRAYED;
	s_keys_center_view_action.generic.x		= 0;
	s_keys_center_view_action.generic.y		= y += 9;
	s_keys_center_view_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_center_view_action.generic.localdata[0] = ++i;
	s_keys_center_view_action.generic.name	= bindnames[s_keys_center_view_action.generic.localdata[0]][1];

	s_keys_mouse_look_action.generic.type	= MTYPE_ACTION;
	s_keys_mouse_look_action.generic.flags  = QMF_GRAYED;
	s_keys_mouse_look_action.generic.x		= 0;
	s_keys_mouse_look_action.generic.y		= y += 9;
	s_keys_mouse_look_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_mouse_look_action.generic.localdata[0] = ++i;
	s_keys_mouse_look_action.generic.name	= bindnames[s_keys_mouse_look_action.generic.localdata[0]][1];

	s_keys_keyboard_look_action.generic.type	= MTYPE_ACTION;
	s_keys_keyboard_look_action.generic.flags  = QMF_GRAYED;
	s_keys_keyboard_look_action.generic.x		= 0;
	s_keys_keyboard_look_action.generic.y		= y += 9;
	s_keys_keyboard_look_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_keyboard_look_action.generic.localdata[0] = ++i;
	s_keys_keyboard_look_action.generic.name	= bindnames[s_keys_keyboard_look_action.generic.localdata[0]][1];

	s_keys_move_up_action.generic.type	= MTYPE_ACTION;
	s_keys_move_up_action.generic.flags  = QMF_GRAYED;
	s_keys_move_up_action.generic.x		= 0;
	s_keys_move_up_action.generic.y		= y += 9;
	s_keys_move_up_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_move_up_action.generic.localdata[0] = ++i;
	s_keys_move_up_action.generic.name	= bindnames[s_keys_move_up_action.generic.localdata[0]][1];

	s_keys_move_down_action.generic.type	= MTYPE_ACTION;
	s_keys_move_down_action.generic.flags  = QMF_GRAYED;
	s_keys_move_down_action.generic.x		= 0;
	s_keys_move_down_action.generic.y		= y += 9;
	s_keys_move_down_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_move_down_action.generic.localdata[0] = ++i;
	s_keys_move_down_action.generic.name	= bindnames[s_keys_move_down_action.generic.localdata[0]][1];

	s_keys_inventory_action.generic.type	= MTYPE_ACTION;
	s_keys_inventory_action.generic.flags  = QMF_GRAYED;
	s_keys_inventory_action.generic.x		= 0;
	s_keys_inventory_action.generic.y		= y += 9;
	s_keys_inventory_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_inventory_action.generic.localdata[0] = ++i;
	s_keys_inventory_action.generic.name	= bindnames[s_keys_inventory_action.generic.localdata[0]][1];

	s_keys_inv_use_action.generic.type	= MTYPE_ACTION;
	s_keys_inv_use_action.generic.flags  = QMF_GRAYED;
	s_keys_inv_use_action.generic.x		= 0;
	s_keys_inv_use_action.generic.y		= y += 9;
	s_keys_inv_use_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_inv_use_action.generic.localdata[0] = ++i;
	s_keys_inv_use_action.generic.name	= bindnames[s_keys_inv_use_action.generic.localdata[0]][1];

	s_keys_inv_drop_action.generic.type	= MTYPE_ACTION;
	s_keys_inv_drop_action.generic.flags  = QMF_GRAYED;
	s_keys_inv_drop_action.generic.x		= 0;
	s_keys_inv_drop_action.generic.y		= y += 9;
	s_keys_inv_drop_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_inv_drop_action.generic.localdata[0] = ++i;
	s_keys_inv_drop_action.generic.name	= bindnames[s_keys_inv_drop_action.generic.localdata[0]][1];

	s_keys_inv_prev_action.generic.type	= MTYPE_ACTION;
	s_keys_inv_prev_action.generic.flags  = QMF_GRAYED;
	s_keys_inv_prev_action.generic.x		= 0;
	s_keys_inv_prev_action.generic.y		= y += 9;
	s_keys_inv_prev_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_inv_prev_action.generic.localdata[0] = ++i;
	s_keys_inv_prev_action.generic.name	= bindnames[s_keys_inv_prev_action.generic.localdata[0]][1];

	s_keys_inv_next_action.generic.type	= MTYPE_ACTION;
	s_keys_inv_next_action.generic.flags  = QMF_GRAYED;
	s_keys_inv_next_action.generic.x		= 0;
	s_keys_inv_next_action.generic.y		= y += 9;
	s_keys_inv_next_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_inv_next_action.generic.localdata[0] = ++i;
	s_keys_inv_next_action.generic.name	= bindnames[s_keys_inv_next_action.generic.localdata[0]][1];

	s_keys_help_computer_action.generic.type	= MTYPE_ACTION;
	s_keys_help_computer_action.generic.flags  = QMF_GRAYED;
	s_keys_help_computer_action.generic.x		= 0;
	s_keys_help_computer_action.generic.y		= y += 9;
	s_keys_help_computer_action.generic.ownerdraw = DrawKeyBindingFunc;
	s_keys_help_computer_action.generic.localdata[0] = ++i;
	s_keys_help_computer_action.generic.name	= bindnames[s_keys_help_computer_action.generic.localdata[0]][1];

	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_attack_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_change_weapon_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_walk_forward_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_backpedal_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_turn_left_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_turn_right_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_run_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_step_left_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_step_right_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_sidestep_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_look_up_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_look_down_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_center_view_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_mouse_look_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_keyboard_look_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_move_up_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_move_down_action );

	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_inventory_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_inv_use_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_inv_drop_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_inv_prev_action );
	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_inv_next_action );

	Menu_AddItem( &s_keys_menu, ( void * ) &s_keys_help_computer_action );
	
	Menu_SetStatusBar( &s_keys_menu, "enter to change, backspace to clear" );
	Menu_Center( &s_keys_menu );
}

static void Keys_MenuDraw (void)
{
	Menu_AdjustCursor( &s_keys_menu, 1 );
	Menu_Draw( &s_keys_menu );
}

static const char *Keys_MenuKey( int key )
{
	menuaction_s *item = ( menuaction_s * ) Menu_ItemAtCursor( &s_keys_menu );

	if ( bind_grab )
	{	
		if ( key != K_ESCAPE && key != '`' )
		{
			char cmd[1024];

			Com_sprintf (cmd, sizeof(cmd), "bind \"%s\" \"%s\"\n", Key_KeynumToString(key), bindnames[item->generic.localdata[0]][0]);
			Cbuf_InsertText (cmd);
		}
		
		Menu_SetStatusBar( &s_keys_menu, "enter to change, backspace to clear" );
		bind_grab = false;
		return menu_out_sound;
	}

	switch ( key )
	{
	case K_KP_ENTER:
	case K_ENTER:
		KeyBindingFunc( item );
		return menu_in_sound;
	case K_BACKSPACE:		// delete bindings
	case K_DEL:				// delete bindings
	case K_KP_DEL:
		M_UnbindCommand( bindnames[item->generic.localdata[0]][0] );
		return menu_out_sound;
	default:
		return Default_MenuKey( &s_keys_menu, key );
	}
}

void M_Menu_Keys_f (void)
{
	Keys_MenuInit();
	M_PushMenu( Keys_MenuDraw, Keys_MenuKey );
}


/*
=======================================================================

CONTROLS MENU

=======================================================================
*/
#if !defined(__DJGPP__) && !defined(_3DS)
static cvar_t *win_noalttab;
#endif

#ifdef USE_JOYSTICK
extern cvar_t *in_joystick;
#endif

static menuframework_s	s_extended_options_menu; /* FS: Added */
static menuaction_s		s_options_defaults_action;
static menuaction_s		s_options_customize_options_action;
static menuaction_s		s_options_extended_options_action; /* FS: Added */
static menuslider_s		s_options_sensitivity_slider;
static menulist_s		s_options_freelook_box;

#if !defined(__DJGPP__) && !defined(_3DS)
static menulist_s		s_options_noalttab_box;
#endif

static menulist_s		s_options_alwaysrun_box;
static menulist_s		s_options_invertmouse_box;
static menulist_s		s_options_lookspring_box;
static menulist_s		s_options_lookstrafe_box;
static menulist_s		s_options_crosshair_box;
static menuslider_s		s_options_sfxvolume_slider;

#ifdef USE_JOYSTICK
static menulist_s		s_options_joystick_box;
#endif

static menulist_s		s_options_cdvolume_box;
static menuslider_s		s_options_musicvolume_slider; /* FS: Added */
static menulist_s		s_options_quality_list;
static menulist_s		s_options_loadas8bit_box; /* FS: Added */

#ifdef WIN32
static menulist_s		s_options_compatibility_list;
#endif

static menulist_s		s_options_console_action;

#ifdef _3DS
extern cvar_t 	*cstick_sensitivity;
extern cvar_t	*circlepad_sensitivity;
extern cvar_t	*circlepad_look;

static menuslider_s		s_options_circlepad_slider;
static menuslider_s		s_options_cstick_slider;
static menulist_s 		s_options_circlepadlook_box;
#endif

static void CrosshairFunc( void *unused )
{
	Cvar_SetValue( "crosshair", s_options_crosshair_box.curvalue );
}

#ifdef USE_JOYSTICK
static void JoystickFunc( void *unused )
{
	Cvar_SetValue( "in_joystick", s_options_joystick_box.curvalue );
}
#endif

static void ExtendedOptionsFunc( void *unused ) /* FS: Added */
{
	M_Menu_Extended_Options_f();
}

static void CustomizeControlsFunc( void *unused )
{
	M_Menu_Keys_f();
}

static void AlwaysRunFunc( void *unused )
{
	Cvar_SetValue( "cl_run", s_options_alwaysrun_box.curvalue );
}

static void FreeLookFunc( void *unused )
{
	Cvar_SetValue( "freelook", s_options_freelook_box.curvalue );
}

static void MouseSpeedFunc( void *unused )
{
	Cvar_SetValue( "sensitivity", s_options_sensitivity_slider.curvalue / 2.0F );
}

#if !defined(__DJGPP__) && !defined(_3DS)
static void NoAltTabFunc( void *unused )
{
	Cvar_SetValue( "win_noalttab", s_options_noalttab_box.curvalue );
}
#endif

#ifdef _3DS

static void CirclepadSpeedFunc( void *unused )
{
	Cvar_SetValue( "circlepad_sensitivity", s_options_circlepad_slider.curvalue / 2.0F );
}

static void CStickSpeedFunc( void *unused )
{
	Cvar_SetValue( "cstick_sensitivity", s_options_cstick_slider.curvalue / 2.0F );
}

static void CirclepadLookFunc( void *unused )
{
	Cvar_SetValue( "circlepad_look", s_options_circlepadlook_box.curvalue );
}

#endif

#if 0
float ClampCvar( float min, float max, float value )
{
	if ( value < min ) return min;
	if ( value > max ) return max;
	return value;
}
#endif	//duplicate from cl_main

static int Get_S_KHZ_Quality (void)  /* FS: Added */
{
	if (s_khz->intValue <= 19293)
	{
		return 0;
	}
	else if (s_khz->intValue <= 22050)
	{
		return 1;
	}
	else if (s_khz->intValue <= 44100)
	{
		return 2;
	}
	else if (s_khz->intValue <= 48000)
	{
		if (havegus == 1) /* FS: Max of GUS Classic is 44100 */
		{
			return 2;
		}
		return 3;
	}
	else
	{
		if (havegus)
		{
			if (havegus == 1)
			{
				return 2; /* FS: Max of GUS Classic is 44100 */
			}
			else
			{
				return 3; /* FS: Max of GUS MAX/PnP is 48000 */
			}
		}

		return 4;
	}

	return 0;
}

static void ControlsSetMenuItemValues( void )
{
	s_options_sfxvolume_slider.curvalue		= Cvar_VariableValue( "s_volume" ) * 10;
	s_options_cdvolume_box.curvalue 		= !Cvar_VariableValue("cd_nocd");
	s_options_quality_list.curvalue			= Get_S_KHZ_Quality();  /* FS: Added */
	s_options_sensitivity_slider.curvalue	= ( sensitivity->value ) * 2;

	 /* FS: Added */
	s_options_loadas8bit_box.curvalue		= !Cvar_VariableValue("s_loadas8bit");

	Cvar_SetValue( "cl_run", ClampCvar( 0, 1, cl_run->value ) );
	s_options_alwaysrun_box.curvalue		= cl_run->value;

	s_options_invertmouse_box.curvalue		= m_pitch->value < 0;

	Cvar_SetValue( "lookspring", ClampCvar( 0, 1, lookspring->value ) );
	s_options_lookspring_box.curvalue		= lookspring->value;

	Cvar_SetValue( "lookstrafe", ClampCvar( 0, 1, lookstrafe->value ) );
	s_options_lookstrafe_box.curvalue		= lookstrafe->value;

	Cvar_SetValue( "freelook", ClampCvar( 0, 1, freelook->value ) );
	s_options_freelook_box.curvalue			= freelook->value;

	Cvar_SetValue( "crosshair", ClampCvar( 0, 3, crosshair->value ) );
	s_options_crosshair_box.curvalue		= crosshair->value;

#ifdef USE_JOYSTICK
	Cvar_SetValue( "in_joystick", ClampCvar( 0, 1, in_joystick->value ) );
	s_options_joystick_box.curvalue		= in_joystick->value;
#endif

#if !defined(__DJGPP__) && !defined(_3DS)
	s_options_noalttab_box.curvalue			= win_noalttab->value;
#endif
}

static void ControlsResetDefaultsFunc( void *unused )
{
	Cbuf_AddText ("exec default.cfg\n");
	Cbuf_Execute();

#ifdef _3DS
	Sys_DefaultConfig();
#endif

	ControlsSetMenuItemValues();
}

static void InvertMouseFunc( void *unused )
{
	if ( s_options_invertmouse_box.curvalue == 0 )
	{
		Cvar_SetValue( "m_pitch", fabs( m_pitch->value ) );
	}
	else
	{
		Cvar_SetValue( "m_pitch", -fabs( m_pitch->value ) );
	}
}

static void LookspringFunc( void *unused )
{
	Cvar_SetValue( "lookspring", s_options_lookspring_box.curvalue );
}

static void LookstrafeFunc( void *unused )
{
	Cvar_SetValue( "lookstrafe", s_options_lookstrafe_box.curvalue );
}

static void UpdateVolumeFunc( void *unused )
{
	Cvar_SetValue( "s_volume", s_options_sfxvolume_slider.curvalue / 10 );
}

static void UpdateCDVolumeFunc( void *unused )
{
	Cvar_SetValue( "cd_nocd", !s_options_cdvolume_box.curvalue );
}

static void UpdateMusicVolumeFunc (void *unused) /* FS: WAV and OGG Music Volume */
{
	Cvar_SetValue ( "s_musicvolume", s_options_musicvolume_slider.curvalue / 10 );
}

static void ConsoleFunc( void *unused )
{
	/*
	** the proper way to do this is probably to have ToggleConsole_f accept a parameter
	*/
	extern void Key_ClearTyping( void );

	if ( cl.attractloop )
	{
		Cbuf_AddText ("killserver\n");
		return;
	}

	Key_ClearTyping ();
	Con_ClearNotify ();

	M_ForceMenuOff ();
	cls.key_dest = key_console;
}

static void UpdateSoundQualityFunc( void *unused )
{
	switch (s_options_quality_list.curvalue)
	{
		case 0:
			if(havegus == 1)
			{
				Cvar_SetValue( "s_khz", 19293 );
			}
			else
			{
				Cvar_SetValue( "s_khz", 11025 );
			}
			break;
		case 1:
			Cvar_SetValue( "s_khz", 22050 );
			break;
		case 2:
			Cvar_SetValue( "s_khz", 44100 );
			break;
		case 3:
			if(havegus == 1)
			{
				Cvar_SetValue( "s_khz", 44100 );
			}
			else
			{
				Cvar_SetValue( "s_khz", 48000 );
			}
			break;
		case 4:
			Cvar_SetValue( "s_khz", 96000 );
			break;
		default:
			Cvar_SetValue( "s_khz", 22050 );
			break;
	}

#ifdef WIN32
	Cvar_SetValue( "s_primary", s_options_compatibility_list.curvalue );
#endif

	Cvar_SetValue( "s_loadas8bit", !s_options_loadas8bit_box.curvalue );  /* FS: Added */

	M_DrawTextBox( 8, 120 - 48, 36, 3 );
	M_Print( 16 + 16, 120 - 48 + 8,  "Restarting the sound system. This" );
	M_Print( 16 + 16, 120 - 48 + 16, "could take up to a minute, so" );
	M_Print( 16 + 16, 120 - 48 + 24, "please be patient." );

	// the text box won't show up unless we do a buffer swap
	re.EndFrame();

	CL_Snd_Restart_f();
}

void Options_MenuInit( void )
{
	static const char *cd_music_items[] =
	{
		"disabled",
		"enabled",
		0
	};

#ifdef WIN32
	static const char *compatibility_items[] =
	{
		"max compatibility", "max performance", 0
	};
#endif
	static const char *yesno_names[] =
	{
		"no",
		"yes",
		0
	};

	static const char *crosshair_names[] =
	{
		"none",
		"cross",
		"dot",
		"angle",
		0
	};

#ifdef WIN32
	static const char *skhz_win32_frequency[] =
	{
		"11025",
		"22050",
		"44100",
		"48000",
		"96000",
		0
	};
#else

	static const char *skhz_gusmax_frequency[] =
	{
		"11025",
		"22050",
		"44100",
		"48000",
		0
	};

	static const char *skhz_gusclassic_frequency[] =
	{
		"19293",
		"22050",
		"44100",
		0
	};

	static const char *skhz_generic_dos_frequency[] =
	{
		"11025",
		"22050",
		"44100",
		"48000",
		"96000",
		0
	};
#endif // WIN32

	static const char *sloadas8bit_names[] =  /* FS: Added */
	{
		"8-bit",
		"16-bit",
		0
	};

#if !defined(__DJGPP__) && !defined(_3DS)
	win_noalttab = Cvar_Get( "win_noalttab", "0", CVAR_ARCHIVE );
#endif

	/*
	** configure controls menu and menu items
	*/
	s_options_menu.x = viddef.width / 2;
	s_options_menu.y = viddef.height / 2 - 58;
	s_options_menu.nitems = 0;

	int current_y = 0;

	s_options_sfxvolume_slider.generic.type	= MTYPE_SLIDER;
	s_options_sfxvolume_slider.generic.x	= 0;
	s_options_sfxvolume_slider.generic.y	= current_y;
	s_options_sfxvolume_slider.generic.name	= "effects volume";
	s_options_sfxvolume_slider.generic.callback	= UpdateVolumeFunc;
	s_options_sfxvolume_slider.minvalue		= 0;
	s_options_sfxvolume_slider.maxvalue		= 10;
	s_options_sfxvolume_slider.curvalue		= Cvar_VariableValue( "s_volume" ) * 10;

/* FS: For Music played from WAV or OGG files */
	s_options_musicvolume_slider.generic.type	= MTYPE_SLIDER;
	s_options_musicvolume_slider.generic.x	= 0;
	s_options_musicvolume_slider.generic.y	= (current_y += 10);
	s_options_musicvolume_slider.generic.name	= "music volume";
	s_options_musicvolume_slider.generic.callback	= UpdateMusicVolumeFunc;
	s_options_musicvolume_slider.minvalue		= 0;
	s_options_musicvolume_slider.maxvalue		= 10;
	s_options_musicvolume_slider.curvalue		= Cvar_VariableValue( "s_musicvolume" ) * 10;

#ifndef _3DS
	s_options_cdvolume_box.generic.type	= MTYPE_SPINCONTROL;
	s_options_cdvolume_box.generic.x		= 0;
	s_options_cdvolume_box.generic.y		= (current_y += 10);
	s_options_cdvolume_box.generic.name	= "CD music";
	s_options_cdvolume_box.generic.callback	= UpdateCDVolumeFunc;
	s_options_cdvolume_box.itemnames		= cd_music_items;
	s_options_cdvolume_box.curvalue 		= !Cvar_VariableValue("cd_nocd");

	s_options_quality_list.generic.type	= MTYPE_SPINCONTROL;
	s_options_quality_list.generic.x		= 0;
	s_options_quality_list.generic.y		= (current_y += 10);
	s_options_quality_list.generic.name		= "sound frequency";
	s_options_quality_list.generic.callback = UpdateSoundQualityFunc;
#ifdef WIN32
	s_options_quality_list.itemnames		= skhz_win32_frequency;
#else
	if (havegus)
	{
		if(havegus == 1)
		{
			s_options_quality_list.itemnames		= skhz_gusclassic_frequency;
		}
		else
		{
			s_options_quality_list.itemnames		= skhz_gusmax_frequency;
		}
	}
	else
	{
		s_options_quality_list.itemnames		= skhz_generic_dos_frequency;
	}
#endif // WIN32
	s_options_quality_list.curvalue			= Get_S_KHZ_Quality();  /* FS: Added */
#endif // _3DS

#ifndef _3DS
	 /* FS: Added */
	s_options_loadas8bit_box.generic.type = MTYPE_SPINCONTROL;
	s_options_loadas8bit_box.generic.x	= 0;
	s_options_loadas8bit_box.generic.y	= (current_y += 10);
	s_options_loadas8bit_box.generic.name	= "sound bit-depth";
	s_options_loadas8bit_box.generic.callback = UpdateSoundQualityFunc;
	s_options_loadas8bit_box.itemnames = sloadas8bit_names;
#endif

#ifdef WIN32
	s_options_compatibility_list.generic.type	= MTYPE_SPINCONTROL;
	s_options_compatibility_list.generic.x		= 0;
	s_options_compatibility_list.generic.y		= (current_y += 10);
	s_options_compatibility_list.generic.name	= "sound compatibility";
	s_options_compatibility_list.generic.callback = UpdateSoundQualityFunc;
	s_options_compatibility_list.itemnames		= compatibility_items;
	s_options_compatibility_list.curvalue		= Cvar_VariableValue( "s_primary" );
#endif // WIN32

#ifdef _3DS
	s_options_circlepad_slider.generic.type	= MTYPE_SLIDER;
	s_options_circlepad_slider.generic.x		= 0;
	s_options_circlepad_slider.generic.y		= (current_y += 20);
	s_options_circlepad_slider.generic.name	= "circlepad speed";
	s_options_circlepad_slider.generic.callback = CirclepadSpeedFunc;
	s_options_circlepad_slider.minvalue		= 1;
	s_options_circlepad_slider.maxvalue		= 10;

	s_options_cstick_slider.generic.type	= MTYPE_SLIDER;
	s_options_cstick_slider.generic.x		= 0;
	s_options_cstick_slider.generic.y		= (current_y += 10);
	s_options_cstick_slider.generic.name	= "C-Stick speed";
	s_options_cstick_slider.generic.callback = CStickSpeedFunc;
	s_options_cstick_slider.minvalue		= 1;
	s_options_cstick_slider.maxvalue		= 10;

	s_options_sensitivity_slider.generic.type	= MTYPE_SLIDER;
	s_options_sensitivity_slider.generic.x		= 0;
	s_options_sensitivity_slider.generic.y		= (current_y += 10);
	s_options_sensitivity_slider.generic.name	= "touch speed";
	s_options_sensitivity_slider.generic.callback = MouseSpeedFunc;
	s_options_sensitivity_slider.minvalue		= 1;
	s_options_sensitivity_slider.maxvalue		= 10;
#else
	s_options_sensitivity_slider.generic.type	= MTYPE_SLIDER;
	s_options_sensitivity_slider.generic.x		= 0;
	s_options_sensitivity_slider.generic.y		= (current_y += 10);
	s_options_sensitivity_slider.generic.name	= "mouse speed";
	s_options_sensitivity_slider.generic.callback = MouseSpeedFunc;
	s_options_sensitivity_slider.minvalue		= 2;
	s_options_sensitivity_slider.maxvalue		= 22;
#endif

	s_options_alwaysrun_box.generic.type = MTYPE_SPINCONTROL;
	s_options_alwaysrun_box.generic.x	= 0;
	s_options_alwaysrun_box.generic.y	= (current_y += 10);
	s_options_alwaysrun_box.generic.name	= "always run";
	s_options_alwaysrun_box.generic.callback = AlwaysRunFunc;
	s_options_alwaysrun_box.itemnames = yesno_names;

	s_options_invertmouse_box.generic.type = MTYPE_SPINCONTROL;
	s_options_invertmouse_box.generic.x	= 0;
	s_options_invertmouse_box.generic.y	= (current_y += 10);
	s_options_invertmouse_box.generic.name	= "invert mouse";
	s_options_invertmouse_box.generic.callback = InvertMouseFunc;
	s_options_invertmouse_box.itemnames = yesno_names;

	s_options_lookspring_box.generic.type = MTYPE_SPINCONTROL;
	s_options_lookspring_box.generic.x	= 0;
	s_options_lookspring_box.generic.y	= (current_y += 10);
	s_options_lookspring_box.generic.name	= "lookspring";
	s_options_lookspring_box.generic.callback = LookspringFunc;
	s_options_lookspring_box.itemnames = yesno_names;

	s_options_lookstrafe_box.generic.type = MTYPE_SPINCONTROL;
	s_options_lookstrafe_box.generic.x	= 0;
	s_options_lookstrafe_box.generic.y	= (current_y += 10);
	s_options_lookstrafe_box.generic.name	= "lookstrafe";
	s_options_lookstrafe_box.generic.callback = LookstrafeFunc;
	s_options_lookstrafe_box.itemnames = yesno_names;

#ifdef _3DS
	s_options_circlepadlook_box.generic.type = MTYPE_SPINCONTROL;
	s_options_circlepadlook_box.generic.x	= 0;
	s_options_circlepadlook_box.generic.y	= (current_y += 10);
	s_options_circlepadlook_box.generic.name	= "circlepad look";
	s_options_circlepadlook_box.generic.callback = CirclepadLookFunc;
	s_options_circlepadlook_box.itemnames = yesno_names;
#else
	s_options_freelook_box.generic.type = MTYPE_SPINCONTROL;
	s_options_freelook_box.generic.x	= 0;
	s_options_freelook_box.generic.y	= (current_y += 10);
	s_options_freelook_box.generic.name	= "free look";
	s_options_freelook_box.generic.callback = FreeLookFunc;
	s_options_freelook_box.itemnames = yesno_names;
#endif

	s_options_crosshair_box.generic.type = MTYPE_SPINCONTROL;
	s_options_crosshair_box.generic.x	= 0;
	s_options_crosshair_box.generic.y	= (current_y += 10);
	s_options_crosshair_box.generic.name	= "crosshair";
	s_options_crosshair_box.generic.callback = CrosshairFunc;
	s_options_crosshair_box.itemnames = crosshair_names;

#if !defined(__DJGPP__) && !defined(_3DS)
	s_options_noalttab_box.generic.type = MTYPE_SPINCONTROL;
	s_options_noalttab_box.generic.x	= 0;
	s_options_noalttab_box.generic.y	= (current_y += 10);
	s_options_noalttab_box.generic.name	= "disable alt-tab";
	s_options_noalttab_box.generic.callback = NoAltTabFunc;
	s_options_noalttab_box.itemnames = yesno_names;
#endif

#ifdef USE_JOYSTICK
	s_options_joystick_box.generic.type = MTYPE_SPINCONTROL;
	s_options_joystick_box.generic.x	= 0;
	s_options_joystick_box.generic.y	= (current_y += 10);
	s_options_joystick_box.generic.name	= "use joystick";
	s_options_joystick_box.generic.callback = JoystickFunc;
	s_options_joystick_box.itemnames = yesno_names;
#endif

	 /* FS: Added */
	s_options_extended_options_action.generic.type	= MTYPE_ACTION;
	s_options_extended_options_action.generic.x		= 0;
	s_options_extended_options_action.generic.y		= (current_y += 10);
	s_options_extended_options_action.generic.name	= "extended options";
	s_options_extended_options_action.generic.statusbar	= "extended Q2DOS options";
	s_options_extended_options_action.generic.callback = ExtendedOptionsFunc;

	s_options_customize_options_action.generic.type	= MTYPE_ACTION;
	s_options_customize_options_action.generic.x		= 0;
	s_options_customize_options_action.generic.y		= (current_y += 10);
	s_options_customize_options_action.generic.name	= "customize controls";
	s_options_customize_options_action.generic.statusbar	= "customize controls";
	s_options_customize_options_action.generic.callback = CustomizeControlsFunc;

	s_options_defaults_action.generic.type	= MTYPE_ACTION;
	s_options_defaults_action.generic.x		= 0;
	s_options_defaults_action.generic.y		= (current_y += 10);
	s_options_defaults_action.generic.name	= "reset defaults";
	s_options_defaults_action.generic.statusbar	= "reset options to default values";
	s_options_defaults_action.generic.callback = ControlsResetDefaultsFunc;

	s_options_console_action.generic.type	= MTYPE_ACTION;
	s_options_console_action.generic.x		= 0;
	s_options_console_action.generic.y		= (current_y += 10);
	s_options_console_action.generic.name	= "go to console";
	s_options_console_action.generic.statusbar	= "go to command console";
	s_options_console_action.generic.callback = ConsoleFunc;

	ControlsSetMenuItemValues();

/* FS: If you add stuff you gotta be certain it's in order it appears! */
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_sfxvolume_slider );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_musicvolume_slider);  /* FS: Added */

#ifndef _3DS
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_cdvolume_box );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_quality_list );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_loadas8bit_box );  /* FS: Added */
#endif

#ifdef WIN32
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_compatibility_list );
#endif

#ifdef _3DS
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_circlepad_slider );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_cstick_slider );
#endif

	Menu_AddItem( &s_options_menu, ( void * ) &s_options_sensitivity_slider );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_alwaysrun_box );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_invertmouse_box );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_lookspring_box );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_lookstrafe_box );

#ifdef _3DS
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_circlepadlook_box );
#else
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_freelook_box );
#endif
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_crosshair_box );

#if !defined(__DJGPP__) && !defined(_3DS)
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_noalttab_box );
#endif

#ifdef USE_JOYSTICK
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_joystick_box );
#endif
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_extended_options_action );  /* FS: Added */
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_customize_options_action );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_defaults_action );
	Menu_AddItem( &s_options_menu, ( void * ) &s_options_console_action );
}

void Options_MenuDraw (void)
{
	M_Banner( "m_banner_options" );
	Menu_AdjustCursor( &s_options_menu, 1 );
	Menu_Draw( &s_options_menu );
}

const char *Options_MenuKey( int key )
{
	return Default_MenuKey( &s_options_menu, key );
}

void M_Menu_Options_f (void)
{
	Options_MenuInit();
	M_PushMenu ( Options_MenuDraw, Options_MenuKey );
}

/*
=======================================================================

EXTENDED OPTIONS MENU

=======================================================================
*/
/* FS: New stuff unique to Q2DOS */
static menulist_s		s_extended_options_showfps_box;
static menulist_s		s_extended_options_showtime_box;
static menulist_s		s_extended_options_showuptime_box;
static menulist_s		s_extended_options_altcolours_box;
static menulist_s		s_extended_options_hidegunicon_box;
static menulist_s		s_extended_options_3rd_person_cam_box;
static menulist_s		s_extended_options_back_action;

extern cvar_t	*cl_drawfps;
extern cvar_t	*cl_drawtime;
extern cvar_t	*cl_drawuptime;
extern cvar_t	*cl_drawaltcolours;
extern cvar_t	*cl_hide_gun_icon;

static void Extended_ControlsSetMenuItemValues(void)
{
#if 0
	s_options_sfxvolume_slider.curvalue		= Cvar_VariableValue( "s_volume" ) * 10;
	s_options_cdvolume_box.curvalue 		= !Cvar_VariableValue("cd_nocd");
	s_options_quality_list.curvalue			= Get_S_KHZ_Quality();  /* FS: Added */
	s_options_sensitivity_slider.curvalue	= ( sensitivity->value ) * 2;
#endif

	Cvar_SetValue( "cl_drawfps", ClampCvar( 0, 1, cl_drawfps->value ) );
	s_extended_options_showfps_box.curvalue		= cl_drawfps->value;

	Cvar_SetValue( "cl_drawtime", ClampCvar( 0, 2, cl_drawtime->value ) );
	s_extended_options_showtime_box.curvalue		= cl_drawtime->value;

	Cvar_SetValue( "cl_drawuptime", ClampCvar( 0, 2, cl_drawuptime->value ) );
	s_extended_options_showuptime_box.curvalue		= cl_drawuptime->value;

	Cvar_SetValue( "cl_drawaltcolours", ClampCvar( 0, 1, cl_drawaltcolours->value ) );
	s_extended_options_altcolours_box.curvalue		= cl_drawaltcolours->value;

	Cvar_SetValue( "cl_hide_gun_icon", ClampCvar( 0, 1, cl_hide_gun_icon->value ) );
	s_extended_options_hidegunicon_box.curvalue		= cl_hide_gun_icon->value;

	Cvar_SetValue( "cl_3dcam", ClampCvar( 0, 1, cl_3dcam->value ) );
	s_extended_options_3rd_person_cam_box.curvalue		= cl_3dcam->value;

#ifdef _3DS
	s_options_circlepad_slider.curvalue = circlepad_sensitivity->value * 2;
	s_options_cstick_slider.curvalue 	= cstick_sensitivity->value * 2;

	Cvar_SetValue( "circlepad_look", ClampCvar( 0, 1, circlepad_look->value ) );
	s_options_circlepadlook_box.curvalue			= circlepad_look->value;
#endif
}

static void GoToOptionsFunc(void *unused)
{
	M_Menu_Options_f();
}

static void showFPSFunc(void *unused)
{
	Cvar_SetValue( "cl_drawfps", s_extended_options_showfps_box.curvalue );
}

static void showTimeFunc(void *unused)
{
	Cvar_SetValue( "cl_drawtime", s_extended_options_showtime_box.curvalue );
}

static void showUptimeFunc(void *unused)
{
	Cvar_SetValue( "cl_drawuptime", s_extended_options_showuptime_box.curvalue );
}

static void altColoursFunc(void *unused)
{
	Cvar_SetValue( "cl_drawaltcolours", s_extended_options_altcolours_box.curvalue );
}

static void hideGunIconFunc(void *unused)
{
	Cvar_SetValue( "cl_hide_gun_icon", s_extended_options_hidegunicon_box.curvalue );
}

static void thirdPersonCameraFunc (void *unused)
{
	Cvar_SetValue("cl_3dcam", s_extended_options_3rd_person_cam_box.curvalue);
}

void Extended_Options_MenuInit( void )
{
	static const char *yesno_names[] =
	{
		"no",
		"yes",
		0
	};

	static const char *showtime_names[] =
	{
		"no",
		"military",
		"AM/PM",
		0
	};

	static const char *showuptime_names[] =
	{
		"no",
		"map uptime",
		"total uptime",
		0
	};

	/*
	** configure controls menu and menu items
	*/
	s_extended_options_menu.x = viddef.width / 2;
	s_extended_options_menu.y = viddef.height / 2 - 78;
	s_extended_options_menu.nitems = 0;

	s_extended_options_showfps_box.generic.type = MTYPE_SPINCONTROL;
	s_extended_options_showfps_box.generic.x = 0;
	s_extended_options_showfps_box.generic.y = 10;
	s_extended_options_showfps_box.generic.name	= "show FPS";
	s_extended_options_showfps_box.generic.statusbar = "draw FPS on screen";
	s_extended_options_showfps_box.generic.callback = showFPSFunc;
	s_extended_options_showfps_box.itemnames = yesno_names;

	s_extended_options_showtime_box.generic.type = MTYPE_SPINCONTROL;
	s_extended_options_showtime_box.generic.x = 0;
	s_extended_options_showtime_box.generic.y = 20;
	s_extended_options_showtime_box.generic.name = "show time";
	s_extended_options_showtime_box.generic.statusbar = "draw current time on screen";
	s_extended_options_showtime_box.generic.callback = showTimeFunc;
	s_extended_options_showtime_box.itemnames = showtime_names;

	s_extended_options_showuptime_box.generic.type = MTYPE_SPINCONTROL;
	s_extended_options_showuptime_box.generic.x	= 0;
	s_extended_options_showuptime_box.generic.y	= 30;
	s_extended_options_showuptime_box.generic.name	= "show uptime";
	s_extended_options_showuptime_box.generic.statusbar = "draw uptime on screen";
	s_extended_options_showuptime_box.generic.callback = showUptimeFunc;
	s_extended_options_showuptime_box.itemnames = showuptime_names;

	s_extended_options_altcolours_box.generic.type = MTYPE_SPINCONTROL;
	s_extended_options_altcolours_box.generic.x	= 0;
	s_extended_options_altcolours_box.generic.y	= 40;
	s_extended_options_altcolours_box.generic.name	= "draw alt colours";
	s_extended_options_altcolours_box.generic.statusbar = "draw alternate colours for draw cvars";
	s_extended_options_altcolours_box.generic.callback = altColoursFunc;
	s_extended_options_altcolours_box.itemnames = yesno_names;

	s_extended_options_hidegunicon_box.generic.type = MTYPE_SPINCONTROL;
	s_extended_options_hidegunicon_box.generic.x = 0;
	s_extended_options_hidegunicon_box.generic.y = 50;
	s_extended_options_hidegunicon_box.generic.name = "hide gun icon";
	s_extended_options_hidegunicon_box.generic.statusbar = "hide the gun/help icon";
	s_extended_options_hidegunicon_box.generic.callback = hideGunIconFunc;
	s_extended_options_hidegunicon_box.itemnames = yesno_names;

	s_extended_options_3rd_person_cam_box.generic.type = MTYPE_SPINCONTROL;
	s_extended_options_3rd_person_cam_box.generic.x = 0;
	s_extended_options_3rd_person_cam_box.generic.y = 60;
	s_extended_options_3rd_person_cam_box.generic.name = "3rd person camera";
	s_extended_options_3rd_person_cam_box.generic.statusbar = "3rd person camera";
	s_extended_options_3rd_person_cam_box.generic.callback = thirdPersonCameraFunc;
	s_extended_options_3rd_person_cam_box.itemnames = yesno_names;

	s_extended_options_back_action.generic.type	= MTYPE_ACTION;
	s_extended_options_back_action.generic.x = 0;
	s_extended_options_back_action.generic.y = 100;
	s_extended_options_back_action.generic.name = "go to options";
	s_extended_options_back_action.generic.statusbar = "go back to the options menu";
	s_extended_options_back_action.generic.callback = GoToOptionsFunc;

	Extended_ControlsSetMenuItemValues();

/* FS: If you add stuff you gotta be certain it's in order it appears! */
	Menu_AddItem(&s_extended_options_menu, (void *) &s_extended_options_showfps_box);
	Menu_AddItem(&s_extended_options_menu, (void *) &s_extended_options_showtime_box);
	Menu_AddItem(&s_extended_options_menu, (void *) &s_extended_options_showuptime_box);
	Menu_AddItem(&s_extended_options_menu, (void *) &s_extended_options_altcolours_box);
	Menu_AddItem(&s_extended_options_menu, (void *) &s_extended_options_hidegunicon_box);
	Menu_AddItem(&s_extended_options_menu, (void*)&s_extended_options_3rd_person_cam_box);
	Menu_AddItem(&s_extended_options_menu, (void *) &s_extended_options_back_action);
}

void Extended_Options_MenuDraw (void)
{
	Menu_AdjustCursor( &s_extended_options_menu, 1 );
	Menu_Draw( &s_extended_options_menu );
}

const char *Extended_Options_MenuKey( int key )
{
	return Default_MenuKey( &s_extended_options_menu, key );
}

void M_Menu_Extended_Options_f (void)
{
	Extended_Options_MenuInit();
	M_PushMenu ( Extended_Options_MenuDraw, Extended_Options_MenuKey );
}

/*
=======================================================================

VIDEO MENU

=======================================================================
*/

void M_Menu_Video_f (void)
{
	VID_MenuInit();
	M_PushMenu( VID_MenuDraw, VID_MenuKey );
}

/*
=============================================================================

END GAME MENU

=============================================================================
*/
static int credits_start_time;
// Knigthtmare added- allow credits to scroll past top of screen
static int credits_start_line;
static const char **credits;
static char *creditsIndex[256];
static char *creditsBuffer;
static const char *idcredits[] =
{
	"+QUAKE II BY ID SOFTWARE",
	"",
	"+PROGRAMMING",
	"John Carmack",
	"John Cash",
	"Brian Hook",
	"",
	"+ART",
	"Adrian Carmack",
	"Kevin Cloud",
	"Paul Steed",
	"",
	"+LEVEL DESIGN",
	"Tim Willits",
	"American McGee",
	"Christian Antkow",
	"Paul Jaquays",
	"Brandon James",
	"",
	"+BIZ",
	"Todd Hollenshead",
	"Barrett (Bear) Alexander",
	"Donna Jackson",
	"",
	"",
	"+SPECIAL THANKS",
	"Ben Donges for beta testing",
	"",
	"",
	"",
	"",
	"",
	"",
	"+ADDITIONAL SUPPORT",
	"",
	"+LINUX PORT AND CTF",
	"Dave \"Zoid\" Kirsch",
	"",
	"+CINEMATIC SEQUENCES",
	"Ending Cinematic by Blur Studio - ",
	"Venice, CA",
	"",
	"Environment models for Introduction",
	"Cinematic by Karl Dolgener",
	"",
	"Assistance with environment design",
	"by Cliff Iwai",
	"",
	"+SOUND EFFECTS AND MUSIC",
	"Sound Design by Soundelux Media Labs.",
	"Music Composed and Produced by",
	"Soundelux Media Labs.  Special thanks",
	"to Bill Brown, Tom Ozanich, Brian",
	"Celano, Jeff Eisner, and The Soundelux",
	"Players.",
	"",
	"\"Level Music\" by Sonic Mayhem",
	"www.sonicmayhem.com",
	"",
	"\"Quake II Theme Song\"",
	"(C) 1997 Rob Zombie. All Rights",
	"Reserved.",
	"",
	"Track 10 (\"Climb\") by Jer Sypult",
	"",
	"Voice of computers by",
	"Carly Staehlin-Taylor",
	"",
	"+THANKS TO ACTIVISION",
	"+IN PARTICULAR:",
	"",
	"John Tam",
	"Steve Rosenthal",
	"Marty Stratton",
	"Henk Hartong",
	"",
	"Quake II(tm) (C)1997 Id Software, Inc.",
	"All Rights Reserved.  Distributed by",
	"Activision, Inc. under license.",
	"Quake II(tm), the Id Software name,",
	"the \"Q II\"(tm) logo and id(tm)",
	"logo are trademarks of Id Software,",
	"Inc. Activision(R) is a registered",
	"trademark of Activision, Inc. All",
	"other trademarks and trade names are",
	"properties of their respective owners.",
	0
};

static const char *xatcredits[] =
{
	"+QUAKE II MISSION PACK: THE RECKONING",
	"+BY",
	"+XATRIX ENTERTAINMENT, INC.",
	"",
	"+DESIGN AND DIRECTION",
	"Drew Markham",
	"",
	"+PRODUCED BY",
	"Greg Goodrich",
	"",
	"+PROGRAMMING",
	"Rafael Paiz",
	"",
	"+LEVEL DESIGN / ADDITIONAL GAME DESIGN",
	"Alex Mayberry",
	"",
	"+LEVEL DESIGN",
	"Mal Blackwell",
	"Dan Koppel",
	"",
	"+ART DIRECTION",
	"Michael \"Maxx\" Kaufman",
	"",
	"+COMPUTER GRAPHICS SUPERVISOR AND",
	"+CHARACTER ANIMATION DIRECTION",
	"Barry Dempsey",
	"",
	"+SENIOR ANIMATOR AND MODELER",
	"Jason Hoover",
	"",
	"+CHARACTER ANIMATION AND",
	"+MOTION CAPTURE SPECIALIST",
	"Amit Doron",
	"",
	"+ART",
	"Claire Praderie-Markham",
	"Viktor Antonov",
	"Corky Lehmkuhl",
	"",
	"+INTRODUCTION ANIMATION",
	"Dominique Drozdz",
	"",
	"+ADDITIONAL LEVEL DESIGN",
	"Aaron Barber",
	"Rhett Baldwin",
	"",
	"+3D CHARACTER ANIMATION TOOLS",
	"Gerry Tyra, SA Technology",
	"",
	"+ADDITIONAL EDITOR TOOL PROGRAMMING",
	"Robert Duffy",
	"",
	"+ADDITIONAL PROGRAMMING",
	"Ryan Feltrin",
	"",
	"+PRODUCTION COORDINATOR",
	"Victoria Sylvester",
	"",
	"+SOUND DESIGN",
	"Gary Bradfield",
	"",
	"+MUSIC BY",
	"Sonic Mayhem",
	"",
	"",
	"",
	"+SPECIAL THANKS",
	"+TO",
	"+OUR FRIENDS AT ID SOFTWARE",
	"",
	"John Carmack",
	"John Cash",
	"Brian Hook",
	"Adrian Carmack",
	"Kevin Cloud",
	"Paul Steed",
	"Tim Willits",
	"Christian Antkow",
	"Paul Jaquays",
	"Brandon James",
	"Todd Hollenshead",
	"Barrett (Bear) Alexander",
	"Dave \"Zoid\" Kirsch",
	"Donna Jackson",
	"",
	"",
	"",
	"+THANKS TO ACTIVISION",
	"+IN PARTICULAR:",
	"",
	"Marty Stratton",
	"Henk \"The Original Ripper\" Hartong",
	"Kevin Kraff",
	"Jamey Gottlieb",
	"Chris Hepburn",
	"",
	"+AND THE GAME TESTERS",
	"",
	"Tim Vanlaw",
	"Doug Jacobs",
	"Steven Rosenthal",
	"David Baker",
	"Chris Campbell",
	"Aaron Casillas",
	"Steve Elwell",
	"Derek Johnstone",
	"Igor Krinitskiy",
	"Samantha Lee",
	"Michael Spann",
	"Chris Toft",
	"Juan Valdes",
	"",
	"+THANKS TO INTERGRAPH COMPUTER SYTEMS",
	"+IN PARTICULAR:",
	"",
	"Michael T. Nicolaou",
	"",
	"",
	"Quake II Mission Pack: The Reckoning",
	"(tm) (C)1998 Id Software, Inc. All",
	"Rights Reserved. Developed by Xatrix",
	"Entertainment, Inc. for Id Software,",
	"Inc. Distributed by Activision Inc.",
	"under license. Quake(R) is a",
	"registered trademark of Id Software,",
	"Inc. Quake II Mission Pack: The",
	"Reckoning(tm), Quake II(tm), the Id",
	"Software name, the \"Q II\"(tm) logo",
	"and id(tm) logo are trademarks of Id",
	"Software, Inc. Activision(R) is a",
	"registered trademark of Activision,",
	"Inc. Xatrix(R) is a registered",
	"trademark of Xatrix Entertainment,",
	"Inc. All other trademarks and trade",
	"names are properties of their",
	"respective owners.",
	0
};

static const char *roguecredits[] =
{
	"+QUAKE II MISSION PACK 2: GROUND ZERO",
	"+BY",
	"+ROGUE ENTERTAINMENT, INC.",
	"",
	"+PRODUCED BY",
	"Jim Molinets",
	"",
	"+PROGRAMMING",
	"Peter Mack",
	"Patrick Magruder",
	"",
	"+LEVEL DESIGN",
	"Jim Molinets",
	"Cameron Lamprecht",
	"Berenger Fish",
	"Robert Selitto",
	"Steve Tietze",
	"Steve Thoms",
	"",
	"+ART DIRECTION",
	"Rich Fleider",
	"",
	"+ART",
	"Rich Fleider",
	"Steve Maines",
	"Won Choi",
	"",
	"+ANIMATION SEQUENCES",
	"Creat Studios",
	"Steve Maines",
	"",
	"+ADDITIONAL LEVEL DESIGN",
	"Rich Fleider",
	"Steve Maines",
	"Peter Mack",
	"",
	"+SOUND",
	"James Grunke",
	"",
	"+GROUND ZERO THEME",
	"+AND",
	"+MUSIC BY",
	"Sonic Mayhem",
	"",
	"+VWEP MODELS",
	"Brent \"Hentai\" Dill",
	"",
	"",
	"",
	"+SPECIAL THANKS",
	"+TO",
	"+OUR FRIENDS AT ID SOFTWARE",
	"",
	"John Carmack",
	"John Cash",
	"Brian Hook",
	"Adrian Carmack",
	"Kevin Cloud",
	"Paul Steed",
	"Tim Willits",
	"Christian Antkow",
	"Paul Jaquays",
	"Brandon James",
	"Todd Hollenshead",
	"Barrett (Bear) Alexander",
	"Katherine Anna Kang",
	"Donna Jackson",
	"Dave \"Zoid\" Kirsch",
	"",
	"",
	"",
	"+THANKS TO ACTIVISION",
	"+IN PARTICULAR:",
	"",
	"Marty Stratton",
	"Henk Hartong",
	"Mitch Lasky",
	"Steve Rosenthal",
	"Steve Elwell",
	"",
	"+AND THE GAME TESTERS",
	"",
	"The Ranger Clan",
	"Dave \"Zoid\" Kirsch",
	"Nihilistic Software",
	"Robert Duffy",
	"",
	"And Countless Others",
	"",
	"",
	"",
	"Quake II Mission Pack 2: Ground Zero",
	"(tm) (C)1998 Id Software, Inc. All",
	"Rights Reserved. Developed by Rogue",
	"Entertainment, Inc. for Id Software,",
	"Inc. Distributed by Activision Inc.",
	"under license. Quake(R) is a",
	"registered trademark of Id Software,",
	"Inc. Quake II Mission Pack 2: Ground",
	"Zero(tm), Quake II(tm), the Id",
	"Software name, the \"Q II\"(tm) logo",
	"and id(tm) logo are trademarks of Id",
	"Software, Inc. Activision(R) is a",
	"registered trademark of Activision,",
	"Inc. Rogue(R) is a registered",
	"trademark of Rogue Entertainment,",
	"Inc. All other trademarks and trade",
	"names are properties of their",
	"respective owners.",
	0
};


void M_Credits_MenuDraw( void )
{
	int i, y;

	// Knightmare - check if start line needs to be incremented
	if ((viddef.height - ((cls.realtime - credits_start_time)/40.0F)
		+ credits_start_line * 10) < 0)
	{
		credits_start_line++;
		if (!credits[credits_start_line])
		{
			credits_start_line = 0;
			credits_start_time = cls.realtime;
		}
	}

	/*
	** draw the credits
	*/
	for ( i = credits_start_line, y = viddef.height - ((cls.realtime - credits_start_time) / 40.0F) + credits_start_line * 10;
			credits[i] && y < viddef.height; y += 10, i++ )
	{
		int j, stringoffset = 0;
		int bold = false;

		if ( y <= -8 )
			continue;

		if ( credits[i][0] == '+' )
		{
			bold = true;
			stringoffset = 1;
		}
		else
		{
			bold = false;
			stringoffset = 0;
		}

		for ( j = 0; credits[i][j+stringoffset]; j++ )
		{
			int x;

			x = ( viddef.width - strlen( credits[i] ) * 8 - stringoffset * 8 ) / 2 + ( j + stringoffset ) * 8;

			if ( bold )
				re.DrawChar( x, y, credits[i][j+stringoffset] + 128 );
			else
				re.DrawChar( x, y, credits[i][j+stringoffset] );
		}
	}

//	if ( y < 0 )
//		credits_start_time = cls.realtime;
}

const char *M_Credits_Key( int key )
{
	switch (key)
	{
	case K_ESCAPE:
		if (creditsBuffer)
			FS_FreeFile (creditsBuffer);
		M_PopMenu ();
		break;
	}

	return menu_out_sound;

}

extern int Developer_searchpath (int who);

void M_Menu_Credits_f( void )
{
	int		n;
	int		count;
	char	*p;
	int		isdeveloper = 0;

	creditsBuffer = NULL;
	count = FS_LoadFile ("credits", (void **)&creditsBuffer); /* FS: Compiler warning */
	if (count != -1)
	{
		p = creditsBuffer;
		for (n = 0; n < 255; n++)
		{
			creditsIndex[n] = p;
			while (*p != '\r' && *p != '\n')
			{
				p++;
				if (--count == 0)
					break;
			}
			if (*p == '\r')
			{
				*p++ = 0;
				if (--count == 0)
					break;
			}
			*p++ = 0;
			if (--count == 0)
				break;
		}
		creditsIndex[++n] = 0;
		credits = (const char **)creditsIndex; /* FS: Compiler warning */
	}
	else
	{
		isdeveloper = Developer_searchpath (1);
		
		if (isdeveloper == 1)			// xatrix
			credits = xatcredits;
		else if (isdeveloper == 2)		// ROGUE
			credits = roguecredits;
		else
		{
			credits = idcredits;	
		}

	}

	credits_start_time = cls.realtime;
	credits_start_line = 0; // Knightmare- allow credits to scroll past top of screen
	M_PushMenu( M_Credits_MenuDraw, M_Credits_Key);
}

/*
=============================================================================

GAME MENU

=============================================================================
*/

static int		m_game_cursor;

static menuframework_s	s_game_menu;
static menuaction_s		s_easy_game_action;
static menuaction_s		s_medium_game_action;
static menuaction_s		s_hard_game_action;
static menuaction_s		s_nightmare_game_action; /* FS: Added nightmare to start game menu */
static menuaction_s		s_load_game_action;
static menuaction_s		s_save_game_action;
static menuaction_s		s_credits_action;
static menuseparator_s	s_blankline;

static void StartGame( void )
{
	// disable updates and start the cinematic going
	cl.servercount = -1;
	M_ForceMenuOff ();
	Cvar_SetValue( "deathmatch", 0 );
	Cvar_SetValue( "coop", 0 );

	Cvar_SetValue( "gamerules", 0 );		//PGM

	Cbuf_AddText ("loading ; killserver ; wait ; newgame\n");
	cls.key_dest = key_game;
}

static void EasyGameFunc( void *data )
{
	Cvar_ForceSet( "skill", "0" );
	StartGame();
}

static void MediumGameFunc( void *data )
{
	Cvar_ForceSet( "skill", "1" );
	StartGame();
}

static void HardGameFunc( void *data )
{
	Cvar_ForceSet( "skill", "2" );
	StartGame();
}

static void NightmareGameFunc( void *data ) /* FS: Added nightmare to start game menu */
{
	Cvar_ForceSet( "skill", "3" );
	StartGame();
}

static void LoadGameFunc( void *unused )
{
	M_Menu_LoadGame_f ();
}

static void SaveGameFunc( void *unused )
{
	M_Menu_SaveGame_f();
}

static void CreditsFunc( void *unused )
{
	M_Menu_Credits_f();
}

void Game_MenuInit( void )
{
	s_game_menu.x = viddef.width * 0.50;
	s_game_menu.nitems = 0;

	s_easy_game_action.generic.type	= MTYPE_ACTION;
	s_easy_game_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_easy_game_action.generic.x		= 0;
	s_easy_game_action.generic.y		= 0;
	s_easy_game_action.generic.name	= "easy";
	s_easy_game_action.generic.callback = EasyGameFunc;

	s_medium_game_action.generic.type	= MTYPE_ACTION;
	s_medium_game_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_medium_game_action.generic.x		= 0;
	s_medium_game_action.generic.y		= 10;
	s_medium_game_action.generic.name	= "medium";
	s_medium_game_action.generic.callback = MediumGameFunc;

	s_hard_game_action.generic.type	= MTYPE_ACTION;
	s_hard_game_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_hard_game_action.generic.x		= 0;
	s_hard_game_action.generic.y		= 20;
	s_hard_game_action.generic.name	= "hard";
	s_hard_game_action.generic.callback = HardGameFunc;

	/* FS: Added nightmare to start game menu */
	s_nightmare_game_action.generic.type	= MTYPE_ACTION;
	s_nightmare_game_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_nightmare_game_action.generic.x		= 0;
	s_nightmare_game_action.generic.y		= 30;
	s_nightmare_game_action.generic.name	= "nightmare";
	s_nightmare_game_action.generic.callback = NightmareGameFunc;

	s_blankline.generic.type = MTYPE_SEPARATOR;

	s_load_game_action.generic.type	= MTYPE_ACTION;
	s_load_game_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_load_game_action.generic.x		= 0;
	s_load_game_action.generic.y		= 50;
	s_load_game_action.generic.name	= "load game";
	s_load_game_action.generic.callback = LoadGameFunc;

	s_save_game_action.generic.type	= MTYPE_ACTION;
	s_save_game_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_save_game_action.generic.x		= 0;
	s_save_game_action.generic.y		= 60;
	s_save_game_action.generic.name	= "save game";
	s_save_game_action.generic.callback = SaveGameFunc;

	s_credits_action.generic.type	= MTYPE_ACTION;
	s_credits_action.generic.flags  = QMF_LEFT_JUSTIFY;
	s_credits_action.generic.x		= 0;
	s_credits_action.generic.y		= 70;
	s_credits_action.generic.name	= "credits";
	s_credits_action.generic.callback = CreditsFunc;

	Menu_AddItem( &s_game_menu, ( void * ) &s_easy_game_action );
	Menu_AddItem( &s_game_menu, ( void * ) &s_medium_game_action );
	Menu_AddItem( &s_game_menu, ( void * ) &s_hard_game_action );
	Menu_AddItem( &s_game_menu, ( void * ) &s_nightmare_game_action ); /* FS: Added nightmare to start game menu */
	Menu_AddItem( &s_game_menu, ( void * ) &s_blankline );
	Menu_AddItem( &s_game_menu, ( void * ) &s_load_game_action );
	Menu_AddItem( &s_game_menu, ( void * ) &s_save_game_action );
	Menu_AddItem( &s_game_menu, ( void * ) &s_blankline );
	Menu_AddItem( &s_game_menu, ( void * ) &s_credits_action );

	Menu_Center( &s_game_menu );
}

void Game_MenuDraw( void )
{
	M_Banner( "m_banner_game" );
	Menu_AdjustCursor( &s_game_menu, 1 );
	Menu_Draw( &s_game_menu );
}

const char *Game_MenuKey( int key )
{
	switch(key)
	{
	/* FS: Keyboard shortcuts */
	case 'e':
	case 'E':
		M_ForceMenuOff();
		EasyGameFunc (NULL);
		break;

	case 'm':
	case 'M':
		MediumGameFunc(NULL);
		break;

	case 'h':
	case 'H':
		HardGameFunc(NULL);
		break;

	case 'n':
	case 'N':
		NightmareGameFunc(NULL);
		break;

	case 'l':
	case 'L':
		M_Menu_LoadGame_f();
		break;

	case 's':
	case 'S':
		M_Menu_SaveGame_f();
		break;

	case 'c':
	case 'C':
		M_Menu_Credits_f();
		break;
	}

	return Default_MenuKey( &s_game_menu, key );
}

void M_Menu_Game_f (void)
{
	Game_MenuInit();
	M_PushMenu( Game_MenuDraw, Game_MenuKey );
	m_game_cursor = 1;
}

/*
=============================================================================

LOADGAME MENU

=============================================================================
*/

#define	MAX_SAVEGAMES	15

static menuframework_s	s_savegame_menu;

static menuframework_s	s_loadgame_menu;
static menuaction_s		s_loadgame_actions[MAX_SAVEGAMES];

char		m_savestrings[MAX_SAVEGAMES][32];
qboolean	m_savevalid[MAX_SAVEGAMES];

void Create_Savestrings (void)
{
	int		i;
	FILE	*f;
	char	name[MAX_OSPATH];

	for (i=0 ; i<MAX_SAVEGAMES ; i++)
	{
		Com_sprintf (name, sizeof(name), "%s/save/dossv%i/server.ssv", FS_Gamedir(), i);
		f = fopen (name, "rb");
		if (!f)
		{
			strcpy (m_savestrings[i], "<EMPTY>");
			m_savevalid[i] = false;
		}
		else
		{
			FS_Read (m_savestrings[i], sizeof(m_savestrings[i]), f);
			fclose (f);
			m_savevalid[i] = true;
		}
	}
}

void LoadGameCallback( void *self )
{
	menuaction_s *a = ( menuaction_s * ) self;

	if ( m_savevalid[ a->generic.localdata[0] ] )
		Cbuf_AddText (va("load dossv%i\n",  a->generic.localdata[0] ) );
	M_ForceMenuOff ();
}

void LoadGame_MenuInit( void )
{
	int i;

	s_loadgame_menu.x = viddef.width / 2 - 120;
	s_loadgame_menu.y = viddef.height / 2 - 58;
	s_loadgame_menu.nitems = 0;

	Create_Savestrings();

	for ( i = 0; i < MAX_SAVEGAMES; i++ )
	{
		s_loadgame_actions[i].generic.name			= m_savestrings[i];
		s_loadgame_actions[i].generic.flags			= QMF_LEFT_JUSTIFY;
		s_loadgame_actions[i].generic.localdata[0]	= i;
		s_loadgame_actions[i].generic.callback		= LoadGameCallback;

		s_loadgame_actions[i].generic.x = 0;
		s_loadgame_actions[i].generic.y = ( i ) * 10;
		if (i>0)	// separate from autosave
			s_loadgame_actions[i].generic.y += 10;

		s_loadgame_actions[i].generic.type = MTYPE_ACTION;

		Menu_AddItem( &s_loadgame_menu, &s_loadgame_actions[i] );
	}
}

void LoadGame_MenuDraw( void )
{
	M_Banner( "m_banner_load_game" );
//	Menu_AdjustCursor( &s_loadgame_menu, 1 );
	Menu_Draw( &s_loadgame_menu );
}

const char *LoadGame_MenuKey( int key )
{
	if ( key == K_ESCAPE || key == K_ENTER )
	{
		s_savegame_menu.cursor = s_loadgame_menu.cursor - 1;
		if ( s_savegame_menu.cursor < 0 )
			s_savegame_menu.cursor = 0;
	}
	return Default_MenuKey( &s_loadgame_menu, key );
}

void M_Menu_LoadGame_f (void)
{
	LoadGame_MenuInit();
	M_PushMenu( LoadGame_MenuDraw, LoadGame_MenuKey );
}


/*
=============================================================================

SAVEGAME MENU

=============================================================================
*/
static menuframework_s	s_savegame_menu;
static menuaction_s		s_savegame_actions[MAX_SAVEGAMES];

void SaveGameCallback( void *self )
{
	menuaction_s *a = ( menuaction_s * ) self;

	Cbuf_AddText (va("save dossv%i\n", a->generic.localdata[0] ));
	M_ForceMenuOff ();
}

void SaveGame_MenuDraw( void )
{
	M_Banner( "m_banner_save_game" );
	Menu_AdjustCursor( &s_savegame_menu, 1 );
	Menu_Draw( &s_savegame_menu );
}

void SaveGame_MenuInit( void )
{
	int i;

	s_savegame_menu.x = viddef.width / 2 - 120;
	s_savegame_menu.y = viddef.height / 2 - 58;
	s_savegame_menu.nitems = 0;

	Create_Savestrings();

	// don't include the autosave slot
	for ( i = 0; i < MAX_SAVEGAMES-1; i++ )
	{
		s_savegame_actions[i].generic.name = m_savestrings[i+1];
		s_savegame_actions[i].generic.localdata[0] = i+1;
		s_savegame_actions[i].generic.flags = QMF_LEFT_JUSTIFY;
		s_savegame_actions[i].generic.callback = SaveGameCallback;

		s_savegame_actions[i].generic.x = 0;
		s_savegame_actions[i].generic.y = ( i ) * 10;

		s_savegame_actions[i].generic.type = MTYPE_ACTION;

		Menu_AddItem( &s_savegame_menu, &s_savegame_actions[i] );
	}
}

const char *SaveGame_MenuKey( int key )
{
	if ( key == K_ENTER || key == K_ESCAPE )
	{
		s_loadgame_menu.cursor = s_savegame_menu.cursor - 1;
		if ( s_loadgame_menu.cursor < 0 )
			s_loadgame_menu.cursor = 0;
	}
	return Default_MenuKey( &s_savegame_menu, key );
}

void M_Menu_SaveGame_f (void)
{
	if (!Com_ServerState())
		return;		// not playing a game

	SaveGame_MenuInit();
	M_PushMenu( SaveGame_MenuDraw, SaveGame_MenuKey );
	Create_Savestrings ();
}


/*
=============================================================================

JOIN SERVER MENU

=============================================================================
*/
#define MAX_LOCAL_SERVERS 12 /* FS: Was 8 -- Max 320x200 can handle */

static menuframework_s	s_joinserver_menu;
static menuseparator_s	s_joinserver_server_title;
static menuaction_s		s_joinserver_search_action;
static menuaction_s		s_joinserver_address_book_action;
static menuaction_s		s_joinserver_server_actions[MAX_LOCAL_SERVERS];

static int	m_num_servers;

#ifdef GAMESPY /* FS: GameSpy Browser */
#define MAX_GAMESPY_MENU_SERVERS MAX_SERVERS /* FS: Maximum number of servers to show in the browser */
static menuframework_s	s_joingamespyserver_menu;
static menuseparator_s	s_joingamespyserver_server_title;
static menuaction_s		s_joingamespyserver_search_action;
static menuaction_s		s_joingamespyserver_prevpage_action;
static menuaction_s		s_joingamespyserver_nextpage_action;
static menuaction_s		s_joingamespyserver_server_actions[MAX_GAMESPY_MENU_SERVERS];

static int	m_num_gamespy_servers;
static int	m_num_active_gamespy_servers;
static int	curPageScale;
static int	gspyCurPage;
static int	totalAllowedBrowserPages;
static void JoinGamespyServer_Redraw(int serverscale);
#endif

#define	NO_SERVER_STRING	"<no server>"

// user readable information
static char local_server_names[MAX_LOCAL_SERVERS][80];
#ifdef GAMESPY
static char gamespy_server_names[MAX_GAMESPY_MENU_SERVERS][80]; /* FS: GameSpy Browser */
static char gamespy_connect_string[MAX_GAMESPY_MENU_SERVERS][128]; /* FS: GameSpy Browser: Connect string */
#endif

// network address
static netadr_t local_server_netadr[MAX_LOCAL_SERVERS];

void M_AddToServerList (netadr_t adr, char *info)
{
	int		i;

	if (m_num_servers == MAX_LOCAL_SERVERS)
		return;
	while ( *info == ' ' )
		info++;

	// ignore if duplicated
	for (i=0 ; i<m_num_servers ; i++)
		if (!strcmp(info, local_server_names[i]))
			return;

	local_server_netadr[m_num_servers] = adr;
	strncpy (local_server_names[m_num_servers], info, sizeof(local_server_names[0])-1);
	m_num_servers++;
}

static void JoinServerFunc(void *self)
{
	char	buffer[128];
	int		index;

	index = ( menuaction_s * ) self - s_joinserver_server_actions;

	if ( Q_stricmp( local_server_names[index], NO_SERVER_STRING ) == 0 )
	{
		return;
	}

	if (index >= m_num_servers)
	{
		return;
	}

	Com_sprintf (buffer, sizeof(buffer), "connect %s\n", NET_AdrToString (local_server_netadr[index]));
	Cbuf_AddText (buffer);
	M_ForceMenuOff ();
}

#ifdef GAMESPY
static void ConnectGamespyServerFunc(void *self) /* FS: GameSpy Browser Connect Function */
{
	char	buffer[128];
	int		index;

	index = (( menuaction_s * ) self - s_joingamespyserver_server_actions) + curPageScale;

	if ( Q_stricmp( gamespy_server_names[index], NO_SERVER_STRING ) == 0 )
	{
		return;
	}

	if (index >= m_num_gamespy_servers)
	{
		return;
	}

	Com_sprintf (buffer, sizeof(buffer), "%s\n", gamespy_connect_string[index]);
	Cbuf_AddText (buffer);
	M_ForceMenuOff ();
}
#endif

static void AddressBookFunc(void *self)
{
	M_Menu_AddressBook_f();
}

#if 0 /* unused */
void NullCursorDraw(void *self)
{
}
#endif

#ifdef GAMESPY
static void FormatGamespyList (void)
{
	int j;
	int skip = 0;

	m_num_gamespy_servers = 0;
	m_num_active_gamespy_servers = 0;

	for (j = 0; j< MAX_SERVERS; j++)
	{
		if (m_num_gamespy_servers == MAX_GAMESPY_MENU_SERVERS)
			break;

		if ((browserList[j].hostname[0] != 0)) /* FS: Only show populated servers first */
		{
			if(browserList[j].curPlayers > 0)
			{
				if(viddef.height <= 300) /* FS: Special formatting for low res. */
				{
					char buffer[80];

					Q_strncpyz(gamespy_server_names[m_num_gamespy_servers], browserList[j].hostname, 20);

					if (strlen(browserList[j].hostname) >= 20)
					{
						strcat(gamespy_server_names[m_num_gamespy_servers], "...");
					}

					Com_sprintf(buffer, sizeof(buffer), " [%d] %d/%d", browserList[j].ping, browserList[j].curPlayers, browserList[j].maxPlayers);
					Com_strcat(gamespy_server_names[m_num_gamespy_servers], sizeof(gamespy_server_names[m_num_gamespy_servers]), buffer);
				}
				else
				{
					Com_sprintf(gamespy_server_names[m_num_gamespy_servers], sizeof(gamespy_server_names[m_num_gamespy_servers]), "%s [%d] %d/%d", browserList[j].hostname, browserList[j].ping, browserList[j].curPlayers, browserList[j].maxPlayers);
				}
				Com_sprintf(gamespy_connect_string[m_num_gamespy_servers], sizeof(gamespy_server_names[m_num_gamespy_servers]), "connect %s:%d", browserList[j].ip, browserList[j].port);
				m_num_gamespy_servers++;
				m_num_active_gamespy_servers++;
			}
		}
		else
		{
			break;
		}
	}

	j = 0;

	for(j = 0; j< MAX_SERVERS; j++)
	{
		if (m_num_gamespy_servers == MAX_GAMESPY_MENU_SERVERS)
			break;

		if ((browserListAll[j].hostname[0] != 0))
		{
			if(viddef.height <= 300) /* FS: Special formatting for low res. */
			{
				char buffer[80];

				Q_strncpyz(gamespy_server_names[m_num_gamespy_servers-skip], browserListAll[j].hostname, 20);

				if (strlen(browserListAll[j].hostname) >= 20)
				{
					strcat(gamespy_server_names[m_num_gamespy_servers-skip], "...");
				}

				Com_sprintf(buffer, sizeof(buffer), " [%d] %d/%d", browserListAll[j].ping, browserListAll[j].curPlayers, browserListAll[j].maxPlayers);
				Com_strcat(gamespy_server_names[m_num_gamespy_servers-skip], sizeof(gamespy_server_names[m_num_gamespy_servers-skip]), buffer);
			}
			else
			{
				Com_sprintf(gamespy_server_names[m_num_gamespy_servers-skip], sizeof(gamespy_server_names[m_num_gamespy_servers-skip]), "%s [%d] %d/%d", browserListAll[j].hostname, browserListAll[j].ping, browserListAll[j].curPlayers, browserListAll[j].maxPlayers);
			}
			Com_sprintf(gamespy_connect_string[m_num_gamespy_servers-skip], sizeof(gamespy_server_names[m_num_gamespy_servers-skip]), "connect %s:%d", browserListAll[j].ip, browserListAll[j].port);
			m_num_gamespy_servers++;
		}
		else
		{
			skip++;
			continue;
		}
	}
}

void Update_Gamespy_Menu (void)
{
	static char	found[64];

	FormatGamespyList();

	Com_sprintf(found, sizeof(found), "Found %d servers (%d active) in %i seconds\n", m_num_gamespy_servers, m_num_active_gamespy_servers, (((int)Sys_Milliseconds()-cls.gamespystarttime) / 1000));
	s_joingamespyserver_search_action.generic.statusbar = found;
	Com_Printf(found);
}

static void SearchGamespyGames (void)
{
	int		i;

	m_num_gamespy_servers = 0;
	for (i = 0; i < MAX_GAMESPY_MENU_SERVERS; i++)
	{
		strcpy (gamespy_server_names[i], NO_SERVER_STRING);
	}

	if(viddef.height < 300) /* FS: 400x300 can handle the longer string */
	{
		s_joingamespyserver_search_action.generic.statusbar = "Querying GameSpy. . .";
	}
	else
	{
		s_joingamespyserver_search_action.generic.statusbar = "Querying GameSpy for servers, please wait. . .";
	}

	// send out info packets
	CL_PingNetServers_f ();
}
#endif /* GAMESPY */

static void SearchLocalGames(void)
{
	int		i;

	m_num_servers = 0;
	for (i=0 ; i<MAX_LOCAL_SERVERS ; i++)
		strcpy (local_server_names[i], NO_SERVER_STRING);

	M_DrawTextBox( 8, 120 - 48, 36, 3 );
	M_Print( 16 + 16, 120 - 48 + 8,  "Searching for local servers, this" );
	M_Print( 16 + 16, 120 - 48 + 16, "could take up to a minute, so" );
	M_Print( 16 + 16, 120 - 48 + 24, "please be patient." );

	// the text box won't show up unless we do a buffer swap
	re.EndFrame();

	// send out info packets
	CL_PingServers_f();
}

static void SearchLocalGamesFunc(void *self)
{
	SearchLocalGames();
}

#ifdef GAMESPY
static void SearchGamespyGamesFunc(void *self)
{
	SearchGamespyGames();
}

static int Get_Vidscale(void)
{
	/* FS: Special function for some what scaling of the server browser depending on video resolution height. */
	if (viddef.height <= 240)
		return 18;
	if (viddef.height <= 300)
		return 21;
	if (viddef.height <= 400)
		return 25;
	if (viddef.height <= 480)
		return 30;
	if (viddef.height <= 800)
		return 36;

	return 38; /* > 800 */
}
#endif /* GAMESPY */

static void JoinServer_MenuInit(void)
{
	int i;

	s_joinserver_menu.x = viddef.width * 0.50 - 120;
	s_joinserver_menu.y = viddef.height * 0.50 - 58;
	s_joinserver_menu.nitems = 0;

	s_joinserver_address_book_action.generic.type	= MTYPE_ACTION;
	s_joinserver_address_book_action.generic.name	= "address book";
	s_joinserver_address_book_action.generic.flags	= QMF_LEFT_JUSTIFY;
	s_joinserver_address_book_action.generic.x		= 0;
	s_joinserver_address_book_action.generic.y		= 0;
	s_joinserver_address_book_action.generic.callback = AddressBookFunc;

	s_joinserver_search_action.generic.type = MTYPE_ACTION;
	s_joinserver_search_action.generic.name	= "refresh server list";
	s_joinserver_search_action.generic.flags	= QMF_LEFT_JUSTIFY;
	s_joinserver_search_action.generic.x	= 0;
	s_joinserver_search_action.generic.y	= 10;
	s_joinserver_search_action.generic.callback = SearchLocalGamesFunc;
	s_joinserver_search_action.generic.statusbar = "search for servers";

	s_joinserver_server_title.generic.type = MTYPE_SEPARATOR;
	s_joinserver_server_title.generic.name = "connect to...";
	s_joinserver_server_title.generic.x    = 80;
	s_joinserver_server_title.generic.y	   = 30;

	for ( i = 0; i < MAX_LOCAL_SERVERS; i++ )
	{
		s_joinserver_server_actions[i].generic.type	= MTYPE_ACTION;
		strcpy (local_server_names[i], NO_SERVER_STRING);
		s_joinserver_server_actions[i].generic.name	= local_server_names[i];
		s_joinserver_server_actions[i].generic.flags	= QMF_LEFT_JUSTIFY;
		s_joinserver_server_actions[i].generic.x		= 0;
		s_joinserver_server_actions[i].generic.y		= 40 + i*10;
		s_joinserver_server_actions[i].generic.callback = JoinServerFunc;
		s_joinserver_server_actions[i].generic.statusbar = "press ENTER to connect";
	}

	Menu_AddItem( &s_joinserver_menu, &s_joinserver_address_book_action );
	Menu_AddItem( &s_joinserver_menu, &s_joinserver_server_title );
	Menu_AddItem( &s_joinserver_menu, &s_joinserver_search_action );

	for ( i = 0; i < MAX_LOCAL_SERVERS; i++ )
	{
		Menu_AddItem( &s_joinserver_menu, &s_joinserver_server_actions[i] );
	}

	SearchLocalGames();
}

#ifdef GAMESPY
static void JoinGamespyServer_NextPageFunc (void *unused)
{
	int vidscale = Get_Vidscale();
	int serverscale = Get_Vidscale();

	if((gspyCurPage + 1) > totalAllowedBrowserPages)
	{
		return;
	}

	gspyCurPage++;
	serverscale = vidscale*gspyCurPage;

	JoinGamespyServer_Redraw(serverscale);
	curPageScale = serverscale;
}

static void JoinGamespyServer_PrevPageFunc (void *unused)
{
	int vidscale = Get_Vidscale();
	int serverscale = Get_Vidscale();

	if((gspyCurPage - 1) < 0)
	{
		return;
	}

	gspyCurPage--;
	serverscale = (vidscale*gspyCurPage);

	JoinGamespyServer_Redraw(serverscale);
	curPageScale = serverscale;
}

static void JoinGamespyServer_MenuInit(void)
{
	int i, vidscale;

	s_joingamespyserver_menu.x = viddef.width * 0.50 - 120;
	s_joingamespyserver_menu.y = viddef.height * 0.50 - 118;
	s_joingamespyserver_menu.nitems = 0;
	s_joingamespyserver_menu.cursor = 0; /* FS: Set the cursor at the top */

	s_joingamespyserver_search_action.generic.type = MTYPE_ACTION;
	s_joingamespyserver_search_action.generic.name	= "Query server list";
	s_joingamespyserver_search_action.generic.flags	= QMF_LEFT_JUSTIFY;
	s_joingamespyserver_search_action.generic.x	= 0;
	s_joingamespyserver_search_action.generic.y	= 0;
	s_joingamespyserver_search_action.generic.callback = SearchGamespyGamesFunc;
	s_joingamespyserver_search_action.generic.statusbar = "search for servers";

	s_joingamespyserver_server_title.generic.type = MTYPE_SEPARATOR;
	s_joingamespyserver_server_title.generic.name = "connect to...";
	s_joingamespyserver_server_title.generic.x    = 80;
	s_joingamespyserver_server_title.generic.y	   = 20;

	vidscale = Get_Vidscale();

	for (i = 0; i < MAX_GAMESPY_MENU_SERVERS; i++)
	{
		strcpy (gamespy_server_names[i], NO_SERVER_STRING);
		memset (&gamespy_connect_string, 0, sizeof(gamespy_connect_string));
	}

	totalAllowedBrowserPages = (MAX_GAMESPY_MENU_SERVERS/vidscale);
	i = 0;

	for ( i = 0; i < vidscale; i++ )
	{
		s_joingamespyserver_server_actions[i].generic.type	= MTYPE_ACTION;
		s_joingamespyserver_server_actions[i].generic.name	= gamespy_server_names[i];
		s_joingamespyserver_server_actions[i].generic.flags	= QMF_LEFT_JUSTIFY;
		s_joingamespyserver_server_actions[i].generic.x		= 0;
		s_joingamespyserver_server_actions[i].generic.y		= 30 + i*10;
		s_joingamespyserver_server_actions[i].generic.callback = ConnectGamespyServerFunc;
		s_joingamespyserver_server_actions[i].generic.statusbar = "press ENTER to connect";
	}

	Menu_AddItem( &s_joingamespyserver_menu, &s_joingamespyserver_search_action );
	Menu_AddItem( &s_joingamespyserver_menu, &s_joingamespyserver_server_title );

	i = 0;

	for ( i = 0; i < vidscale; i++ )
	{
		Menu_AddItem( &s_joingamespyserver_menu, &s_joingamespyserver_server_actions[i] );
	}

	i++;

	s_joingamespyserver_nextpage_action.generic.type = MTYPE_ACTION;
	s_joingamespyserver_nextpage_action.generic.name	= "<Next Page>";
	s_joingamespyserver_nextpage_action.generic.flags	= QMF_LEFT_JUSTIFY;
	s_joingamespyserver_nextpage_action.generic.x	= 0;
	s_joingamespyserver_nextpage_action.generic.y	= 30 + i*10;
	s_joingamespyserver_nextpage_action.generic.callback = JoinGamespyServer_NextPageFunc;
	s_joingamespyserver_nextpage_action.generic.statusbar = "continue browsing list";

	Menu_AddItem (&s_joingamespyserver_menu, &s_joingamespyserver_nextpage_action );

	curPageScale = 0;
	gspyCurPage = 0;

	FormatGamespyList(); /* FS: Incase we changed resolution or ran slist2 in the console and went back to this menu... */
}

static void JoinGamespyServer_Redraw( int serverscale )
{
	int i, vidscale;
	qboolean didBreak = false;

	s_joingamespyserver_menu.x = viddef.width * 0.50 - 120;
	s_joingamespyserver_menu.y = viddef.height * 0.50 - 118;
	s_joingamespyserver_menu.nitems = 0;
	s_joingamespyserver_menu.cursor = 0; /* FS: Set the cursor at the top */

	s_joingamespyserver_search_action.generic.type = MTYPE_ACTION;
	s_joingamespyserver_search_action.generic.name	= "Query server list";
	s_joingamespyserver_search_action.generic.flags	= QMF_LEFT_JUSTIFY;
	s_joingamespyserver_search_action.generic.x	= 0;
	s_joingamespyserver_search_action.generic.y	= 0;
	s_joingamespyserver_search_action.generic.callback = SearchGamespyGamesFunc;
	s_joingamespyserver_search_action.generic.statusbar = "search for servers";

	s_joingamespyserver_server_title.generic.type = MTYPE_SEPARATOR;
	s_joingamespyserver_server_title.generic.name = "connect to...";
	s_joingamespyserver_server_title.generic.x    = 80;
	s_joingamespyserver_server_title.generic.y	   = 20;

	vidscale = Get_Vidscale();

	for (i = 0; i < MAX_GAMESPY_MENU_SERVERS; i++)
	{
		strcpy (gamespy_server_names[i], NO_SERVER_STRING);
	}

	i = 0;

	for ( i = 0; i < vidscale; i++ )
	{
		s_joingamespyserver_server_actions[i].generic.type	= MTYPE_ACTION;
		s_joingamespyserver_server_actions[i].generic.name	= gamespy_server_names[i+serverscale];
		s_joingamespyserver_server_actions[i].generic.flags	= QMF_LEFT_JUSTIFY;
		s_joingamespyserver_server_actions[i].generic.x		= 0;
		s_joingamespyserver_server_actions[i].generic.y		= 30 + i*10;
		s_joingamespyserver_server_actions[i].generic.callback = ConnectGamespyServerFunc;
		s_joingamespyserver_server_actions[i].generic.statusbar = "press ENTER to connect";
	}

	Menu_AddItem( &s_joingamespyserver_menu, &s_joingamespyserver_search_action );
	Menu_AddItem( &s_joingamespyserver_menu, &s_joingamespyserver_server_title );

	i = 0;

	for ( i = 0; i < vidscale; i++ )
	{
		if (i+serverscale > MAX_GAMESPY_MENU_SERVERS)
		{
			didBreak = true;
			break;
		}

		Menu_AddItem( &s_joingamespyserver_menu, &s_joingamespyserver_server_actions[i] );
	}

	s_joingamespyserver_prevpage_action.generic.type = MTYPE_ACTION;
	s_joingamespyserver_prevpage_action.generic.name	= "<Previous Page>";
	s_joingamespyserver_prevpage_action.generic.flags	= QMF_LEFT_JUSTIFY;
	s_joingamespyserver_prevpage_action.generic.x	= 0;
	s_joingamespyserver_prevpage_action.generic.y	= 30 + i*10;
	s_joingamespyserver_prevpage_action.generic.callback = JoinGamespyServer_PrevPageFunc;
	s_joingamespyserver_prevpage_action.generic.statusbar = "continue browsing list";

	i++;

	s_joingamespyserver_nextpage_action.generic.type = MTYPE_ACTION;
	s_joingamespyserver_nextpage_action.generic.name	= "<Next Page>";
	s_joingamespyserver_nextpage_action.generic.flags	= QMF_LEFT_JUSTIFY;
	s_joingamespyserver_nextpage_action.generic.x	= 0;
	s_joingamespyserver_nextpage_action.generic.y	= 30 + i*10;
	s_joingamespyserver_nextpage_action.generic.callback = JoinGamespyServer_NextPageFunc;
	s_joingamespyserver_nextpage_action.generic.statusbar = "continue browsing list";

	if(serverscale)
	{
		Menu_AddItem (&s_joingamespyserver_menu, &s_joingamespyserver_prevpage_action );
	}

	if(!didBreak)
	{
		Menu_AddItem (&s_joingamespyserver_menu, &s_joingamespyserver_nextpage_action );
	}

	FormatGamespyList(); /* FS: Incase we changed resolution or ran slist2 in the console and went back to this menu... */
}
#endif

void JoinServer_MenuDraw(void)
{
	M_Banner( "m_banner_join_server" );
	Menu_Draw( &s_joinserver_menu );
}

const char *JoinServer_MenuKey( int key )
{
	return Default_MenuKey( &s_joinserver_menu, key );
}

void M_Menu_JoinServer_f (void)
{
	JoinServer_MenuInit();
	M_PushMenu( JoinServer_MenuDraw, JoinServer_MenuKey );
}

#ifdef GAMESPY
static void JoinGamespyServer_MenuDraw(void)
{
//	M_Banner( "m_banner_join_server" );
	Menu_Draw( &s_joingamespyserver_menu );
}

static const char *JoinGamespyServer_MenuKey( int key )
{
	extern	qboolean keydown[256];

	if(key == K_HOME || key == K_KP_HOME)
	{
		JoinGamespyServer_MenuInit();
		S_StartLocalSound(menu_move_sound);
		return NULL;
	}

	if(key == K_END || key == K_KP_END)
	{
		gspyCurPage = totalAllowedBrowserPages-1;
		JoinGamespyServer_NextPageFunc(NULL);
		S_StartLocalSound(menu_move_sound);
		return NULL;
	}

	if(key == K_PGDN || key == K_KP_PGDN)
	{
		JoinGamespyServer_NextPageFunc(NULL);
		S_StartLocalSound(menu_move_sound);
		return NULL;
	}

	if(key == K_PGUP || key == K_KP_PGUP)
	{
		JoinGamespyServer_PrevPageFunc(NULL);
		S_StartLocalSound(menu_move_sound);
		return NULL;
	}

	if( key == 'c' ) /* FS: Added ability to press ctrl+c to abort a GameSpy list update */
	{
		if ( keydown[K_CTRL] )
		{
			if (cls.gamespyupdate)
			{
				Cbuf_AddText ("gspystop\n");
			}
		}
	}
	return Default_MenuKey( &s_joingamespyserver_menu, key );
}

void M_Menu_JoinGamespyServer_f (void)
{
	JoinGamespyServer_MenuInit();
	M_PushMenu( JoinGamespyServer_MenuDraw, JoinGamespyServer_MenuKey );
}
#endif


/*
=============================================================================

START SERVER MENU

=============================================================================
*/
static menuframework_s s_startserver_menu;
static char **mapnames;
static int	  nummaps;

static menuaction_s	s_startserver_start_action;
static menuaction_s	s_startserver_dmoptions_action;
static menufield_s	s_timelimit_field;
static menufield_s	s_fraglimit_field;
static menufield_s	s_maxclients_field;
static menufield_s	s_hostname_field;
static menulist_s	s_startmap_list;
static menulist_s	s_rules_box;

void DMOptionsFunc( void *self )
{
	if (s_rules_box.curvalue == 1)
		return;
	M_Menu_DMOptions_f();
}

void RulesChangeFunc ( void *self )
{
	// DM
	if (s_rules_box.curvalue == 0)
	{
		s_maxclients_field.generic.statusbar = NULL;
		s_startserver_dmoptions_action.generic.statusbar = NULL;
	}
	else if(s_rules_box.curvalue == 1)		// coop				// PGM
	{
		s_maxclients_field.generic.statusbar = "4 maximum for cooperative";
		if (atoi(s_maxclients_field.buffer) > 4)
			strcpy( s_maxclients_field.buffer, "4" );
		s_startserver_dmoptions_action.generic.statusbar = "N/A for cooperative";
	}
//=====
//PGM
	// ROGUE GAMES
	else if(Developer_searchpath(2) == 2)
	{
		if (s_rules_box.curvalue == 2)			// tag	
		{
			s_maxclients_field.generic.statusbar = NULL;
			s_startserver_dmoptions_action.generic.statusbar = NULL;
		}
/*
		else if(s_rules_box.curvalue == 3)		// deathball
		{
			s_maxclients_field.generic.statusbar = NULL;
			s_startserver_dmoptions_action.generic.statusbar = NULL;
		}
*/
	}
//PGM
//=====
}

void StartServerActionFunc( void *self )
{
	char	startmap[1024];
	int		timelimit;
	int		fraglimit;
	int		maxclients;
	char	*spot;

	strcpy( startmap, strchr( mapnames[s_startmap_list.curvalue], '\n' ) + 1 );

	maxclients  = atoi( s_maxclients_field.buffer );
	timelimit	= atoi( s_timelimit_field.buffer );
	fraglimit	= atoi( s_fraglimit_field.buffer );

	Cvar_SetValue( "maxclients", ClampCvar( 0, maxclients, maxclients ) );
	Cvar_SetValue ("timelimit", ClampCvar( 0, timelimit, timelimit ) );
	Cvar_SetValue ("fraglimit", ClampCvar( 0, fraglimit, fraglimit ) );
	Cvar_Set("hostname", s_hostname_field.buffer );
//	Cvar_SetValue ("deathmatch", !s_rules_box.curvalue );
//	Cvar_SetValue ("coop", s_rules_box.curvalue );

//PGM
	if((s_rules_box.curvalue < 2) || (Developer_searchpath(2) != 2))
	{
		Cvar_SetValue ("deathmatch", !s_rules_box.curvalue );
		Cvar_SetValue ("coop", s_rules_box.curvalue );
		Cvar_SetValue ("gamerules", 0 );
	}
	else
	{
		Cvar_SetValue ("deathmatch", 1 );	// deathmatch is always true for rogue games, right?
		Cvar_SetValue ("coop", 0 );			// FIXME - this might need to depend on which game we're running
		Cvar_SetValue ("gamerules", s_rules_box.curvalue );
	}
//PGM

	spot = NULL;
	if (s_rules_box.curvalue == 1)		// PGM
	{
 		if(Q_stricmp(startmap, "bunk1") == 0)
  			spot = "start";
 		else if(Q_stricmp(startmap, "mintro") == 0)
  			spot = "start";
 		else if(Q_stricmp(startmap, "fact1") == 0)
  			spot = "start";
 		else if(Q_stricmp(startmap, "power1") == 0)
  			spot = "pstart";
 		else if(Q_stricmp(startmap, "biggun") == 0)
  			spot = "bstart";
 		else if(Q_stricmp(startmap, "hangar1") == 0)
  			spot = "unitstart";
 		else if(Q_stricmp(startmap, "city1") == 0)
  			spot = "unitstart";
 		else if(Q_stricmp(startmap, "boss1") == 0)
			spot = "bosstart";
	}

	if (spot)
	{
		if (Com_ServerState())
			Cbuf_AddText ("disconnect\n");
		Cbuf_AddText (va("gamemap \"*%s$%s\"\n", startmap, spot));
	}
	else
	{
		Cbuf_AddText (va("map %s\n", startmap));
	}

	M_ForceMenuOff ();
}

void StartServer_MenuInit( void )
{
	static const char *dm_coop_names[] =
	{
		"deathmatch",
		"cooperative",
		0
	};
//=======
//PGM
	static const char *dm_coop_names_rogue[] =
	{
		"deathmatch",
		"cooperative",
		"tag",
//		"deathball",
		0
	};
//PGM
//=======
	char *buffer;
	char  mapsname[1024];
	char *s;
	int length;
	int i;
	FILE *fp;

	/*
	** load the list of map names
	*/
	Com_sprintf( mapsname, sizeof( mapsname ), "%s/maps.lst", FS_Gamedir() );
	if ( ( fp = fopen( mapsname, "rb" ) ) == 0 )
	{
		if ( ( length = FS_LoadFile( "maps.lst", ( void ** ) &buffer ) ) == -1 )
			Com_Error( ERR_DROP, "couldn't find maps.lst\n" );
	}
	else
	{
#ifdef _WIN32
		length = filelength( fileno( fp  ) );
#else
		fseek(fp, 0, SEEK_END);
		length = ftell(fp);
		fseek(fp, 0, SEEK_SET);
#endif
		buffer = malloc( length );
		if (!buffer)
		{
			Com_Error(ERR_FATAL, "StartServer_MenuInit:  Failed to allocate memory.\n");
			return;
		}
		fread( buffer, length, 1, fp );
	}

	s = buffer;

	i = 0;
	while ( i < length )
	{
		if ( s && s[i] == '\r' )
			nummaps++;
		i++;
	}

	if (nummaps == 0)
	{
		Com_Error(ERR_DROP, "no maps in maps.lst\n");
		return;
	}

	mapnames = malloc( sizeof( char * ) * ( nummaps + 1 ) );
	if (!mapnames)
	{
		Com_Error(ERR_FATAL, "StartServer_MenuInit:  Failed to allocate memory.\n");
		return;
	}
	memset( mapnames, 0, sizeof( char * ) * ( nummaps + 1 ) );

	s = buffer;

	for ( i = 0; i < nummaps; i++ )
	{
		char  shortname[MAX_TOKEN_CHARS];
		char  longname[MAX_TOKEN_CHARS];
		char  scratch[200];

		strcpy( shortname, COM_Parse( &s ) );
		Q_strupr( shortname );
		strcpy( longname, COM_Parse( &s ) );
		Com_sprintf( scratch, sizeof( scratch ), "%s\n%s", longname, shortname );

		mapnames[i] = malloc( strlen( scratch ) + 1 );
		if (!mapnames[i])
		{
			Com_Error(ERR_FATAL, "StartServer_MenuInit:  Failed to allocate memory.\n");
			return;
		}
		strcpy( mapnames[i], scratch );
	}
	mapnames[nummaps] = 0;

	if ( fp != 0 )
	{
		fp = 0;
		free( buffer );
	}
	else
	{
		FS_FreeFile( buffer );
	}

	/*
	** initialize the menu stuff
	*/
	s_startserver_menu.x = viddef.width * 0.50;
	s_startserver_menu.y = viddef.height * 0.50 - 58;
	s_startserver_menu.nitems = 0;

	s_startmap_list.generic.type = MTYPE_SPINCONTROL;
	s_startmap_list.generic.x	= 0;
	s_startmap_list.generic.y	= 0;
	s_startmap_list.generic.name	= "initial map";
	s_startmap_list.itemnames = (const char **)mapnames; /* FS: Compiler warning */

	s_rules_box.generic.type = MTYPE_SPINCONTROL;
	s_rules_box.generic.x	= 0;
	s_rules_box.generic.y	= 20;
	s_rules_box.generic.name	= "rules";
	
//PGM - rogue games only available with rogue DLL.
	if(Developer_searchpath(2) == 2)
		s_rules_box.itemnames = dm_coop_names_rogue;
	else
		s_rules_box.itemnames = dm_coop_names;
//PGM

	if (Cvar_VariableValueInt("coop"))
		s_rules_box.curvalue = 1;
	else
		s_rules_box.curvalue = 0;
	s_rules_box.generic.callback = RulesChangeFunc;

	s_timelimit_field.generic.type = MTYPE_FIELD;
	s_timelimit_field.generic.name = "time limit";
	s_timelimit_field.generic.flags = QMF_NUMBERSONLY;
	s_timelimit_field.generic.x	= 0;
	s_timelimit_field.generic.y	= 36;
	s_timelimit_field.generic.statusbar = "0 = no limit";
	s_timelimit_field.length = 3;
	s_timelimit_field.visible_length = 3;
	strcpy( s_timelimit_field.buffer, Cvar_VariableString("timelimit") );

	s_fraglimit_field.generic.type = MTYPE_FIELD;
	s_fraglimit_field.generic.name = "frag limit";
	s_fraglimit_field.generic.flags = QMF_NUMBERSONLY;
	s_fraglimit_field.generic.x	= 0;
	s_fraglimit_field.generic.y	= 54;
	s_fraglimit_field.generic.statusbar = "0 = no limit";
	s_fraglimit_field.length = 3;
	s_fraglimit_field.visible_length = 3;
	strcpy( s_fraglimit_field.buffer, Cvar_VariableString("fraglimit") );

	/*
	** maxclients determines the maximum number of players that can join
	** the game.  If maxclients is only "1" then we should default the menu
	** option to 8 players, otherwise use whatever its current value is. 
	** Clamping will be done when the server is actually started.
	*/
	s_maxclients_field.generic.type = MTYPE_FIELD;
	s_maxclients_field.generic.name = "max players";
	s_maxclients_field.generic.flags = QMF_NUMBERSONLY;
	s_maxclients_field.generic.x	= 0;
	s_maxclients_field.generic.y	= 72;
	s_maxclients_field.generic.statusbar = NULL;
	s_maxclients_field.length = 3;
	s_maxclients_field.visible_length = 3;
	if ( Cvar_VariableValueInt( "maxclients" ) == 1 )
		strcpy( s_maxclients_field.buffer, "8" );
	else 
		strcpy( s_maxclients_field.buffer, Cvar_VariableString("maxclients") );

	s_hostname_field.generic.type = MTYPE_FIELD;
	s_hostname_field.generic.name = "hostname";
	s_hostname_field.generic.flags = 0;
	s_hostname_field.generic.x	= 0;
	s_hostname_field.generic.y	= 90;
	s_hostname_field.generic.statusbar = NULL;
	s_hostname_field.length = 12;
	s_hostname_field.visible_length = 12;
	strcpy( s_hostname_field.buffer, Cvar_VariableString("hostname") );

	s_startserver_dmoptions_action.generic.type = MTYPE_ACTION;
	s_startserver_dmoptions_action.generic.name	= " deathmatch flags";
	s_startserver_dmoptions_action.generic.flags= QMF_LEFT_JUSTIFY;
	s_startserver_dmoptions_action.generic.x	= 24;
	s_startserver_dmoptions_action.generic.y	= 108;
	s_startserver_dmoptions_action.generic.statusbar = NULL;
	s_startserver_dmoptions_action.generic.callback = DMOptionsFunc;

	s_startserver_start_action.generic.type = MTYPE_ACTION;
	s_startserver_start_action.generic.name	= " begin";
	s_startserver_start_action.generic.flags= QMF_LEFT_JUSTIFY;
	s_startserver_start_action.generic.x	= 24;
	s_startserver_start_action.generic.y	= 128;
	s_startserver_start_action.generic.callback = StartServerActionFunc;

	Menu_AddItem( &s_startserver_menu, &s_startmap_list );
	Menu_AddItem( &s_startserver_menu, &s_rules_box );
	Menu_AddItem( &s_startserver_menu, &s_timelimit_field );
	Menu_AddItem( &s_startserver_menu, &s_fraglimit_field );
	Menu_AddItem( &s_startserver_menu, &s_maxclients_field );
	Menu_AddItem( &s_startserver_menu, &s_hostname_field );
	Menu_AddItem( &s_startserver_menu, &s_startserver_dmoptions_action );
	Menu_AddItem( &s_startserver_menu, &s_startserver_start_action );

//	Menu_Center( &s_startserver_menu );

	// call this now to set proper inital state
	RulesChangeFunc ( NULL );
}

void StartServer_MenuDraw(void)
{
	M_Banner ("m_banner_start_server"); // Knightmare added banner
	Menu_Draw( &s_startserver_menu );
}

const char *StartServer_MenuKey( int key )
{
	if ( key == K_ESCAPE )
	{
		if ( mapnames )
		{
			int i;

			for ( i = 0; i < nummaps; i++ )
				free( mapnames[i] );
			free( mapnames );
		}
		mapnames = 0;
		nummaps = 0;
	}

	return Default_MenuKey( &s_startserver_menu, key );
}

void M_Menu_StartServer_f (void)
{
	StartServer_MenuInit();
	M_PushMenu( StartServer_MenuDraw, StartServer_MenuKey );
}

/*
=============================================================================

DMOPTIONS BOOK MENU

=============================================================================
*/
static char dmoptions_statusbar[128];

static menuframework_s s_dmoptions_menu;

static menulist_s	s_friendlyfire_box;
static menulist_s	s_falls_box;
static menulist_s	s_weapons_stay_box;
static menulist_s	s_instant_powerups_box;
static menulist_s	s_powerups_box;
static menulist_s	s_health_box;
static menulist_s	s_spawn_farthest_box;
static menulist_s	s_teamplay_box;
static menulist_s	s_samelevel_box;
static menulist_s	s_force_respawn_box;
static menulist_s	s_armor_box;
static menulist_s	s_allow_exit_box;
static menulist_s	s_infinite_ammo_box;
static menulist_s	s_fixed_fov_box;
static menulist_s	s_quad_drop_box;

//ROGUE
static menulist_s	s_no_mines_box;
static menulist_s	s_no_nukes_box;
static menulist_s	s_stack_double_box;
static menulist_s	s_no_spheres_box;
//ROGUE

static void DMFlagCallback( void *self )
{
	menulist_s *f = ( menulist_s * ) self;
	int flags;
	int bit = 0;

	flags = Cvar_VariableValue( "dmflags" );

	if ( f == &s_friendlyfire_box )
	{
		if ( f->curvalue )
			flags &= ~DF_NO_FRIENDLY_FIRE;
		else
			flags |= DF_NO_FRIENDLY_FIRE;
		goto setvalue;
	}
	else if ( f == &s_falls_box )
	{
		if ( f->curvalue )
			flags &= ~DF_NO_FALLING;
		else
			flags |= DF_NO_FALLING;
		goto setvalue;
	}
	else if ( f == &s_weapons_stay_box ) 
	{
		bit = DF_WEAPONS_STAY;
	}
	else if ( f == &s_instant_powerups_box )
	{
		bit = DF_INSTANT_ITEMS;
	}
	else if ( f == &s_allow_exit_box )
	{
		bit = DF_ALLOW_EXIT;
	}
	else if ( f == &s_powerups_box )
	{
		if ( f->curvalue )
			flags &= ~DF_NO_ITEMS;
		else
			flags |= DF_NO_ITEMS;
		goto setvalue;
	}
	else if ( f == &s_health_box )
	{
		if ( f->curvalue )
			flags &= ~DF_NO_HEALTH;
		else
			flags |= DF_NO_HEALTH;
		goto setvalue;
	}
	else if ( f == &s_spawn_farthest_box )
	{
		bit = DF_SPAWN_FARTHEST;
	}
	else if ( f == &s_teamplay_box )
	{
		if ( f->curvalue == 1 )
		{
			flags |=  DF_SKINTEAMS;
			flags &= ~DF_MODELTEAMS;
		}
		else if ( f->curvalue == 2 )
		{
			flags |=  DF_MODELTEAMS;
			flags &= ~DF_SKINTEAMS;
		}
		else
		{
			flags &= ~( DF_MODELTEAMS | DF_SKINTEAMS );
		}

		goto setvalue;
	}
	else if ( f == &s_samelevel_box )
	{
		bit = DF_SAME_LEVEL;
	}
	else if ( f == &s_force_respawn_box )
	{
		bit = DF_FORCE_RESPAWN;
	}
	else if ( f == &s_armor_box )
	{
		if ( f->curvalue )
			flags &= ~DF_NO_ARMOR;
		else
			flags |= DF_NO_ARMOR;
		goto setvalue;
	}
	else if ( f == &s_infinite_ammo_box )
	{
		bit = DF_INFINITE_AMMO;
	}
	else if ( f == &s_fixed_fov_box )
	{
		bit = DF_FIXED_FOV;
	}
	else if ( f == &s_quad_drop_box )
	{
		bit = DF_QUAD_DROP;
	}

//=======
//ROGUE
	else if (Developer_searchpath(2) == 2)
	{
		if ( f == &s_no_mines_box)
		{
			bit = DF_NO_MINES;
		}
		else if ( f == &s_no_nukes_box)
		{
			bit = DF_NO_NUKES;
		}
		else if ( f == &s_stack_double_box)
		{
			bit = DF_NO_STACK_DOUBLE;
		}
		else if ( f == &s_no_spheres_box)
		{
			bit = DF_NO_SPHERES;
		}
	}
//ROGUE
//=======

	if ( f )
	{
		if ( f->curvalue == 0 )
			flags &= ~bit;
		else
			flags |= bit;
	}

setvalue:
	Cvar_SetValue ("dmflags", flags);

	Com_sprintf( dmoptions_statusbar, sizeof( dmoptions_statusbar ), "dmflags = %d", flags );

}

void DMOptions_MenuInit( void )
{
	static const char *yes_no_names[] =
	{
		"no", "yes", 0
	};
	static const char *teamplay_names[] = 
	{
		"disabled", "by skin", "by model", 0
	};
	int dmflags = Cvar_VariableValue( "dmflags" );
	int y = 0;

	s_dmoptions_menu.x = viddef.width * 0.50;
	s_dmoptions_menu.y = viddef.height * 0.50 - 58;
	s_dmoptions_menu.nitems = 0;

	s_falls_box.generic.type = MTYPE_SPINCONTROL;
	s_falls_box.generic.x	= 0;
	s_falls_box.generic.y	= y;
	s_falls_box.generic.name	= "falling damage";
	s_falls_box.generic.callback = DMFlagCallback;
	s_falls_box.itemnames = yes_no_names;
	s_falls_box.curvalue = ( dmflags & DF_NO_FALLING ) == 0;

	s_weapons_stay_box.generic.type = MTYPE_SPINCONTROL;
	s_weapons_stay_box.generic.x	= 0;
	s_weapons_stay_box.generic.y	= y += 10;
	s_weapons_stay_box.generic.name	= "weapons stay";
	s_weapons_stay_box.generic.callback = DMFlagCallback;
	s_weapons_stay_box.itemnames = yes_no_names;
	s_weapons_stay_box.curvalue = ( dmflags & DF_WEAPONS_STAY ) != 0;

	s_instant_powerups_box.generic.type = MTYPE_SPINCONTROL;
	s_instant_powerups_box.generic.x	= 0;
	s_instant_powerups_box.generic.y	= y += 10;
	s_instant_powerups_box.generic.name	= "instant powerups";
	s_instant_powerups_box.generic.callback = DMFlagCallback;
	s_instant_powerups_box.itemnames = yes_no_names;
	s_instant_powerups_box.curvalue = ( dmflags & DF_INSTANT_ITEMS ) != 0;

	s_powerups_box.generic.type = MTYPE_SPINCONTROL;
	s_powerups_box.generic.x	= 0;
	s_powerups_box.generic.y	= y += 10;
	s_powerups_box.generic.name	= "allow powerups";
	s_powerups_box.generic.callback = DMFlagCallback;
	s_powerups_box.itemnames = yes_no_names;
	s_powerups_box.curvalue = ( dmflags & DF_NO_ITEMS ) == 0;

	s_health_box.generic.type = MTYPE_SPINCONTROL;
	s_health_box.generic.x	= 0;
	s_health_box.generic.y	= y += 10;
	s_health_box.generic.callback = DMFlagCallback;
	s_health_box.generic.name	= "allow health";
	s_health_box.itemnames = yes_no_names;
	s_health_box.curvalue = ( dmflags & DF_NO_HEALTH ) == 0;

	s_armor_box.generic.type = MTYPE_SPINCONTROL;
	s_armor_box.generic.x	= 0;
	s_armor_box.generic.y	= y += 10;
	s_armor_box.generic.name	= "allow armor";
	s_armor_box.generic.callback = DMFlagCallback;
	s_armor_box.itemnames = yes_no_names;
	s_armor_box.curvalue = ( dmflags & DF_NO_ARMOR ) == 0;

	s_spawn_farthest_box.generic.type = MTYPE_SPINCONTROL;
	s_spawn_farthest_box.generic.x	= 0;
	s_spawn_farthest_box.generic.y	= y += 10;
	s_spawn_farthest_box.generic.name	= "spawn farthest";
	s_spawn_farthest_box.generic.callback = DMFlagCallback;
	s_spawn_farthest_box.itemnames = yes_no_names;
	s_spawn_farthest_box.curvalue = ( dmflags & DF_SPAWN_FARTHEST ) != 0;

	s_samelevel_box.generic.type = MTYPE_SPINCONTROL;
	s_samelevel_box.generic.x	= 0;
	s_samelevel_box.generic.y	= y += 10;
	s_samelevel_box.generic.name	= "same map";
	s_samelevel_box.generic.callback = DMFlagCallback;
	s_samelevel_box.itemnames = yes_no_names;
	s_samelevel_box.curvalue = ( dmflags & DF_SAME_LEVEL ) != 0;

	s_force_respawn_box.generic.type = MTYPE_SPINCONTROL;
	s_force_respawn_box.generic.x	= 0;
	s_force_respawn_box.generic.y	= y += 10;
	s_force_respawn_box.generic.name	= "force respawn";
	s_force_respawn_box.generic.callback = DMFlagCallback;
	s_force_respawn_box.itemnames = yes_no_names;
	s_force_respawn_box.curvalue = ( dmflags & DF_FORCE_RESPAWN ) != 0;

	s_teamplay_box.generic.type = MTYPE_SPINCONTROL;
	s_teamplay_box.generic.x	= 0;
	s_teamplay_box.generic.y	= y += 10;
	s_teamplay_box.generic.name	= "teamplay";
	s_teamplay_box.generic.callback = DMFlagCallback;
	s_teamplay_box.itemnames = teamplay_names;

	s_allow_exit_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_exit_box.generic.x	= 0;
	s_allow_exit_box.generic.y	= y += 10;
	s_allow_exit_box.generic.name	= "allow exit";
	s_allow_exit_box.generic.callback = DMFlagCallback;
	s_allow_exit_box.itemnames = yes_no_names;
	s_allow_exit_box.curvalue = ( dmflags & DF_ALLOW_EXIT ) != 0;

	s_infinite_ammo_box.generic.type = MTYPE_SPINCONTROL;
	s_infinite_ammo_box.generic.x	= 0;
	s_infinite_ammo_box.generic.y	= y += 10;
	s_infinite_ammo_box.generic.name	= "infinite ammo";
	s_infinite_ammo_box.generic.callback = DMFlagCallback;
	s_infinite_ammo_box.itemnames = yes_no_names;
	s_infinite_ammo_box.curvalue = ( dmflags & DF_INFINITE_AMMO ) != 0;

	s_fixed_fov_box.generic.type = MTYPE_SPINCONTROL;
	s_fixed_fov_box.generic.x	= 0;
	s_fixed_fov_box.generic.y	= y += 10;
	s_fixed_fov_box.generic.name	= "fixed FOV";
	s_fixed_fov_box.generic.callback = DMFlagCallback;
	s_fixed_fov_box.itemnames = yes_no_names;
	s_fixed_fov_box.curvalue = ( dmflags & DF_FIXED_FOV ) != 0;

	s_quad_drop_box.generic.type = MTYPE_SPINCONTROL;
	s_quad_drop_box.generic.x	= 0;
	s_quad_drop_box.generic.y	= y += 10;
	s_quad_drop_box.generic.name	= "quad drop";
	s_quad_drop_box.generic.callback = DMFlagCallback;
	s_quad_drop_box.itemnames = yes_no_names;
	s_quad_drop_box.curvalue = ( dmflags & DF_QUAD_DROP ) != 0;

	s_friendlyfire_box.generic.type = MTYPE_SPINCONTROL;
	s_friendlyfire_box.generic.x	= 0;
	s_friendlyfire_box.generic.y	= y += 10;
	s_friendlyfire_box.generic.name	= "friendly fire";
	s_friendlyfire_box.generic.callback = DMFlagCallback;
	s_friendlyfire_box.itemnames = yes_no_names;
	s_friendlyfire_box.curvalue = ( dmflags & DF_NO_FRIENDLY_FIRE ) == 0;

//============
//ROGUE
	if(Developer_searchpath(2) == 2)
	{
		s_no_mines_box.generic.type = MTYPE_SPINCONTROL;
		s_no_mines_box.generic.x	= 0;
		s_no_mines_box.generic.y	= y += 10;
		s_no_mines_box.generic.name	= "remove mines";
		s_no_mines_box.generic.callback = DMFlagCallback;
		s_no_mines_box.itemnames = yes_no_names;
		s_no_mines_box.curvalue = ( dmflags & DF_NO_MINES ) != 0;

		s_no_nukes_box.generic.type = MTYPE_SPINCONTROL;
		s_no_nukes_box.generic.x	= 0;
		s_no_nukes_box.generic.y	= y += 10;
		s_no_nukes_box.generic.name	= "remove nukes";
		s_no_nukes_box.generic.callback = DMFlagCallback;
		s_no_nukes_box.itemnames = yes_no_names;
		s_no_nukes_box.curvalue = ( dmflags & DF_NO_NUKES ) != 0;

		s_stack_double_box.generic.type = MTYPE_SPINCONTROL;
		s_stack_double_box.generic.x	= 0;
		s_stack_double_box.generic.y	= y += 10;
		s_stack_double_box.generic.name	= "2x/4x stacking off";
		s_stack_double_box.generic.callback = DMFlagCallback;
		s_stack_double_box.itemnames = yes_no_names;
		s_stack_double_box.curvalue = ( dmflags & DF_NO_STACK_DOUBLE ) != 0;

		s_no_spheres_box.generic.type = MTYPE_SPINCONTROL;
		s_no_spheres_box.generic.x	= 0;
		s_no_spheres_box.generic.y	= y += 10;
		s_no_spheres_box.generic.name	= "remove spheres";
		s_no_spheres_box.generic.callback = DMFlagCallback;
		s_no_spheres_box.itemnames = yes_no_names;
		s_no_spheres_box.curvalue = ( dmflags & DF_NO_SPHERES ) != 0;

	}
//ROGUE
//============

	Menu_AddItem( &s_dmoptions_menu, &s_falls_box );
	Menu_AddItem( &s_dmoptions_menu, &s_weapons_stay_box );
	Menu_AddItem( &s_dmoptions_menu, &s_instant_powerups_box );
	Menu_AddItem( &s_dmoptions_menu, &s_powerups_box );
	Menu_AddItem( &s_dmoptions_menu, &s_health_box );
	Menu_AddItem( &s_dmoptions_menu, &s_armor_box );
	Menu_AddItem( &s_dmoptions_menu, &s_spawn_farthest_box );
	Menu_AddItem( &s_dmoptions_menu, &s_samelevel_box );
	Menu_AddItem( &s_dmoptions_menu, &s_force_respawn_box );
	Menu_AddItem( &s_dmoptions_menu, &s_teamplay_box );
	Menu_AddItem( &s_dmoptions_menu, &s_allow_exit_box );
	Menu_AddItem( &s_dmoptions_menu, &s_infinite_ammo_box );
	Menu_AddItem( &s_dmoptions_menu, &s_fixed_fov_box );
	Menu_AddItem( &s_dmoptions_menu, &s_quad_drop_box );
	Menu_AddItem( &s_dmoptions_menu, &s_friendlyfire_box );

//=======
//ROGUE
	if(Developer_searchpath(2) == 2)
	{
		Menu_AddItem( &s_dmoptions_menu, &s_no_mines_box );
		Menu_AddItem( &s_dmoptions_menu, &s_no_nukes_box );
		Menu_AddItem( &s_dmoptions_menu, &s_stack_double_box );
		Menu_AddItem( &s_dmoptions_menu, &s_no_spheres_box );
	}
//ROGUE
//=======

//	Menu_Center( &s_dmoptions_menu );

	// set the original dmflags statusbar
	DMFlagCallback( 0 );
	Menu_SetStatusBar( &s_dmoptions_menu, dmoptions_statusbar );
}

void DMOptions_MenuDraw(void)
{
	M_Banner ("m_banner_multiplayer"); // Knightmare added banner
	Menu_Draw( &s_dmoptions_menu );
}

const char *DMOptions_MenuKey( int key )
{
	return Default_MenuKey( &s_dmoptions_menu, key );
}

void M_Menu_DMOptions_f (void)
{
	DMOptions_MenuInit();
	M_PushMenu( DMOptions_MenuDraw, DMOptions_MenuKey );
}

/*
=============================================================================

DOWNLOADOPTIONS BOOK MENU

=============================================================================
*/
static menuframework_s s_downloadoptions_menu;

static menuseparator_s	s_download_title;
static menulist_s	s_allow_download_box;
#ifdef USE_CURL
static menulist_s	s_allow_download_http_box; /* FS: Added option for HTTP downloading */
#endif
static menulist_s	s_allow_download_maps_box;
static menulist_s	s_allow_download_models_box;
static menulist_s	s_allow_download_players_box;
static menulist_s	s_allow_download_sounds_box;

static void DownloadCallback( void *self )
{
	menulist_s *f = ( menulist_s * ) self;

	if (f == &s_allow_download_box)
	{
		Cvar_SetValue("allow_download", f->curvalue);
	}
#ifdef USE_CURL
	else if (f == &s_allow_download_http_box)
	{
		Cvar_SetValue("cl_http_downloads", f->curvalue);
	}
#endif
	else if (f == &s_allow_download_maps_box)
	{
		Cvar_SetValue("allow_download_maps", f->curvalue);
	}

	else if (f == &s_allow_download_models_box)
	{
		Cvar_SetValue("allow_download_models", f->curvalue);
	}

	else if (f == &s_allow_download_players_box)
	{
		Cvar_SetValue("allow_download_players", f->curvalue);
	}

	else if (f == &s_allow_download_sounds_box)
	{
		Cvar_SetValue("allow_download_sounds", f->curvalue);
	}
}

void DownloadOptions_MenuInit( void )
{
	static const char *yes_no_names[] =
	{
		"no", "yes", 0
	};
	int y = 0;

	s_downloadoptions_menu.x = viddef.width * 0.50;
	s_downloadoptions_menu.y = viddef.height * 0.50 - 58;
	s_downloadoptions_menu.nitems = 0;

	s_download_title.generic.type = MTYPE_SEPARATOR;
	s_download_title.generic.name = "Download Options";
	s_download_title.generic.x    = 48;
	s_download_title.generic.y	 = y;

	s_allow_download_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_box.generic.x	= 0;
	s_allow_download_box.generic.y	= y += 20;
	s_allow_download_box.generic.name	= "allow downloading";
	s_allow_download_box.generic.callback = DownloadCallback;
	s_allow_download_box.itemnames = yes_no_names;
	s_allow_download_box.curvalue = (Cvar_VariableValue("allow_download") != 0);

#ifdef USE_CURL
	s_allow_download_http_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_http_box.generic.x	= 0;
	s_allow_download_http_box.generic.y	= y += 20;
	s_allow_download_http_box.generic.name	= "http downloading";
	s_allow_download_http_box.generic.callback = DownloadCallback;
	s_allow_download_http_box.itemnames = yes_no_names;
	s_allow_download_http_box.curvalue = (Cvar_VariableValue("cl_http_downloads") != 0);
#else
	y += 10;
#endif

	s_allow_download_maps_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_maps_box.generic.x	= 0;
	s_allow_download_maps_box.generic.y	= y += 10;
	s_allow_download_maps_box.generic.name	= "maps";
	s_allow_download_maps_box.generic.callback = DownloadCallback;
	s_allow_download_maps_box.itemnames = yes_no_names;
	s_allow_download_maps_box.curvalue = (Cvar_VariableValue("allow_download_maps") != 0);

	s_allow_download_players_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_players_box.generic.x	= 0;
	s_allow_download_players_box.generic.y	= y += 10;
	s_allow_download_players_box.generic.name	= "player models/skins";
	s_allow_download_players_box.generic.callback = DownloadCallback;
	s_allow_download_players_box.itemnames = yes_no_names;
	s_allow_download_players_box.curvalue = (Cvar_VariableValue("allow_download_players") != 0);

	s_allow_download_models_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_models_box.generic.x	= 0;
	s_allow_download_models_box.generic.y	= y += 10;
	s_allow_download_models_box.generic.name	= "models";
	s_allow_download_models_box.generic.callback = DownloadCallback;
	s_allow_download_models_box.itemnames = yes_no_names;
	s_allow_download_models_box.curvalue = (Cvar_VariableValue("allow_download_models") != 0);

	s_allow_download_sounds_box.generic.type = MTYPE_SPINCONTROL;
	s_allow_download_sounds_box.generic.x	= 0;
	s_allow_download_sounds_box.generic.y	= y += 10;
	s_allow_download_sounds_box.generic.name	= "sounds";
	s_allow_download_sounds_box.generic.callback = DownloadCallback;
	s_allow_download_sounds_box.itemnames = yes_no_names;
	s_allow_download_sounds_box.curvalue = (Cvar_VariableValue("allow_download_sounds") != 0);

	Menu_AddItem( &s_downloadoptions_menu, &s_download_title );
	Menu_AddItem( &s_downloadoptions_menu, &s_allow_download_box );

#ifdef USE_CURL
	Menu_AddItem( &s_downloadoptions_menu, &s_allow_download_http_box );
#endif

	Menu_AddItem( &s_downloadoptions_menu, &s_allow_download_maps_box );
	Menu_AddItem( &s_downloadoptions_menu, &s_allow_download_players_box );
	Menu_AddItem( &s_downloadoptions_menu, &s_allow_download_models_box );
	Menu_AddItem( &s_downloadoptions_menu, &s_allow_download_sounds_box );

	Menu_Center( &s_downloadoptions_menu );

	// skip over title
	if (s_downloadoptions_menu.cursor == 0)
	{
		s_downloadoptions_menu.cursor = 1;
	}
}

void DownloadOptions_MenuDraw(void)
{
	M_Banner ("m_banner_multiplayer");
	Menu_Draw( &s_downloadoptions_menu );
}

const char *DownloadOptions_MenuKey( int key )
{
	return Default_MenuKey( &s_downloadoptions_menu, key );
}

void M_Menu_DownloadOptions_f (void)
{
	DownloadOptions_MenuInit();
	M_PushMenu( DownloadOptions_MenuDraw, DownloadOptions_MenuKey );
}
/*
=============================================================================

ADDRESS BOOK MENU

=============================================================================
*/
#define NUM_ADDRESSBOOK_ENTRIES 9

static menuframework_s	s_addressbook_menu;
static menufield_s		s_addressbook_fields[NUM_ADDRESSBOOK_ENTRIES];

void AddressBook_MenuInit( void )
{
	int i;

	s_addressbook_menu.x = viddef.width / 2 - 142;
	s_addressbook_menu.y = viddef.height / 2 - 58;
	s_addressbook_menu.nitems = 0;

	for ( i = 0; i < NUM_ADDRESSBOOK_ENTRIES; i++ )
	{
		cvar_t *adr;
		char buffer[20];

		Com_sprintf( buffer, sizeof( buffer ), "adr%d", i );

		adr = Cvar_Get( buffer, "", CVAR_ARCHIVE );

		s_addressbook_fields[i].generic.type = MTYPE_FIELD;
		s_addressbook_fields[i].generic.name = 0;
		s_addressbook_fields[i].generic.callback = 0;
		s_addressbook_fields[i].generic.x		= 0;
		s_addressbook_fields[i].generic.y		= i * 18 + 0;
		s_addressbook_fields[i].generic.localdata[0] = i;
		s_addressbook_fields[i].cursor			= 0;
		s_addressbook_fields[i].length			= 60;
		s_addressbook_fields[i].visible_length	= 30;

		strcpy( s_addressbook_fields[i].buffer, adr->string );

		Menu_AddItem( &s_addressbook_menu, &s_addressbook_fields[i] );
	}
}

const char *AddressBook_MenuKey( int key )
{
	if ( key == K_ESCAPE )
	{
		int index;
		char buffer[20];

		for ( index = 0; index < NUM_ADDRESSBOOK_ENTRIES; index++ )
		{
			Com_sprintf( buffer, sizeof( buffer ), "adr%d", index );
			Cvar_Set( buffer, s_addressbook_fields[index].buffer );
		}
	}
	return Default_MenuKey( &s_addressbook_menu, key );
}

void AddressBook_MenuDraw(void)
{
	M_Banner( "m_banner_addressbook" );
	Menu_Draw( &s_addressbook_menu );
}

void M_Menu_AddressBook_f(void)
{
	AddressBook_MenuInit();
	M_PushMenu( AddressBook_MenuDraw, AddressBook_MenuKey );
}

/*
=============================================================================

PLAYER CONFIG MENU

=============================================================================
*/
static menuframework_s	s_player_config_menu;
static menufield_s		s_player_name_field;
static menulist_s		s_player_model_box;
static menulist_s		s_player_skin_box;
static menulist_s		s_player_handedness_box;
static menulist_s		s_player_rate_box;
static menuseparator_s	s_player_skin_title;
static menuseparator_s	s_player_model_title;
static menuseparator_s	s_player_hand_title;
static menuseparator_s	s_player_rate_title;
static menuaction_s		s_player_download_action;

#define MAX_DISPLAYNAME 16
#define MAX_PLAYERMODELS 1024

typedef struct
{
	int		nskins;
	char	**skindisplaynames;
	char	displayname[MAX_DISPLAYNAME];
	char	directory[MAX_QPATH];
} playermodelinfo_s;

static playermodelinfo_s s_pmi[MAX_PLAYERMODELS];
static char *s_pmnames[MAX_PLAYERMODELS];
static int s_numplayermodels;

static int rate_tbl[] = { 2500, 3200, 5000, 10000, 25000, 0 };
static const char *rate_names[] = { "28.8 Modem", "33.6 Modem", "Single ISDN",
	"Dual ISDN/Cable", "T1/LAN", "User defined", 0 };

void DownloadOptionsFunc( void *self )
{
	M_Menu_DownloadOptions_f();
}

static void HandednessCallback( void *unused )
{
	Cvar_SetValue( "hand", s_player_handedness_box.curvalue );
}

static void RateCallback( void *unused )
{
	if (s_player_rate_box.curvalue != sizeof(rate_tbl) / sizeof(*rate_tbl) - 1)
		Cvar_SetValue( "rate", rate_tbl[s_player_rate_box.curvalue] );
}

static void ModelCallback( void *unused )
{
	s_player_skin_box.itemnames = (const char **)s_pmi[s_player_model_box.curvalue].skindisplaynames; /* FS: Compiler warning */
	s_player_skin_box.curvalue = 0;
}

static void FreeFileList( char **list, int n )
{
	int i;

	for ( i = 0; i < n; i++ )
	{
		if ( list[i] )
		{
			free( list[i] );
			list[i] = 0;
		}
	}
	free( list );
}

static qboolean IconOfSkinExists( char *skin, char **pcxfiles, int npcxfiles )
{
	int i;
	char scratch[1024];

	strcpy( scratch, skin );
	*strrchr( scratch, '.' ) = 0;
	strcat( scratch, "_i.pcx" );

	for ( i = 0; i < npcxfiles; i++ )
	{
		if ( strcmp( pcxfiles[i], scratch ) == 0 )
			return true;
	}

	return false;
}

static qboolean IsValidSkin (char **filelist, int numFiles, int index)
{
	int		len = strlen(filelist[index]);

	if ( !strcmp (filelist[index]+max(len-4,0), ".pcx") )
	{
		if ( strcmp (filelist[index]+max(len-6,0), "_i.pcx") != 0 )
		{
			if ( IconOfSkinExists (filelist[index], filelist, numFiles-1) )
			{
				return true;
			}
		}
	}
	return false;
}

static qboolean PlayerConfig_ScanDirectories( void )
{
	char findname[1024];
	char scratch[1024];
	int ndirs = 0, npms = 0;
	char **dirnames;
	char *path = NULL;
	int i;

	extern char **FS_ListFiles( char *, int *, unsigned, unsigned );

	s_numplayermodels = 0;

	// Knightmare- Loop back to here as long as there is another path to search.
	// This fixes the bug with patched player models in a mod's gamedir.
	do
	{
		/*
		** get a list of directories
		*/
		do 
		{
			path = FS_NextPath( path );
			Com_sprintf( findname, sizeof(findname), "%s/players/*.*", path );

			if ( ( dirnames = FS_ListFiles( findname, &ndirs, SFF_SUBDIR, 0 ) ) != 0 )
			{
				break;
			}
		}
		while ( path );

		if ( !dirnames )
		{
			return false;
		}

		/*
		** go through the subdirectories
		*/
		npms = ndirs;
		if ( npms > MAX_PLAYERMODELS )
		{
			npms = MAX_PLAYERMODELS;
		}
		if ( (s_numplayermodels + npms) > MAX_PLAYERMODELS ) // Knightmare added
		{
			npms = MAX_PLAYERMODELS - s_numplayermodels;
		}

		for ( i = 0; i < npms; i++ )
		{
			int			k, s;
			char		*a, *b, *c;
			char		**pcxnames;
			char		**skinnames;
			int			npcxfiles;
			int			nskins = 0;
			qboolean	already_added = false;	

			if ( dirnames[i] == 0 )
				continue;

			// Knightmare- check if dirnames[i] is already added to the ui_pmi[i].directory list
			a = strrchr(dirnames[i], '/');
			b = strrchr(dirnames[i], '\\');
			c = (a > b) ? a : b;
			for (k=0; k < s_numplayermodels; k++)
				if (!strcmp(s_pmi[k].directory, c+1))
				{	already_added = true;	break;	}
			if (already_added)
			{	// todo: add any skins for this model not already listed to skinDisplayNames
				continue;
			}
			// end Knightmare

			// verify the existence of tris.md2
			strcpy( scratch, dirnames[i] );
			strcat( scratch, "/tris.md2" );
			if ( !Sys_FindFirst( scratch, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM ) )
			{
				free( dirnames[i] );
				dirnames[i] = 0;
				Sys_FindClose();
				continue;
			}
			Sys_FindClose();

			// verify the existence of at least one pcx skin
			strcpy( scratch, dirnames[i] );
			strcat( scratch, "/*.pcx" );
			pcxnames = FS_ListFiles( scratch, &npcxfiles, 0, SFF_SUBDIR | SFF_HIDDEN | SFF_SYSTEM );

			if ( !pcxnames )
			{
				free( dirnames[i] );
				dirnames[i] = 0;
				continue;
			}

			// count valid skins, which consist of a skin with a matching "_i" icon
			for ( k = 0; k < npcxfiles-1; k++ )
				if ( IsValidSkin(pcxnames, npcxfiles, k) )
					nskins++;
		/*	{
				if ( !strstr( pcxnames[k], "_i.pcx" ) )
				{
					if ( IconOfSkinExists( pcxnames[k], pcxnames, npcxfiles - 1 ) )
					{
						nskins++;
					}
				}
			}*/
			if ( !nskins )
				continue;

			skinnames = malloc( sizeof( char * ) * ( nskins + 1 ) );
			if (!skinnames)
			{
				Com_Error(ERR_FATAL, "PlayerConfig_ScanDirectories:  Failed to allocate memory.\n");
				return false;
			}
			memset( skinnames, 0, sizeof( char * ) * ( nskins + 1 ) );

			// copy the valid skins
			for ( s = 0, k = 0; k < npcxfiles-1; k++ )
			{
				char *a, *b, *c;

				if ( !strstr( pcxnames[k], "_i.pcx" ) )
				{
					if ( IconOfSkinExists( pcxnames[k], pcxnames, npcxfiles - 1 ) )
					{
						a = strrchr( pcxnames[k], '/' );
						b = strrchr( pcxnames[k], '\\' );
						c = (a > b) ? a : b;

						strcpy( scratch, c + 1 );

						if ( strrchr( scratch, '.' ) )
							*strrchr( scratch, '.' ) = 0;

						skinnames[s] = strdup( scratch );
						s++;
					}
				}
			}

			// at this point we have a valid player model
			s_pmi[s_numplayermodels].nskins = nskins;
			s_pmi[s_numplayermodels].skindisplaynames = skinnames;

			// make short name for the model
			a = strrchr( dirnames[i], '/' );
			b = strrchr( dirnames[i], '\\' );
			c = (a > b) ? a : b;

			strncpy( s_pmi[s_numplayermodels].displayname, c + 1, MAX_DISPLAYNAME-1 );
			strcpy( s_pmi[s_numplayermodels].directory, c + 1 );

			FreeFileList( pcxnames, npcxfiles );

			s_numplayermodels++;
		}
		if ( dirnames )
			FreeFileList( dirnames, ndirs );

	// If no valid player models found in path,
	// try next path, if there is one.
	} while (path);

	return true;	//** DMP warning fix
}

static int pmicmpfnc( const void *_a, const void *_b )
{
	const playermodelinfo_s *a = ( const playermodelinfo_s * ) _a;
	const playermodelinfo_s *b = ( const playermodelinfo_s * ) _b;

	/*
	** sort by male, female, then alphabetical
	*/
	if ( strcmp( a->directory, "male" ) == 0 )
		return -1;
	else if ( strcmp( b->directory, "male" ) == 0 )
		return 1;

	if ( strcmp( a->directory, "female" ) == 0 )
		return -1;
	else if ( strcmp( b->directory, "female" ) == 0 )
		return 1;

	return strcmp( a->directory, b->directory );
}


qboolean PlayerConfig_MenuInit( void )
{
	extern cvar_t *name;
	char currentdirectory[1024];
	char currentskin[1024];
	int i = 0;

	int currentdirectoryindex = 0;
	int currentskinindex = 0;

	cvar_t *hand = Cvar_Get( "hand", "0", CVAR_USERINFO | CVAR_ARCHIVE );

	static const char *handedness[] = { "right", "left", "center", 0 };

	PlayerConfig_ScanDirectories();

	if (s_numplayermodels == 0)
		return false;

	if ( hand->value < 0 || hand->value > 2 )
		Cvar_SetValue( "hand", 0 );

	// r1ch: buffer overflow fix
//	strcpy( currentdirectory, skin->string );
	strncpy( currentdirectory, Cvar_VariableString ("skin"), sizeof(currentdirectory)-1);

	if ( strchr( currentdirectory, '/' ) )
	{
		strcpy( currentskin, strchr( currentdirectory, '/' ) + 1 );
		*strchr( currentdirectory, '/' ) = 0;
	}
	else if ( strchr( currentdirectory, '\\' ) )
	{
		strcpy( currentskin, strchr( currentdirectory, '\\' ) + 1 );
		*strchr( currentdirectory, '\\' ) = 0;
	}
	else
	{
		strcpy( currentdirectory, "male" );
		strcpy( currentskin, "grunt" );
	}

	qsort( s_pmi, s_numplayermodels, sizeof( s_pmi[0] ), pmicmpfnc );

	memset( s_pmnames, 0, sizeof( s_pmnames ) );
	for ( i = 0; i < s_numplayermodels; i++ )
	{
		s_pmnames[i] = s_pmi[i].displayname;
		if ( Q_stricmp( s_pmi[i].directory, currentdirectory ) == 0 )
		{
			int j;

			currentdirectoryindex = i;

			for ( j = 0; j < s_pmi[i].nskins; j++ )
			{
				if ( Q_stricmp( s_pmi[i].skindisplaynames[j], currentskin ) == 0 )
				{
					currentskinindex = j;
					break;
				}
			}
		}
	}

	s_player_config_menu.x = viddef.width / 2 - 95; 

	if(viddef.height >= 300) /* FS: Will draw out of bounds on extremely low res */
	{
		s_player_config_menu.y = viddef.height / 2 - 58; // was - 97;
	}
	else
	{
		s_player_config_menu.y = viddef.height / 2 - 97;
	}

	s_player_config_menu.nitems = 0;

	s_player_name_field.generic.type = MTYPE_FIELD;
	s_player_name_field.generic.name = "name";
	s_player_name_field.generic.callback = 0;
	s_player_name_field.generic.x		= 0;
	s_player_name_field.generic.y		= 0;
	s_player_name_field.length	= 20;
	s_player_name_field.visible_length = 20;
	strcpy( s_player_name_field.buffer, name->string );
	s_player_name_field.cursor = strlen( name->string );

	s_player_model_title.generic.type = MTYPE_SEPARATOR;
	s_player_model_title.generic.name = "model";
	s_player_model_title.generic.x    = -8;
	s_player_model_title.generic.y	 = 60;

	s_player_model_box.generic.type = MTYPE_SPINCONTROL;
	s_player_model_box.generic.x	= -56;
	s_player_model_box.generic.y	= 70;
	s_player_model_box.generic.callback = ModelCallback;
	s_player_model_box.generic.cursor_offset = -48;
	s_player_model_box.curvalue = currentdirectoryindex;
	s_player_model_box.itemnames = (const char **)s_pmnames; /* FS: Compiler warning */

	s_player_skin_title.generic.type = MTYPE_SEPARATOR;
	s_player_skin_title.generic.name = "skin";
	s_player_skin_title.generic.x    = -16;
	s_player_skin_title.generic.y	 = 84;

	s_player_skin_box.generic.type = MTYPE_SPINCONTROL;
	s_player_skin_box.generic.x	= -56;
	s_player_skin_box.generic.y	= 94;
	s_player_skin_box.generic.name	= 0;
	s_player_skin_box.generic.callback = 0;
	s_player_skin_box.generic.cursor_offset = -48;
	s_player_skin_box.curvalue = currentskinindex;
	s_player_skin_box.itemnames = (const char **)s_pmi[currentdirectoryindex].skindisplaynames;

	s_player_hand_title.generic.type = MTYPE_SEPARATOR;
	s_player_hand_title.generic.name = "handedness";
	s_player_hand_title.generic.x    = 32;
	s_player_hand_title.generic.y	 = 108;

	s_player_handedness_box.generic.type = MTYPE_SPINCONTROL;
	s_player_handedness_box.generic.x	= -56;
	s_player_handedness_box.generic.y	= 118;
	s_player_handedness_box.generic.name	= 0;
	s_player_handedness_box.generic.cursor_offset = -48;
	s_player_handedness_box.generic.callback = HandednessCallback;
	s_player_handedness_box.curvalue = Cvar_VariableValue( "hand" );
	s_player_handedness_box.itemnames = handedness;

	for (i = 0; i < sizeof(rate_tbl) / sizeof(*rate_tbl) - 1; i++)
		if (Cvar_VariableValue("rate") == rate_tbl[i])
			break;

	s_player_rate_title.generic.type = MTYPE_SEPARATOR;
	s_player_rate_title.generic.name = "connect speed";
	s_player_rate_title.generic.x    = 56;
	s_player_rate_title.generic.y	 = 156;

	s_player_rate_box.generic.type = MTYPE_SPINCONTROL;
	s_player_rate_box.generic.x	= -56;
	s_player_rate_box.generic.y	= 166;
	s_player_rate_box.generic.name	= 0;
	s_player_rate_box.generic.cursor_offset = -48;
	s_player_rate_box.generic.callback = RateCallback;
	s_player_rate_box.curvalue = i;
	s_player_rate_box.itemnames = rate_names;

	s_player_download_action.generic.type = MTYPE_ACTION;
	s_player_download_action.generic.name	= "download options";
	s_player_download_action.generic.flags= QMF_LEFT_JUSTIFY;
	s_player_download_action.generic.x	= -24;
	s_player_download_action.generic.y	= 186;
	s_player_download_action.generic.statusbar = NULL;
	s_player_download_action.generic.callback = DownloadOptionsFunc;

	Menu_AddItem( &s_player_config_menu, &s_player_name_field );
	Menu_AddItem( &s_player_config_menu, &s_player_model_title );
	Menu_AddItem( &s_player_config_menu, &s_player_model_box );
	if ( s_player_skin_box.itemnames )
	{
		Menu_AddItem( &s_player_config_menu, &s_player_skin_title );
		Menu_AddItem( &s_player_config_menu, &s_player_skin_box );
	}
	Menu_AddItem( &s_player_config_menu, &s_player_hand_title );
	Menu_AddItem( &s_player_config_menu, &s_player_handedness_box );
	Menu_AddItem( &s_player_config_menu, &s_player_rate_title );
	Menu_AddItem( &s_player_config_menu, &s_player_rate_box );
	Menu_AddItem( &s_player_config_menu, &s_player_download_action );

	return true;
}

void PlayerConfig_MenuDraw( void )
{
	extern float CalcFov( float fov_x, float w, float h );
	refdef_t refdef;
	char scratch[MAX_QPATH];

	if(viddef.height >= 300) /* FS: Will draw out of bounds on extremely low res */
	{
		M_Banner( "m_banner_plauer_setup" ); // Knightmare added banner
	}

	memset( &refdef, 0, sizeof( refdef ) );

	refdef.x = viddef.width / 2;

	if(viddef.height >= 300) /* FS: Will draw out of bounds on extremely low res */
	{
		refdef.y = viddef.height / 2 - 33; // was - 72
	}
	else
	{
		refdef.y = viddef.height / 2 - 72;
	}

	refdef.width = 144;
	refdef.height = 168;
	refdef.fov_x = 40;
	refdef.fov_y = CalcFov( refdef.fov_x, refdef.width, refdef.height );
	refdef.time = cls.realtime*0.001;

	// Knightmare- redid this to show weapon model
	if ( s_pmi[s_player_model_box.curvalue].skindisplaynames )
	{
		int			yaw; // was static
		vec3_t		modelOrg;
		entity_t	entity[2], *ent;

		refdef.num_entities = 0;
		refdef.entities = entity;

		yaw = anglemod(cl.time/10);
		VectorSet (modelOrg, 80, 0, 0);

		// Setup player model
		ent = &entity[0];
		memset( &entity[0], 0, sizeof( entity[0] ) );

		Com_sprintf( scratch, sizeof( scratch ), "players/%s/tris.md2", s_pmi[s_player_model_box.curvalue].directory );
		ent->model = re.RegisterModel( scratch );
		Com_sprintf( scratch, sizeof( scratch ), "players/%s/%s.pcx", s_pmi[s_player_model_box.curvalue].directory, s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue] );
		ent->skin = re.RegisterSkin( scratch );
		ent->flags = RF_FULLBRIGHT;
		VectorCopy( modelOrg, ent->origin );
		VectorCopy( ent->origin, ent->oldorigin );
		ent->frame = 0;
		ent->oldframe = 0;
		ent->backlerp = 0.0;
		ent->angles[1] = yaw;
		refdef.num_entities++;

		// Setup weapon model
		ent = &entity[1];
		memset( &entity[1], 0, sizeof( entity[1] ) );

		Com_sprintf( scratch, sizeof( scratch ), "players/%s/weapon.md2", s_pmi[s_player_model_box.curvalue].directory );
		ent->model = re.RegisterModel( scratch );
		if (ent->model)
		{
			ent->skinnum = 0;
			ent->flags = RF_FULLBRIGHT;
			VectorCopy( modelOrg, ent->origin );
			VectorCopy( ent->origin, ent->oldorigin );
			ent->frame = 0;
			ent->oldframe = 0;
			ent->backlerp = 0.0;
			ent->angles[1] = yaw;
			refdef.num_entities++;
		}

		refdef.areabits = 0;
		refdef.lightstyles = 0;
		refdef.rdflags = RDF_NOWORLDMODEL;

		Menu_Draw( &s_player_config_menu );

	//	M_DrawTextBox( ( refdef.x ) * ( 320.0F / viddef.width ) - 8, ( viddef.height / 2 ) * ( 240.0F / viddef.height) - 77, refdef.width / 8, refdef.height / 8 );
		if(viddef.height >= 300) /* FS: Will draw out of bounds on extremely low res */
		{
			M_DrawTextBox( ( refdef.x ) * ( 320.0F / viddef.width ) - 8, ( viddef.height / 2 ) * ( 240.0F / viddef.height) - 38, refdef.width / 8, refdef.height / 8 );
		}
		else
		{
			M_DrawTextBox( ( refdef.x ) * ( 320.0F / viddef.width ) - 8, ( viddef.height / 2 ) * ( 240.0F / viddef.height) - 77, refdef.width / 8, refdef.height / 8 );
		}
		refdef.height += 4;

		re.RenderFrame( &refdef );

		Com_sprintf( scratch, sizeof( scratch ), "/players/%s/%s_i.pcx", 
			s_pmi[s_player_model_box.curvalue].directory,
			s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue] );
		re.DrawPic( s_player_config_menu.x - 40, refdef.y, scratch );
	}
}

const char *PlayerConfig_MenuKey (int key)
{
	int i;

	if ( key == K_ESCAPE )
	{
		char scratch[1024];

		Cvar_Set( "name", s_player_name_field.buffer );

		Com_sprintf( scratch, sizeof( scratch ), "%s/%s", 
			s_pmi[s_player_model_box.curvalue].directory, 
			s_pmi[s_player_model_box.curvalue].skindisplaynames[s_player_skin_box.curvalue] );

		Cvar_Set( "skin", scratch );

		for ( i = 0; i < s_numplayermodels; i++ )
		{
			int j;

			for ( j = 0; j < s_pmi[i].nskins; j++ )
			{
				if ( s_pmi[i].skindisplaynames[j] )
					free( s_pmi[i].skindisplaynames[j] );
				s_pmi[i].skindisplaynames[j] = 0;
			}
			free( s_pmi[i].skindisplaynames );
			s_pmi[i].skindisplaynames = 0;
			s_pmi[i].nskins = 0;
		}
	}
	return Default_MenuKey( &s_player_config_menu, key );
}


void M_Menu_PlayerConfig_f (void)
{
	if (!PlayerConfig_MenuInit())
	{
		Menu_SetStatusBar( &s_multiplayer_menu, "No valid player models found" );
		return;
	}
	Menu_SetStatusBar( &s_multiplayer_menu, NULL );
	M_PushMenu( PlayerConfig_MenuDraw, PlayerConfig_MenuKey );
}


/*
=======================================================================

GALLERY MENU

=======================================================================
*/
#if 0
void M_Menu_Gallery_f( void )
{
	extern void Gallery_MenuDraw( void );
	extern const char *Gallery_MenuKey( int key );

	M_PushMenu( Gallery_MenuDraw, Gallery_MenuKey );
}
#endif

/*
=======================================================================

QUIT MENU

=======================================================================
*/

const char *M_Quit_Key (int key)
{
	switch (key)
	{
	case K_ESCAPE:
	case 'n':
	case 'N':
		M_PopMenu ();
		break;

	case K_ENTER: /* FS: Press enter to quit as well */
	case 'Y':
	case 'y':
		cls.key_dest = key_console;
		CL_Quit_f ();
		break;

	default:
		break;
	}

	return NULL;

}


void M_Quit_Draw (void)
{
	int		w, h;

	re.DrawGetPicSize (&w, &h, "quit");
	re.DrawPic ( (viddef.width-w)/2, (viddef.height-h)/2, "quit");
}


void M_Menu_Quit_f (void)
{
	M_PushMenu (M_Quit_Draw, M_Quit_Key);
}



//=============================================================================
/* Menu Subsystem */


/*
=================
M_Init
=================
*/
void M_Init (void)
{
	Cmd_AddCommand ("menu_main", M_Menu_Main_f);
	Cmd_AddCommand ("menu_game", M_Menu_Game_f);
		Cmd_AddCommand ("menu_loadgame", M_Menu_LoadGame_f);
		Cmd_AddCommand ("menu_savegame", M_Menu_SaveGame_f);
		Cmd_AddCommand ("menu_joinserver", M_Menu_JoinServer_f);
#ifdef GAMESPY
		Cmd_AddCommand ("menu_gamespy", M_Menu_JoinGamespyServer_f);  /* FS: GameSpy Browser */
#endif
			Cmd_AddCommand ("menu_addressbook", M_Menu_AddressBook_f);
		Cmd_AddCommand ("menu_startserver", M_Menu_StartServer_f);
			Cmd_AddCommand ("menu_dmoptions", M_Menu_DMOptions_f);
		Cmd_AddCommand ("menu_playerconfig", M_Menu_PlayerConfig_f);
			Cmd_AddCommand ("menu_downloadoptions", M_Menu_DownloadOptions_f);
		Cmd_AddCommand ("menu_credits", M_Menu_Credits_f );
	Cmd_AddCommand ("menu_multiplayer", M_Menu_Multiplayer_f );
	Cmd_AddCommand ("menu_video", M_Menu_Video_f);
	Cmd_AddCommand ("menu_options", M_Menu_Options_f);
		Cmd_AddCommand ("menu_extended_options", M_Menu_Extended_Options_f);
		Cmd_AddCommand ("menu_keys", M_Menu_Keys_f);
	Cmd_AddCommand ("menu_quit", M_Menu_Quit_f);
}


/*
=================
M_Draw
=================
*/
void M_Draw (void)
{
	if (cls.key_dest != key_menu)
	{
		return;
	}

	// repaint everything next frame
	SCR_DirtyScreen ();

	// dim everything behind it down
	if (cl.cinematictime > 0)
	{
		re.DrawFill (0,0,viddef.width, viddef.height, 0);
	}
	else
	{
		re.DrawFadeScreen ();
	}

	m_drawfunc ();

	// delay playing the enter sound until after the
	// menu has been drawn, to avoid delay while
	// caching images
	if (m_entersound)
	{
		S_StartLocalSound( menu_in_sound );
		m_entersound = false;
	}
}


/*
=================
M_Keydown
=================
*/
extern	qboolean keydown[256];
void M_Keydown (int key)
{
	const char *s;

	if (m_keyfunc)
	{
		if ( ( s = m_keyfunc( key ) ) != 0 )
		{
			S_StartLocalSound( ( char * ) s );
		}
	}
}
