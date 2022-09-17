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

#include "esp_log.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>
#include <stdatomic.h>
#include "host/ble_hs.h"
#include "host/ble_uuid.h"
#include "services/gap/ble_svc_gap.h"
#include "services/gatt/ble_svc_gatt.h"
#include "bler946.h"
#include "ble_descriptor.h"
#include "controller.h"

static const char* tag = "GATT server";

static const char *manuf_name = "Reflow946";
static const char *model_num = "Reflow946 ESP32 controller";
uint16_t rs_temperature_handle;
extern uint8_t temprature_sens_read();

static int
gatt_svr_chr_access_rs_temperature(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

static int
gatt_svr_chr_access_rs_target(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

static int
gatt_svr_chr_access_rs_profile(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

static int
gatt_svr_chr_access_rs_ac_freq(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

static int
gatt_svr_chr_access_rs_duty(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

static int
gatt_svr_chr_access_device_info(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg);

static int
gatt_svr_chr_access_temperature_celcius(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg);

static int
gatt_svr_att_access_format_ac_freq(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg);

static int
gatt_svr_att_access_format_target(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg);

static int
gatt_svr_chr_write(struct os_mbuf *om, uint16_t min_len, uint16_t max_len,
                   void *dst, uint16_t *len);

static const struct ble_gatt_svc_def gatt_svr_svcs[] = {
    {
        /* Service: Reflow */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID128_DECLARE(GATT_RS_UUID),
        .characteristics = (struct ble_gatt_chr_def[])
        { {
                /* Characteristic: Tempeature measurement */
                .uuid = BLE_UUID128_DECLARE(GATT_RS_TEMPERATURE_UUID),
                .access_cb = gatt_svr_chr_access_rs_temperature,
                .val_handle = &rs_temperature_handle,
                .flags = BLE_GATT_CHR_F_NOTIFY,
                .descriptors = (struct ble_gatt_dsc_def[])
                { {
                        .uuid = BLE_UUID16_DECLARE(0x2904),
                        .att_flags = BLE_ATT_F_READ,
                        .access_cb = gatt_svr_att_access_format_target,
                    }, {
                        0,
                    }
                },
            }, {
                /* Characteristic: Temperature control */
                .uuid = BLE_UUID128_DECLARE(GATT_RS_TARGET_UUID),
                .access_cb = gatt_svr_chr_access_rs_target,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
                .descriptors = (struct ble_gatt_dsc_def[])
                { {
                        .uuid = BLE_UUID16_DECLARE(0x2904),
                        .att_flags = BLE_ATT_F_READ,
                        .access_cb = gatt_svr_att_access_format_target,
                    }, {
                        0,
                    }
                },
            }, {
                /* Characteristic: Reflow profile */
                .uuid = BLE_UUID128_DECLARE(GATT_RS_PROFILE_UUID),
                .access_cb = gatt_svr_chr_access_rs_profile,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
            }, {
                /* Characteristic: NVS Reflow profile */
                .uuid = BLE_UUID128_DECLARE(GATT_RS_NVS_PROFILE_UUID),
                .access_cb = gatt_svr_chr_access_rs_profile,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
            }, {
                /* Characteristic: AC half period */
                .uuid = BLE_UUID128_DECLARE(GATT_RS_AC_HALF_FREQ_UUID),
                .access_cb = gatt_svr_chr_access_rs_ac_freq,
                .flags = BLE_GATT_CHR_F_READ,
                .descriptors = (struct ble_gatt_dsc_def[])
                { {
                        .uuid = BLE_UUID16_DECLARE(0x2904),
                        .att_flags = BLE_ATT_F_READ,
                        .access_cb = gatt_svr_att_access_format_ac_freq,
                    }, {
                        0,
                    }
                },
            }, {
                /* Characteristic: TRIAC duty cycle */
                .uuid = BLE_UUID128_DECLARE(GATT_RS_DUTY_UUID),
                .access_cb = gatt_svr_chr_access_rs_duty,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
            }, {
                0, /* No more characteristics in this service */
            },
        }
    },

    {
        /* Service: Device Information */
        .type = BLE_GATT_SVC_TYPE_PRIMARY,
        .uuid = BLE_UUID16_DECLARE(GATT_DEVICE_INFO_UUID),
        .characteristics = (struct ble_gatt_chr_def[])
        { {
                /* Characteristic: * Manufacturer name */
                .uuid = BLE_UUID16_DECLARE(GATT_MANUFACTURER_NAME_UUID),
                .access_cb = gatt_svr_chr_access_device_info,
                .flags = BLE_GATT_CHR_F_READ,
            }, {
                /* Characteristic: Model number string */
                .uuid = BLE_UUID16_DECLARE(GATT_MODEL_NUMBER_UUID),
                .access_cb = gatt_svr_chr_access_device_info,
                .flags = BLE_GATT_CHR_F_READ,
            }, {
                /* Characteristic: Temperature celcius */
                .uuid = BLE_UUID16_DECLARE(GATT_TEMPERATURE_CELCIUS_UUID),
                .access_cb = gatt_svr_chr_access_device_info,
                .flags = BLE_GATT_CHR_F_READ,
            }, {
                0, /* No more characteristics in this service */
            },
        }
    },

    {
        0, /* No more services */
    },
};

static int
gatt_svr_chr_access_rs_temperature(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    const ble_uuid_t *uuid = ctxt->chr->uuid;

    if (ble_uuid_cmp(uuid, BLE_UUID128_DECLARE(GATT_RS_TEMPERATURE_UUID)) == 0) {
        switch (ctxt->op) {
        default:
            assert(0);
            return BLE_ATT_ERR_UNLIKELY;
        }
    }

    assert(0);
    return BLE_ATT_ERR_UNLIKELY;
}

static int
gatt_svr_chr_access_rs_target(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int16_t target;
    int rc;

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        target = atomic_load(&ato_target) * 10;
        rc = os_mbuf_append(ctxt->om, &target,
                            sizeof target);
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        rc = gatt_svr_chr_write(ctxt->om,
                                sizeof target,
                                sizeof target,
                                &target, NULL);
        reflow_stop();
        atomic_store(&ato_target, target / 10);
        return rc;

    default:
        assert(0);
        return BLE_ATT_ERR_UNLIKELY;
    }
}

static int
gatt_svr_chr_access_rs_profile(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    ble_uuid_t *uuid = ctxt->chr->uuid;
    reflow_profile_t profile = {0};
    reflow_profile_t *p_profile = &profile;
    uint16_t profile_len;
    int rc;

    switch (ctxt->op) {
    case BLE_GATT_ACCESS_OP_READ_CHR:
        if (ble_uuid_cmp(uuid, BLE_UUID128_DECLARE(GATT_RS_NVS_PROFILE_UUID)) == 0) {
            load_profile(&profile);
        } else {
            p_profile = get_profile;
        }
        rc = os_mbuf_append(ctxt->om, p_profile,
                            sizeof(reflow_profile_t));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        rc = gatt_svr_chr_write(ctxt->om,
                                2,
                                sizeof profile,
                                &profile.data, &profile_len);

        ESP_LOG_BUFFER_HEX_LEVEL(tag, profile.data, profile_len, ESP_LOG_INFO);
        for (int step = 0; step < MAX_REFLOW_STEPS; step++) {
            ESP_LOGI(tag, "%i °C for %i s", profile.data[step].temperature, profile.data[step].duration);
        }

        set_profile(&profile);
        if (ble_uuid_cmp(uuid, BLE_UUID128_DECLARE(GATT_RS_NVS_PROFILE_UUID)) == 0) {
            store_profile(&profile);
        } else {
            reflow_start();
        }
        return rc;

    default:
        assert(0);
        return BLE_ATT_ERR_UNLIKELY;
    }
}

static int
gatt_svr_chr_access_rs_ac_freq(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int rc;

    uint16_t half_ac_freq = atomic_load(&ato_half_ac_freq);
    rc = os_mbuf_append(ctxt->om, &half_ac_freq, sizeof half_ac_freq);
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int
gatt_svr_chr_access_rs_duty(uint16_t conn_handle, uint16_t attr_handle,
                                    struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int rc;

    uint32_t pulse_delay;
    switch (ctxt->op) {
#ifndef CONFIG_ZERO_CROSSING_DRIVER
    case BLE_GATT_ACCESS_OP_READ_CHR:
        atomic_load(&ato_pulse_delay);
        rc = os_mbuf_append(ctxt->om, &pulse_delay,
                            sizeof pulse_delay);
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

    case BLE_GATT_ACCESS_OP_WRITE_CHR:
        rc = gatt_svr_chr_write(ctxt->om,
                                sizeof pulse_delay,
                                sizeof pulse_delay,
                                &pulse_delay, NULL);
        atomic_store(&ato_pulse_delay, pulse_delay);
        return rc;
#endif

    default:
        assert(0);
        return BLE_ATT_ERR_UNLIKELY;
    }
}

static int
gatt_svr_chr_access_device_info(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    uint16_t uuid;
    int rc;

    uuid = ble_uuid_u16(ctxt->chr->uuid);

    if (uuid == GATT_MODEL_NUMBER_UUID) {
        rc = os_mbuf_append(ctxt->om, model_num, strlen(model_num));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    if (uuid == GATT_MANUFACTURER_NAME_UUID) {
        rc = os_mbuf_append(ctxt->om, manuf_name, strlen(manuf_name));
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    if (uuid == GATT_TEMPERATURE_CELCIUS_UUID) {
        int16_t temperature = ((temprature_sens_read() - 32) / 1.8) * 10;
        rc = os_mbuf_append(ctxt->om, &temperature, sizeof temperature);
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }

    assert(0);
    return BLE_ATT_ERR_UNLIKELY;
}

static int
gatt_svr_att_access_format_target(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int rc;
    ble2904_data_t ble2904 = {
        .format = 0x0E,
        .exponent = -1,
        .unit = 0x272F,
        .namespace = 1, // 1 = Bluetooth SIG Assigned Numbers
    };
    rc = os_mbuf_append(ctxt->om, &ble2904, sizeof(ble2904));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int
gatt_svr_att_access_format_ac_freq(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    int rc;
    ble2904_data_t ble2904 = {
        .format = 0x06,
        .exponent = -2,
        .unit = 0x2722,
        .namespace = 1, // 1 = Bluetooth SIG Assigned Numbers
    };
    rc = os_mbuf_append(ctxt->om, &ble2904, sizeof(ble2904));
    return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
}

static int
gatt_svr_chr_write(struct os_mbuf *om, uint16_t min_len, uint16_t max_len,
                   void *dst, uint16_t *len)
{
    uint16_t om_len;
    int rc;

    om_len = OS_MBUF_PKTLEN(om);
    if (om_len < min_len || om_len > max_len) {
        return BLE_ATT_ERR_INVALID_ATTR_VALUE_LEN;
    }

    rc = ble_hs_mbuf_to_flat(om, dst, max_len, len);
    if (rc != 0) {
        return BLE_ATT_ERR_UNLIKELY;
    }

    return 0;
}

void
gatt_svr_register_cb(struct ble_gatt_register_ctxt *ctxt, void *arg)
{
    char buf[BLE_UUID_STR_LEN];

    switch (ctxt->op) {
    case BLE_GATT_REGISTER_OP_SVC:
        MODLOG_DFLT(DEBUG, "registered service %s with handle=%d\n",
                    ble_uuid_to_str(ctxt->svc.svc_def->uuid, buf),
                    ctxt->svc.handle);
        break;

    case BLE_GATT_REGISTER_OP_CHR:
        MODLOG_DFLT(DEBUG, "registering characteristic %s with "
                    "def_handle=%d val_handle=%d\n",
                    ble_uuid_to_str(ctxt->chr.chr_def->uuid, buf),
                    ctxt->chr.def_handle,
                    ctxt->chr.val_handle);
        break;

    case BLE_GATT_REGISTER_OP_DSC:
        MODLOG_DFLT(DEBUG, "registering descriptor %s with handle=%d\n",
                    ble_uuid_to_str(ctxt->dsc.dsc_def->uuid, buf),
                    ctxt->dsc.handle);
        break;

    default:
        assert(0);
        break;
    }
}

int
gatt_svr_init(void)
{
    int rc;

    ble_svc_gap_init();
    ble_svc_gatt_init();

    rc = ble_gatts_count_cfg(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    rc = ble_gatts_add_svcs(gatt_svr_svcs);
    if (rc != 0) {
        return rc;
    }

    return 0;
}

