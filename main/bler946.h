/*
 * Licensed to the Apache Software Foundation (ASF) under one
 * or more contributor license agreements.  See the NOTICE file
 * distributed with this work for additional information
 * regarding copyright ownership.  The ASF licenses this file
 * to you under the Apache License, Version 2.0 (the
 * "License"); you may not use this file except in compliance
 * with the License.  You may obtain a copy of the License at
 *
 *  http://www.apache.org/licenses/LICENSE-2.0
 *
 * Unless required by applicable law or agreed to in writing,
 * software distributed under the License is distributed on an
 * "AS IS" BASIS, WITHOUT WARRANTIES OR CONDITIONS OF ANY
 * KIND, either express or implied.  See the License for the
 * specific language governing permissions and limitations
 * under the License.
 */

#ifndef H_BLER946_
#define H_BLER946_

#include "nimble/ble.h"
#include "modlog/modlog.h"

#ifdef __cplusplus
extern "C" {
#endif

/* Reflow configuration */
#define GATT_RS_UUID                            0x09,0x46,0x02,0x00,0xc9,0xee,0x41,0x95,0x81,0xce,0x3e,0xc8,0x9c,0x3d,0x6e,0x3b
#define GATT_RS_TEMPERATURE_UUID                0x2A1F
#define GATT_RS_TARGET_UUID                     0x09,0x46,0x02,0x02,0xc9,0xee,0x41,0x95,0x81,0xce,0x3e,0xc8,0x9c,0x3d,0x6e,0x3b
#define GATT_RS_AC_HALF_FREQ_UUID               0x09,0x46,0x02,0x03,0xc9,0xee,0x41,0x95,0x81,0xce,0x3e,0xc8,0x9c,0x3d,0x6e,0x3b
#define GATT_RS_DUTY_UUID                       0x09,0x46,0x02,0x04,0xc9,0xee,0x41,0x95,0x81,0xce,0x3e,0xc8,0x9c,0x3d,0x6e,0x3b
#define GATT_RS_PROFILE_UUID                    0x09,0x46,0x02,0x05,0xc9,0xee,0x41,0x95,0x81,0xce,0x3e,0xc8,0x9c,0x3d,0x6e,0x3b
#define GATT_DEVICE_INFO_UUID                   0x180A
#define GATT_MANUFACTURER_NAME_UUID             0x2A29
#define GATT_MODEL_NUMBER_UUID                  0x2A24

extern uint16_t rs_temperature_handle;

struct ble_hs_cfg;
struct ble_gatt_register_ctxt;

void gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg);
int gatt_svr_init(void);

void bler_tx_temperature(float celcius);

#ifdef __cplusplus
}
#endif

#endif
