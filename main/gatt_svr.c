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

static int
gatt_svr_chr_access_reflow(uint16_t conn_handle, uint16_t attr_handle,
                               struct ble_gatt_access_ctxt *ctxt, void *arg);

static int
gatt_svr_chr_access_device_info(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg);

static int
gatt_svr_att_access_presentation_format(uint16_t conn_handle, uint16_t attr_handle,
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
                .uuid = BLE_UUID16_DECLARE(GATT_RS_TEMPERATURE_UUID),
                .access_cb = gatt_svr_chr_access_reflow,
                .val_handle = &rs_temperature_handle,
                .flags = BLE_GATT_CHR_F_NOTIFY,
            }, {
                /* Characteristic: Temperature control */
                .uuid = BLE_UUID128_DECLARE(GATT_RS_TARGET_UUID),
                .access_cb = gatt_svr_chr_access_reflow,
                .flags = BLE_GATT_CHR_F_READ | BLE_GATT_CHR_F_WRITE,
            }, {
                /* Characteristic: Reflow profile */
                .uuid = BLE_UUID128_DECLARE(GATT_RS_PROFILE_UUID),
                .access_cb = gatt_svr_chr_access_reflow,
                .flags = BLE_GATT_CHR_F_WRITE,
            }, {
                /* Characteristic: AC half period */
                .uuid = BLE_UUID128_DECLARE(GATT_RS_PERIOD_UUID),
                .access_cb = gatt_svr_chr_access_reflow,
                .flags = BLE_GATT_CHR_F_READ,
                .descriptors = (struct ble_gatt_dsc_def[])
                { {
                        .uuid = BLE_UUID16_DECLARE(0x2904),
                        .att_flags = BLE_ATT_F_READ,
                        .access_cb = gatt_svr_att_access_presentation_format,
                    }, {
                        0,
                    }

                },
            }, {
                /* Characteristic: TRIAC duty cycle */
                .uuid = BLE_UUID128_DECLARE(GATT_RS_DUTY_UUID),
                .access_cb = gatt_svr_chr_access_reflow,
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
                0, /* No more characteristics in this service */
            },
        }
    },

    {
        0, /* No more services */
    },
};

static int
gatt_svr_chr_access_reflow(uint16_t conn_handle, uint16_t attr_handle,
                                struct ble_gatt_access_ctxt *ctxt, void *arg)
{
    ble_uuid_t *uuid = ctxt->chr->uuid;
    int rc;

    if (ble_uuid_cmp(uuid, BLE_UUID128_DECLARE(GATT_RS_PERIOD_UUID)) == 0) {
        uint32_t pval = 0;//atomic_load(&period);
        rc = os_mbuf_append(ctxt->om, &pval, 4);
        return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;
    }
    if (ble_uuid_cmp(uuid, BLE_UUID128_DECLARE(GATT_RS_PROFILE_UUID)) == 0) {
        switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            ;
            uint16_t profile[10];
            uint16_t profile_len;
            rc = gatt_svr_chr_write(ctxt->om,
                                    2,
                                    sizeof profile,
                                    &profile, &profile_len);
            ESP_LOG_BUFFER_HEX_LEVEL(tag, profile, profile_len, ESP_LOG_INFO);
            return rc;
        default:
            assert(0);
            return BLE_ATT_ERR_UNLIKELY;
        }
    }
    if (ble_uuid_cmp(uuid, BLE_UUID128_DECLARE(GATT_RS_DUTY_UUID)) == 0) {
        uint32_t pulse;
        switch (ctxt->op) {
        case BLE_GATT_ACCESS_OP_READ_CHR:
            pulse = 0;// atomic_load(&pulse_duration);
            rc = os_mbuf_append(ctxt->om, &pulse,
                                sizeof pulse);
            return rc == 0 ? 0 : BLE_ATT_ERR_INSUFFICIENT_RES;

        case BLE_GATT_ACCESS_OP_WRITE_CHR:
            rc = gatt_svr_chr_write(ctxt->om,
                                    sizeof pulse,
                                    sizeof pulse,
                                    &pulse, NULL);
            // atomic_store(&pulse_duration, pulse);
            return rc;

        default:
            assert(0);
            return BLE_ATT_ERR_UNLIKELY;
        }
    }

    assert(0);
    return BLE_ATT_ERR_UNLIKELY;
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

    assert(0);
    return BLE_ATT_ERR_UNLIKELY;
}

static int
gatt_svr_att_access_presentation_format(uint16_t conn_handle,
                                     uint16_t attr_handle,
                                     struct ble_gatt_access_ctxt *ctxt,
                                     void *arg)
{
    int rc;
    ble2904_data_t period_dsc = {
	    .format = 6,
	    .exponent = 2,
	    .unit = 0x2722,
	    .namespace = 1, // 1 = Bluetooth SIG Assigned Numbers
    };
    rc = os_mbuf_append(ctxt->om, &period_dsc, sizeof(period_dsc));
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

    // ble_svc_gap_init();
    // ble_svc_gatt_init();

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

