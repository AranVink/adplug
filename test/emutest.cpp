/*
 * Adplug - Replayer for many OPL2/OPL3 audio file formats.
 * Copyright (C) 1999 - 2005 Simon Peter, <dn.tlp@gmx.net>, et al.
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
 * emutest.cpp - Test AdPlug emulators, by Simon Peter <dn.tlp@gmx.net>
 */

#include <stdlib.h>
#include <stdio.h>

#include "../src/emuopl.h"

/***** Local functions *****/

#define BUF_SIZE	1024

static bool check_emu_output(CEmuopl *emu)
  /*
   * Test if the emulator produces any output.
   */
{
  short	*buf = (short *)calloc(BUF_SIZE, sizeof(short));
  bool	nonull = false, no10k = true, nom10k = true;

  // init emulator
  emu->init();
  emu->write(1, 5 << 1);

  // set test instrument
  emu->write(0x20, 1);
  emu->write(0x40, 0x10);
  emu->write(0x60, 0xf0);
  emu->write(0x80, 0x77);
  emu->write(0xa0, 0x98);
  emu->write(0x23, 1);
  emu->write(0x43, 0);
  emu->write(0x63, 0xf0);
  emu->write(0x83, 0x77);
  emu->write(0xb0, 0x31);

  // check output from emu
  emu->update(buf, BUF_SIZE);
  for(int i = 0; i < BUF_SIZE; i++) {
    if(buf[i] != 0) nonull = true;
    if(buf[i] > 10000) no10k = false;
    if(buf[i] < -10000) nom10k = false;
  }

  free(buf);

  return (nonull && no10k && nom10k);
}

static bool check_channel_metrics()
{
  CEmuopl emu(8000, true, false);
  short buffer[BUF_SIZE];
  CEmuopl::Adsr adsr;

  if(emu.get_channel_data(0, 0) || emu.get_channel_samples() != 0)
    return false;

  emu.set_channel_data_enabled(true);
  emu.init();
  emu.write(0x20, 1);
  emu.write(0x40, 0x10);
  emu.write(0x60, 0xa5);
  emu.write(0x80, 0xc7);
  emu.write(0x23, 1);
  emu.write(0x43, 0);
  emu.write(0x63, 0x32);
  emu.write(0x83, 0x4d);
  emu.write(0xa0, 0x98);
  emu.write(0xb0, 0x31);
  emu.update(buffer, BUF_SIZE);

  if(emu.get_channel_samples() != BUF_SIZE)
    return false;

  const short *channel0 = emu.get_channel_data(0, 0);
  const short *channel1 = emu.get_channel_data(0, 1);
  if(!channel0 || !channel1)
    return false;

  bool channel0_nonzero = false;
  for(int i = 0; i < BUF_SIZE; i++) {
    if(channel0[i] != 0)
      channel0_nonzero = true;
    if(channel1[i] != 0)
      return false;
  }

  if(!channel0_nonzero ||
     !emu.get_channel_adsr(0, 0, 0, adsr) ||
     adsr.attack != 10 || adsr.decay != 5 ||
     adsr.sustain != 12 || adsr.release != 7)
    return false;

  if(!emu.get_channel_adsr(0, 0, 1, adsr) ||
     adsr.attack != 3 || adsr.decay != 2 ||
     adsr.sustain != 4 || adsr.release != 13)
    return false;

  return !emu.get_channel_adsr(2, 0, 0, adsr) &&
	 !emu.get_channel_data(0, 9);
}

/***** Main program *****/

int main(int argc, char *argv[])
{
  bool	retval = check_channel_metrics();

  {
    CEmuopl emu(8000, true, false);
    retval = check_emu_output(&emu) && retval;
  }

  return retval ? EXIT_SUCCESS : EXIT_FAILURE;
}
