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
#include <3ds.h>

extern int keyboard_toggled;
void Touch_Update();

static circlePosition 	cstick, circlepad;
static touchPosition 	old_touch, touch;

cvar_t *in_joystick;
cvar_t *circlepad_sensitivity;
cvar_t *cstick_sensitivity;
cvar_t *circlepad_look;

void IN_Init (void)
{
	in_joystick	= Cvar_Get ("in_joystick", "1",	CVAR_ARCHIVE);
	circlepad_sensitivity = Cvar_Get ("circlepad_sensitivity", "2.0", CVAR_ARCHIVE);
	cstick_sensitivity = Cvar_Get ("cstick_sensitivity", "2.0", CVAR_ARCHIVE);
	circlepad_look = Cvar_Get ("circlepad_look", "0", CVAR_ARCHIVE);
}

void IN_Shutdown (void)
{
}

void IN_Commands (void)
{
}

void IN_Frame (void)
{
	Touch_Update();
}

void IN_Move (usercmd_t *cmd)
{
	hidCircleRead(&circlepad);

	if(hidKeysDown() & KEY_TOUCH)
		hidTouchRead(&old_touch);

	if((hidKeysHeld() & KEY_TOUCH) && !keyboard_toggled)
	{
		hidTouchRead(&touch);

		if(touch.px < 268)
		{
			int tx = touch.px - old_touch.px;
			int ty = touch.py - old_touch.py;

			if(m_pitch->value < 0)
				ty = -ty;

			cl.viewangles[YAW]   -= abs(tx) > 1 ? tx * sensitivity->value * 0.25f : 0;
			cl.viewangles[PITCH] += abs(ty) > 1 ? ty * sensitivity->value * 0.25f : 0;
		}

		old_touch = touch;
	}

	if(abs(circlepad.dy) > 10)
	{
		float y_value = circlepad.dy;

		if( (m_pitch->value < 0) && circlepad_look->value)
			y_value = -y_value;

		if(circlepad_look->value)
			cl.viewangles[PITCH] -= y_value * circlepad_sensitivity->value * 0.025f;
		else
			cmd->forwardmove += y_value * circlepad_sensitivity->value;
	}

	if(abs(circlepad.dx) > 10)
	{
		float x_value = circlepad.dx;

		if( ((in_strafe.state & 1) || (lookstrafe->value)) && !circlepad_look->value )
			cmd->sidemove += x_value * circlepad_sensitivity->value;
		else
			cl.viewangles[YAW] -= x_value * circlepad_sensitivity->value * 0.025f;
	}

	hidCstickRead(&cstick);

	if(m_pitch->value < 0)
		cstick.dy = -cstick.dy;

	cstick.dx = abs(cstick.dx) > 1 ? cstick.dx * cstick_sensitivity->value * 0.01f : 0;
	cstick.dy = abs(cstick.dy) > 1 ? cstick.dy * cstick_sensitivity->value * 0.01f : 0;

	cl.viewangles[YAW] -= cstick.dx;
	cl.viewangles[PITCH] -= cstick.dy;
}

void IN_Activate (qboolean active)
{
}

void IN_ActivateMouse (void)
{
}

void IN_DeactivateMouse (void)
{
}

void IN_MouseEvent (int mstate)
{
}