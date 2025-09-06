#!/usr/bin/env bash
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
VENV_DIR="$SCRIPT_DIR/.venv"

if [ ! -d "$VENV_DIR" ]; then
  echo "Creating virtual environment in $VENV_DIR"
  if command -v uv >/dev/null 2>&1; then
    uv venv "$VENV_DIR"
  else
    python3 -m venv "$VENV_DIR"
  fi
fi

source "$VENV_DIR/bin/activate"

if ! python -c 'import mcp' >/dev/null 2>&1; then
  if command -v uv >/dev/null 2>&1; then
    uv pip install -r "$SCRIPT_DIR/requirements.txt"
  else
    python -m pip install --upgrade pip || true
    pip install -r "$SCRIPT_DIR/requirements.txt"
  fi
fi

exec python "$SCRIPT_DIR/lldb_mcp.py" "$@"
