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
#define GATT_RS_UUID                            0xde,0xc1,0x88,0x0a,0x99,0x45,0x29,0x68,0x5f,0xa4,0x89,0xca,0x00,0x02,0x6c,0x94
#define GATT_RS_TEMPERATURE_UUID                0xde,0xc1,0x88,0x0a,0x99,0x45,0x29,0x68,0x5f,0xa4,0x89,0xca,0x01,0x02,0x6c,0x94
#define GATT_RS_TARGET_UUID                     0xde,0xc1,0x88,0x0a,0x99,0x45,0x29,0x68,0x5f,0xa4,0x89,0xca,0x02,0x02,0x6c,0x94
#define GATT_RS_AC_HALF_FREQ_UUID               0xde,0xc1,0x88,0x0a,0x99,0x45,0x29,0x68,0x5f,0xa4,0x89,0xca,0x03,0x02,0x6c,0x94
#define GATT_RS_DUTY_UUID                       0xde,0xc1,0x88,0x0a,0x99,0x45,0x29,0x68,0x5f,0xa4,0x89,0xca,0x04,0x02,0x6c,0x94
#define GATT_RS_PROFILE_UUID                    0xde,0xc1,0x88,0x0a,0x99,0x45,0x29,0x68,0x5f,0xa4,0x89,0xca,0x05,0x02,0x6c,0x94
#define GATT_RS_NVS_PROFILE_UUID                0xde,0xc1,0x88,0x0a,0x99,0x45,0x29,0x68,0x5f,0xa4,0x89,0xca,0x06,0x02,0x6c,0x94
#define GATT_DEVICE_INFO_UUID                   0x180A
#define GATT_MANUFACTURER_NAME_UUID             0x2A29
#define GATT_MODEL_NUMBER_UUID                  0x2A24
#define GATT_TEMPERATURE_CELCIUS_UUID           0x2A1F

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
