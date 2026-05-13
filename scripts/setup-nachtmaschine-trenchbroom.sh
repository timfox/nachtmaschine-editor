#!/usr/bin/env bash
# Merge Nachtmaschine paths into TrenchBroom Preferences.json (Linux portable layout).
# Run after building the engine (nachtmap) and choosing your checkout root if not this tree.
#
# Usage: setup-nachtmaschine-trenchbroom.sh [--help]
# Env:   NACHTMAP — override path to nachtmap (default: \$REPO_ROOT/build/tools/compilers/nachtmap)
set -euo pipefail

if [[ "${1:-}" == "-h" || "${1:-}" == "--help" ]]; then
  sed -n '1,20p' "$0"
  exit 0
fi

REPO_ROOT="$(cd -- "$(dirname -- "${BASH_SOURCE[0]}")/../.." && pwd)"
NACHTMAP="${NACHTMAP:-${REPO_ROOT}/build/tools/compilers/nachtmap}"
PREFS="${HOME}/.TrenchBroom/Preferences.json"

if [[ ! -x "${NACHTMAP}" ]]; then
  echo "nachtmap not found or not executable: ${NACHTMAP}" >&2
  echo "Build the engine first (see repo AGENTS.md) or export NACHTMAP=/path/to/nachtmap" >&2
  exit 1
fi

mkdir -p "$(dirname "${PREFS}")"
if [[ -f "${PREFS}" ]]; then
  python3 - <<PY
import json, pathlib
p = pathlib.Path("${PREFS}")
data = json.loads(p.read_text())
data["Games/Nachtmaschine/Path"] = "${REPO_ROOT}"
data["Games/Nachtmaschine/Tool Path/nachtmap"] = "${NACHTMAP}"
p.write_text(json.dumps(data, indent=2) + "\n")
PY
else
  python3 - <<PY
import json, pathlib
p = pathlib.Path("${PREFS}")
data = {
  "Games/Nachtmaschine/Path": "${REPO_ROOT}",
  "Games/Nachtmaschine/Tool Path/nachtmap": "${NACHTMAP}",
}
p.write_text(json.dumps(data, indent=2) + "\n")
PY
fi

echo "Wrote ${PREFS}"
echo "  Games/Nachtmaschine/Path -> ${REPO_ROOT}"
echo "  Games/Nachtmaschine/Tool Path/nachtmap -> ${NACHTMAP}"
echo "Editor: bash editor/scripts/configure-linux-nacht.sh && cmake --build editor/build --target TrenchBroom -j\$(nproc)"
