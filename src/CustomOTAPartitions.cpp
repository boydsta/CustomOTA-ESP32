/*
 * CustomOTA partition table access for ESP32
 *
 * CRITICAL DESIGN REQUIREMENT: NEVER USE delay() ANYWHERE!
 * See CustomOTA.cpp for the real-time compatibility rules.
 *
 * Created by Boyd Benton
 */
#ifndef CUSTOM_OTA_DEBUG
#define CUSTOM_OTA_DEBUG 0
#endif
#include "CustomOTA.h"
#include "CustomOTAPartitions.h"

#if CUSTOM_OTA_PARTITIONS

#include <esp_flash.h>
#include <esp_rom_md5.h>
#include <esp_flash_encrypt.h>
#include <esp_secure_boot.h>

namespace {

// Fills the MD5 entry that follows the partition entries, as gen_esp32part.py does:
// magic 0xEBEB, 14 bytes of 0xFF, then the MD5 of all the entries before it
void writeMd5Entry(uint8_t* sector, size_t entriesLen) {
    uint8_t* md5Entry = sector + entriesLen;
    md5Entry[0] = ESP_PARTITION_MAGIC_MD5 & 0xFF;
    md5Entry[1] = ESP_PARTITION_MAGIC_MD5 >> 8;
    memset(md5Entry + 2, 0xFF, ESP_PARTITION_MD5_OFFSET - 2);

    md5_context_t ctx;
    esp_rom_md5_init(&ctx);
    esp_rom_md5_update(&ctx, sector, entriesLen);
    esp_rom_md5_final(md5Entry + ESP_PARTITION_MD5_OFFSET, &ctx);
}

}  // namespace

bool CustomOTAPartitions::load(Table& table, String& err) {
    table.clear();
    uint8_t* buf = (uint8_t*)malloc(ESP_PARTITION_TABLE_MAX_LEN);
    if (!buf) {
        err = F("Out of memory reading the partition table");
        return false;
    }

    esp_err_t res = esp_flash_read(nullptr, buf, ESP_PARTITION_TABLE_OFFSET, ESP_PARTITION_TABLE_MAX_LEN);
    if (res != ESP_OK) {
        err = String(F("Partition table read failed: ")) + esp_err_to_name(res);
        free(buf);
        return false;
    }

    int count = 0;
    if (esp_partition_table_verify((const esp_partition_info_t*)buf, false, &count) != ESP_OK) {
        err = F("Partition table in flash is not valid");
        free(buf);
        return false;
    }

    const esp_partition_info_t* entries = (const esp_partition_info_t*)buf;
    for (size_t i = 0; i < ESP_PARTITION_TABLE_MAX_ENTRIES && entries[i].magic == ESP_PARTITION_MAGIC; i++) {
        table.push_back(entries[i]);
    }
    free(buf);
    return true;
}

bool CustomOTAPartitions::serialise(const Table& table, uint8_t* sector) {
    // Room is needed for the MD5 entry after the partition entries
    if (table.size() + 1 > ESP_PARTITION_TABLE_MAX_ENTRIES) {
        return false;
    }
    memset(sector, 0xFF, ESP_PARTITION_TABLE_SIZE);
    size_t entriesLen = table.size() * sizeof(esp_partition_info_t);
    memcpy(sector, table.data(), entriesLen);
    writeMd5Entry(sector, entriesLen);
    return true;
}

bool CustomOTAPartitions::matchesFlash(const uint8_t* sector) {
    uint8_t* buf = (uint8_t*)malloc(ESP_PARTITION_TABLE_SIZE);
    if (!buf) return false;
    bool same = esp_flash_read(nullptr, buf, ESP_PARTITION_TABLE_OFFSET, ESP_PARTITION_TABLE_SIZE) == ESP_OK &&
                memcmp(buf, sector, ESP_PARTITION_TABLE_SIZE) == 0;
    free(buf);
    return same;
}

esp_err_t CustomOTAPartitions::write(const uint8_t* sector) {
    // Encrypted or signed tables cannot be rebuilt here
    if (esp_flash_encryption_enabled() || esp_secure_boot_enabled()) {
        COTA_DEBUG_PRINTLN("CustomOTA: Partition write refused - flash encryption or secure boot enabled");
        return ESP_ERR_NOT_SUPPORTED;
    }

    // Never write a table the bootloader would reject
    int count = 0;
    if (esp_partition_table_verify((const esp_partition_info_t*)sector, true, &count) != ESP_OK) {
        COTA_DEBUG_PRINTLN("CustomOTA: Partition write refused - new table failed verification");
        return ESP_ERR_INVALID_ARG;
    }

    // The core is built with CONFIG_SPI_FLASH_DANGEROUS_WRITE_ABORTS, so a write to the
    // partition table through the default chip aborts. That check is the os_func
    // region_protected hook; a copy of the default chip without the hook keeps the normal
    // locking and cache handling but allows the write.
    esp_flash_os_functions_t osFuncs = *esp_flash_default_chip->os_func;
    osFuncs.region_protected = nullptr;
    esp_flash_t chip = *esp_flash_default_chip;
    chip.os_func = &osFuncs;

    COTA_DEBUG_PRINTF("CustomOTA: Writing partition table (%d entries) at 0x%x\n", count, ESP_PARTITION_TABLE_OFFSET);

    esp_err_t res = esp_flash_erase_region(&chip, ESP_PARTITION_TABLE_OFFSET, ESP_PARTITION_TABLE_SIZE);
    if (res == ESP_OK) {
        res = esp_flash_write(&chip, sector, ESP_PARTITION_TABLE_OFFSET, ESP_PARTITION_TABLE_SIZE);
    }
    if (res != ESP_OK) {
        COTA_DEBUG_PRINTF("CustomOTA: ERROR - Partition table write failed: %s\n", esp_err_to_name(res));
        return res;
    }

    if (!matchesFlash(sector)) {
        COTA_DEBUG_PRINTLN("CustomOTA: ERROR - Partition table read-back does not match");
        return ESP_ERR_INVALID_CRC;
    }
    COTA_DEBUG_PRINTLN("CustomOTA: Partition table written and verified");
    return ESP_OK;
}

const char* CustomOTAPartitions::typeName(uint8_t type) {
    switch (type) {
        case PART_TYPE_APP:  return "app";
        case PART_TYPE_DATA: return "data";
        default:             return "custom";
    }
}

String CustomOTAPartitions::subtypeName(uint8_t type, uint8_t subtype) {
    if (type == PART_TYPE_APP) {
        if (subtype == PART_SUBTYPE_FACTORY) return F("factory");
        if (subtype == PART_SUBTYPE_TEST) return F("test");
        if ((subtype & ~PART_SUBTYPE_OTA_MASK) == PART_SUBTYPE_OTA_FLAG) {
            return String(F("ota_")) + String(subtype & PART_SUBTYPE_OTA_MASK);
        }
    } else if (type == PART_TYPE_DATA) {
        switch (subtype) {
            case 0x00: return F("ota");
            case 0x01: return F("phy");
            case 0x02: return F("nvs");
            case 0x03: return F("coredump");
            case 0x04: return F("nvs_keys");
            case 0x05: return F("efuse");
            case 0x06: return F("undefined");
            case 0x80: return F("esphttpd");
            case 0x81: return F("fat");
            case 0x82: return F("spiffs");
            case 0x83: return F("littlefs");
        }
    }
    char hex[6];
    snprintf(hex, sizeof(hex), "0x%02x", subtype);
    return String(hex);
}

String CustomOTAPartitions::label(const esp_partition_info_t& entry) {
    char buf[sizeof(entry.label) + 1];
    memcpy(buf, entry.label, sizeof(entry.label));
    buf[sizeof(entry.label)] = '\0';
    return String(buf);
}

#endif // CUSTOM_OTA_PARTITIONS
