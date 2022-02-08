#!/bin/bash

set -e

. /runner-common.sh

# Check we're running a supported architecture
cpu_arch=$(get_arch)
if [ "$cpu_arch" = "unsupported" ]; then
    echo "Unsupported CPU arch."
    exit 1
fi

gh_pat=${GH_RUNNER_PAT:-}
if [ -f "$GH_RUNNER_PAT_FILE" ]; then
    gh_pat=$(cat "$GH_RUNNER_PAT_FILE")
fi

if [ -z "$gh_pat" ]; then
    echo "Github Personal Access Token (PAT) required"
    exit 1
fi

if [ -n "$GH_RUNNER_LABELS" ]; then
    runner_labels="--labels ${GH_RUNNER_LABELS}"
fi

# Check if we need to download the latest GH runner. If we don't have it, or the version we have
# is over a day old, get the latest release
if [ -f "$GH_RUNNER_ARCHIVE" ]; then
    archive_ts=$(stat "$GH_RUNNER_ARCHIVE" -c %Z)
    curr_ts=$(date +%s)

    if (( (curr_ts - archive_ts) > 86400 )); then
        echo "Existing GH runner archive is more than 24h old."
        echo "Downloading new version."
        runner_vers="$(get_runner_vers)"
        download_runner "$runner_vers" "$cpu_arch" "$GH_RUNNER_ARCHIVE"
    else
        echo "Using existing GH runner archive"
    fi
else
    echo "No GH runner archive. Downloading."
    runner_vers="$(get_runner_vers)"
    download_runner "$runner_vers" "$cpu_arch" "$GH_RUNNER_ARCHIVE"
fi

# Clean up the GH runner dir if it isnt already clean
rm -rf "${GH_RUNNER_DIR:?}/*"

tar -xzf "$GH_RUNNER_ARCHIVE" -C "$GH_RUNNER_DIR"

cd "$GH_RUNNER_DIR"

runner_name="${GH_RUNNER_NAME:-$HOSTNAME}-${REPLICA:-0}"

# We need to get a temporary registration token
reg_token=$(curl -X POST \
    -H "Authorization: token ${gh_pat}" \
    -H "Accept: application/vnd.github.v3+json" \
    https://api.github.com/repos/dosbox-staging/dosbox-staging/actions/runners/registration-token \
    | jq -r '.token')

# Configure the runner
# Note, $runner_labels should be split
# shellcheck disable=SC2086
./config.sh \
    --unattended \
    --url https://github.com/dosbox-staging/dosbox-staging \
    --token "$reg_token" \
    --name "$runner_name" \
    $runner_labels \
    --ephemeral

# Finally, start the runner
./run.sh
