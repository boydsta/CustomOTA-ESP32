// CustomOTA partition table rewrite test - see platformio.ini for the commands
#include <Arduino.h>
#include <CustomOTA.h>
#include <CustomOTAPartitions.h>
#include <esp_ota_ops.h>

static void printTable() {
    CustomOTAPartitions::Table table;
    String err;
    if (!CustomOTAPartitions::load(table, err)) {
        Serial.printf("Load failed: %s\n", err.c_str());
        return;
    }

    const esp_partition_t* running = esp_ota_get_running_partition();
    Serial.printf("Flash size: %u bytes, running: %s, sketch size: %u bytes\n",
                  ESP.getFlashChipSize(), running ? running->label : "?", ESP.getSketchSize());
    Serial.println("Label            Type    Subtype     Offset     Size");
    for (const esp_partition_info_t& e : table) {
        Serial.printf("%-16s %-7s %-11s 0x%06x   0x%06x (%u KB)\n",
                      CustomOTAPartitions::label(e).c_str(),
                      CustomOTAPartitions::typeName(e.type),
                      CustomOTAPartitions::subtypeName(e.type, e.subtype).c_str(),
                      e.pos.offset, e.pos.size, e.pos.size / 1024);
    }

    uint8_t* sector = (uint8_t*)malloc(ESP_PARTITION_TABLE_SIZE);
    if (sector && CustomOTAPartitions::serialise(table, sector)) {
        Serial.printf("Rebuilt table matches flash: %s\n",
                      CustomOTAPartitions::matchesFlash(sector) ? "YES" : "NO");
    }
    free(sector);
}

static void rewriteTable() {
    CustomOTAPartitions::Table table;
    String err;
    if (!CustomOTAPartitions::load(table, err)) {
        Serial.printf("Load failed: %s\n", err.c_str());
        return;
    }
    uint8_t* sector = (uint8_t*)malloc(ESP_PARTITION_TABLE_SIZE);
    if (!sector || !CustomOTAPartitions::serialise(table, sector)) {
        Serial.println("Serialise failed");
        free(sector);
        return;
    }
    if (!CustomOTAPartitions::matchesFlash(sector)) {
        // Only rewrite an identical table in this test - a mismatch means the rebuild is wrong
        Serial.println("Rebuilt table differs from flash - not writing");
        free(sector);
        return;
    }
    uint32_t start = micros();
    esp_err_t res = CustomOTAPartitions::write(sector);
    Serial.printf("write(): %s in %u us\n", esp_err_to_name(res), micros() - start);
    free(sector);
}

void setup() {
    Serial.begin(115200);
    Serial.printf("\nCustomOTA %s partition test - commands: p, w, r\n", CUSTOMOTA_VERSION_STRING);
    printTable();
}

void loop() {
    if (!Serial.available()) return;
    switch (Serial.read()) {
        case 'p': printTable(); break;
        case 'w': rewriteTable(); break;
        case 'r': ESP.restart(); break;
    }
}
