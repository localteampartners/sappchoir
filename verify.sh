#!/usr/bin/env bash
# verify.sh — fast feedback loop for sappchoir.
#
# The PLUGIN is built here on purpose. sappkeys#1's postmortem: the tests were
# green while the installed binary stayed stale, because verification skipped
# the plugin target. The headless station regression (sappchoir #1) also runs
# the real processor, so it only exists when the plugin is built.
#
# Two trees, as CONVENTIONS.md describes: build/ (core+CLI+tests, plugin OFF)
# is the fast inner loop; build-plugin/ carries the plugin + the headless
# harness. Both are incremental after the first run.

set -e
set -o pipefail   # a failing test must fail the script, not just print
cd "$(dirname "$0")"

echo "▶ configure"
if [ ! -d build ]; then
  cmake -S . -B build -DCMAKE_BUILD_TYPE=Release -DSAPPCHOIR_BUILD_PLUGIN=OFF > /dev/null
fi
if [ ! -d build-plugin ]; then
  cmake -S . -B build-plugin -DCMAKE_BUILD_TYPE=Release > /dev/null
fi

echo "▶ build (core + cli + tests)"
cmake --build build -j8 --target SappChoirTests sappchoir-cli 2>&1 | grep -E "error|FAILED" && exit 1 || true

echo "▶ build (plugin + headless harness)"
cmake --build build-plugin -j8 --target SappChoirPlugin_All SappChoirHeadless 2>&1 \
  | grep -E "error|FAILED" && exit 1 || true

echo "▶ tests"
./build/SappChoirTests --reporter compact 2>&1 | grep -E "passed|failed" | tail -1

echo "▶ headless station regression (no GUI, no message loop)"
./build-plugin/SappChoirHeadless_artefacts/Release/sappchoir-headless selftest \
  2>/dev/null | grep -E "FAIL|selftest:"

echo "▶ cli smoke"
./build/sappchoir params > /dev/null
./build/sappchoir vowels > /dev/null
./build/sappchoir inspect --diagnostic | head -c 120; echo " ..."

echo "✓ verify passed"
