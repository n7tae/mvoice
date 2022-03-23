/*
** Copyright (c) 2002-2016, Erik de Castro Lopo <erikd@mega-nerd.com>
** All rights reserved.
**
** This code is released under 2-clause BSD license. Please see the
** file at : https://github.com/erikd/libsamplerate/blob/master/COPYING
*/

#include <vector>
#include <iostream>
#include <stdlib.h>
#include <string.h>
#include <cmath>

#include "Resampler.h"
#include "fastest_coeffs.h"

#define	SHIFT_BITS				12
#define	FP_ONE					((double)(((int) 1) << SHIFT_BITS))
#define	INV_FP_ONE				(1.0 / FP_ONE)

int CResampler::double_to_fp(double x)
{
	return lrint(x * FP_ONE);
}

int CResampler::int_to_fp(int x)
{
	return ((int)(x)) << SHIFT_BITS;
}

int CResampler::fp_to_int(int x)
{
	return ((x) >> SHIFT_BITS);
}

int CResampler::fp_fraction_part(int x)
{
	return (x) & ((((int) 1) << SHIFT_BITS) - 1);
}

double CResampler::fp_to_double(int x)
{
	return fp_fraction_part(x) * INV_FP_ONE;
}

void CResampler::sinc_set_converter()
{
	filter.Init();

	filter.coeffs = fastest_coeffs.coeffs;
	filter.coeff_half_len = ARRAY_LEN - 2;
	filter.index_inc = fastest_coeffs.increment;

	filter.b_len = lrint(2.5 * filter.coeff_half_len / (filter.index_inc * 1.0) * 40.0);
	if (filter.b_len < 4096)
		filter.b_len = 4096;

	filter.buffer.resize(filter.b_len + 1, 0.0f);

	Reset();

	int count = filter.coeff_half_len;
	int bits;
	for(bits = 0; (1 << bits) < count; bits++)
		count |= (1 << bits);

	if (bits + SHIFT_BITS - 1 >= (int)(sizeof(int) * 8))
		std::cerr << "Internal error. Filter length too large." << std::endl;
}

void CResampler::Reset()
{
	filter.b_current = filter.b_end = 0;
	filter.b_real_end = -1;

	filter.input_index = 0.0;

	for (auto it=filter.buffer.begin(); it!=filter.buffer.end(); it++)
		*it = 0;
}

double CResampler::calc_output_single(int increment, int start_filter_index)
{
	/* Convert input parameters into fixed point. */
	int max_filter_index = int_to_fp(filter.coeff_half_len);

	/* First apply the left half of the filter. */
	int filter_index = start_filter_index;
	int coeff_count = (max_filter_index - filter_index) / increment;
	filter_index += coeff_count * increment;
	int data_index = filter.b_current - coeff_count;

	double left = 0.0;
	do
	{
		double fraction = fp_to_double(filter_index);
		int indx = fp_to_int(filter_index);

		double icoeff = filter.coeffs[indx] + fraction * (filter.coeffs[indx + 1] - filter.coeffs[indx]);

		left += icoeff * filter.buffer[data_index];

		filter_index -= increment;
		data_index = data_index + 1;
	}
	while (filter_index >= 0);

	/* Now apply the right half of the filter. */
	filter_index = increment - start_filter_index;
	coeff_count = (max_filter_index - filter_index) / increment;
	filter_index = filter_index + coeff_count * increment;
	data_index = filter.b_current + 1 + coeff_count;

	double right = 0.0;
	do
	{
		double fraction = fp_to_double(filter_index);
		int indx = fp_to_int(filter_index);

		double icoeff = filter.coeffs[indx] + fraction * (filter.coeffs[indx + 1] - filter.coeffs[indx]);

		right += icoeff * filter.buffer[data_index];

		filter_index -= increment;
		data_index = data_index - 1;
	}
	while (filter_index > 0);

	return(left + right);
}

bool CResampler::Process(SDATA &data)
{
	/* If there is not a problem, this will be optimised out. */
	if (sizeof(filter.buffer[0]) != sizeof(data.data_in[0])) {
		std::cerr << "Internal error. Input data / internal buffer size difference. Please report this." << std::endl;
		return true;
	}

	filter.in_count = data.input_frames;
	filter.out_count = data.output_frames;
	filter.in_used = filter.out_gen = 0;

	/* Check the sample rate ratio wrt the buffer len. */
	double count = (filter.coeff_half_len + 2.0) / filter.index_inc;
	if (data.ratio < 1.0)
		count /= data.ratio;

	/* Maximum coefficientson either side of center point. */
	int half_filter_chan_len = lrint(count) + 1;

	double input_index = last_position;
	double float_increment = filter.index_inc;

	double rem = fmod_one(input_index);
	filter.b_current = (filter.b_current + lrint(input_index - rem)) % filter.b_len;
	input_index = rem;

	double terminate = 1.0 / data.ratio + 1e-20;

	/* Main processing loop. */
	while (filter.out_gen < filter.out_count)
	{
		/* Need to reload buffer? */
		int samples_in_hand = (filter.b_end - filter.b_current + filter.b_len) % filter.b_len;

		if (samples_in_hand <= half_filter_chan_len)
		{
			if (prepare_data(data, half_filter_chan_len))
				return true;

			samples_in_hand = (filter.b_end - filter.b_current + filter.b_len) % filter.b_len;
			if (samples_in_hand <= half_filter_chan_len)
				break;
		};

		/* This is the termination condition. */
		if (filter.b_real_end >= 0)
		{
			if (filter.b_current + input_index + terminate > filter.b_real_end)
				break;
		};

		float_increment = filter.index_inc * (data.ratio < 1.0 ? data.ratio : 1.0);
		int increment = double_to_fp(float_increment);

		int start_filter_index = double_to_fp(input_index * float_increment);

		data.data_out[filter.out_gen] = (float)((float_increment / filter.index_inc) * calc_output_single(increment, start_filter_index));
		filter.out_gen ++;

		/* Figure out the next index. */
		input_index += 1.0 / data.ratio;
		rem = fmod_one(input_index);

		filter.b_current = (filter.b_current + lrint(input_index - rem)) % filter.b_len;
		input_index = rem;
	};

	last_position = input_index;

	data.input_frames_used = filter.in_used;
	data.output_frames_gen = filter.out_gen;

	return false;
}

bool CResampler::prepare_data(SDATA &data, int half_filter_chan_len)
{
	int len = 0;

	if (filter.b_real_end >= 0)
		return 0 ;	/* Should be terminating. Just return. */

	if (filter.b_current == 0)
	{
		/* Initial state. Set up zeros at the start of the buffer and
		** then load new data after that.
		*/
		len = filter.b_len - 2 * half_filter_chan_len;

		filter.b_current = filter.b_end = half_filter_chan_len;
	}
	else if (filter.b_end + half_filter_chan_len + 1 < filter.b_len)
	{
		/*  Load data at current end position. */
		int test = filter.b_len - filter.b_current - half_filter_chan_len;
		len = (test > 0) ? test : 0;
	}
	else
	{
		/* Move data at end of buffer back to the start of the buffer. */
		len = filter.b_end - filter.b_current;
		memmove(filter.buffer.data(), filter.buffer.data() + filter.b_current - half_filter_chan_len, (half_filter_chan_len + len) * sizeof(filter.buffer[0]));

		filter.b_current = half_filter_chan_len;
		filter.b_end = filter.b_current + len;

		/* Now load data at current end of buffer. */
		int test = filter.b_len - filter.b_current - half_filter_chan_len;
		len = (test > 0) ? test : 0;
	};

	int test = int(filter.in_count - filter.in_used);
	if (test < len)
		len = test;
	//len -= (len % filter.channels);

	if (len < 0 || filter.b_end + len > filter.b_len) {
		std::cerr << "Internal error : Bad length in prepare_data()." << std::endl;
		return true;
	}

	memcpy(filter.buffer.data() + filter.b_end, data.data_in + filter.in_used, len * sizeof(filter.buffer[0]));

	filter.b_end += len;
	filter.in_used += len;

	if (filter.in_used == filter.in_count && filter.b_end - filter.b_current < 2 * half_filter_chan_len && data.end_of_input)
	{
		/* Handle the case where all data in the current buffer has been
		** consumed and this is the last buffer.
		*/

		if (filter.b_len - filter.b_end < half_filter_chan_len + 5)
		{
			/* If necessary, move data down to the start of the buffer. */
			len = filter.b_end - filter.b_current;
			memmove(filter.buffer.data(), filter.buffer.data() + filter.b_current - half_filter_chan_len, (half_filter_chan_len + len) * sizeof(filter.buffer[0]));

			filter.b_current = half_filter_chan_len;
			filter.b_end = filter.b_current + len;
		};

		filter.b_real_end = filter.b_end;
		len = half_filter_chan_len + 5;

		if (len < 0 || filter.b_end + len > filter.b_len)
			len = filter.b_len - filter.b_end;

		memset(filter.buffer.data() + filter.b_end, 0, len * sizeof(filter.buffer[0]));
		filter.b_end += len;
	};

	return false;
}
bool CResampler::SetRatio(SDATA &data, double new_ratio)
{
	if (new_ratio > 40.0 || new_ratio < 1.0/40.0) {
		std::cerr << "Resample ratio of " << new_ratio << " is out of range" << std::endl;
		return true;
	}
	data.ratio = new_ratio;
	return false;
}

void CResampler::Short2Float(const short *in, float *out, int len)
{
	while (len)
	{
		len --;
		out[len] = (float)(in[len] / (1.0 * 0x8000));
	};

	return;
}

double CResampler::fmod_one(const double x)
{
	double ipart;

	return modf(x, &ipart);
}

void CResampler::Float2Short(const float *in, short *out, int len)
{
	double scaled_value;

	while (len)
	{
		len --;

		scaled_value = in[len] * (8.0 * 0x10000000);
		if (scaled_value >= (1.0 * 0x7FFFFFFF))
		{
			out[len] = 32767;
			continue;
		};
		if (scaled_value <= (-8.0 * 0x10000000))
		{
			out[len] = -32768;
			continue;
		};

		out[len] = (short)(lrint(scaled_value) >> 16);
	};

}
