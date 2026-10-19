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
#include <stdarg.h>

namespace {

const uint32_t SECTOR = 0x1000;
const uint32_t APP_ALIGN = 0x10000;
// Erasing more than this in a web request would trip the AsyncTCP watchdog
const uint32_t MAX_APPLY_ERASE = 0x40000;

// The core is built with CONFIG_SPI_FLASH_DANGEROUS_WRITE_ABORTS, so writes to the
// partition table or the running app's partition through the default chip abort. That
// check is the os_func region_protected hook; a copy of the default chip without the hook
// keeps the normal locking and cache handling but allows the write. Callers are
// responsible for not touching the running firmware.
struct UnprotectedChip {
    esp_flash_os_functions_t osFuncs;
    esp_flash_t chip;
    UnprotectedChip() {
        osFuncs = *esp_flash_default_chip->os_func;
        osFuncs.region_protected = nullptr;
        chip = *esp_flash_default_chip;
        chip.os_func = &osFuncs;
    }
};

String fmt(const char* format, ...) {
    char buf[256];
    va_list args;
    va_start(args, format);
    vsnprintf(buf, sizeof(buf), format, args);
    va_end(args);
    return String(buf);
}

String kb(uint32_t bytes) {
    return String(bytes / 1024) + " KB";
}

bool isOtaApp(const esp_partition_info_t& e) {
    return e.type == PART_TYPE_APP && (e.subtype & ~PART_SUBTYPE_OTA_MASK) == PART_SUBTYPE_OTA_FLAG;
}

bool isFilesystem(const esp_partition_info_t& e) {
    return e.type == PART_TYPE_DATA && (e.subtype == 0x81 || e.subtype == 0x82 || e.subtype == 0x83);
}

const esp_partition_info_t* find(const CustomOTAPartitions::Table& table, uint8_t type, uint8_t subtype) {
    for (const esp_partition_info_t& e : table) {
        if (e.type == type && e.subtype == subtype) return &e;
    }
    return nullptr;
}

// What the bootloader starts when otadata is erased: factory if there is one, else ota_0
const esp_partition_info_t* defaultBoot(const CustomOTAPartitions::Table& table) {
    const esp_partition_info_t* factory = find(table, PART_TYPE_APP, PART_SUBTYPE_FACTORY);
    return factory ? factory : find(table, PART_TYPE_APP, PART_SUBTYPE_OTA_FLAG);
}

// True when current has no partition of the same type, subtype, offset and size as e
bool changed(const CustomOTAPartitions::Table& current, const esp_partition_info_t& e) {
    for (const esp_partition_info_t& c : current) {
        if (c.type == e.type && c.subtype == e.subtype &&
            c.pos.offset == e.pos.offset && c.pos.size == e.pos.size) {
            return false;
        }
    }
    return true;
}

struct Region {
    uint32_t offset;
    uint32_t size;
};

// Regions apply() erases so the new layout starts clean: otadata always (the bootloader then
// starts ota_0), changed nvs/nvs_keys/coredump (stale contents would be misread), and the
// first sector of changed app slots (so they do not look like a backup). Filesystems are
// left alone - too slow to erase here; the app formats them or a filesystem upload rewrites them.
std::vector<Region> erasePlan(const CustomOTAPartitions::Table& current, const CustomOTAPartitions::Table& next) {
    std::vector<Region> plan;
    const esp_partition_info_t* boot = defaultBoot(next);
    for (const esp_partition_info_t& e : next) {
        if (e.type == PART_TYPE_DATA && e.subtype == 0x00) {
            plan.push_back({e.pos.offset, e.pos.size});
        } else if (changed(current, e)) {
            if (e.type == PART_TYPE_DATA && (e.subtype == 0x02 || e.subtype == 0x03 || e.subtype == 0x04)) {
                plan.push_back({e.pos.offset, e.pos.size});
            } else if (e.type == PART_TYPE_APP && &e != boot) {
                plan.push_back({e.pos.offset, SECTOR});
            }
        }
    }
    return plan;
}

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

    UnprotectedChip unprotected;

    COTA_DEBUG_PRINTF("CustomOTA: Writing partition table (%d entries) at 0x%x\n", count, ESP_PARTITION_TABLE_OFFSET);

    esp_err_t res = esp_flash_erase_region(&unprotected.chip, ESP_PARTITION_TABLE_OFFSET, ESP_PARTITION_TABLE_SIZE);
    if (res == ESP_OK) {
        res = esp_flash_write(&unprotected.chip, sector, ESP_PARTITION_TABLE_OFFSET, ESP_PARTITION_TABLE_SIZE);
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

bool CustomOTAPartitions::parse(const uint8_t* data, size_t len, Table& table, String& err) {
    table.clear();
    if (len == 0 || len > ESP_PARTITION_TABLE_SIZE) {
        err = F("This is not a partition table (wrong size). Select partitions.bin from .pio/build/<env>/, not firmware.bin or spiffs.bin.");
        return false;
    }
    uint8_t* sector = (uint8_t*)malloc(ESP_PARTITION_TABLE_SIZE);
    if (!sector) {
        err = F("Out of memory reading the uploaded table");
        return false;
    }
    memset(sector, 0xFF, ESP_PARTITION_TABLE_SIZE);
    memcpy(sector, data, len);

    int count = 0;
    bool ok = esp_partition_table_verify((const esp_partition_info_t*)sector, false, &count) == ESP_OK;
    if (ok) {
        const esp_partition_info_t* entries = (const esp_partition_info_t*)sector;
        for (size_t i = 0; i < ESP_PARTITION_TABLE_MAX_ENTRIES && entries[i].magic == ESP_PARTITION_MAGIC; i++) {
            table.push_back(entries[i]);
        }
    }
    free(sector);
    if (!ok || table.empty()) {
        err = F("This is not a valid partition table. Select partitions.bin from .pio/build/<env>/, not firmware.bin or spiffs.bin.");
        table.clear();
        return false;
    }
    return true;
}

void CustomOTAPartitions::check(const Table& current, const Table& next,
                                std::vector<String>& errors, std::vector<String>& warnings) {
    if (esp_flash_encryption_enabled() || esp_secure_boot_enabled()) {
        errors.push_back(F("Flash encryption or secure boot is enabled - the partition table cannot be changed over the air."));
        return;
    }

    if (next.size() == current.size() &&
        memcmp(next.data(), current.data(), next.size() * sizeof(esp_partition_info_t)) == 0) {
        errors.push_back(F("The uploaded table is the same as the one already on the device - nothing to change."));
        return;
    }

    // Layout: alignment, bounds and overlaps
    const uint32_t flashSize = ESP.getFlashChipSize();
    const uint32_t firstFree = ESP_PARTITION_TABLE_OFFSET + ESP_PARTITION_TABLE_SIZE;
    for (size_t i = 0; i < next.size(); i++) {
        const esp_partition_info_t& e = next[i];
        String n = label(e);
        if (e.pos.size == 0 || e.pos.offset % SECTOR || e.pos.size % SECTOR) {
            errors.push_back(fmt("%s: offset and size must be non-zero multiples of 4 KB.", n.c_str()));
        }
        if (e.type == PART_TYPE_APP && e.pos.offset % APP_ALIGN) {
            errors.push_back(fmt("%s: app partitions must start on a 64 KB boundary (0x%x is not).", n.c_str(), e.pos.offset));
        }
        if (e.pos.offset < firstFree) {
            errors.push_back(fmt("%s: starts at 0x%x, inside the bootloader or partition table (first free address is 0x%x).",
                                 n.c_str(), e.pos.offset, firstFree));
        }
        if ((uint64_t)e.pos.offset + e.pos.size > flashSize) {
            errors.push_back(fmt("%s: ends at 0x%x, past the end of this device's %u MB flash.",
                                 n.c_str(), e.pos.offset + e.pos.size, flashSize / (1024 * 1024)));
        }
        for (size_t j = i + 1; j < next.size(); j++) {
            const esp_partition_info_t& o = next[j];
            if (e.pos.offset < o.pos.offset + o.pos.size && o.pos.offset < e.pos.offset + e.pos.size) {
                errors.push_back(fmt("%s and %s overlap.", n.c_str(), label(o).c_str()));
            }
        }
    }

    // What CustomOTA needs: otadata and two OTA slots
    int otadataCount = 0;
    for (const esp_partition_info_t& e : next) {
        if (e.type == PART_TYPE_DATA && e.subtype == 0x00) {
            otadataCount++;
            if (e.pos.size != 2 * SECTOR) {
                errors.push_back(F("otadata must be exactly 0x2000 (8 KB)."));
            }
        }
    }
    if (otadataCount != 1) {
        errors.push_back(F("The table needs exactly one otadata partition (data, ota)."));
    }
    const esp_partition_info_t* ota0 = find(next, PART_TYPE_APP, PART_SUBTYPE_OTA_FLAG);
    const esp_partition_info_t* ota1 = find(next, PART_TYPE_APP, PART_SUBTYPE_OTA_FLAG | 1);
    if (!ota0 || !ota1) {
        errors.push_back(F("CustomOTA needs two OTA app slots (app, ota_0 and app, ota_1)."));
    } else if (ota0->pos.size != ota1->pos.size) {
        warnings.push_back(fmt("%s (%s) and %s (%s) are different sizes - firmware must fit the smaller one.",
                               label(*ota0).c_str(), kb(ota0->pos.size).c_str(),
                               label(*ota1).c_str(), kb(ota1->pos.size).c_str()));
    }

    // After the restart otadata is blank, so the bootloader starts factory (if any) or ota_0.
    // The running firmware must already be at that address.
    const esp_partition_t* running = esp_ota_get_running_partition();
    const esp_partition_info_t* boot = defaultBoot(next);
    uint32_t sketchSize = ESP.getSketchSize();
    if (running && boot) {
        if (boot->pos.offset != running->address) {
            const esp_partition_info_t* oldAtBoot = nullptr;
            for (const esp_partition_info_t& c : current) {
                if (c.type == PART_TYPE_APP && c.pos.offset == boot->pos.offset) oldAtBoot = &c;
            }
            if (oldAtBoot) {
                errors.push_back(fmt("The device is running from %s (0x%x), but after the change it will start the firmware "
                                     "at 0x%x, which is %s today. Upload the current firmware once more so it runs from %s, "
                                     "then upload partitions.bin again.",
                                     running->label, running->address, boot->pos.offset,
                                     label(*oldAtBoot).c_str(), label(*oldAtBoot).c_str()));
            } else {
                errors.push_back(fmt("After the change the device will start the firmware at 0x%x (%s), but the running "
                                     "firmware is at 0x%x and nothing is at 0x%x today. Change the new table so %s starts at 0x%x.",
                                     boot->pos.offset, label(*boot).c_str(), running->address,
                                     boot->pos.offset, label(*boot).c_str(), running->address));
            }
        } else if (boot->pos.size < sketchSize) {
            errors.push_back(fmt("%s would be %s but the running firmware is %s.",
                                 label(*boot).c_str(), kb(boot->pos.size).c_str(), kb(sketchSize).c_str()));
        }
    }

    // Erasing must stay quick and must never reach the running firmware
    uint32_t eraseTotal = 0;
    for (const Region& r : erasePlan(current, next)) {
        eraseTotal += r.size;
        if (running && r.offset < running->address + sketchSize && running->address < r.offset + r.size) {
            errors.push_back(fmt("The new layout would erase 0x%x-0x%x, which holds the running firmware.",
                                 r.offset, r.offset + r.size));
        }
    }
    if (eraseTotal > MAX_APPLY_ERASE) {
        errors.push_back(fmt("The new nvs/coredump partitions need %s erased - too much to do over the network (limit %s).",
                             kb(eraseTotal).c_str(), kb(MAX_APPLY_ERASE).c_str()));
    }

    // What will be lost
    for (const esp_partition_info_t& e : next) {
        if (!changed(current, e)) continue;
        String n = label(e);
        if (e.type == PART_TYPE_DATA && e.subtype == 0x02) {
            warnings.push_back(fmt("%s (settings) is new, moved or resized: ALL saved settings will be erased and the device "
                                   "starts with defaults.", n.c_str()));
        } else if (isFilesystem(e)) {
            warnings.push_back(fmt("%s (filesystem) is new, moved or resized: its files are lost. After the restart, upload a "
                                   "filesystem image built for the new layout.", n.c_str()));
        } else if (e.type == PART_TYPE_APP && &e != boot) {
            warnings.push_back(fmt("%s is new, moved or resized: the backup firmware is lost, so Rollback is unavailable "
                                   "until the next firmware update.", n.c_str()));
        } else if (e.type == PART_TYPE_DATA && e.subtype == 0x03) {
            warnings.push_back(fmt("%s (coredump) is new, moved or resized and will be cleared.", n.c_str()));
        }
    }
    for (const esp_partition_info_t& c : current) {
        bool kept = false;
        for (const esp_partition_info_t& e : next) {
            if (memcmp(c.label, e.label, sizeof(c.label)) == 0) kept = true;
        }
        if (!kept) {
            warnings.push_back(fmt("%s is not in the new table - its contents are lost.", label(c).c_str()));
        }
    }
}

esp_err_t CustomOTAPartitions::apply(const Table& current, const Table& next, String& err) {
    std::vector<String> errors, warnings;
    check(current, next, errors, warnings);
    if (!errors.empty()) {
        err = errors[0];
        return ESP_ERR_INVALID_ARG;
    }

    uint8_t* sector = (uint8_t*)malloc(ESP_PARTITION_TABLE_SIZE);
    if (!sector || !serialise(next, sector)) {
        free(sector);
        err = F("Could not build the new table (out of memory or too many partitions)");
        return ESP_ERR_NO_MEM;
    }

    // Erase first and write the table last: until the table is written the old layout is
    // intact and, with otadata blank, still boots the running firmware from ota_0
    UnprotectedChip unprotected;
    for (const Region& r : erasePlan(current, next)) {
        COTA_DEBUG_PRINTF("CustomOTA: Erasing 0x%x-0x%x for the new layout\n", r.offset, r.offset + r.size);
        esp_err_t res = esp_flash_erase_region(&unprotected.chip, r.offset, r.size);
        if (res != ESP_OK) {
            free(sector);
            err = fmt("Erasing 0x%x failed (%s). The partition table was not changed.", r.offset, esp_err_to_name(res));
            return res;
        }
    }

    esp_err_t res = write(sector);
    free(sector);
    if (res != ESP_OK) {
        err = fmt("Writing the partition table failed (%s). Do not restart - reflash over USB if this persists.",
                  esp_err_to_name(res));
    }
    return res;
}

bool CustomOTAPartitions::filesystemChanged(const Table& current, const Table& next) {
    for (const esp_partition_info_t& e : next) {
        if (isFilesystem(e) && changed(current, e)) return true;
    }
    return false;
}

void CustomOTAPartitions::appendJson(String& out, const Table& table) {
    out += '[';
    for (size_t i = 0; i < table.size(); i++) {
        const esp_partition_info_t& e = table[i];
        if (i) out += ',';
        out += F("{\"label\":");
        appendJsonString(out, label(e));
        out += F(",\"type\":\"");
        out += typeName(e.type);
        out += F("\",\"subtype\":");
        appendJsonString(out, subtypeName(e.type, e.subtype));
        out += F(",\"offset\":");
        out += String(e.pos.offset);
        out += F(",\"size\":");
        out += String(e.pos.size);
        out += '}';
    }
    out += ']';
}

void CustomOTAPartitions::appendJsonString(String& out, const String& s) {
    out += '"';
    for (size_t i = 0; i < s.length(); i++) {
        char c = s[i];
        if (c == '"' || c == '\\') {
            out += '\\';
            out += c;
        } else if ((uint8_t)c < 0x20) {
            out += ' ';
        } else {
            out += c;
        }
    }
    out += '"';
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
