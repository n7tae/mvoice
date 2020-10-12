/*
 * Library: libcrc
 * Git:     https://github.com/lammertb/libcrc
 * Author:  Lammert Bies
 *
 * This file is licensed under the MIT License as stated below
 *
 * Copyright (c) 1999-2016 Lammert Bies
 * Copyright (c) 2020 Thomas A. Early, N7TAE
 *
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 *
 * The above copyright notice and this permission notice shall be included in all
 * copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
 * SOFTWARE.
 *
 * Description
 * -----------
 * The source file contains routines which calculate the CCITT CRC
 * values for an incomming byte string.
 */

#include <stdlib.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "CRC.h"

#define CRC_POLY_16 0x5935u
#define CRC_START_16 0xFFFFu

CCRC::CCRC()
{
	for (uint16_t i=0; i<256; i++)
	{
		uint16_t crc = 0;
		uint16_t c = i << 8;

		for (uint16_t j=0; j<8; j++)
		{
			if ( (crc ^ c) & 0x8000 )
				crc = ( crc << 1 ) ^ CRC_POLY_16;
			else
				crc = crc << 1;

			c = c << 1;
		}
		crc_tab16[i] = crc;
	}
}

uint16_t CCRC::CalcCRC(const SM17Frame &frame) const
{
	uint16_t crc = CRC_START_16;
	const uint8_t *input_str = frame.magic;

	for (size_t a=0; a<sizeof(SM17Frame)-2; a++)
	{
		crc = (crc << 8) ^ crc_tab16[ ((crc >> 8) ^ uint16_t(input_str[a])) & 0x00FF ];
	}

	return crc;
}
