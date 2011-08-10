/*
* Copyright 2011 Anders Ma (andersma.net). All rights reserved.
*
* Redistribution and use in source and binary forms, with or without
* modification, are permitted provided that the following conditions are met:
*
* 1. Redistributions of source code must retain the above copyright
* notice, this list of conditions and the following disclaimer.
*
* 2. Redistributions in binary form must reproduce the above copyright
* notice, this list of conditions and the following disclaimer in the
* documentation and/or other materials provided with the distribution.
*
* 3. The name of the copyright holder may not be used to endorse or promote
* products derived from this software without specific prior written
* permission.
*
* THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDER ``AS IS'' AND ANY
* EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED
* WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE
* DISCLAIMED. IN NO EVENT SHALL WILLIAM TISÄTER BE LIABLE FOR ANY
* DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
* (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES;
* LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND
* ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT
* (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
* SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*
*/

#ifndef ATTITUDE_H_
#define ATTITUDE_H_

struct attitude_struct {
	// ACC & Gyro normal voltage (mv)
	int acc_nml_x;
	int acc_nml_y;
	int acc_nml_z;
	int gyro_nml_x;
	int gyro_nml_y;
	int gyro_nml_z;
	// ACC & Gyro current voltage (mv)
	int acc_crt_x;
	int acc_crt_y;
	int acc_crt_z;
	int gyro_crt_x;
	int gyro_crt_y;
	int gyro_crt_z;
	// Gyro X, Y, Z Integral
	float gyro_integral_x;
	float gyro_integral_y;
	float gyro_integral_z;
	// Acc & Gyro features
	int acc_1g; // voltage (mv) for one g
	float gyro_dps; // voltage (mv) for one degree/second
};

typedef struct attitude_struct attitude_t;

extern float pitch_angle, roll_angle, yaw_angle;
extern void attitude_init(attitude_t *atd);
extern void attitude_by_acc(attitude_t *atd);

#endif
