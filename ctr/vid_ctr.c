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

#include "../client/client.h"
#include "../client/qmenu.h"

#define REF_SOFT    0
#define REF_OPENGL  1
#define REF_3DFX    2
#define REF_POWERVR 3
#define REF_VERITE  4

cvar_t *vid_ref;
cvar_t *vid_fullscreen;
cvar_t *vid_gamma;
cvar_t *scr_viewsize;

static cvar_t *sw_mode;
static cvar_t *sw_stipplealpha;

extern cvar_t   *sw_mipcap;
extern void M_ForceMenuOff( void );

#define SOFTWARE_MENU 0
#define OPENGL_MENU   1

static menuframework_s  s_software_menu;
static menuframework_s  s_opengl_menu;
static menuframework_s *s_current_menu;
static int              s_current_menu_index;

static menulist_s       s_mode_list[2];
static menulist_s       s_ref_list[2];
static menuslider_s     s_tq_slider;
static menuslider_s     s_screensize_slider;
static menuslider_s     s_brightness_slider;
static menuslider_s     s_mipcap_slider;
static menulist_s       s_fs_box;
static menulist_s       s_stipple_box;
static menulist_s       s_paletted_texture_box;
static menulist_s       s_finish_box;
static menuaction_s     s_cancel_action;
static menuaction_s     s_defaults_action;


viddef_t    viddef;             // global video state

refexport_t re;

refexport_t GetRefAPI (refimport_t rimp);


/*
==========================================================================

DIRECT LINK GLUE

==========================================================================
*/

#define MAXPRINTMSG 4096
void VID_Printf (int print_level, char *fmt, ...)
{
        va_list     argptr;
        char        msg[MAXPRINTMSG];

        va_start (argptr,fmt);
        vsprintf (msg,fmt,argptr);
        va_end (argptr);

        if (print_level == PRINT_ALL)
                Com_Printf ("%s", msg);
        else
                Com_DPrintf ("%s", msg);
}

void VID_Error (int err_level, char *fmt, ...)
{
        va_list     argptr;
        char        msg[MAXPRINTMSG];

        va_start (argptr,fmt);
        vsprintf (msg,fmt,argptr);
        va_end (argptr);

        Com_Error (err_level, "%s", msg);
}

void VID_NewWindow (int width, int height)
{
        viddef.width = width;
        viddef.height = height;
        printf("VID_NewWindow %d %\n",width,height);
}

/*
** VID_GetModeInfo
*/
typedef struct vidmode_s
{
    const char *description;
    int         width, height;
    int         mode;
} vidmode_t;

vidmode_t vid_modes[] =
{
    /*{ "Mode 0: 320x240",   320, 240,   0 },
    { "Mode 1: 400x300",   400, 300,   1 },
    { "Mode 2: 512x384",   512, 384,   2 },
    { "Mode 3: 640x480",   640, 480,   3 },
    { "Mode 4: 800x600",   800, 600,   4 },
    { "Mode 5: 960x720",   960, 720,   5 },
    { "Mode 6: 1024x768",  1024, 768,  6 },
    { "Mode 7: 1152x864",  1152, 864,  7 },
    { "Mode 8: 1280x960",  1280, 960, 8 },
    { "Mode 9: 1600x1200", 1600, 1200, 9 }
*/
    { "Mode 0: 400x240",   400, 240,   0 }
};
#define VID_NUM_MODES ( sizeof( vid_modes ) / sizeof( vid_modes[0] ) )

qboolean VID_GetModeInfo( int *width, int *height, int mode )
{
    if ( mode < 0 || mode >= VID_NUM_MODES )
        return false;

    *width  = vid_modes[mode].width;
    *height = vid_modes[mode].height;
    printf("VID_GetModeInfo %dx%d mode %d\n",width,height,mode);

    return true;
}

static void DriverCallback( void *unused )
{
}

static void ScreenSizeCallback( void *s )
{
    menuslider_s *slider = ( menuslider_s * ) s;

    Cvar_SetValue( "viewsize", slider->curvalue * 10 );
}

static void MipcapCallback( void *s)
{
    menuslider_s *slider = ( menuslider_s * ) s;

    Cvar_SetValue( "sw_mipcap", slider->curvalue );
}

static void BrightnessCallback( void *s )
{
    menuslider_s *slider = ( menuslider_s * ) s;

    if ( strcmp( vid_ref->string, "soft" ) == 0 )
    {
        float gamma = ( 0.8 - ( slider->curvalue/10.0 - 0.5 ) ) + 0.5;

        Cvar_SetValue( "vid_gamma", gamma );
    }
}

static void ResetDefaults( void *unused )
{
    VID_MenuInit();
}

static void ApplyChanges( void *unused )
{
    float gamma;

    /*
    ** invert sense so greater = brighter, and scale to a range of 0.5 to 1.3
    */
    gamma = ( 0.8 - ( s_brightness_slider.curvalue/10.0 - 0.5 ) ) + 0.5;

    Cvar_SetValue( "vid_gamma", gamma );
    Cvar_SetValue( "sw_stipplealpha", s_stipple_box.curvalue );
    Cvar_SetValue( "vid_fullscreen", s_fs_box.curvalue );
    //Cvar_SetValue( "sw_mode", s_mode_list.curvalue );

    Cvar_Set( "vid_ref", "soft" );

    M_ForceMenuOff();
}

static void CancelChanges( void *unused )
{
    extern void M_PopMenu( void );

    M_PopMenu();
}

void    VID_Init (void)
{
    refimport_t ri;

    viddef.width = 400;
    viddef.height = 240; //was originally 240

    ri.Cmd_AddCommand = Cmd_AddCommand;
    ri.Cmd_RemoveCommand = Cmd_RemoveCommand;
    ri.Cmd_Argc = Cmd_Argc;
    ri.Cmd_Argv = Cmd_Argv;
    ri.Cmd_ExecuteText = Cbuf_ExecuteText;
    ri.Con_Printf = VID_Printf;
    ri.Sys_Error = VID_Error;
    ri.FS_LoadFile = FS_LoadFile;
    ri.FS_FreeFile = FS_FreeFile;
    ri.FS_Gamedir = FS_Gamedir;
    ri.Vid_NewWindow = VID_NewWindow;
    ri.Cvar_Get = Cvar_Get;
    ri.Cvar_Set = Cvar_Set;
    ri.Cvar_SetValue = Cvar_SetValue;
    ri.Vid_GetModeInfo = VID_GetModeInfo;

    //JASON this is called from the video DLL
    re = GetRefAPI(ri);

    if (re.api_version != API_VERSION)
        Com_Error (ERR_FATAL, "Re has incompatible api_version");
    
        // call the init function
    if (re.Init (NULL, NULL) == -1)
        Com_Error (ERR_FATAL, "Couldn't start refresh");

    vid_ref = Cvar_Get ("vid_ref", "soft", CVAR_ARCHIVE);
    vid_fullscreen = Cvar_Get ("vid_fullscreen", "0", CVAR_ARCHIVE);
    vid_gamma = Cvar_Get( "vid_gamma", "1", CVAR_ARCHIVE );

}

void    VID_Shutdown (void)
{
    if (re.Shutdown)
        re.Shutdown ();
}

void    VID_CheckChanges (void)
{
}

void    VID_MenuInit (void)
{
    static const char *yesno_names[] =
    {
        "no",
        "yes",
        0
    };
    int i;

    if ( !sw_stipplealpha )
        sw_stipplealpha = Cvar_Get( "sw_stipplealpha", "0", CVAR_ARCHIVE );

    s_mode_list[SOFTWARE_MENU].curvalue = sw_mode->value;

    if ( !scr_viewsize )
        scr_viewsize = Cvar_Get ("viewsize", "100", CVAR_ARCHIVE);

    s_screensize_slider.curvalue = scr_viewsize->value/10;

    s_mipcap_slider.curvalue = sw_mipcap->value;

    if ( strcmp( vid_ref->string, "soft" ) == 0 )
    {
        s_current_menu_index = SOFTWARE_MENU;
        s_ref_list[0].curvalue = s_ref_list[1].curvalue = REF_SOFT;
    }

    s_software_menu.x = viddef.width * 0.50;
    s_software_menu.nitems = 0;

    s_screensize_slider.generic.type = MTYPE_SLIDER;
    s_screensize_slider.generic.x        = 0;
    s_screensize_slider.generic.y        = 20;
    s_screensize_slider.generic.name = "screen size";
    s_screensize_slider.minvalue = 3;
    s_screensize_slider.maxvalue = 12;
    s_screensize_slider.generic.callback = ScreenSizeCallback;

    s_brightness_slider.generic.type = MTYPE_SLIDER;
    s_brightness_slider.generic.x    = 0;
    s_brightness_slider.generic.y    = 30;
    s_brightness_slider.generic.name = "brightness";
    s_brightness_slider.generic.callback = BrightnessCallback;
    s_brightness_slider.minvalue = 5;
    s_brightness_slider.maxvalue = 13;
    s_brightness_slider.curvalue = ( 1.3 - vid_gamma->value + 0.5 ) * 10;

    s_fs_box.generic.type = MTYPE_SPINCONTROL;
    s_fs_box.generic.x   = 0;
    s_fs_box.generic.y   = 40;
    s_fs_box.generic.name    = "fullscreen";
    s_fs_box.itemnames = yesno_names;
    s_fs_box.curvalue = vid_fullscreen->value;

    s_defaults_action.generic.type = MTYPE_ACTION;
    s_defaults_action.generic.name = "reset to defaults";
    s_defaults_action.generic.x    = 0;
    s_defaults_action.generic.y    = 90;
    s_defaults_action.generic.callback = ResetDefaults;

    s_cancel_action.generic.type = MTYPE_ACTION;
    s_cancel_action.generic.name = "cancel";
    s_cancel_action.generic.x    = 0;
    s_cancel_action.generic.y    = 100;
    s_cancel_action.generic.callback = CancelChanges;

    s_stipple_box.generic.type = MTYPE_SPINCONTROL;
    s_stipple_box.generic.x = 0;
    s_stipple_box.generic.y = 60;
    s_stipple_box.generic.name  = "stipple alpha";
    s_stipple_box.curvalue = sw_stipplealpha->value;
    s_stipple_box.itemnames = yesno_names;

    s_mipcap_slider.generic.type = MTYPE_SLIDER;
    s_mipcap_slider.generic.x        = 0;
    s_mipcap_slider.generic.y        = 70;
    s_mipcap_slider.generic.name = "mipcap";
    s_mipcap_slider.minvalue = 0;
    s_mipcap_slider.maxvalue = 4;
    s_mipcap_slider.generic.callback = MipcapCallback;

    Menu_AddItem( &s_software_menu, ( void * ) &s_screensize_slider );
    Menu_AddItem( &s_software_menu, ( void * ) &s_brightness_slider );
    Menu_AddItem( &s_software_menu, ( void * ) &s_fs_box );
    Menu_AddItem( &s_software_menu, ( void * ) &s_stipple_box );
    Menu_AddItem( &s_software_menu, ( void * ) &s_mipcap_slider );

    Menu_AddItem( &s_software_menu, ( void * ) &s_defaults_action );
    Menu_AddItem( &s_software_menu, ( void * ) &s_cancel_action );

    Menu_Center( &s_software_menu );

    s_software_menu.x -= 8;
}

void    VID_MenuDraw (void)
{
    int w, h;

    if ( s_current_menu_index == 0 )
        s_current_menu = &s_software_menu;
    else
        s_current_menu = &s_opengl_menu;

    /*
    ** draw the banner
    */
    re.DrawGetPicSize( &w, &h, "m_banner_video" );
    re.DrawPic( viddef.width / 2 - w / 2, viddef.height /2 - 110, "m_banner_video" );

    /*
    ** move cursor to a reasonable starting position
    */
    Menu_AdjustCursor( s_current_menu, 1 );

    /*
    ** draw the menu
    */
    Menu_Draw( s_current_menu );
}

const char *VID_MenuKey( int k)
{
    menuframework_s *m = s_current_menu;
    static const char *sound = "misc/menu1.wav";

    switch ( k )
    {
    case K_ESCAPE:
        ApplyChanges( 0 );
        return NULL;
    case K_KP_UPARROW:
    case K_UPARROW:
        m->cursor--;
        Menu_AdjustCursor( m, -1 );
        break;
    case K_KP_DOWNARROW:
    case K_DOWNARROW:
        m->cursor++;
        Menu_AdjustCursor( m, 1 );
        break;
    case K_KP_LEFTARROW:
    case K_LEFTARROW:
        Menu_SlideItem( m, -1 );
        break;
    case K_KP_RIGHTARROW:
    case K_RIGHTARROW:
        Menu_SlideItem( m, 1 );
        break;
    case K_KP_ENTER:
    case K_ENTER:
        if ( !Menu_SelectItem( m ) )
            ApplyChanges( NULL );
        break;
    }

    return sound;
//  return NULL;
}