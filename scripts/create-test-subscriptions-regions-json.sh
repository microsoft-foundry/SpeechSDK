#!/usr/bin/env bash
#
# Copyright (c) Microsoft Corporation. All rights reserved.
# Licensed under the MIT license.
#
# -----------------------------------------------------------------------------
# create-test-subscriptions-regions-json.sh
#
# Generates a test.subscriptions.regions.json file from environment variables.
#
# Required environment variables:
#   SPEECH_SERVICE_REGION   – Azure Speech service region
#   SPEECH_SUBSCRIPTION_KEY – Azure Speech subscription key
#
# Optional arguments:
#   $1  – Output file path (default: test.subscriptions.regions.json)
# -----------------------------------------------------------------------------

set -euo pipefail

OUTPUT_FILE="${1:-test.subscriptions.regions.json}"

# --- Validate required environment variables ---------------------------------

MISSING=()
[[ -z "${SPEECH_SERVICE_REGION:-}" ]] && MISSING+=("SPEECH_SERVICE_REGION")
[[ -z "${SPEECH_SUBSCRIPTION_KEY:-}" ]] && MISSING+=("SPEECH_SUBSCRIPTION_KEY")

if [[ ${#MISSING[@]} -gt 0 ]]; then
    echo "Error: the following required environment variable(s) are not set:" >&2
    for var in "${MISSING[@]}"; do
        echo "  - $var" >&2
    done
    echo "" >&2
    echo "Usage:" >&2
    echo "  export SPEECH_SERVICE_REGION=\"<region>\"" >&2
    echo "  export SPEECH_SUBSCRIPTION_KEY=\"<key>\"" >&2
    echo "  bash $(basename "$0") [output-file]" >&2
    exit 1
fi

# --- Write JSON --------------------------------------------------------------

cat > "$OUTPUT_FILE" <<EOF
{
    "UnifiedSpeechSubscription": {
        "Region": "${SPEECH_SERVICE_REGION}",
        "Key": "${SPEECH_SUBSCRIPTION_KEY}"
    }
}
EOF

echo "Created: $OUTPUT_FILE"
