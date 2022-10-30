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
#include "../client/snd_loc.h"
#include <3ds.h>

#define SAMPLE_RATE   	44100
#define NUM_SAMPLES 	4096
#define SAMPLE_SIZE		4
#define BUFFER_SIZE 	NUM_SAMPLES * SAMPLE_SIZE

static int sound_initialized = 0;
static byte *audio_buffer;
static ndspWaveBuf wave_buf;

qboolean SNDDMA_Init(void)
{
	sound_initialized = 0;

  	if(ndspInit() != 0)
    	return false;

    //Force Quake to use our settings
    Cvar_SetValue( "s_khz", 44 );
	Cvar_SetValue( "s_loadas8bit", false );

    audio_buffer = linearAlloc(BUFFER_SIZE);

    ndspSetOutputMode(NDSP_OUTPUT_STEREO);
	ndspChnReset(0);
	ndspChnWaveBufClear(0);
	ndspChnSetInterp(0, NDSP_INTERP_LINEAR);
	ndspChnSetRate(0, (float)SAMPLE_RATE);
	ndspChnSetFormat(0, NDSP_FORMAT_STEREO_PCM16);

	memset(&wave_buf, 0, sizeof(wave_buf));
	wave_buf.data_vaddr = audio_buffer;
	wave_buf.nsamples 	= BUFFER_SIZE / 4;
	wave_buf.looping	= 1;

	/* Fill the audio DMA information block */
	dma.samplebits = 16;
	dma.speed = SAMPLE_RATE;
	dma.channels = 2;
	dma.samples = BUFFER_SIZE / 2;
	dma.samplepos = 0;
	dma.submission_chunk = 1;
	dma.buffer = audio_buffer;

	ndspChnWaveBufAdd(0, &wave_buf);

	sound_initialized = 1;

	return true;
}

int SNDDMA_GetDMAPos(void)
{
	if(!sound_initialized)
		return 0;

	dma.samplepos = ndspChnGetSamplePos(0);
	return dma.samplepos;
}

void SNDDMA_Shutdown(void)
{
	if(!sound_initialized)
		return;

	ndspChnWaveBufClear(0);
	ndspExit();
	linearFree(audio_buffer);

	sound_initialized = 0;
}

/*
==============
SNDDMA_Submit

Send sound to device if buffer isn't really the dma buffer
===============
*/
void SNDDMA_Submit(void)
{
	if(sound_initialized)
		DSP_FlushDataCache(audio_buffer, BUFFER_SIZE);
}

void SNDDMA_BeginPainting(void)
{    
}