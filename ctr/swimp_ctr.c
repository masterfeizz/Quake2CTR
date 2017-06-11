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

#include "../ref_soft/r_local.h"
#include <3ds.h>

uint16_t  	*framebuffer;
uint16_t 	d_8to16table[256];

void SWimp_BeginFrame( float camera_separation )
{
}

void SWimp_EndFrame (void)
{
	int x, y;
	for(x=0; x<400; x++)
	{
		for(y=0; y<240;y++)
		{
			framebuffer[(x*240 + (239 -y))] = d_8to16table[vid.buffer[y*400 + x]];
		}
	}
}

int			SWimp_Init( void *hInstance, void *wndProc )
{
	framebuffer = (uint16_t*)gfxGetFramebuffer(GFX_TOP, GFX_LEFT, NULL, NULL);
	return 0;
}

void		SWimp_SetPalette( const unsigned char *palette)
{
	int i;
	unsigned char r, g, b;

	if(palette==NULL)
		return;

	for(i = 0; i < 256; i++){
		r = palette[i*4+0];
		g = palette[i*4+1];
		b = palette[i*4+2];
		d_8to16table[i] = RGB8_to_565(r,g,b);
	}
}

void		SWimp_Shutdown( void )
{
	free(vid.buffer);
}

rserr_t		SWimp_SetMode( int *pwidth, int *pheight, int mode, qboolean fullscreen )
{
	vid.height = 240;
	vid.width = 400;
	vid.rowbytes = 400;
	vid.buffer = malloc(400*240*8);

	return rserr_ok;
}

void		SWimp_AppActivate( qboolean active )
{
}