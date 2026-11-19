#!/bin/bash
set -euo pipefail
tmp=$(mktemp -d)
trap 'rm -rf -- "$tmp"' EXIT
chmod 0777 "$tmp"
TMPDIR="$tmp" "$1"
