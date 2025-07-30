## Description
Briefly describe the changes made in this pull request.

## Type of Change
Please select the relevant option:

- [ ] 🐛 Bug fix (non-breaking change that fixes an issue)
- [ ] ✨ New feature (non-breaking change that adds functionality)
- [ ] 💥 Breaking change (fix or feature that would cause existing functionality to not work as expected)
- [ ] 📚 Documentation update (changes to documentation only)
- [ ] 🔧 Maintenance (dependency updates, build system changes, etc.)

## Testing Performed
Please describe the tests that you ran to verify your changes:

### Hardware Testing
- [ ] ESP32
- [ ] ESP32-S2  
- [ ] ESP32-S3
- [ ] ESP32-C3
- [ ] Other: ____________

### Flash Memory Sizes Tested
- [ ] 4MB
- [ ] 8MB
- [ ] 16MB
- [ ] Other: ____________

### Functionality Testing
- [ ] Firmware upload (successful)
- [ ] Firmware upload (failure scenarios)
- [ ] Filesystem upload
- [ ] Authentication (when enabled)
- [ ] Rollback functionality
- [ ] Multiple rollbacks (back and forth)
- [ ] Network interruption handling
- [ ] Memory usage under load
- [ ] Real-time compatibility verified

### Debug System Testing
- [ ] Debug output enabled (CUSTOM_OTA_DEBUG=1)
- [ ] Debug output disabled (CUSTOM_OTA_DEBUG=0)
- [ ] No performance impact when debug disabled

## Real-Time Compatibility ⚡
**CRITICAL**: Please confirm real-time compatibility:

- [ ] ✅ No `delay()` calls added anywhere in the code
- [ ] ✅ All timing operations use `millis()` comparisons
- [ ] ✅ Functions return quickly (microseconds, not milliseconds)
- [ ] ✅ No blocking operations in main execution path
- [ ] ✅ Tested integration with time-critical applications

## Code Quality Checklist
- [ ] Code follows the project's coding standards
- [ ] Self-review of code completed
- [ ] Code is commented, particularly in hard-to-understand areas
- [ ] Corresponding changes to documentation made
- [ ] No compiler warnings introduced
- [ ] Memory usage is reasonable and tested

## Debug System Compliance
- [ ] Uses `COTA_DEBUG_PRINT`, `COTA_DEBUG_PRINTLN`, `COTA_DEBUG_PRINTF` macros
- [ ] No direct `Serial.print()` calls in library code
- [ ] Debug output controllable via `CUSTOM_OTA_DEBUG` build flag
- [ ] No runtime debug switches that affect performance

## Backward Compatibility
- [ ] Changes are backward compatible
- [ ] Breaking changes are documented and justified
- [ ] Migration guide updated (if applicable)

## Documentation Updates
- [ ] README.md updated (if needed)
- [ ] CHANGELOG.md updated
- [ ] Examples updated or added (if needed)
- [ ] API documentation updated (if needed)

## Additional Notes
Provide any additional information, context, or screenshots that would be helpful for reviewers:

## Related Issues
Closes #(issue number) <!-- if applicable -->

---

### Reviewer Guidelines
**For Maintainers**: Please verify the following before merging:

1. **Real-Time Compatibility**: All code maintains microsecond-level timing
2. **Memory Safety**: No memory leaks or excessive heap usage  
3. **Hardware Testing**: Tested on multiple ESP32 variants
4. **Debug System**: Proper use of debug macros
5. **Documentation**: Updates reflect the changes made
6. **Backward Compatibility**: Existing implementations still work

### Testing Instructions for Reviewers
1. Test with debug enabled and disabled
2. Verify rollback functionality works repeatedly
3. Test upload failures and recovery
4. Check memory usage under continuous operation
5. Validate real-time timing with oscilloscope if possible
