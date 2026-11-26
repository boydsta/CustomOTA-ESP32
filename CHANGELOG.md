# Changelog

All notable changes to the CustomOTA library will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

## [Unreleased]

### Fixed
- `CUSTOMOTA_VERSION_PATCH` was 0 in 1.6.1; it now matches `CUSTOMOTA_VERSION_STRING`.
- `library.json` names the `mathieucarbou` AsyncTCP and ESPAsyncWebServer as dependencies, so a registry install gets the versions the library is built against. It lists Arduino as the only framework.
- The examples no longer call `delay()` in `loop()`.

### Added
- README: Security section.

## [1.6.1] - 2026-10-29

### Changed
- **The partition section on the OTA page is folded up by default.** It shows as one line, "Partition Options (advanced)", and opens on click, so the partition editor can be built in without cluttering the page. It unfolds on its own when the filesystem is missing after a table change or a new table is waiting to be applied, so those messages are never hidden.

## [1.6.0] - 2026-10-28

### Fixed
- **Dropped uploads no longer stay "in progress" until reboot.** A dropped connection (browser closed, phone left the WiFi) never delivers the final chunk, so `isUpdateInProgress()` stayed true and the `Update` session stayed open. The upload is now aborted after `setUploadTimeout()` (default 30 s) without data. The running firmware is untouched, because the boot partition is only switched after a complete upload.
- **A second upload no longer breaks the one in progress.** Starting an upload used to abort the running one and restart the flash write, and the first upload's remaining data was appended to it, so both failed. An unauthenticated upload request reset the upload state the same way before its credentials were checked. Now only the request that started the upload can write to it: a second upload gets 409 "Another upload is in progress", and an unauthenticated one gets 401, without disturbing it.
- **Rollback is no longer offered when the backup slot holds no valid firmware.** A failed firmware upload overwrites the backup slot, but `hasBackup` stayed true, so the OTA page offered a rollback that was then refused. Backup detection only looked at the first bytes of the slot, and a failed upload can leave a valid header there. `hasBackup` now clears as soon as an upload writes to the slot, and the backup is found by verifying the whole image (header, segments and checksum) the way the bootloader does. Because that reads the whole image, it runs in a low-priority background task 5 s after `begin()` (on the other core from `loop()` where there is one), so it doesn't slow start-up or interrupt the application.

### Added
- `setUploadTimeout(unsigned long timeoutMs)` - how long an upload may go without data before it is aborted (default 30000 ms).
- `isBusy()` - upload in progress or restart pending. Use it to hold off sleep or power-down.
- `setBackupCheckDelay(unsigned long delayMs)` - when the background backup check runs after `begin()` (default 5000 ms). `isBackupChecked()` tells whether it has finished; `/update/status` reports it as `backupChecked`. Until then `hasBackup` is false and rollback requests get 409.
- `/update/status` reports `lastUploadFailed`, true from a failed or stalled upload until the next upload starts.
- OTA page shows the device's upload state on load and on its 5 s poll:
  - an upload it did not start (another browser, or one cut off before a reload): "An upload is in progress (43%) - wait for it to finish", with the progress bar, and Upload disabled;
  - the last failed upload, e.g. "Last upload failed: Upload stalled at 43% - not applied, running firmware unchanged. Safe to try again.";
  - when a watched upload finishes, the page reloads once the device is back;
  - the Rollback button once the background backup check has finished.
- `tools/ota_test.py` - tests a running device over HTTP: bad, stalled, dropped and concurrent uploads, authentication, the partition editor, install, reboot and rollback. Standard-library Python, not part of the published library.

### Changed
- Upload is disabled while this page's own upload is running.
- The upload confirmation dialogs are two lines shorter, so they fit without scrolling.
- `setFirmwareVersion()` no longer re-reads the backup status, and `refreshBackupStatus()` now verifies the whole backup image (it blocks briefly) and does nothing while the background check is running.

## [1.5.0] - 2026-10-23

### Added
- **Update gate** - `setUpdateAllowed(bool allowed, const char* reason = nullptr)`. While blocked:
  - firmware/filesystem uploads, `/update/reboot`, `/update/rollback` and `/update/partitions/apply` are refused with **409** and the reason;
  - an upload already in progress is aborted at its next chunk (running firmware untouched);
  - a restart that is already scheduled is held, and runs once updates are allowed again.
  Default is allowed, so applications that don't call it are unchanged.
- `isUpdateAllowed()` and `isRestartPending()`.
- `/update/status` reports `updateAllowed` and `blockedReason`.
- OTA page shows the blocked reason and greys out Upload, Reboot, Rollback and partition Apply while blocked. It re-checks the gate every 5 seconds.

### Changed
- `shouldRestart()` is false while updates are blocked.
- A failed reboot request now shows the device's reply on the OTA page, not only the HTTP code.

## [1.4.0] - 2026-10-19

### Staying on 1.3
1.4.0 only adds the partition editor, and the editor is off unless you build with `-D CUSTOM_OTA_PARTITIONS=1`. Without the flag, 1.4.0 builds the same code as 1.3. To stay on 1.3 anyway, pin the tag in `platformio.ini`:
```ini
https://github.com/boydsta/CustomOTA-ESP32.git#v1.3.0
```
An unpinned `https://github.com/boydsta/CustomOTA-ESP32.git` (or `#main`) moves to 1.4.0.

### Added
- **Partition editor** (`-D CUSTOM_OTA_PARTITIONS=1`, off by default; with it off the code is unchanged) - The OTA page gets a Partition Table section: upload `partitions.bin`, review the new table, its checks and what will be lost, then apply and restart. Built on `CustomOTAPartitions` (`parse`, `check`, `apply`). After a change that moves the filesystem, the page reminds you to upload the filesystem image until one is uploaded.
- Routes `/update/partitions` (GET, POST), `/update/partitions/apply`, `/update/partitions/cancel`.
- Rollback is refused while a restart is pending (partition editor builds only), so it cannot write otadata for the old layout.
- The OTA page and `/update/status` show the CustomOTA version (`libraryVersion`). This was on `main` after the v1.3.0 tag.

### Tested
- On hardware: a production device moved from 1 MB to 1600 KB app slots over the air (Arduino 2.0.17 / ESP-IDF 4.4). Also builds on the current pioarduino core.

## [1.3.0] - 2026-10-18

### Security
- **Upload authentication** - Uploads are now authenticated on the first chunk, before anything is written. Previously a successful firmware upload rebooted before the password check ran, so anyone on the network could flash firmware.
- **`setCredentials()` + `begin(server)`** - `begin(server)` no longer clears credentials set earlier, which had silently turned authentication off (the order used in the README and the Advanced/Secure examples).

### Fixed
- **`GET /update/status` returned the OTA page** - The specific `/update/*` routes are now registered before `/update`.
- **Filesystem uploads written to the firmware partition** - The page sent the `type` field after the file; it is now sent first and also in the URL.
- **Failed uploads got no response** - Low memory, `Update.begin` and write failures left the browser waiting for its 2-minute timeout; every upload now gets a success or error response.
- **Status stuck on "in progress"** after a filesystem update.
- **SPIFFS remount formatted the new image** - The remount after a filesystem update no longer formats on failure.
- **Debug output could not be enabled** - `-DCUSTOM_OTA_DEBUG=1` was overridden by a `#define` in the .cpp.
- **Filesystem updates overwrote the rollback record.**
- **Progress never reached 100%** - Progress is measured against the upload size, and resets between uploads.
- **Restart timer and `millis()` wrap** - Uses the wrap-safe form.
- **Rollback target** - Uses the OTA slot logic, so a factory partition is never chosen.
- **Build date did not change** - It came from when the library was last compiled. `build_timestamp.py` now sets it on every PlatformIO build.

### Changed
- **Deferred restart** - Firmware update, reboot and rollback now send their response first and restart after `setRestartDelay()` (default 3 s) from `handle()`. The page shows the real result instead of treating any dropped connection as success, and reloads once the device is back.
- **Heap** - The OTA page is a constant in flash, streamed without a heap copy (it was a ~23 KB `String` built per request). Device details are loaded from `/update/status`.
- **Rollback check** - Replaced the 4 KB stack-buffer sector check with a magic-byte check; `esp_ota_set_boot_partition()` verifies the full image.
- **Page** - Shows the hardware ID as the heading and tab title, and the CustomOTA version in the footer. Emoji icons removed.
- **`/update/status`** - Added `hardwareID`, `runningPartition`, `backupPartition`, `canRollback`, `restartPending` and `libraryVersion`.
- **Uploads are refused while a restart is pending.**

## [1.2.4] - 2025-08-29

### Fixed
- **Serial Upload Boot Partition Inconsistency** - Fixed device booting to wrong partition after serial upload
- **OTA Boot State Correction** - Added automatic detection and correction of stale boot partition settings
- **Partition State Validation** - Enhanced startup validation to detect and fix partition mismatches

### Changed
- **Boot Partition Management** - Enhanced boot partition handling for mixed OTA/serial workflows
- **Startup Validation** - Added partition state checking on library initialization

### Technical Details
- Added boot partition mismatch detection in `begin()`
- Stale OTA boot partition settings are corrected to match the running partition

## [1.2.3] - 2025-08-29

### Fixed
- **Robust First-Run Behavior** - Improved handling of missing NVS keys on initial device startup
- **Clean Debug Output** - Eliminated spurious NVS error messages from serial output

### Changed
- **NVS Operations** - Preferences operations check that a key exists first
- **Error Handling** - More graceful handling of missing or corrupted backup metadata

### Technical Details
- Replaced direct `getBytesLength()` calls with `isKey()` existence checks

## [1.2.2] - 2025-08-29

### Fixed
- **Critical Upload Progress Bug** - Fixed progress bar stuck at 0% due to routing mismatch
- **Unexpected Reboot Dialogs** - Removed reboot confirmation from upload process (moved to separate reboot function)
- **Missing POST Route** - Added POST route for `/update` endpoint to handle multipart file uploads
- **Upload Reliability** - Enhanced error handling for network disconnections during device reboot
- **JavaScript Upload Function** - Separated upload and reboot functionality for cleaner user experience

### Changed
- **Upload Process** - Reboot confirmation now only appears when manually rebooting device
- **Progress Tracking** - Upload progress now correctly displays from 0% to 100%
- **Error Handling** - Better distinction between actual failures and expected connection drops

### Technical Details
- Fixed routing mismatch where JavaScript sent to `/update` but server only handled `/update/start`
- Moved reboot confirmation from `uploadUpdate()` function to separate `rebootDevice()` function
- Enhanced network error handling to properly detect successful uploads that result in device reboot
- Maintained real-time compatibility and non-blocking operations

## [1.2.1] - 2025-08-27

### Added
- **Heap Monitoring** - Automatic memory checks before updates to prevent insufficient memory failures
- **Enhanced Rollback Validation** - Sector-by-sector firmware integrity validation for safer rollbacks
- **Configurable Restart Delays** - Customizable reboot timing with `setRestartDelay()` method
- **Safer SPIFFS Handling** - Configurable SPIFFS formatting behavior to prevent data loss
- **Advanced Configuration API** - New methods for fine-tuning library behavior:
  - `setRestartDelay(unsigned long delayMs)` - Configure restart delay (default: 3000ms)
  - `setSpiffsFormatOnFailure(bool allowFormat)` - Control automatic SPIFFS formatting (default: true)
- **Enhanced Security Options** - Better control over SPIFFS data protection
- **Improved Error Handling** - More comprehensive error detection and recovery

### Changed
- **SPIFFS Mount Logic** - More conservative formatting approach to preserve user data
- **Rollback Process** - Enhanced validation with sector-by-sector integrity checks
- **Update Process** - Added heap monitoring before starting updates
- **Library Metadata** - Updated version information and feature descriptions
- **Debug Output** - Enhanced logging for new features and validation steps

### Fixed
- **SPIFFS Data Loss** - Fixed potential data loss during SPIFFS mount failures
- **Memory-Related Failures** - Added heap monitoring to prevent out-of-memory update failures
- **Rollback Reliability** - Enhanced validation prevents rollback to corrupted firmware
- **Timing Issues** - Configurable restart delays for different application requirements

### Security
- **SPIFFS Protection** - New option to disable automatic formatting for maximum data security
- **Enhanced Validation** - Multiple layers of firmware integrity checking
- **Memory Safety** - Heap monitoring prevents memory exhaustion attacks

### Technical Details
- Heap monitoring requires minimum 32KB free memory for updates
- Sector-by-sector validation checks first 32KB of firmware for ESP32 image integrity
- Configurable restart delays maintain real-time operation compatibility
- SPIFFS formatting behavior is now user-controllable for data protection

## [1.2.0] - 2025-08-07

### Added
- **Automatic Backup Detection** - New `autoDetectBackup()` function automatically detects firmware in inactive partitions
- **Enhanced Rollback Support** - Improved rollback functionality for serial upload → OTA workflow compatibility
- **Force Backup Detection** - Rollback now attempts auto-detection if no backup metadata exists
- **Version Information API** - Added static functions to query library version and feature support:
  - `getLibraryVersion()`, `getVersionMajor()`, `getVersionMinor()`, `getVersionPatch()`
  - `getBuildInfo()` for build timestamp information
  - Feature availability checks: `supportsFilesystemUpdate()`, `supportsAutoBackupDetection()`, etc.
- **Enhanced Debug Output** - Added comprehensive version and feature reporting during initialization
- **Improved Partition Analysis** - Better detection and validation of ESP32 dual partition system

### Changed
- **Backup Metadata Saving** - `saveUpdateInfo()` now proactively saves current firmware as backup before OTA update
- **Rollback Process** - Enhanced `restoreFromBackup()` with fallback auto-detection mechanism
- **Library Identification** - Updated library.json and library.properties with new version and enhanced descriptions
- **Debug Messaging** - More detailed logging for backup detection and rollback operations

### Fixed
- **Serial Upload Compatibility** - Fixed rollback not working after serial upload followed by OTA update
- **Backup Detection Edge Cases** - Resolved issues where valid backup firmware wasn't detected
- **Metadata Persistence** - Improved backup metadata saving and validation

### Technical Details
- Automatic detection checks ESP32 image header in inactive partition
- Enhanced preference storage for backup metadata
- Better error handling and fallback mechanisms
- Maintains real-time operation compatibility throughout

## [1.0.0] - 2025-07-31

### Added
- **ESP32 Dual Partition System** - Automatic firmware backup using ESP32's built-in dual OTA partitions
- **Modern Web Interface** - Drag & drop upload with real-time progress tracking
- **Infinite Rollback Support** - Switch between partitions indefinitely
- **Real-Time Compatibility** - Non-blocking operations (no delay() calls)
- **Partition Tables** - Pre-configured partition tables for 4MB, 8MB, and 16MB flash
- **Debug Control System** - Configurable debug output via `CUSTOM_OTA_DEBUG` build flag
- **HTTP Authentication** - Optional username/password protection
- **Build Timestamp Display** - Shows compile date/time in web interface
- **Comprehensive Documentation** - Complete setup guide with partition requirements
- **Filesystem Updates** - SPIFFS filesystem updates, selectable on the web page, with automatic SPIFFS partition detection
- **Partition Display** - Running and backup partition shown on the web page

### Features
- Dual OTA partition backup system
- Modern responsive web interface
- Real-time upload progress
- Automatic partition detection
- Rollback functionality
- Build timestamp tracking
- Debug logging system
- Authentication support

### Dependencies
- AsyncTCP ^3.2.3
- ESPAsyncWebServer ^3.1.1
- Arduino ESP32 Core

### Requirements
- ESP32 with dual OTA partitions configured
- Minimum 4MB flash memory
- PlatformIO or Arduino IDE
