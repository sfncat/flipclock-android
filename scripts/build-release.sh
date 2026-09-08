#!/usr/bin/env bash
# build-release.sh — Linux 一键构建带签名的 release APK
# 用法：./scripts/build-release.sh
# 部署到 Linux 服务器后需 chmod +x scripts/build-release.sh

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

echo "=== FlipClock Android Release Build ==="

# 1. 检查签名配置
PROPS_FILE="$PROJECT_ROOT/keystore/keystore.properties"
TEMPLATE_FILE="$PROJECT_ROOT/keystore/keystore.properties.template"
if [ ! -f "$PROPS_FILE" ]; then
    echo "[!] keystore/keystore.properties 不存在，从模板创建..."
    cp "$TEMPLATE_FILE" "$PROPS_FILE"
    echo "    请编辑 keystore/keystore.properties 填写密码后重新运行。"
    exit 1
fi
if grep -qE "your_store_password|your_key_alias" "$PROPS_FILE"; then
    echo "[!] keystore.properties 仍是模板默认值，请填写真实密码后重新运行。"
    exit 1
fi
echo "[+] 签名配置已加载"

# 2. 检查 Java
if [ -z "${JAVA_HOME:-}" ]; then
    echo "[!] JAVA_HOME 未设置，尝试使用系统 java..."
    if ! command -v java &>/dev/null; then
        echo "[X] 未找到 Java，请安装 JDK 21 并设置 JAVA_HOME。"
        exit 1
    fi
else
    echo "[+] JAVA_HOME = $JAVA_HOME"
fi

# 3. 检查 Android SDK
if [ -z "${ANDROID_HOME:-}" ] && [ -z "${ANDROID_SDK_ROOT:-}" ]; then
    echo "[!] ANDROID_HOME / ANDROID_SDK_ROOT 未设置，Gradle 可能无法找到 SDK。"
fi

# 4. 读取版本号
VERSION_NAME="unknown"
VERSION_CODE="unknown"
while IFS='=' read -r key value; do
    key="$(echo "$key" | tr -d '[:space:]')"
    case "$key" in
        APP_VERSION_NAME) VERSION_NAME="$(echo "$value" | tr -d '[:space:]')" ;;
        APP_VERSION_CODE) VERSION_CODE="$(echo "$value" | tr -d '[:space:]')" ;;
    esac
done < "$PROJECT_ROOT/gradle.properties"
echo "[+] 版本: $VERSION_NAME (code $VERSION_CODE)"

# 5. 构建
echo ""
echo "开始构建 release APK..."
chmod +x "$PROJECT_ROOT/gradlew"
"$PROJECT_ROOT/gradlew" assembleRelease --no-daemon

# 6. 整理产物
APK_SRC="$PROJECT_ROOT/app/build/outputs/apk/release/app-release.apk"
if [ ! -f "$APK_SRC" ]; then
    echo "[X] 未找到 app-release.apk，构建可能未签名成功。"
    exit 1
fi

RELEASE_DIR="$PROJECT_ROOT/releases"
mkdir -p "$RELEASE_DIR"
APK_DST="$RELEASE_DIR/FlipClockV2-${VERSION_NAME}-release.apk"
cp "$APK_SRC" "$APK_DST"

echo ""
echo "=== 构建成功 ==="
echo "APK: $APK_DST"
echo "版本: $VERSION_NAME (code $VERSION_CODE)"

# 7. 验证签名
SDK_ROOT="${ANDROID_HOME:-${ANDROID_SDK_ROOT:-}}"
if [ -n "$SDK_ROOT" ]; then
    BUILD_TOOLS="$(ls -d "$SDK_ROOT/build-tools/"* 2>/dev/null | sort -V | tail -1)"
    if [ -n "$BUILD_TOOLS" ] && [ -x "$BUILD_TOOLS/apksigner" ]; then
        echo ""
        echo "验证签名..."
        "$BUILD_TOOLS/apksigner" verify --print-certs "$APK_DST"
    fi
fi
