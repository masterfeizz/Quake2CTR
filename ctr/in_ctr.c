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

void IN_Init (void)
{
	in_joystick	= Cvar_Get ("in_joystick", "1",	CVAR_ARCHIVE);
	circlepad_sensitivity = Cvar_Get ("circlepad_sensitivity", "2.0", CVAR_ARCHIVE);
	cstick_sensitivity = Cvar_Get ("cstick_sensitivity", "2.0", CVAR_ARCHIVE);
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
			cl.viewangles[YAW]   -= (touch.px - old_touch.px) * sensitivity->value/2;
			cl.viewangles[PITCH] += (touch.py - old_touch.py) * sensitivity->value/2;
		}
		old_touch = touch;
	}

	if(abs(circlepad.dy) > 15)
	{
		float y_value = circlepad.dy;
		cmd->forwardmove += (y_value * circlepad_sensitivity->value) * m_forward->value;
	}

	if(abs(circlepad.dx) > 15)
	{
		float x_value = circlepad.dx;
		if((in_strafe.state & 1) || (lookstrafe->value))
			cmd->sidemove += (x_value * circlepad_sensitivity->value) * m_forward->value;
		else
			cl.viewangles[YAW] -= m_side->value * x_value * 0.025f;
	}

	hidCstickRead(&cstick);

	if(m_pitch->value < 0)
		cstick.dy = -cstick.dy;

	cstick.dx = abs(cstick.dx) < 10 ? 0 : cstick.dx * 0.01 * cstick_sensitivity->value;
	cstick.dy = abs(cstick.dy) < 10 ? 0 : cstick.dy * 0.01 * cstick_sensitivity->value;

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