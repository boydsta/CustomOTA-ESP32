# Changelog

All notable changes to the CustomOTA library will be documented in this file.

The format is based on [Keep a Changelog](https://keepachangelog.com/en/1.0.0/),
and this project adheres to [Semantic Versioning](https://semver.org/spec/v2.0.0.html).

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
