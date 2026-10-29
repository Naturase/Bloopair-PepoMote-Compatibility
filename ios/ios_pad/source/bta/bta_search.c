/*
 *   Copyright (C) 2021 GaryOderNichts
 *
 *   This program is free software: you can redistribute it and/or modify
 *   it under the terms of the GNU General Public License as published by
 *   the Free Software Foundation, either version 2 of the License, or
 *   (at your option) any later version.
 *
 *   This program is distributed in the hope that it will be useful,
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *   GNU General Public License for more details.
 *
 *   You should have received a copy of the GNU General Public License
 *   along with this program.  If not, see <https://www.gnu.org/licenses/>.
 */

#include <imports.h>
#include <bta/bta_api.h>
#include "controllers.h"

#define PRO_CONTROLLER_NAME "Nintendo RVL-CNT-01-UC"

void (*const real_bta_search_callback)(uint8_t event, void *p_data) = (void*) 0x11f401a8;

#define WIIMOTE_NAME "Nintendo RVL-CNT-01"
#define BTM_COD_MAJOR_PHONE 0x02

// Devices found during this search that announce themselves as phones (PepoMote)
static BD_ADDR phone_addrs[8];
static uint8_t phone_count = 0;

static int isKnownPhone(const uint8_t* addr)
{
    for (int i = 0; i < phone_count; i++) {
        if (memcmp(phone_addrs[i], addr, sizeof(BD_ADDR)) == 0) {
            return 1;
        }
    }
    return 0;
}

void bta_search_callback(uint8_t event, void *p_data)
{
    DEBUG_PRINT("bta_search_callback called %u %p\n", event, p_data);

    switch (event) {
    case BTA_DM_INQ_RES_EVT: {
        tBTA_DM_INQ_RES* res = (tBTA_DM_INQ_RES*) p_data;

        // If this is a BLE device don't do remote name lookup (not implemented)
        if (res->device_type == BT_DEVICE_TYPE_BLE) {
            res->remt_name_not_required = 1;
        }

        // PepoMote: remember phones so they are never registered as Pro Controllers
        if ((res->dev_class[1] & BTM_COD_MAJOR_CLASS_MASK) == BTM_COD_MAJOR_PHONE &&
            !isKnownPhone(res->bd_addr)) {
            memcpy(phone_addrs[phone_count % 8], res->bd_addr, sizeof(BD_ADDR));
            if (phone_count < 8) {
                phone_count++;
            }
        }
        break;
    }
    
    case BTA_DM_DISC_RES_EVT: {
        tBTA_DM_DISC_RES* res = (tBTA_DM_DISC_RES*) p_data;

        if (res->result == 0 && isKnownPhone(res->bd_addr)) {
            // PepoMote: a phone is always registered as a Wii Remote,
            // even if its name did not arrive in time
            DEBUG_PRINT("[%s] phone \"%s\", registering as Wii Remote\n",
                bdaddr_to_string(res->bd_addr), res->bd_name);
            memcpy(res->bd_name, WIIMOTE_NAME, sizeof(WIIMOTE_NAME));
        } else if (res->result == 0 && !isOfficialName((const char*) res->bd_name)) {
            DEBUG_PRINT("[%s] \"%s\" is non official, replacing name...\n",
                bdaddr_to_string(res->bd_addr), res->bd_name);
            // replace device name
            memcpy(res->bd_name, PRO_CONTROLLER_NAME, sizeof(PRO_CONTROLLER_NAME));
        }
        break;
    }
    default:
        break;
    }

    real_bta_search_callback(event, p_data);
}

static uint8_t current_inq_mode = BTM_LIMITED_INQUIRY;

void (*const real_BTA_DmSearch)(tBTA_DM_INQ *p_dm_inq, uint32_t services, void *p_cback) = (void*) 0x11f04c7c;
void BTA_DmSearch_hook(tBTA_DM_INQ *p_dm_inq, uint32_t services, void *p_cback)
{
    DEBUG_PRINT("BTA_DmSearch_hook %ds (%s)\n", p_dm_inq->duration,
        (current_inq_mode == BTM_LIMITED_INQUIRY) ? "BTM_LIMITED_INQUIRY" : "BTM_GENERAL_INQUIRY");

    // switch between limited and general inquiry to make sure all BR/EDR devices are covered
    // BLE is always enabled
    p_dm_inq->mode = current_inq_mode | BTM_BLE_GENERAL_INQUIRY;
    current_inq_mode = (current_inq_mode == BTM_LIMITED_INQUIRY) ? BTM_GENERAL_INQUIRY : BTM_LIMITED_INQUIRY;

    // only search for peripherals
    // BLE filtering happens in `btm_ble_is_discoverable_hook`
    p_dm_inq->filter_type = BTM_FILTER_COND_DEVICE_CLASS;
    p_dm_inq->filter_cond.dev_class_cond.dev_class_mask[0] = 0;
    p_dm_inq->filter_cond.dev_class_cond.dev_class_mask[1] = BTM_COD_MAJOR_CLASS_MASK;
    p_dm_inq->filter_cond.dev_class_cond.dev_class_mask[2] = 0;
    p_dm_inq->filter_cond.dev_class_cond.dev_class[0] = 0;
    p_dm_inq->filter_cond.dev_class_cond.dev_class[1] = BTM_COD_MAJOR_PERIPHERAL;
    p_dm_inq->filter_cond.dev_class_cond.dev_class[2] = 0;

    real_BTA_DmSearch(p_dm_inq, services, p_cback);
}
