/*
 * CustomOTA partition table access for ESP32
 *
 * Reads, rebuilds and rewrites the partition table at CONFIG_PARTITION_TABLE_OFFSET.
 * Only compiled when the project sets -D CUSTOM_OTA_PARTITIONS=1, so projects that
 * do not use the partition editor pay nothing for it.
 *
 * WARNING: the partition table has no backup copy. A power loss between the erase and
 * the write of the table sector leaves the device unbootable until it is reflashed
 * over serial. The window is one sector erase plus write (about 50 ms).
 *
 * Created by Boyd Benton
 */

#ifndef CUSTOMOTA_PARTITIONS_H
#define CUSTOMOTA_PARTITIONS_H

#ifndef CUSTOM_OTA_PARTITIONS
#define CUSTOM_OTA_PARTITIONS 0
#endif

#if CUSTOM_OTA_PARTITIONS

#include <Arduino.h>
#include <vector>
#include <esp_err.h>
#include <esp_flash_partitions.h>

class CustomOTAPartitions {
public:
    // Partition entries in table order, without the MD5 entry or terminator
    typedef std::vector<esp_partition_info_t> Table;

    // Reads the table from flash and checks its magic and MD5.
    // On failure returns false and sets err.
    static bool load(Table& table, String& err);

    // Builds the full table sector: entries, MD5 entry, 0xFF fill.
    // sector must be ESP_PARTITION_TABLE_SIZE bytes. Returns false if there are too many entries.
    static bool serialise(const Table& table, uint8_t* sector);

    // Erases and writes the table sector, then reads it back and compares.
    // The sector is checked with esp_partition_table_verify() before flash is touched.
    // Refused when flash encryption or secure boot is enabled.
    // Takes effect on the next boot - ESP-IDF caches the table at startup.
    static esp_err_t write(const uint8_t* sector);

    // True when the given sector matches what is in flash now
    static bool matchesFlash(const uint8_t* sector);

    // Readable names for logging and the web page
    static const char* typeName(uint8_t type);
    static String subtypeName(uint8_t type, uint8_t subtype);
    static String label(const esp_partition_info_t& entry);
};

#endif // CUSTOM_OTA_PARTITIONS
#endif // CUSTOMOTA_PARTITIONS_H
