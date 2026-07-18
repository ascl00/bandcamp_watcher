#!/usr/bin/env bash

set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
REPO_DIR="$(cd "${SCRIPT_DIR}/.." && pwd)"
PROJECT_PATH="${REPO_DIR}/bandcamp_watcher.xcodeproj"
BUILD_ROOT="${REPO_DIR}/build/release-install"
PRODUCTS_DIR="${BUILD_ROOT}/Build/Products/Release"
INSTALL_DIR="${HOME}/bin"
CLI_PRODUCT="${PRODUCTS_DIR}/bandcamp_watcher"
APP_PRODUCT="${PRODUCTS_DIR}/Bandcamp Watcher Status.app"
APP_DESTINATION="${INSTALL_DIR}/Bandcamp Watcher Status.app"
PLIST_TEMPLATE="${REPO_DIR}/bandcamp_watcher/bandcamp_watcher.plist"
LAUNCH_AGENTS_DIR="${HOME}/Library/LaunchAgents"
PLIST_DESTINATION="${LAUNCH_AGENTS_DIR}/launched.bandcamp_watcher.plist"
CURRENT_USERNAME="$(id -un)"
TEMP_DIR="$(mktemp -d "${TMPDIR:-/tmp}/bandcamp_watcher-install.XXXXXX")"
TEMP_PLIST="${TEMP_DIR}/launched.bandcamp_watcher.plist"

cleanup() {
  rm -f "${TEMP_PLIST}"
  rmdir "${TEMP_DIR}" 2>/dev/null || true
}
trap cleanup EXIT

echo "Building bandcamp_watcher (Release)..."
xcodebuild \
  -project "${PROJECT_PATH}" \
  -scheme bandcamp_watcher \
  -configuration Release \
  -derivedDataPath "${BUILD_ROOT}" \
  build

echo "Building Bandcamp Watcher Status (Release)..."
xcodebuild \
  -project "${PROJECT_PATH}" \
  -scheme "Bandcamp Watcher Status" \
  -configuration Release \
  -derivedDataPath "${BUILD_ROOT}" \
  build

if [[ ! -x "${CLI_PRODUCT}" ]]; then
  echo "CLI build product not found: ${CLI_PRODUCT}" >&2
  exit 1
fi

if [[ ! -d "${APP_PRODUCT}" ]]; then
  echo "Status app build product not found: ${APP_PRODUCT}" >&2
  exit 1
fi

echo "Installing release products to ${INSTALL_DIR}..."
mkdir -p "${INSTALL_DIR}"
install -m 0755 "${CLI_PRODUCT}" "${INSTALL_DIR}/bandcamp_watcher"
/usr/bin/ditto "${APP_PRODUCT}" "${APP_DESTINATION}"

echo "Preparing launchd plist for ${CURRENT_USERNAME}..."
cp "${PLIST_TEMPLATE}" "${TEMP_PLIST}"
/usr/bin/plutil -replace UserName -string "${CURRENT_USERNAME}" "${TEMP_PLIST}"
/usr/bin/plutil -replace ProgramArguments -json "[\"${INSTALL_DIR}/bandcamp_watcher\"]" "${TEMP_PLIST}"
/usr/bin/plutil -replace StandardOutPath -string "${HOME}/Library/Logs/bandcamp_watcher.log" "${TEMP_PLIST}"
/usr/bin/plutil -replace StandardErrorPath -string "${HOME}/Library/Logs/bandcamp_watcher.error.log" "${TEMP_PLIST}"
/usr/bin/plutil -lint "${TEMP_PLIST}"

echo "Installing launchd plist to ${PLIST_DESTINATION}..."
mkdir -p "${LAUNCH_AGENTS_DIR}"
install -m 0644 "${TEMP_PLIST}" "${PLIST_DESTINATION}"

echo "Installed:"
echo "  ${INSTALL_DIR}/bandcamp_watcher"
echo "  ${APP_DESTINATION}"
echo "  ${PLIST_DESTINATION}"
