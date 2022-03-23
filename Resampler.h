/*
 *   Copyright (c) 2022 by Thomas A. Early N7TAE
 *
 *   This program is free software; you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation; either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program; if not, write to the Free Software
 *   Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#pragma once

#include <vector>

using SDATA = struct data_tag
{
	float	*data_in;
	float	*data_out;

	long	input_frames, output_frames;
	long	input_frames_used, output_frames_gen;

	bool	end_of_input;

	double	ratio;
};

using SINC_FILTER = struct sinc_filter_tab
{
	int		coeff_half_len, index_inc;
	int		b_current, b_end, b_real_end, b_len;

	long	in_count, in_used;
	long	out_count, out_gen;

	double	input_index;
	double	left_calc, right_calc;

	const float *coeffs;

	std::vector<float> buffer;

	void Init()
	{
		coeff_half_len = index_inc = b_current = b_end = b_len = 0;
		in_count = in_used = out_count = out_gen = 0;
		b_real_end = -1;
		input_index = left_calc = right_calc = 0.0;
		coeffs = 0;
	}
};

class CResampler
{
public:
	CResampler() { sinc_set_converter(); }
	bool Process(SDATA &data);
	void Reset();
	bool SetRatio(SDATA &data, double new_ratio);

	void Short2Float(const short *in, float *out, int len);
	void Float2Short(const float *in, short *out, int len);

private:
	SINC_FILTER filter;
	bool reset;
	float last_value;

	double	last_position;
	int double_to_fp(double x);
	int int_to_fp(int x);
	int fp_to_int(int x);
	int fp_fraction_part(int x);
	double fp_to_double(int x);
	void sinc_set_converter();
	double calc_output_single(int increment, int start_filter_index);
	bool prepare_data(SDATA &data, int half_filter_chan_len);
	double fmod_one(const double x);
};
