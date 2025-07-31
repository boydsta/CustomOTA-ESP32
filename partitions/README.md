# CustomOTA Partition Tables

CustomOTA **requires** dual OTA partitions to function properly. Without these partitions, the library will not work.

## Available Partition Tables

### default.csv (4MB Flash)
- **app0**: 1.6MB (0x1A0000)
- **app1**: 1.6MB (0x1A0000) 
- **spiffs**: 704KB (0xB0000)
- **Total Flash Used**: ~4MB

### default_8MB.csv (8MB Flash)
- **app0**: 3MB (0x300000)
- **app1**: 3MB (0x300000)
- **spiffs**: 2MB (0x1F0000)
- **Total Flash Used**: ~8MB

### default_16MB.csv (16MB Flash)
- **app0**: 6MB (0x600000)
- **app1**: 6MB (0x600000)
- **spiffs**: 4MB (0x3F0000)
- **Total Flash Used**: ~16MB

## Required Partitions

All partition tables **must** include these partitions for CustomOTA:

| Name    | Type | SubType | Description |
|---------|------|---------|-------------|
| nvs     | data | nvs     | Non-volatile storage |
| otadata | data | ota     | OTA partition info |
| app0    | app  | ota_0   | First firmware partition |
| app1    | app  | ota_1   | Second firmware partition |
| spiffs  | data | spiffs  | File system (optional) |

## How CustomOTA Uses Partitions

1. **Initial Flash**: Firmware goes to `app0` partition
2. **OTA Update**: New firmware goes to `app1` partition  
3. **Boot**: ESP32 automatically boots from newest partition
4. **Rollback**: Can switch back to `app0` if needed
5. **Next OTA**: Alternates between `app0` and `app1`

## Usage Instructions

### Step 1: Copy Partition File
```bash
# Choose the appropriate file for your flash size
cp lib/CustomOTA/partitions/default.csv ./partitions.csv          # 4MB
cp lib/CustomOTA/partitions/default_8MB.csv ./partitions.csv      # 8MB  
cp lib/CustomOTA/partitions/default_16MB.csv ./partitions.csv     # 16MB
```

### Step 2: Configure platformio.ini
```ini
[env:esp32dev]
platform = espressif32
board = esp32dev
framework = arduino

; CRITICAL: This line is required for CustomOTA!
board_build.partitions = partitions.csv

lib_deps = 
    mathieucarbou/AsyncTCP@^3.2.3
    mathieucarbou/ESPAsyncWebServer@^3.1.1
    https://github.com/boydsta/CustomOTA-ESP32.git
```

### Step 3: First Upload
```bash
# Upload initial firmware
pio run --target upload
```

Now your ESP32 is ready for OTA updates with CustomOTA!

## Troubleshooting

**"No OTA partitions found"** - You haven't configured dual OTA partitions  
**"Not enough space"** - Choose a larger partition table for your flash size  
**"Upload failed"** - Check that your firmware fits in the app partition size

## Custom Partition Tables

If you need custom sizes, ensure your partition table includes:
- `otadata` partition (required for OTA switching)
- Two `app` partitions of equal size (`ota_0` and `ota_1`)
- Sufficient space for your firmware in each app partition

Example custom partition:
```csv
# Name,   Type, SubType, Offset,  Size
nvs,      data, nvs,     0x9000,  0x5000
otadata,  data, ota,     0xe000,  0x2000  
app0,     app,  ota_0,   0x10000, 0x200000  # 2MB
app1,     app,  ota_1,   0x210000,0x200000  # 2MB
spiffs,   data, spiffs,  0x410000,0x100000  # 1MB
```
