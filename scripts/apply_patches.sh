#!/bin/bash
set -e

[[ $1 == --benchmark && -n $2 ]] || { echo "Usage: $0 --benchmark <benchmark-name>"; exit 1; }
bm=$2
ROOT_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)
pd=$ROOT_DIR/../benchmarks/patches/$bm
rd=$ROOT_DIR/../benchmarks/deps/$bm

[[ -d $pd ]] || { echo "Patch directory not found: $pd"; exit 1; }
git -C "$rd" rev-parse --is-inside-work-tree &>/dev/null || { echo "Target repository is not a valid Git repo: $rd"; exit 1; }

echo -e "Applying patches for benchmark: $bm\nPatches from: $pd\nTarget repo: $rd\n"

for f in "$pd"/*.patch; do
  echo "Processing patch: $f"
  git -C "$rd" am --3way --whitespace=fix < "$f" || { echo "Failed to apply patch: $f"; exit 1; }
  echo "Patch applied successfully: ${f##*/}"
done
