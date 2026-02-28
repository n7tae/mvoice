/*
 *   Copyright (c) 2025 by Thomas A. Early N7TAE
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

#include <cstdint>
#include <cstring>
#include <cmath>
#include <iostream>
#include <iomanip>

enum class EGnssSourceType { Client, OpenRTX, Other };
enum class EGnssStationType { Fixed, Mobile, HandHeld, Other };

class CGNSS
{
public:
	CGNSS() {
		memset(data, 0, 14);
	}

	CGNSS(uint8_t *in) : CGNSS()
	{
		memcpy(data, in, 14);
	}

	virtual ~CGNSS() {}

	bool IsValid()
	{
		return (data[1] >> 7) ? true : false;
	}

	void CopyGnssMetaData(uint8_t *pdata) const
	{
		memcpy(pdata, data, 14);
	}

	// be sure to call Set() before calling this method
	void SetDataStationTypes(EGnssSourceType source, EGnssStationType station)
	{
		SetDataSource(source);
		SetStationType(station);
	}

	void Set(float latitude, float longitude, bool avalid = false, float altitude = 0.0f, bool vvalid = false, float bearing = 0u, float speed = 0.0f, bool rvalid = false, float radius = 0.0f)
	{
		memset(data, 0, 14);
		if (latitude or longitude) {
			SetValidity(true, avalid, vvalid, rvalid);
		} else {
			return;
		}
		SetPosition(latitude, longitude);
		if (avalid)
			SetAltitude(altitude);
		if (vvalid)
		{
			SetBearing(bearing);
			SetSpeed(speed);
		}
		if (rvalid)
			SetRadius(radius);
	}

	EGnssSourceType GetDataSource(const uint8_t *pdata) const
	{
		switch (pdata[0] >> 4)
		{
			case 0: return EGnssSourceType::Client;
			case 1: return EGnssSourceType::OpenRTX;
			default: return EGnssSourceType::Other;
		}
	}

	EGnssStationType GetStationType(const uint8_t *pdata) const
	{
		switch (pdata[0] & 0xfu)
		{
			case 0: return EGnssStationType::Fixed;
			case 1: return EGnssStationType::Mobile;
			case 2: return EGnssStationType::HandHeld;
			default: return EGnssStationType::Other;
		}
	}

	void GetValidity(const uint8_t *pdata, bool &position, bool &altitude, bool &velocity, bool &radius) const
	{
		position = pdata[1] & 0x80u;
		altitude = pdata[1] & 0x40u;
		velocity = pdata[1] & 0x20u;
		radius   = pdata[1] & 0x10u;
	}

	unsigned GetRadius(const uint8_t *pdata) const
	{
		unsigned radius = 1;
		for (int i=((pdata[1]>>1)&7); i>0; radius<<=1, i--)
			;
		return radius;
	}

	float GetBearing(const uint8_t *pdata) const
	{
		return 256u * (pdata[1] & 0x1u) + pdata[2];
	}

	void GetPosition(const uint8_t *pdata, float &lat, float &lon) const
	{
		constexpr float a = 1.0f / (8388607.0f);
		auto nlat = (int32_t(pdata[3] << 24) + (pdata[4] << 16) + (pdata[5] << 8)) >> 8;
		lat = 90.0f * nlat * a;
		auto nlon = (int32_t(pdata[6] << 24) + (pdata[7] << 16) + (pdata[8] << 8)) >> 8;
		lon = 180.0f * nlon * a;
	}

	float GetAltitude(const uint8_t *pdata) const
	{
		return 0.5f * (256 * pdata[9] + pdata[10]) - 500.0f;
	}

	float GetSpeed(const uint8_t *pdata) const
	{
		return 0.5f * (16 * pdata[11] + (pdata[12] / 16));
	}

	const uint8_t *GetData() { return data; }

	void Dump()
	{
		unsigned i = 0;
		do {
			std::cout << std::hex << std::setw(2) << std::setfill('0') << uint32_t(data[i++]);
			std::cout << ((i<14u) ? ' ' : '\n');
		} while (i < 14u);
		bool pv, av, vv, rv;
		GetValidity(data, pv, av, vv, rv);
		auto defprec = std::cout.precision();
		if (pv)
		{
			float lat, lon;
			GetPosition(data, lat, lon);
			std::cout << std::fixed << std::setprecision(5) << "Latitude: " << lat << " Longitude: " << lon;
		}
		else
			std::cout << "Latitude & Longitude: unavailable";
		std::cout << std::setprecision(1) << "\nAltitude: ";
		if (av)
			std::cout << GetAltitude(data) << " m";
		else
			std::cout << "unavailable";
		std::cout << "\nVelocity: ";
		if (vv)
			std::cout << GetSpeed(data) << " km/h toward " << GetBearing(data) << " degrees";
		else
			std::cout << "unavailable";
		std::cout << std::dec << std::setw(0) << std::setfill(' ') << "\nRadius: ";
		if (rv)
			std::cout << GetRadius(data) << " m\n";
		else
			std::cout << "unavailable\n";
		std::cout << std::defaultfloat << std::setprecision(defprec);
	}

private:
	
	void SetDataSource(EGnssSourceType t)
	{
		data[0] &= 0xfu;
		switch (t)
		{
			case EGnssSourceType::Client:
				break;
			case EGnssSourceType::OpenRTX:
				data[0] |= 0x10u;
				break;
			default:
				data[0] |= 0xf0u;
				break;
		}
	}

	void SetStationType(EGnssStationType t)
	{
		data[0] &= 0xf0u;
		switch (t)
		{
			case EGnssStationType::Fixed:
				break;
			case EGnssStationType::Mobile:
				data[0] |= 0x1u;
				break;
			case EGnssStationType::HandHeld:
				data[0] |= 0x2u;
				break;
			default:
				data[0] |= 0xfu;
				break;
		}
	}

	void SetValidity(bool position, bool altitude, bool velocity, bool radius) {
		uint8_t val = position ? 1u : 0;
		val <<= 1;
		if (altitude) val++;
		val <<= 1;
		if (velocity) val++;
		val <<= 1;
		if (radius) val++;
		data[1] = (0xfu & data[1]) | (val << 4);
	}

	void SetRadius(float radius)
	{
		data[1] &= 0xf1u;	// clear radius bits
		if (radius <= 1.0f)
			return;	// already set
		auto r = unsigned(ceilf(log2f(radius)));
		if (r > 7)
			r = 7;
		data[1] |= r << 1;
	}

	void SetBearing(float b)
	{
		if (fabsf(b) >= 360.0f)
			b = fmodf(b, 360.0f);
		if (b < 0.0)
			b = 360.0f - b;
		auto bearing = uint16_t(b);

		data[1] &= 0xfeu; // clear the last byte of data[1]
		if (bearing > 255u) // now set it, if needed
			data[1] |= 0x1u;
		data[2] = bearing & 0xffu;
	}

	void SetPosition(float lat, float lon)
	{
		if (lat > 90.0f)
			lat = 90.0f;
		else if (lat < -90.0f)
			lat = -90.0f;
		if (lon > 180.0f)
			lon = 180.0f;
		else if (lon < -180.0f)
			lon = -180.0f;
		uint32_t frac = lroundf(8388607.0f * lat / 90.0f);
		for (int i=0; i<3; i++)
		{
			data[5-i] = frac & 0xff;
			frac >>= 8;
		}
		frac = lroundf(8388607.0f * lon / 180.0f);
		for (int i=0; i<3; i++)
		{
			data[8-i] = frac & 0xff;
			frac >>= 8;
		}
	}

	void SetAltitude(float altitude)
	{
		if (altitude < -500.0f)
			altitude = -500.0f;
		else if (altitude > 31767.5f)
			altitude = 31767.5f;
		unsigned a = lroundf(2.0f * (altitude + 500.0f));
		if (a > 65535) a = 65535;
		data[9] = a >> 8;
		data[10] = a & 0xffu;
	}

	void SetSpeed(float speed)
	{
		if (speed < 0.0f)
			speed = 0.0f;
		if (speed > 2047.5f)
			speed = 2047.5f;
		auto s = lroundf(2.0f * speed);
		data[11] = s >> 4;
		// don't touch the reserved bits
		data[12] &= 0xf0u; // clear the first nybble
		data[12] |= (s & 0xfu) << 4; // OR 4 lsbits in
	}

	// Meta data
	uint8_t data[14];
};
