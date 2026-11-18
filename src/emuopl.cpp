/*
 * AdPlug - Replayer for many OPL2/OPL3 audio file formats.
 * Copyright (C) 1999 - 2005 Simon Peter <dn.tlp@gmx.net>, et al.
 * 
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation; either
 * version 2.1 of the License, or (at your option) any later version.
 * 
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 * 
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * emuopl.cpp - Emulated OPL, by Simon Peter <dn.tlp@gmx.net>
 */

#include "emuopl.h"
#include <string.h>

CEmuopl::CEmuopl(int rate, bool bit16, bool usestereo)
  : use16bit(bit16), stereo(usestereo), channelbuf(0), channeltemp(0),
    mixbufSamples(0), channelSamples(0), channelDataEnabled(false)
{
  opl[0] = OPLCreate(OPL_TYPE_YM3812, 3579545, rate);
  opl[1] = OPLCreate(OPL_TYPE_YM3812, 3579545, rate);
  memset(registers, 0, sizeof(registers));

  currType = TYPE_DUAL_OPL2;

  init();
}

CEmuopl::~CEmuopl()
{
  OPLDestroy(opl[0]); OPLDestroy(opl[1]);

  if(mixbufSamples) {
    delete [] mixbuf0;
    delete [] mixbuf1;
    delete [] mixbuf2;
    delete [] channelbuf;
    delete [] channeltemp;
  }
}

void CEmuopl::update(short *buf, int samples)
{
  int i;

  //ensure that our mix buffers are adequately sized
  if(mixbufSamples < samples) {
    if(mixbufSamples) {
      delete[] mixbuf0;
      delete[] mixbuf1;
      delete[] mixbuf2;
      delete[] channelbuf;
      delete[] channeltemp;
    }
    mixbufSamples = samples;

    //*2 = make room for stereo, if we need it
    mixbuf0 = new short[samples*2]; 
    mixbuf1 = new short[samples*2];
    mixbuf2 = new short[samples*2];
    channelbuf = channelDataEnabled ? new short[samples*18] : 0;
    channeltemp = channelDataEnabled ? new short[samples*9] : 0;
  }
  if(channelDataEnabled && !channelbuf) {
    channelbuf = new short[mixbufSamples*18];
    channeltemp = new short[mixbufSamples*9];
  }
  channelSamples = channelDataEnabled ? samples : 0;

  //data should be rendered to outbuf
  //tempbuf should be used as a temporary buffer
  //if we are supposed to generate 16bit output,
  //then outbuf may point directly to the actual waveform output "buf"
  //if we are supposed to generate 8bit output,
  //then outbuf cannot point to "buf" (because there will not be enough room)
  //and so it must point to a mixbuf instead--
  //it will be reduced to 8bit and put in "buf" later
  short *outbuf;
  short *tempbuf=mixbuf0;
  short *tempbuf2=mixbuf1;
  if(use16bit) outbuf = buf;
  else outbuf = mixbuf2;

  //all of the following rendering code produces 16bit output

  switch(currType) {
  case TYPE_OPL2:
    //for opl2 mode:
    //render chip0 to the output buffer
    if(channelDataEnabled) {
      YM3812UpdateOneChannels(opl[0],outbuf,samples,channeltemp);
      for(i=0;i<9;i++)
	memcpy(channelbuf+i*mixbufSamples,channeltemp+i*samples,
	       samples*sizeof(short));
      memset(channelbuf+9*mixbufSamples,0,9*mixbufSamples*sizeof(short));
    } else
      YM3812UpdateOne(opl[0],outbuf,samples);

    //if we are supposed to output stereo,
    //then we need to dup the mono channel
    if(stereo)
      for(i=samples-1;i>=0;i--) {
	outbuf[i*2] = outbuf[i];
	outbuf[i*2+1] = outbuf[i];
      }
    break;

  case TYPE_OPL3:	// unsupported
    if(channelDataEnabled)
      memset(channelbuf, 0, 18 * mixbufSamples * sizeof(short));
    break;

  case TYPE_DUAL_OPL2:
    //for dual opl2 mode:
    //render each chip to a different tempbuffer
    if(channelDataEnabled) {
      YM3812UpdateOneChannels(opl[0],tempbuf2,samples,channeltemp);
      for(i=0;i<9;i++)
	memcpy(channelbuf+i*mixbufSamples,channeltemp+i*samples,
	       samples*sizeof(short));
      YM3812UpdateOneChannels(opl[1],tempbuf,samples,channeltemp);
      for(i=0;i<9;i++)
	memcpy(channelbuf+(i+9)*mixbufSamples,channeltemp+i*samples,
	       samples*sizeof(short));
    } else {
      YM3812UpdateOne(opl[0],tempbuf2,samples);
      YM3812UpdateOne(opl[1],tempbuf,samples);
    }

    //output stereo:
    //then we need to interleave the two buffers
    if(stereo){
      //first, spread tempbuf's samples across left channel
      //left channel
      for(i=0;i<samples;i++)
	outbuf[i*2] = tempbuf2[i];
      //next, insert the samples from tempbuf2 into right channel
      for(i=0;i<samples;i++)
	outbuf[i*2+1] = tempbuf[i];
    } else
      //output mono:
      //then we need to mix the two buffers into buf
      for(i=0;i<samples;i++)
	outbuf[i] = (tempbuf[i]>>1) + (tempbuf2[i]>>1);
    break;
  }

  //now reduce to 8bit if we need to
  if(!use16bit)
    for(i=0;i<(stereo ? samples*2 : samples);i++)
      ((char *)buf)[i] = (outbuf[i] >> 8) ^ 0x80;
}

void CEmuopl::write(int reg, int val)
{
  if(reg >= 0 && reg < 256 && currChip >= 0 && currChip < 2)
    registers[currChip][reg] = (unsigned char)val;

  switch(currType){
  case TYPE_OPL2:
  case TYPE_DUAL_OPL2:
    OPLWrite(opl[currChip], 0, reg);
    OPLWrite(opl[currChip], 1, val);
    break;
  case TYPE_OPL3:	// unsupported
    break;
  }
}

void CEmuopl::init()
{
  OPLResetChip(opl[0]); OPLResetChip(opl[1]);
  memset(registers, 0, sizeof(registers));
  if(channelbuf)
    memset(channelbuf, 0, 18 * mixbufSamples * sizeof(short));
  channelSamples = 0;
  currChip = 0;
}

void CEmuopl::set_channel_data_enabled(bool enabled)
{
  channelDataEnabled = enabled;
  if(!enabled)
    channelSamples = 0;
}

const short *CEmuopl::get_channel_data(unsigned int chip,
				       unsigned int channel) const
{
  if(chip >= 2 || channel >= 9 || !channelbuf || channelSamples == 0)
    return 0;

  return channelbuf + (chip * 9 + channel) * mixbufSamples;
}

int CEmuopl::get_channel_samples() const
{
  return channelSamples;
}

bool CEmuopl::get_channel_adsr(unsigned int chip, unsigned int channel,
			       unsigned int op, Adsr &adsr) const
{
  static const unsigned char channel_offsets[9] =
    { 0, 1, 2, 8, 9, 10, 16, 17, 18 };

  if(chip >= 2 || channel >= 9 || op >= 2)
    return false;

  unsigned int reg = channel_offsets[channel] + (op ? 3 : 0);
  unsigned char attack_decay = registers[chip][0x60 + reg];
  unsigned char sustain_release = registers[chip][0x80 + reg];
  adsr.attack = attack_decay >> 4;
  adsr.decay = attack_decay & 0x0f;
  adsr.sustain = sustain_release >> 4;
  adsr.release = sustain_release & 0x0f;
  return true;
}

void CEmuopl::settype(ChipType type)
{
  currType = type;
}
