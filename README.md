# CustomOTA-ESP32 ![Version](https://img.shields.io/badge/Version-1.6.0-blue.svg)

ESP32 Over-The-Air firmware update library with dual partition backup system, filesystem updates, and a web interface.

![ESP32](https://img.shields.io/badge/ESP32-Compatible-green.svg)
![License](https://img.shields.io/badge/License-MIT-yellow.svg)

## What's New in v1.6.0

- **Dropped uploads recover on their own** - If the connection drops mid-upload, the upload is aborted after 30 s without data (`setUploadTimeout()`). Before, it stayed "in progress" until the device rebooted. The running firmware is never touched by a partial upload.
- **The page tells you whether to wait or retry** - On load it shows an upload still running from another browser ("in progress 43% - wait"), or the last failed one ("stalled at 43% - not applied, safe to try again").
- **`isBusy()`** - True while an upload is running or a restart is pending, for example to hold off deep sleep. See [Sleep and Power-Down](#sleep-and-power-down).
- **Uploads can't disturb each other** - A second upload while one is running gets 409, and an unauthenticated one gets 401. Before, either could break the upload in progress.
- **Rollback is only offered when it will work** - The backup slot is verified in full (header, segments, checksum), and a failed upload that overwrote it clears the backup. The check runs in a low-priority background task 5 s after `begin()` (`setBackupCheckDelay()`), so it doesn't slow start-up.

## What's New in v1.5.0

- **Update gate** - `setUpdateAllowed(allowed, reason)` lets your application block firmware updates, reboot, rollback and partition changes while restarting would be unsafe, for example while a vehicle is running. Blocked requests are refused with the reason, an upload in progress is aborted, and a restart that is already scheduled waits until updates are allowed again. See [Blocking Updates While Unsafe](#blocking-updates-while-unsafe).
- **No change unless you use it** - Updates are allowed by default, so applications that never call `setUpdateAllowed()` behave exactly as on 1.4.

## What's New in v1.4.0

- **Change the partition table over the air** - Build with `-D CUSTOM_OTA_PARTITIONS=1` and the OTA page can move a device in the field to a new layout, for example bigger app slots. You upload `partitions.bin`, review the checks, then apply. See [Changing the Partition Table Over the Air](#changing-the-partition-table-over-the-air).
- **Off by default** - Without the flag, 1.4.0 builds the same code as 1.3. To stay on 1.3, see [Staying on v1.3](#staying-on-v13).

## What's New in v1.3.0

- **Security fixes** - Uploads are authenticated before anything is written, and `setCredentials()` followed by `begin(&server)` no longer turns authentication off.
- **Lower heap use** - The OTA page is served straight from flash instead of being built as a ~23 KB `String` on every request.
- **Real results in the browser** - Firmware update, reboot and rollback reply first, then restart after `setRestartDelay()`. The page shows actual success or failure and reloads once the device is back.
- **Many upload fixes** - Filesystem uploads, error responses, progress reaching 100%, `/update/status` routing and more.
- **Build date updates on every build**, and the hardware ID is shown on the page.

See [CHANGELOG.md](CHANGELOG.md) for the full list.

## Installation Methods

### Option 1: PlatformIO Git Dependency (Recommended)
Add to your `platformio.ini`:
```ini
lib_deps = 
    mathieucarbou/AsyncTCP@^3.2.3
    mathieucarbou/ESPAsyncWebServer@^3.1.1
    https://github.com/boydsta/CustomOTA-ESP32.git
```

An unpinned URL always gets the latest `main`. For production builds, pin a release tag so a library update never changes your firmware unexpectedly:
```ini
    https://github.com/boydsta/CustomOTA-ESP32.git#v1.6.0
```

### Staying on v1.3

Pin the v1.3.0 tag:
```ini
lib_deps =
    mathieucarbou/AsyncTCP@^3.2.3
    mathieucarbou/ESPAsyncWebServer@^3.1.1
    https://github.com/boydsta/CustomOTA-ESP32.git#v1.3.0
```
Then delete `.pio/libdeps/<env>/CustomOTA` (or run `pio pkg update`) so PlatformIO fetches the pinned version instead of reusing the one it already downloaded.

You can also pin an exact commit: `#<commit hash>` instead of `#v1.3.0`. 1.4.0 without `-D CUSTOM_OTA_PARTITIONS=1` builds the same code as 1.3, so pinning matters only if you want the library itself never to change.

### Option 2: Manual Installation
Copy the library to your project's `lib` folder:
```
your_project/
├── lib/
│   └── CustomOTA/
│       ├── src/
│       │   ├── CustomOTA.h
│       │   └── CustomOTA.cpp
│       └── library.json
├── src/
│   └── main.cpp
└── platformio.ini
```

### CRITICAL: Partition Configuration Required

**CustomOTA requires dual OTA partitions to function.** You must configure your ESP32 with the correct partition table. See changelog for new partition validation features.

#### Option 1: Use Provided Partition Files
Copy the appropriate partition file from this library to your project:

```bash
# Copy the partition file to your project root
cp lib/CustomOTA/partitions/default.csv ./partitions.csv          # 4MB Flash
cp lib/CustomOTA/partitions/default_8MB.csv ./partitions.csv      # 8MB Flash  
cp lib/CustomOTA/partitions/default_16MB.csv ./partitions.csv     # 16MB Flash
```

Add to your `platformio.ini`:
```ini
board_build.partitions = partitions.csv
```

#### Option 2: Use Custom Partition Table
Ensure your partition table includes these **required partitions**:
```csv
# Name,   Type, SubType, Offset,  Size
nvs,      data, nvs,     0x9000,  0x5000
otadata,  data, ota,     0xe000,  0x2000
app0,     app,  ota_0,   0x10000, <your_app_size>
app1,     app,  ota_1,   <offset>, <your_app_size>
spiffs,   data, spiffs,  <offset>, <remaining_space>
```

**Without dual OTA partitions, CustomOTA will not work!**

## Quick Start

### Basic Setup (No Authentication)
```cpp
#include <WiFi.h>
#include <CustomOTA.h>

AsyncWebServer server(80);
CustomOTA ota;

void setup() {
    Serial.begin(115200);
    WiFi.begin("your-ssid", "your-password");
    while (WiFi.status() != WL_CONNECTED) delay(1000);
    
    ota.setFirmwareVersion("1.0.0");
    ota.setHardwareID("MyDevice v1.0");
    ota.begin(&server);
    server.begin();
    
    Serial.println("OTA: http://" + WiFi.localIP().toString() + "/update");
}

void loop() {
    ota.handle(); // Must call every loop
}
```

### With Authentication
```cpp
// Add before ota.begin():
ota.setCredentials("admin", "password123");
```

### Advanced Configuration
```cpp
// Configure restart delay (default: 3000ms)
ota.setRestartDelay(5000); // 5 seconds

// Abort an upload that stops sending data (default: 30000ms)
ota.setUploadTimeout(20000); // 20 seconds

// Control SPIFFS formatting behavior (default: true)
ota.setSpiffsFormatOnFailure(false); // Never auto-format SPIFFS
```

## API Reference

### Core Methods
- `begin(AsyncWebServer* server)` - Initialize OTA system
- `begin(AsyncWebServer* server, username, password)` - Initialize with authentication
- `handle()` - **Must call every loop** for real-time compatibility
- `setCredentials(username, password)` - Enable authentication
- `setFirmwareVersion(version)` - Set version display
- `setHardwareID(id)` - Set the device name shown as the OTA page heading and browser tab title

### Advanced Configuration Methods
- `setRestartDelay(unsigned long delayMs)` - Delay between replying to the browser and restarting, after a firmware update, reboot or rollback (default: 3000ms)
- `setUploadTimeout(unsigned long timeoutMs)` - Abort an upload when no data has arrived for this long, e.g. after a dropped connection (default: 30000ms)
- `setBackupCheckDelay(unsigned long delayMs)` - When to verify the backup slot after `begin()` (default: 5000ms). Runs in a low-priority background task; until it finishes there is no rollback. Call before `begin()`
- `setSpiffsFormatOnFailure(bool allowFormat)` - Control automatic SPIFFS formatting on mount failure (default: true)
- `setBuildTimestamp(const char* timestamp)` - Override the build timestamp (by default PlatformIO builds set it automatically via `build_timestamp.py`)
- `setUpdateAllowed(bool allowed, const char* reason = nullptr)` - Allow or block updates, reboot, rollback and partition changes (default: allowed). Call it from `loop()` with your own "safe to restart" condition. See [Blocking Updates While Unsafe](#blocking-updates-while-unsafe)

### Status Methods
- `isUpdateInProgress()` - Check if update is in progress. Clears after `setUploadTimeout()` if the upload stops sending data
- `isBusy()` - Upload in progress or restart pending. Hold off sleep or power-down while true
- `getUpdateStatus()` - Get current update status string
- `getUpdateProgress()` - Get update progress percentage (0-100)
- `shouldRestart()` - Check if device should restart (for deferred restart handling). Always false while updates are blocked
- `isRestartPending()` - A restart has been scheduled (after an update, reboot, rollback or partition change), including one held by the update gate
- `isUpdateAllowed()` - Current state of the update gate
- `isBackupChecked()` - The background backup check has finished

### Static Information Methods
- `CustomOTA::getLibraryVersion()` - Get library version string
- `CustomOTA::getVersionMajor()` - Get major version number
- `CustomOTA::getVersionMinor()` - Get minor version number
- `CustomOTA::getVersionPatch()` - Get patch version number
- `CustomOTA::getBuildInfo()` - Get build date and time

## Features

### Core Features
- **Dual Partition Backup** - Automatic firmware rollback capability
- **Filesystem Updates** - Update SPIFFS content via web interface
- **Real-Time Compatible** - Non-blocking operations, no `delay()` calls
- **Web Interface** - File uploads with progress tracking, served from flash
- **HTTP Authentication** - Password protection for OTA operations
- **Debug Control** - Enable with `-DCUSTOM_OTA_DEBUG=1` build flag

### Advanced Features
- **Automatic Backup Detection** - Finds existing firmware in inactive partitions
- **Heap Monitoring** - Prevents updates when memory is insufficient
- **Rollback Validation** - The backup image is verified by ESP-IDF before it is made the boot partition
- **Configurable Restart Delays** - Custom reboot timing for different applications
- **Safer SPIFFS Handling** - Prevents accidental data loss during formatting
- **Progress Tracking** - Real-time upload and update progress
- **Status Monitoring** - Comprehensive system status information
- **Clear Results** - The device replies before restarting, so the page always shows the real outcome

## Web Endpoints

| Route | Method | Purpose |
|---|---|---|
| `/update` | GET | OTA web page |
| `/update` | POST | Upload firmware or filesystem image (multipart, `type=firmware` or `filesystem`) |
| `/update/status` | GET | JSON status: `inProgress`, `progress`, `status`, `lastUploadFailed`, heap, version, build date, hardware ID, partitions, `canRollback`, `restartPending`, `updateAllowed`, `blockedReason`, `libraryVersion` |
| `/update/reboot` | POST | Restart the device |
| `/update/rollback` | POST | Boot the previous firmware |
| `/update/partitions` | GET | JSON: current partition table, running slot, pending table with its check errors/warnings (partition editor only) |
| `/update/partitions` | POST | Upload a `partitions.bin` to check - nothing is written (partition editor only) |
| `/update/partitions/apply` | POST | Write the checked table and restart (partition editor only) |
| `/update/partitions/cancel` | POST | Discard the uploaded table (partition editor only) |

While updates are blocked with `setUpdateAllowed(false, ...)`, `POST /update`, `/update/reboot`, `/update/rollback` and `/update/partitions/apply` answer **409** with the reason. Uploading a `partitions.bin` to check it (`POST /update/partitions`) still works, because nothing is written.

## Blocking Updates While Unsafe

Some devices must not restart at certain times, for example a vehicle controller while the vehicle is running. Call `setUpdateAllowed()` every loop with your own condition:

```cpp
void setup() {
    // ...
    ota.setUpdateAllowed(false, "Starting up"); // Block until loop() has checked the condition
    ota.begin(&server);
    server.begin();
}

void loop() {
    // Only allow updates and restarts with the machine stopped
    bool safe = machineIsStopped();
    ota.setUpdateAllowed(safe, "Machine must be stopped");
    ota.handle();
}
```

While blocked:

| Action | What happens |
|---|---|
| Firmware or filesystem upload | Refused with 409 and the reason - nothing is written |
| Upload already in progress | Aborted at its next chunk. The running firmware is untouched, because the boot partition is only switched after a complete upload |
| Reboot, rollback, partition apply | Refused with 409 and the reason. Rollback is refused before the boot partition is changed |
| Restart already scheduled (e.g. upload finished just before the gate closed) | Held, not cancelled. It runs as soon as updates are allowed again |
| OTA page | Shows the reason and greys out the buttons. It re-checks every 5 seconds, so it follows the gate while open |

Notes:

- **Default is allowed.** Only applications that call `setUpdateAllowed()` are affected.
- **`reason` must be a static string** (a string literal or a global), because the library keeps the pointer. It must not contain double quotes or backslashes, because it is sent as-is in the `/update/status` JSON. `nullptr` uses a default message.
- **Thread safety** - `setUpdateAllowed()` is meant to be called from `loop()`. The web handlers run in the AsyncTCP task and read the gate on every request and upload chunk.
- **Held restarts** - If the gate opens again a long time after a restart was scheduled, the restart happens immediately. Check `isRestartPending()` if your application needs to know one is waiting, or `isBusy()` to delay sleeping (see [Sleep and Power-Down](#sleep-and-power-down)).
- **The boot partition may already be switched** - After a complete firmware upload, the new firmware is the boot partition even while its restart is held. A power cycle in that window boots the new (fully verified) firmware.

## Interrupted Uploads

If the connection drops mid-upload (browser closed, phone left the WiFi), the device gets no more data and no end of request. After `setUploadTimeout()` (default 30 s) without data, the upload is aborted and the status becomes, for example, *"Upload stalled at 43% - not applied, running firmware unchanged. Safe to try again."* A partial firmware upload never changes the boot partition. A partial filesystem upload leaves the filesystem incomplete, so upload it again.

When you open the OTA page again, it shows what the device saw:

| Device state | Page shows |
|---|---|
| Upload still running (another browser, or within the timeout) | "An upload is in progress (43%) - wait for it to finish", with the progress bar. Upload is disabled. If it then stalls, the page switches to the failure below |
| Last upload failed or stalled | "Last upload failed: ..." with the reason. Safe to upload again |
| Nothing pending | The normal page |

Retrying is always safe: a new upload discards whatever the previous one left behind.

## Sleep and Power-Down

If your application sleeps or powers down on its own, for example deep sleep after a period off, hold it off while `isBusy()` is true. Otherwise sleep can cut off an upload or the restart after it:

```cpp
if (idleLongEnough() && !ota.isBusy()) {
    esp_deep_sleep_start();
}
```

`isBusy()` clears by itself: a stalled upload is aborted after `setUploadTimeout()`, and a finished firmware upload restarts the device. The exception is a restart held by `setUpdateAllowed(false)`, which waits until updates are allowed again. If your sleep condition and your gate condition can both be true at once, also cap the hold with your own timer.

## Testing a Device

[tools/ota_test.py](tools/ota_test.py) tests a running device over HTTP. It needs only Python 3.8+ and is not part of the published library.

```bash
python tools/ota_test.py --host 192.168.1.50 --firmware .pio/build/<env>/firmware.bin
python tools/ota_test.py --list    # all tests
```

It sends garbage, truncated, corrupt, stalled, dropped and concurrent uploads, checks authentication (`--user`, `--password`) and the partition editor (`--partitions`), then installs `--firmware`, reboots and rolls back. The failure tests only send damaged images, which ESP-IDF refuses to boot. The run ends back on the firmware the device started with, and it never applies a partition table. `--no-restart` skips the tests that restart the device. `--soak N`, `--gate` (interactive) and `--fs` (replaces the SPIFFS contents) are opt-in. `--upload-timeout` must match the device's `setUploadTimeout()`.

v1.6.0 passes all tests on an ESP32 (Arduino 3.x) with authentication and the partition editor enabled.

## Changing the Partition Table Over the Air

Build with `-D CUSTOM_OTA_PARTITIONS=1` and the OTA page gets a **Partition Table** section. It lets a device in the field move to a new layout, for example bigger app slots, without a USB cable. With the flag off, nothing is compiled in. Tested on hardware moving a production device from 1 MB to 1600 KB app slots (Arduino 2.0.17 / ESP-IDF 4.4).

### Enabling it

Add the flag and keep your old partition file. A second env for the new layout makes both easy to build:

```ini
[env:esp32dev]                       ; old layout - devices not moved yet
board_build.partitions = partitions.csv
build_flags = -D CUSTOM_OTA_PARTITIONS=1

[env:esp32dev_v2]                    ; new layout
extends = env:esp32dev
board_build.partitions = partitions_v2.csv
```

The flag needs CustomOTA 1.4.0 or later (`#v1.4.0`). Version 1.3 ignores it.

The firmware built by either env runs on either layout: the firmware reads the partition table from flash. The `.csv` only sets PlatformIO's size check and the `partitions.bin`/`spiffs.bin` it builds. The firmware you upload in step 2 below must still fit the **old** app slot.

### Supported files

| Upload | File | Notes |
|---|---|---|
| Partition table | `.pio/build/<env>/partitions.bin` | Built from your `.csv` on every build. A `.csv` cannot be uploaded directly. |
| Filesystem | `.pio/build/<env>/spiffs.bin` (`pio run -t buildfs`) | SPIFFS or LittleFS (both use a `spiffs`-subtype partition). FAT (`ffat`) images are not supported. |

### Steps

The page shows these too:

1. **Make the new table.** Add a new `.csv` next to the old one (keep the old one for devices not yet moved), point `board_build.partitions` at it and build. Upload `.pio/build/<env>/partitions.bin` from that build. Also run `pio run -t buildfs` to get a `spiffs.bin` for the new layout.
2. **Firmware must be running from the new `ota_0`.** After the change otadata is blank, so the bootloader starts `ota_0` (or `factory`). If the device is running from the other slot, the check says so: upload the current firmware once more and retry.
3. **Upload `partitions.bin`.** The device checks it and shows the new table with changed rows highlighted. Nothing is written yet.
4. **Apply.** The device erases otadata, any changed nvs/coredump partitions and the first sector of a moved backup slot, then writes the table and restarts.
5. **Upload the filesystem image** if the filesystem moved or was resized. The page shows a reminder until you do.
6. **Upload larger firmware** - the new slot size applies from now on.

What the check refuses: a file that isn't a valid table, overlaps, misalignment (4 KB, 64 KB for apps), anything before 0x9000 or past the end of flash, a missing otadata/ota_0/ota_1, a boot slot that isn't where the running firmware is or is too small for it, more than 256 KB of nvs/coredump to clear, and devices with flash encryption or secure boot. What it warns about: nvs moved (settings erased), filesystem moved (files lost), backup slot moved (no rollback until the next update), coredump cleared, partitions removed.

**Risks:** the partition table has no backup copy. If power is lost during the write (well under a second) the device will not boot until it is reflashed over USB. Filesystems are not erased; the app must format the new one on first boot (`SPIFFS.begin(true)`), which can take 10-20 s. Do this before arming a task watchdog.

## Customizing the Page

The page is the `COTA_OTA_PAGE` constant in `src/CustomOTA.cpp`, with its CSS in the `<style>` block near the top. Edit it there and rebuild. It also loads `/styles.css` from your own web server if you provide one, so you can restyle it without changing the library.

Device-specific values are not in the HTML: the page's script fills them in from `/update/status`.

## File Structure
```
CustomOTA-ESP32/
├── src/
│   ├── CustomOTA.h          # Header file
│   └── CustomOTA.cpp        # Implementation (contains the web page)
├── partitions/
│   ├── default.csv          # 4MB partition table
│   ├── default_8MB.csv      # 8MB partition table
│   └── default_16MB.csv     # 16MB partition table
├── examples/
│   ├── BasicOTA/
│   ├── AdvancedOTA/
│   └── SecureOTA/
├── build_timestamp.py       # Sets the build date on every PlatformIO build
├── library.json             # PlatformIO metadata
├── library.properties       # Arduino IDE metadata
└── README.md
```

## Troubleshooting

### Common Issues & Solutions

**"Upload failed: network connection lost" / timed out**
- The connection dropped before the device replied. The device replies before it restarts, so this is a real failure, not a reboot
- Check WiFi signal and retry. Reloading the OTA page shows what the device saw (see [Interrupted Uploads](#interrupted-uploads))

**"Upload Failed (HTTP 400/500)"**
- Check .bin file validity and ESP32 compatibility
- Ensure sufficient heap memory (minimum 32KB required)
- Verify correct partition table configuration

**"No OTA Partitions Found"**
- **You need dual OTA partitions!** See partition configuration above
- Check your partition table has both `app0` and `app1` partitions

**"No Backup Available"**
- Normal until first OTA update completes
- ESP32 automatically preserves firmware in inactive partition

**"SPIFFS Mount Failed"**
- SPIFFS partition may be corrupted
- Use `setSpiffsFormatOnFailure(true)` to enable automatic recovery
- Or manually format SPIFFS if needed

**Authentication Issues**
- Verify credentials match exactly
- Check for special characters in password
- Disable auth temporarily for testing: `ota.begin(&server)`

**"Insufficient Memory"**
- Free up heap memory before updates
- Check for memory leaks in your application
- Consider increasing ESP32 PSRAM if available

**Interface Won't Load**
- Check ESP32 IP address and network connection
- Verify AsyncWebServer is running on port 80
- Check browser console for JavaScript errors

### Debug Output
Enable detailed logging:
```ini
build_flags = -DCUSTOM_OTA_DEBUG=1
```

This provides extensive debug information including:
- SPIFFS mount status and operations
- Heap memory monitoring
- Update progress and validation steps
- Rollback validation details
- Network and authentication status

## Performance Considerations

### Real-Time Applications
- **No blocking operations** - All functions return quickly
- **Minimal memory footprint** - Optimized for resource-constrained devices
- **Deferred operations** - Time-sensitive tasks use millis() timing
- **Update gate** - Use `setUpdateAllowed()` to stop updates and restarts while the device is doing something it must not be interrupted in

### Memory Usage
- **Base memory**: ~8KB RAM
- **Update buffer**: ~32KB heap required during updates
- **SPIFFS updates**: Variable based on filesystem size

### Network Requirements
- **Stable WiFi connection** required during updates
- **Minimum bandwidth**: 1Mbps recommended
- **Timeout handling**: Automatic recovery from network issues

## License
MIT License - Free for commercial and personal use.

**Note**: This library depends on AsyncTCP and ESPAsyncWebServer which are licensed under LGPL 3.0. This does not affect the MIT license of this library, but users should be aware of the LGPL requirements for those dependencies.

**Created by Boyd Benton**
