# Contributing to CustomOTA

We welcome contributions to the CustomOTA library! This document provides guidelines for contributing to the project.

## 🚀 Getting Started

### Prerequisites
- **PlatformIO** or **Arduino IDE** with ESP32 support
- **ESP32 development board** for testing
- **Git** for version control
- Basic knowledge of **C++** and **ESP32 development**

### Development Setup
1. **Fork the repository** on GitHub
2. **Clone your fork** locally:
   ```bash
   git clone https://github.com/YOUR-USERNAME/CustomOTA-ESP32.git
   cd CustomOTA-ESP32
   ```
3. **Create a feature branch**:
   ```bash
   git checkout -b feature/your-feature-name
   ```

## 🔧 Development Guidelines

### Real-Time Compatibility Requirements
**CRITICAL**: This library is designed for real-time applications. All contributions MUST follow these rules:

1. **NEVER use `delay()`** - Use `millis()` based timing instead
2. **NEVER use blocking operations** in main thread
3. **Keep all functions non-blocking** and return quickly (microseconds)
4. **Use deferred/scheduled operations** for time-sensitive tasks
5. **All timing operations** must use non-blocking `millis()` comparisons

### Code Style Guidelines

#### ✅ **DO - Real-Time Safe Code**
```cpp
// Correct: Non-blocking timing
unsigned long startTime = millis();
if (millis() - startTime > 2000) {
    // Execute after 2 seconds
}

// Correct: Deferred restart
_restartPending = true;
_restartTime = millis() + 2000;
```

#### ❌ **DON'T - Blocking Code**
```cpp
// WRONG: Blocks execution
delay(2000);  // NEVER DO THIS!

// WRONG: Blocking operations
while (condition) {
    // Busy wait without yield
}
```

### Debug System Rules
- Use `COTA_DEBUG_PRINT`, `COTA_DEBUG_PRINTLN`, `COTA_DEBUG_PRINTF` macros
- **NO** direct `Serial.print()` calls in library code
- Debug output must be controllable via `CUSTOM_OTA_DEBUG` build flag
- **NO** runtime debug switches that affect performance

### Memory Management
- **Minimize heap usage** during operations
- **Free allocated memory** promptly
- **Avoid fragmentation** by using consistent allocation sizes
- **Test with low memory conditions**

## 📝 Coding Standards

### Naming Conventions
- **Classes**: `PascalCase` (e.g., `CustomOTA`)
- **Methods/Functions**: `camelCase` (e.g., `handleUpdate`)
- **Variables**: `camelCase` with underscore prefix for private members (e.g., `_isActive`)
- **Constants**: `UPPER_SNAKE_CASE` (e.g., `MAX_UPLOAD_SIZE`)
- **Debug Macros**: `COTA_DEBUG_*` prefix to avoid conflicts

### File Organization
```
src/
├── CustomOTA.h          # Main header with class definition
├── CustomOTA.cpp        # Implementation
└── README.md            # Library documentation

examples/
├── BasicOTA/            # Simple usage example
├── SecureOTA/           # Authentication example
└── AdvancedOTA/         # Complex integration example
```

### Documentation Requirements
- **All public methods** must have clear docstring comments
- **Complex algorithms** need inline comments
- **Examples** must be included for new features
- **README updates** required for new functionality

## 🧪 Testing Guidelines

### Required Testing
Before submitting a pull request, test your changes with:

1. **Multiple ESP32 variants** (ESP32, ESP32-S2, ESP32-S3, ESP32-C3)
2. **Different partition schemes** (4MB, 8MB, 16MB)
3. **With and without authentication**
4. **Both firmware and filesystem updates**
5. **Rollback functionality**
6. **Memory stress testing**
7. **Real-time application integration**

### Test Scenarios
- **Upload success** with various file sizes
- **Upload failure** handling and recovery
- **Network interruption** during upload
- **Power cycle** during update process
- **Invalid file** upload attempts
- **Authentication** success and failure
- **Rollback** between partitions multiple times
- **Memory usage** under continuous operation

## 📋 Pull Request Process

### Before Submitting
1. **Test thoroughly** on real hardware
2. **Update documentation** if needed
3. **Add examples** for new features
4. **Verify real-time compatibility**
5. **Check memory usage**
6. **Ensure debug system works correctly**

### Pull Request Template
```markdown
## Description
Brief description of changes made.

## Type of Change
- [ ] Bug fix (non-breaking change fixing an issue)
- [ ] New feature (non-breaking change adding functionality)
- [ ] Breaking change (fix or feature causing existing functionality to change)
- [ ] Documentation update

## Testing Performed
- [ ] Tested on ESP32 hardware
- [ ] Verified real-time compatibility
- [ ] Tested memory usage
- [ ] Verified rollback functionality
- [ ] Tested with/without authentication
- [ ] Checked debug output control

## Hardware Tested
- [ ] ESP32
- [ ] ESP32-S2
- [ ] ESP32-S3
- [ ] ESP32-C3

## Checklist
- [ ] Code follows project style guidelines
- [ ] No `delay()` calls added
- [ ] Uses COTA_DEBUG_* macros for debug output
- [ ] Self-review of code completed
- [ ] Documentation updated
- [ ] Examples provided for new features
```

## 🐛 Bug Reports

### Bug Report Template
When reporting bugs, please include:

```markdown
**Describe the Bug**
Clear description of what the issue is.

**To Reproduce**
Steps to reproduce the behavior:
1. Go to '...'
2. Click on '....'
3. See error

**Expected Behavior**
What you expected to happen.

**Screenshots/Serial Output**
If applicable, add screenshots or serial monitor output.

**Hardware Information:**
- ESP32 variant: [e.g., ESP32, ESP32-S3]
- Flash size: [e.g., 4MB, 8MB]
- Partition scheme: [e.g., default.csv]
- Arduino/ESP-IDF version: [e.g., 2.0.11]

**Library Configuration:**
- CustomOTA version: [e.g., 2.0.0]
- Debug enabled: [Yes/No]
- Authentication used: [Yes/No]
- Integration type: [Standalone/Real-time application]

**Additional Context**
Any other context about the problem.
```

## 🔄 Feature Requests

### Feature Request Template
```markdown
**Is your feature request related to a problem?**
Clear description of the problem.

**Describe the solution you'd like**
Clear description of what you want to happen.

**Describe alternatives you've considered**
Other solutions you've considered.

**Additional context**
Any other context or screenshots about the feature request.

**Real-Time Compatibility**
How does this feature maintain real-time compatibility?

**Backward Compatibility**
Will this break existing implementations?
```

## 📚 Documentation Contributions

### Documentation Standards
- **Clear examples** for all features
- **Step-by-step instructions** for setup
- **Troubleshooting sections** for common issues
- **API reference** with parameter descriptions
- **Real-time compatibility notes** for all functions

### Areas Needing Documentation
- Advanced integration examples
- Troubleshooting guides
- Performance optimization tips
- Custom styling guides
- Security best practices

## 🏆 Recognition

Contributors will be recognized in:
- **README.md** contributors section
- **CHANGELOG.md** for specific contributions
- **GitHub releases** acknowledgments

## 📞 Getting Help

- **GitHub Issues**: For bug reports and feature requests
- **GitHub Discussions**: For questions and general discussion
- **Code Review**: All pull requests receive thorough review

## 📜 Code of Conduct

### Our Standards
- **Be respectful** and inclusive
- **Provide constructive feedback**
- **Focus on technical merit**
- **Help others learn and grow**
- **Maintain professionalism**

### Enforcement
Instances of abusive, harassing, or otherwise unacceptable behavior may be reported to the project maintainers.

---

Thank you for contributing to CustomOTA! Your efforts help make ESP32 OTA updates better for everyone. 🚀
