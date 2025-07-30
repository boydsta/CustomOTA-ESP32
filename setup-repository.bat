@echo off
REM CustomOTA Repository Setup Script for Windows
REM This script helps set up the CustomOTA library as a standalone Git repository

echo === CustomOTA Repository Setup ===
echo.

REM Check if we're in the right directory
if not exist "library.json" (
    echo ❌ Error: Please run this script from the CustomOTA library directory
    echo Expected files: library.json, src/CustomOTA.h
    pause
    exit /b 1
)

if not exist "src\CustomOTA.h" (
    echo ❌ Error: Please run this script from the CustomOTA library directory  
    echo Expected files: library.json, src/CustomOTA.h
    pause
    exit /b 1
)

echo ✅ Found CustomOTA library files

REM Initialize git repository if not already done
if not exist ".git" (
    echo 🔧 Initializing Git repository...
    git init
    echo ✅ Git repository initialized
) else (
    echo ✅ Git repository already exists
)

REM Add all files to git
echo 📝 Adding files to Git...
git add .

REM Create initial commit if no commits exist
git rev-parse --verify HEAD >nul 2>&1
if errorlevel 1 (
    echo 📝 Creating initial commit...
    git commit -m "Initial commit: CustomOTA v2.0.0" -m "" -m "- Advanced ESP32 OTA library with dual partition backup system" -m "- Modern web interface with drag & drop uploads" -m "- Real-time compatible operations (no delay() calls)" -m "- Configurable debug output system" -m "- Infinite rollback support between partitions" -m "- Optional HTTP authentication" -m "- Comprehensive documentation and examples"
    echo ✅ Initial commit created
) else (
    echo ✅ Repository already has commits
)

REM Check if remote origin exists
git remote get-url origin >nul 2>&1
if errorlevel 1 (
    echo.
    echo 🌐 To publish to GitHub:
    echo 1. Create a new repository on GitHub named 'CustomOTA-ESP32'
    echo 2. Run these commands:
    echo    git remote add origin https://github.com/boydsta/CustomOTA-ESP32.git
    echo    git branch -M main
    echo    git push -u origin main
    echo.
    echo 3. Create a release tag:
    echo    git tag v2.0.0
    echo    git push origin v2.0.0
) else (
    echo ✅ Remote origin already configured
)

echo.
echo 📁 Repository Structure:
echo ├── src/
echo │   ├── CustomOTA.h          # Main header file
echo │   └── CustomOTA.cpp        # Implementation
echo ├── examples/
echo │   ├── BasicOTA/            # Simple usage example
echo │   ├── SecureOTA/           # Authentication example
echo │   └── AdvancedOTA/         # Production-ready example
echo ├── .github/
echo │   ├── workflows/           # GitHub Actions
echo │   └── ISSUE_TEMPLATE/      # Issue templates
echo ├── README.md                # Comprehensive documentation
echo ├── CHANGELOG.md             # Version history
echo ├── CONTRIBUTING.md          # Contribution guidelines
echo ├── library.json             # PlatformIO library manifest
echo ├── library.properties       # Arduino library manifest
echo └── LICENSE                  # MIT License

echo.
echo 🎉 CustomOTA repository is ready!
echo.
echo 📚 Next Steps:
echo 1. Review and customize the README.md if needed
echo 2. Test the examples on your hardware
echo 3. Create GitHub repository and push
echo 4. Submit to Arduino Library Manager (optional)
echo 5. Submit to PlatformIO Library Registry (optional)

echo.
echo 🔧 Library Features Ready:
echo - ✅ ESP32 Dual Partition Backup System
echo - ✅ Modern Web Interface (drag & drop)
echo - ✅ Real-Time Compatible (no delay() calls)
echo - ✅ Configurable Debug Output
echo - ✅ Infinite Rollback Support
echo - ✅ Optional HTTP Authentication
echo - ✅ Comprehensive Documentation
echo - ✅ Production-Ready Examples
echo - ✅ GitHub Actions Workflow
echo - ✅ Issue Templates
echo - ✅ Contributing Guidelines

echo.
echo Happy coding! 🚀
echo.
pause
