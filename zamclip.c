/* zamclip.c  ZamClip mono soft clipper
 * Copyright (C) 2013  Damien Zammit
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#include <math.h>
#include <stdlib.h>
#include <stdio.h>

#include "lv2/lv2plug.in/ns/lv2core/lv2.h"

#define ZAMCLIP_URI "http://zamaudio.com/lv2/zamclip"


typedef enum {
	ZAMCLIP_INPUT = 0,
	ZAMCLIP_OUTPUT = 1,

	ZAMCLIP_SPRINGMASS = 2,
 	ZAMCLIP_THRESH = 3, 
 	ZAMCLIP_GAINR = 4

} PortIndex;


typedef struct {
	float* input;
	float* output;
  
	float* springmass;
	float* thresh;
	float* gainr;
 
	float srate;
	float lastsample;
	float oldgainr;
	int halfsecond;
	float maxgain;
	
} ZamCLIP;

static LV2_Handle
instantiate(const LV2_Descriptor* descriptor,
            double rate,
            const char* bundle_path,
            const LV2_Feature* const* features)
{
	ZamCLIP* zamclip = (ZamCLIP*)malloc(sizeof(ZamCLIP));
	zamclip->srate = rate;
  
	zamclip->lastsample = 0.f;
	zamclip->oldgainr = 0.f;

	return (LV2_Handle)zamclip;
}

static void
connect_port(LV2_Handle instance,
             uint32_t port,
             void* data)
{
	ZamCLIP* zamclip = (ZamCLIP*)instance;
  
	switch ((PortIndex)port) {
	case ZAMCLIP_INPUT:
		zamclip->input = (float*)data;
  	break;
	case ZAMCLIP_OUTPUT:
		zamclip->output = (float*)data;
  	break;
	case ZAMCLIP_SPRINGMASS:
		zamclip->springmass = (float*)data;
	break;
	case ZAMCLIP_THRESH:
		zamclip->thresh = (float*)data;
	break;
	case ZAMCLIP_GAINR:
		zamclip->gainr = (float*)data;
	break;
	}
  
}

// Works on little-endian machines only
static inline bool
is_nan(float& value ) {
	if (((*(uint32_t *) &value) & 0x7fffffff) > 0x7f800000) {
		return true;
	}
return false;
}

// Force already-denormal float value to zero
static inline void
sanitize_denormal(float& value) {
	if (is_nan(value)) {
		value = 0.f;
	}
}

static inline float
from_dB(float gdb) {
	return (exp(gdb/20.f*log(10.f)));
};

static inline float
to_dB(float g) {
	return (20.f*log10(g));
}


static void
activate(LV2_Handle instance)
{
}


static void
run(LV2_Handle instance, uint32_t n_samples)
{
	ZamCLIP* zamclip = (ZamCLIP*)instance;
  
	const float* const input = zamclip->input;
	float* const output = zamclip->output;
  	float* const gainr = zamclip->gainr;

	float springmass = *(zamclip->springmass);
	float sqkm = sqrt(springmass);
	float thresh = from_dB(*(zamclip->thresh));
	float srate = zamclip->srate;

	double p0 = zamclip->lastsample;
	double v0 = (input[0] - p0);
	double xp = (p0-thresh)*cos(sqkm) + 1.f/sqkm*v0*sin(sqkm);
	double xm = (p0+thresh)*cos(sqkm) + 1.f/sqkm*v0*sin(sqkm);
	float tmpgain = 0.f;
	float tmp = 0.f;

	output[0] = (input[0] > thresh) ? xp + thresh : (input[0] < -thresh) ? xm - thresh : input[0];
	zamclip->halfsecond++;

	for (uint32_t i = 1; i < n_samples; ++i) {
		p0 = input[i-1];
		v0 = (input[i] - p0);
		xp = (p0-thresh)*cos(sqkm) + 1.f/sqkm*v0*sin(sqkm);
		xm = (p0+thresh)*cos(sqkm) + 1.f/sqkm*v0*sin(sqkm);
		output[i] = (input[i] > thresh) ? xp + thresh : (input[i] < -thresh) ? xm - thresh : input[i];; 
		tmpgain = (input[i] > thresh) ? fabs(xp+thresh) : (input[i] < -thresh) ? fabs(xm-thresh) : 0.f;
		if (zamclip->halfsecond > srate/8) {
			zamclip->halfsecond = 0;
			tmp = to_dB(tmpgain);
			sanitize_denormal(tmp);
			*gainr = (tmp < -100) ? 0.f : tmp;
			zamclip->maxgain = 0.f;
		} else {
			if (tmpgain > zamclip->maxgain) {
				zamclip->maxgain = tmpgain;
			}
			tmp = to_dB(zamclip->maxgain);
			sanitize_denormal(tmp);
			*gainr = (tmp < -100) ? 0.f : tmp;
		}
		zamclip->halfsecond++;
	}
 	zamclip->lastsample = input[n_samples-1]; 
}


static void
deactivate(LV2_Handle instance)
{
}

static void
cleanup(LV2_Handle instance)
{
	free(instance);
}

const void*
extension_data(const char* uri)
{
	return NULL;
}

static const LV2_Descriptor descriptor = {
	ZAMCLIP_URI,
	instantiate,
	connect_port,
	activate,
	run,
	deactivate,
	cleanup,
	extension_data
};

LV2_SYMBOL_EXPORT
const LV2_Descriptor*
lv2_descriptor(uint32_t index)
{
	switch (index) {
	case 0:
		return &descriptor;
	default:
		return NULL;
	}
}
