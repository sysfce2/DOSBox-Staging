/*
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Copyright (C) 2020-2022  The DOSBox Staging Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License along
 *  with this program; if not, write to the Free Software Foundation, Inc.,
 *  51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 */

#include "math_utils.h"

#include <cstdint>
#include <gtest/gtest.h>

namespace {

TEST(Support_left_shift_signed, Positive)
{
	// shifting zero ...
	int8_t var_8bit = 0;
	int16_t var_16bit = 0;
	int32_t var_32bit = 0;
	// by zero
	EXPECT_EQ(left_shift_signed(var_8bit, 0), 0);
	EXPECT_EQ(left_shift_signed(var_16bit, 0), 0);
	EXPECT_EQ(left_shift_signed(var_32bit, 0), 0);

	// shifting one ...
	var_8bit = 1;
	var_16bit = 1;
	var_32bit = 1;

	// by four
	EXPECT_EQ(left_shift_signed(var_8bit, 4), 16);
	EXPECT_EQ(left_shift_signed(var_16bit, 4), 16);
	EXPECT_EQ(left_shift_signed(var_32bit, 4), 16);

	// by max signed bits
	EXPECT_EQ(left_shift_signed(var_8bit, 6), 64);
	EXPECT_EQ(left_shift_signed(var_16bit, 14), 16384);
	EXPECT_EQ(left_shift_signed(var_32bit, 30), 1073741824);

	// max shiftable value before overflow
	var_8bit = INT8_MAX / 2;
	var_16bit = INT16_MAX / 2;
	var_32bit = INT32_MAX / 2;

	EXPECT_EQ(left_shift_signed(var_8bit, 1), INT8_MAX - 1);
	EXPECT_EQ(left_shift_signed(var_16bit, 1), INT16_MAX - 1);
	EXPECT_EQ(left_shift_signed(var_32bit, 1), INT32_MAX - 1);
}

TEST(Support_left_shift_signed, PositiveOverflow)
{
	int8_t var_8bit = INT8_MAX;
	int16_t var_16bit = INT16_MAX;
	int32_t var_32bit = INT32_MAX;

	EXPECT_DEBUG_DEATH({ left_shift_signed(var_8bit, 1); }, "");
	EXPECT_DEBUG_DEATH({ left_shift_signed(var_16bit, 1); }, "");
	EXPECT_DEBUG_DEATH({ left_shift_signed(var_32bit, 1); }, "");

	var_8bit = 1;
	var_16bit = 1;
	var_32bit = 1;

	EXPECT_DEBUG_DEATH({ left_shift_signed(var_8bit, 7); }, "");
	EXPECT_DEBUG_DEATH({ left_shift_signed(var_16bit, 15); }, "");
	EXPECT_DEBUG_DEATH({ left_shift_signed(var_32bit, 31); }, "");
}

TEST(Support_left_shift_signed, Negative)
{
	// shifting negative one ...
	int8_t var_8bit = -1;
	int16_t var_16bit = -1;
	int32_t var_32bit = -1;

	// by four
	EXPECT_EQ(left_shift_signed(var_8bit, 4), -16);
	EXPECT_EQ(left_shift_signed(var_16bit, 4), -16);
	EXPECT_EQ(left_shift_signed(var_32bit, 4), -16);

	// by max signed bits
	EXPECT_EQ(left_shift_signed(var_8bit, 7), INT8_MIN);
	EXPECT_EQ(left_shift_signed(var_16bit, 15), INT16_MIN);
	EXPECT_EQ(left_shift_signed(var_32bit, 31), INT32_MIN);

	// max shiftable value before overflow
	var_8bit = INT8_MIN / 2;
	var_16bit = INT16_MIN / 2;
	var_32bit = INT32_MIN / 2;

	EXPECT_EQ(left_shift_signed(var_8bit, 1), INT8_MIN);
	EXPECT_EQ(left_shift_signed(var_16bit, 1), INT16_MIN);
	EXPECT_EQ(left_shift_signed(var_32bit, 1), INT32_MIN);
}

TEST(Support_left_shift_signed, NegativeOverflow)
{
	int8_t var_8bit = INT8_MIN;
	int16_t var_16bit = INT16_MIN;
	int32_t var_32bit = INT32_MIN;

	EXPECT_DEBUG_DEATH({ left_shift_signed(var_8bit, 1); }, "");
	EXPECT_DEBUG_DEATH({ left_shift_signed(var_16bit, 1); }, "");
	EXPECT_DEBUG_DEATH({ left_shift_signed(var_32bit, 1); }, "");

	var_8bit = -1;
	var_16bit = -1;
	var_32bit = -1;

	EXPECT_DEBUG_DEATH({ left_shift_signed(var_8bit, 8); }, "");
	EXPECT_DEBUG_DEATH({ left_shift_signed(var_16bit, 16); }, "");
	EXPECT_DEBUG_DEATH({ left_shift_signed(var_32bit, 32); }, "");

	// Shift a negative number of bits
	EXPECT_DEBUG_DEATH({ left_shift_signed(var_8bit, -1); }, "");
	EXPECT_DEBUG_DEATH({ left_shift_signed(var_16bit, -100); }, "");
	EXPECT_DEBUG_DEATH({ left_shift_signed(var_32bit, -10000); }, "");
}

TEST(iroundf, valid)
{
	EXPECT_EQ(iroundf(0.0f), 0);

	EXPECT_EQ(iroundf(0.00000000001f), 0);
	EXPECT_EQ(iroundf(-0.00000000001f), 0);

	EXPECT_EQ(iroundf(0.5f), 1);
	EXPECT_EQ(iroundf(-0.5f), -1);

	EXPECT_EQ(iroundf(0.50001f), 1);
	EXPECT_EQ(iroundf(-0.50001f), -1);

	EXPECT_EQ(iroundf(0.499999f), 0);
	EXPECT_EQ(iroundf(-0.499999f), 0);

	assert(iroundf(1000000.4f) == 1000000);
	assert(iroundf(-1000000.4f) == -1000000);

	assert(iroundf(1000000.5f) == 1000001);
	assert(iroundf(-1000000.5f) == -1000001);
}

TEST(iroundf, invalid)
{
	EXPECT_DEBUG_DEATH({ iroundf(80000000000.0f); }, "");
	EXPECT_DEBUG_DEATH({ iroundf(-80000000000.0f); }, "");
}

TEST(clamp_to_int8, signed_negatives)
{
	EXPECT_EQ(clamp_to_int8(INT16_MIN), INT8_MIN);
	EXPECT_EQ(clamp_to_int8(INT8_MIN - 0), INT8_MIN);
	EXPECT_EQ(clamp_to_int8(INT8_MIN + 1), INT8_MIN + 1);
}

TEST(clamp_to_int16, signed_negatives)
{
	EXPECT_EQ(clamp_to_int16(INT32_MIN), INT16_MIN);
	EXPECT_EQ(clamp_to_int16(INT16_MIN - 0), INT16_MIN);
	EXPECT_EQ(clamp_to_int16(INT16_MIN + 1), INT16_MIN + 1);
}

TEST(clamp_to_int32, signed_negatives)
{
	EXPECT_EQ(clamp_to_int32(INT64_MIN), INT32_MIN);
	EXPECT_EQ(clamp_to_int32(static_cast<int64_t>(INT32_MIN - 0)), INT32_MIN);
	EXPECT_EQ(clamp_to_int32(static_cast<int64_t>(INT32_MIN + 1)), INT32_MIN + 1);
}

TEST(clamp_to_int8, signed_positives)
{
	EXPECT_EQ(clamp_to_int8(INT16_MAX), INT8_MAX);
	EXPECT_EQ(clamp_to_int8(INT8_MAX - 0), INT8_MAX);
	EXPECT_EQ(clamp_to_int8(INT16_MAX - 1), INT8_MAX);
}

TEST(clamp_to_int16, signed_positives)
{
	EXPECT_EQ(clamp_to_int16(INT32_MAX), INT16_MAX);
	EXPECT_EQ(clamp_to_int16(INT16_MAX - 0), INT16_MAX);
	EXPECT_EQ(clamp_to_int16(INT32_MAX - 1), INT16_MAX);
}

TEST(clamp_to_int32, signed_positives)
{
	EXPECT_EQ(clamp_to_int32(INT64_MAX), INT32_MAX);
	EXPECT_EQ(clamp_to_int32(INT64_MAX - static_cast<int64_t>(0)), INT32_MAX);
	EXPECT_EQ(clamp_to_int32(INT64_MAX - static_cast<int64_t>(1)), INT32_MAX);
}

TEST(clamp_to_int8, signed_literals)
{
	EXPECT_EQ(clamp_to_int8(-1'000), INT8_MIN);
	EXPECT_EQ(clamp_to_int8(-100), -100);
	EXPECT_EQ(clamp_to_int8(-1), -1);
	EXPECT_EQ(clamp_to_int8(0), 0);
	EXPECT_EQ(clamp_to_int8(1), 1);
	EXPECT_EQ(clamp_to_int8(100), 100);
	EXPECT_EQ(clamp_to_int8(1'000), INT8_MAX);
}

TEST(clamp_to_int16, signed_literals)
{
	EXPECT_EQ(clamp_to_int16(-100'000), INT16_MIN);
	EXPECT_EQ(clamp_to_int16(-10'000), -10000);
	EXPECT_EQ(clamp_to_int16(-1'000), -1000);
	EXPECT_EQ(clamp_to_int16(-10), -10);
	EXPECT_EQ(clamp_to_int16(0), 0);
	EXPECT_EQ(clamp_to_int16(10), 10);
	EXPECT_EQ(clamp_to_int16(1'000), 1000);
	EXPECT_EQ(clamp_to_int16(10'000), 10000);
	EXPECT_EQ(clamp_to_int16(100'000), INT16_MAX);
}

TEST(clamp_to_int32, signed_literals)
{
	EXPECT_EQ(clamp_to_int32(-10'000'000'000), INT32_MIN);
	EXPECT_EQ(clamp_to_int32(static_cast<int64_t>(-1'000'000'000)),
	          -1'000'000'000);
	EXPECT_EQ(clamp_to_int32(static_cast<int64_t>(-1'000'000)), -1'000'000);
	EXPECT_EQ(clamp_to_int32(static_cast<int64_t>(-100)), -100);
	EXPECT_EQ(clamp_to_int32(0u), 0);
	EXPECT_EQ(clamp_to_int32(100u), 100);
	EXPECT_EQ(clamp_to_int32(1'000'000u), 1'000'000);
	EXPECT_EQ(clamp_to_int32(1'000'000'000u), 1'000'000'000);
	EXPECT_EQ(clamp_to_int32(10'000'000'000u), INT32_MAX);
}

#ifndef UINT8_MIN
constexpr uint8_t UINT8_MIN = {0};
#endif

#ifndef UINT16_MIN
constexpr uint16_t UINT16_MIN = {0};
#endif

#ifndef UINT32_MIN
constexpr uint32_t UINT32_MIN = {0};
#endif

#ifndef UINT64_MIN
constexpr uint64_t UINT64_MIN = {0};
#endif

TEST(clamp_to_int8, unsigned_minimums)
{
	EXPECT_EQ(clamp_to_int8(UINT16_MIN), 0);
	EXPECT_EQ(clamp_to_int8(UINT8_MIN - 0u), 0);
	EXPECT_EQ(clamp_to_int8(UINT8_MIN + 1u), 1);
}

TEST(clamp_to_int16, unsigned_minimums)
{
	EXPECT_EQ(clamp_to_int16(UINT32_MIN), 0);
	EXPECT_EQ(clamp_to_int16(UINT16_MIN - 0u), 0);
	EXPECT_EQ(clamp_to_int16(UINT16_MIN + 1u), 1);
}

TEST(clamp_to_int32, unsigned_minimums)
{
	EXPECT_EQ(clamp_to_int32(UINT64_MIN), 0);
	EXPECT_EQ(clamp_to_int32(UINT32_MIN - 0u), 0);
	EXPECT_EQ(clamp_to_int32(UINT32_MIN + 1u), 1);
}

TEST(clamp_to_int8, unsigned_maximums)
{
	EXPECT_EQ(clamp_to_int8(UINT16_MAX), INT8_MAX);
	EXPECT_EQ(clamp_to_int8(UINT8_MAX - 0u), INT8_MAX);
	EXPECT_EQ(clamp_to_int8(INT16_MAX - 1u), INT8_MAX);
}

TEST(clamp_to_int16, unsigned_maximums)
{
	EXPECT_EQ(clamp_to_int16(UINT32_MAX), INT16_MAX);
	EXPECT_EQ(clamp_to_int16(UINT16_MAX - 0u), INT16_MAX);
	EXPECT_EQ(clamp_to_int16(UINT32_MAX - 1u), INT16_MAX);
}

TEST(clamp_to_int32, unsigned_maximums)
{
	EXPECT_EQ(clamp_to_int32(UINT64_MAX), INT32_MAX);
	EXPECT_EQ(clamp_to_int32(UINT32_MAX - 0u), INT32_MAX);
	EXPECT_EQ(clamp_to_int32(UINT64_MAX - 1u), INT32_MAX);
}

TEST(clamp_to_int8, unsigned_literals)
{
	EXPECT_EQ(clamp_to_int8(0u), 0);
	EXPECT_EQ(clamp_to_int8(1u), 1);
	EXPECT_EQ(clamp_to_int8(100u), 100);
	EXPECT_EQ(clamp_to_int8(1'000u), INT8_MAX);
}

TEST(clamp_to_int16, unsigned_literals)
{
	EXPECT_EQ(clamp_to_int16(0u), 0);
	EXPECT_EQ(clamp_to_int16(10u), 10);
	EXPECT_EQ(clamp_to_int16(1'000u), 1000);
	EXPECT_EQ(clamp_to_int16(10'000u), 10'000);
	EXPECT_EQ(clamp_to_int16(100'000u), INT16_MAX);
}

TEST(clamp_to_int32, unsigned_literals)
{
	EXPECT_EQ(clamp_to_int32(0u), 0);
	EXPECT_EQ(clamp_to_int32(100u), 100);
	EXPECT_EQ(clamp_to_int32(1'000'000u), 1'000'000);
	EXPECT_EQ(clamp_to_int32(1'000'000'000u), 1'000'000'000);
	EXPECT_EQ(clamp_to_int32(10'000'000'000u), INT32_MAX);
}

TEST(in_range, value_in_bounds)
{
	// small positive and negative bounds
	auto result = in_range<-1, 0>(-1);
	EXPECT_TRUE(result);

	result = in_range<-1, 0>(0);
	EXPECT_TRUE(result);

	result = in_range<-1, 1>(0);
	EXPECT_TRUE(result);

	result = in_range<0, 1>(0);
	EXPECT_TRUE(result);

	result = in_range<0, 1>(1);
	EXPECT_TRUE(result);

	// large positive and negative bounds
	result = in_range<-1'000'000'000, 1'000'000'000>(-999'999'999);
	EXPECT_TRUE(result);

	result = in_range<-1'000'000'000, 1'000'000'000>(0);
	EXPECT_TRUE(result);

	result = in_range<-1'000'000'000, 1'000'000'000>(999'999'999);
	EXPECT_TRUE(result);

	// positive-only bounds
	result = in_range<1, 2>(1);
	EXPECT_TRUE(result);

	result = in_range<1, 2>(2);
	EXPECT_TRUE(result);

	result = in_range<1, 3>(2);
	EXPECT_TRUE(result);

	result = in_range<999'000'000, 1'000'000'000>(999'000'000);
	EXPECT_TRUE(result);

	result = in_range<999'000'000, 1'000'000'000>(999'500'000);
	EXPECT_TRUE(result);

	result = in_range<999'000'000, 1'000'000'000>(1'000'000'000);
	EXPECT_TRUE(result);

	// negative-only bounds
	result = in_range<-2, -1>(-2);
	EXPECT_TRUE(result);

	result = in_range<-3, -1>(-2);
	EXPECT_TRUE(result);

	result = in_range<-2, -1>(-1);
	EXPECT_TRUE(result);

	result = in_range<-1'000'000'000, -999'000'000>(-1'000'000'000);
	EXPECT_TRUE(result);

	result = in_range<-1'000'000'000, -999'000'000>(-999'500'000);
	EXPECT_TRUE(result);

	result = in_range<-1'000'000'000, -999'000'000>(-999'000'000);
	EXPECT_TRUE(result);
}

TEST(in_range, value_out_of_bounds)
{
	// small positive and negative bounds
	auto result = in_range<-1, 1>(-2);
	EXPECT_FALSE(result);

	result = in_range<-1, 0>(1);
	EXPECT_FALSE(result);

	result = in_range<-1, 1>(2);
	EXPECT_FALSE(result);

	// large positive and negative bounds
	result = in_range<-1'000'000'000, 1'000'000'000>(-1'000'000'001);
	EXPECT_FALSE(result);

	result = in_range<-1'000'000'000, 1'000'000'000>(1'000'000'001);
	EXPECT_FALSE(result);

	// positive-only bounds
	result = in_range<1, 3>(0);
	EXPECT_FALSE(result);

	result = in_range<1, 2>(3);
	EXPECT_FALSE(result);

	result = in_range<999'000'000, 1'000'000'000>(998'999'999);
	EXPECT_FALSE(result);

	result = in_range<999'000'000, 1'000'000'000>(-1'000'000'000);
	EXPECT_FALSE(result);

	result = in_range<999'000'000, 1'000'000'000>(1'000'000'001);
	EXPECT_FALSE(result);

	result = in_range<999'000'000, 1'000'000'000>(INT32_MAX);
	EXPECT_FALSE(result);

	result = in_range<999'000'000, 1'000'000'000>(INT32_MIN);
	EXPECT_FALSE(result);

	// negative-only bounds
	result = in_range<-3, -1>(-4);
	EXPECT_FALSE(result);

	result = in_range<-2, -1>(-3);
	EXPECT_FALSE(result);

	result = in_range<-2, -1>(0);
	EXPECT_FALSE(result);

	result = in_range<-1'000'000'000, -999'000'000>(-1'000'000'001);
	EXPECT_FALSE(result);

	result = in_range<-1'000'000'000, -999'000'000>(-998'999'999);
	EXPECT_FALSE(result);

	result = in_range<-1'000'000'000, -999'000'000>(INT32_MAX);
	EXPECT_FALSE(result);

	result = in_range<-1'000'000'000, -999'000'000>(-INT32_MAX);
	EXPECT_FALSE(result);
}

TEST(in_range, small_types_value_in_bounds)
{
	// small positive and negative bounds
	auto result = in_range<-1, 1>(static_cast<uint8_t>(0u));
	EXPECT_TRUE(result);

	result = in_range<-1, 0>(static_cast<uint8_t>(0u));
	EXPECT_TRUE(result);

	result = in_range<-1, 1>(static_cast<int8_t>(1));
	EXPECT_TRUE(result);

	// large positive and negative bounds
	result = in_range<-1000, 1000>(static_cast<int16_t>(-1000));
	EXPECT_TRUE(result);

	result = in_range<-1000, 1000>(static_cast<uint16_t>(0));
	EXPECT_TRUE(result);

	result = in_range<-1000, 1000>(static_cast<uint16_t>(1000));
	EXPECT_TRUE(result);

	// positive-only bounds
	result = in_range<1, 3>(static_cast<uint8_t>(1u));
	EXPECT_TRUE(result);

	result = in_range<1, 2>(static_cast<uint8_t>(2u));
	EXPECT_TRUE(result);

	result = in_range<999, 1000>(static_cast<uint16_t>(999u));
	EXPECT_TRUE(result);

	result = in_range<0, 1000>(static_cast<int16_t>(500));
	EXPECT_TRUE(result);

	result = in_range<999, 1000>(static_cast<uint16_t>(1000u));
	EXPECT_TRUE(result);

	result = in_range<20000, 40000>(static_cast<int16_t>(INT16_MAX));
	EXPECT_TRUE(result);

	result = in_range<60000, 80000>(static_cast<uint16_t>(UINT16_MAX));
	EXPECT_TRUE(result);

	result = in_range<0, INT16_MAX>(static_cast<int16_t>(0));
	EXPECT_TRUE(result);

	result = in_range<0, INT16_MAX>(static_cast<int16_t>(INT16_MAX / 2));
	EXPECT_TRUE(result);

	result = in_range<0, INT16_MAX>(static_cast<int16_t>(INT16_MAX));
	EXPECT_TRUE(result);

	// negative-only bounds
	result = in_range<-3, -1>(static_cast<int8_t>(-2));
	EXPECT_TRUE(result);

	result = in_range<-2, -1>(static_cast<int8_t>(-2));
	EXPECT_TRUE(result);

	result = in_range<-2, -1>(static_cast<int8_t>(-1));
	EXPECT_TRUE(result);

	result = in_range<-1000, -999>(static_cast<int16_t>(-1000));
	EXPECT_TRUE(result);

	result = in_range<-1000, -999>(static_cast<int16_t>(-999));
	EXPECT_TRUE(result);

	result = in_range<-40000, -20000>(static_cast<int16_t>(INT16_MIN));
	EXPECT_TRUE(result);

	result = in_range<INT16_MIN, 0>(static_cast<int16_t>(INT16_MIN));
	EXPECT_TRUE(result);

	result = in_range<INT16_MIN, 0>(static_cast<int16_t>(INT16_MIN / 2));
	EXPECT_TRUE(result);

	result = in_range<INT16_MIN, 0>(static_cast<int16_t>(0));
	EXPECT_TRUE(result);
}

TEST(in_range, small_types_value_out_of_bounds)
{
	// small positive and negative bounds
	auto result = in_range<-1, 1>(static_cast<uint8_t>(2u));
	EXPECT_FALSE(result);

	result = in_range<-1, 0>(static_cast<uint8_t>(1u));
	EXPECT_FALSE(result);

	result = in_range<-1, 1>(static_cast<uint8_t>(2u));
	EXPECT_FALSE(result);

	// large positive and negative bounds
	result = in_range<-1000, 1000>(static_cast<int16_t>(-1001));
	EXPECT_FALSE(result);

	result = in_range<-1000, 1000>(static_cast<uint16_t>(1001));
	EXPECT_FALSE(result);

	// positive-only bounds
	result = in_range<1, 3>(static_cast<uint8_t>(0));
	EXPECT_FALSE(result);

	result = in_range<1, 2>(static_cast<uint8_t>(3));
	EXPECT_FALSE(result);

	result = in_range<999, 1000>(static_cast<uint16_t>(998));
	EXPECT_FALSE(result);

	result = in_range<999, 1000>(static_cast<int16_t>(-1000));
	EXPECT_FALSE(result);

	result = in_range<999, 1000>(static_cast<uint16_t>(1001));
	EXPECT_FALSE(result);

	result = in_range<999, 1000>(static_cast<int16_t>(INT16_MIN));
	EXPECT_FALSE(result);

	result = in_range<999, 1000>(static_cast<int16_t>(INT16_MAX));
	EXPECT_FALSE(result);

	// negative-only bounds
	result = in_range<-3, -1>(static_cast<int8_t>(-4));
	EXPECT_FALSE(result);

	result = in_range<-2, -1>(static_cast<int8_t>(-3));
	EXPECT_FALSE(result);

	result = in_range<-2, -1>(static_cast<uint8_t>(0));
	EXPECT_FALSE(result);

	result = in_range<-1000, -999>(static_cast<int16_t>(-1001));
	EXPECT_FALSE(result);

	result = in_range<-1000, -999>(static_cast<int16_t>(-998));
	EXPECT_FALSE(result);

	result = in_range<-1000, -999>(static_cast<int16_t>(INT16_MIN));
	EXPECT_FALSE(result);

	result = in_range<-1000, -999>(-static_cast<int16_t>(INT16_MAX));
	EXPECT_FALSE(result);
}

} // namespace
