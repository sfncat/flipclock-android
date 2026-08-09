
# Uncomment this if you're using STL in your project
# You can find more information here:
# https://developer.android.com/ndk/guides/cpp-support
# APP_STL := c++_shared

APP_ABI := armeabi-v7a arm64-v8a x86 x86_64

# Min runtime API level (21 is the minimum supported by NDK r28).
APP_PLATFORM=android-21

# Keep 16 KB page size support (NDK r28+ default for arm64-v8a/x86_64) so the
# libraries stay 16 KB aligned and load on Android 15+ devices.
APP_SUPPORT_FLEXIBLE_PAGE_SIZES := true
