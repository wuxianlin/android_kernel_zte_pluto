/*
 *
 * API for sensor.
 *
 * Copyright (c) 2011, ZTE Corporation.
 *
 * history:
 *            created by lupoyuan10105246 (lu.poyuan@zte.com.cn) in 20120416
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */
 #include <linux/sensors.h>

/*
 * convert the sensor data axis to make it to be the same as device
 * input :
 *   map : the axis convert matrix
 *            bit 0 1 means revert the X axis otherwise not
 *            bit 1 1 means revert the Y axis otherwise not
 *            bit 2 1 means revert the Z axis otherwise not
 *            bit 3 1 exchange the X and Y axis  otherwise not
 *  datat : the sensor origin data point
*/
void axis_convert(unsigned long map, short *data)
{
	short tmp[3];

	/*revert the axis data*/
	tmp[0] = test_bit(0, &map) ? -data[0] : data[0];
	tmp[1] = test_bit(1, &map) ? -data[1] : data[1];
	tmp[2] = test_bit(2, &map) ? -data[2] : data[2];

	/*exchange the x and y axis*/
	data[0] = test_bit(3, &map) ? tmp[1] : tmp[0];
	data[1] = test_bit(3, &map) ? tmp[0] : tmp[1];
	data[2] = tmp[2];
}
