# PlatformIO library extra script: passes the current build time to CustomOTA.cpp.
# __DATE__/__TIME__ inside the library only change when the library itself is recompiled,
# so the OTA page showed when CustomOTA was last compiled rather than when the firmware was
# built. This define changes on every build, which recompiles CustomOTA.cpp (this library's
# environment only - the rest of the project is not rebuilt).
Import("env")
import time

env.Append(CPPDEFINES=[
    ("COTA_BUILD_TIMESTAMP", env.StringifyMacro(time.strftime("%d/%m/%Y %H:%M:%S")))
])
