#!/bin/bash

get_arch()
{
    local arch
    arch=$(uname -m)
    case "$arch" in
        x86_64|amd64)
            echo "x64" ;;
        arm*)
            echo "arm" ;;
        aarch64)
            echo "arm64" ;;
        *)
            echo "unsupported" ;;
    esac
}

get_runner_vers()
{
    curl --silent "https://api.github.com/repos/actions/runner/releases/latest" | jq -r '.tag_name'
}

download_runner()
{
    local vers="$1"
    local arch="$2"
    local archive="$3"
    curl -L -o "$archive" \
        "https://github.com/actions/runner/releases/download/${vers}/actions-runner-linux-${arch}-${vers:1}-noruntime-noexternals.tar.gz"
}

download_init_runner()
{
    local vers="$1"
    local arch="$2"
    local archive="$3"
    curl -L -o "$archive" \
        "https://github.com/actions/runner/releases/download/${vers}/actions-runner-linux-${arch}-${vers:1}.tar.gz"
}
