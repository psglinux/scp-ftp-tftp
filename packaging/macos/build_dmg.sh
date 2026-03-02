#!/usr/bin/env bash
set -euo pipefail

# ===================================================================
# GotiKinesis — macOS Build + DMG
#
# This is a thin wrapper around scripts/release.sh.
# Run directly from this location or use scripts/release.sh instead.
# ===================================================================

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
exec "$SCRIPT_DIR/../../scripts/release.sh" "$@"
