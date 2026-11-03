#!/usr/bin/env bash
#
# mayhem/test.sh — RUN the known-answer oracles that mayhem/build.sh built (build/ft_oracle for FreeType,
# build/cinder_oracle for Cinder's own OBJ/JSON/Radiance/QOI parsers).
# Each check is a fixed input (a sample TTF, an in-memory BDF font, a hand-built outline) with an
# asserted computed value (family name, glyph count, advance, rendered bitmap checksum, pixel counts).
set -uo pipefail
[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH
: "${SRC:=/mayhem}"
cd "$SRC"

emit_ctrf() {
  local tool="$1" passed="$2" failed="$3" skipped="${4:-0}" pending="${5:-0}" other="${6:-0}"
  local tests=$(( passed + failed + skipped + pending + other ))
  cat > "${CTRF_REPORT:-$SRC/ctrf-report.json}" <<JSON
{
  "results": {
    "tool": { "name": "$tool" },
    "summary": {
      "tests": $tests,
      "passed": $passed,
      "failed": $failed,
      "pending": $pending,
      "skipped": $skipped,
      "other": $other
    }
  }
}
JSON
  printf 'CTRF {"results":{"tool":{"name":"%s"},"summary":{"tests":%d,"passed":%d,"failed":%d,"pending":%d,"skipped":%d,"other":%d}}}\n' \
    "$tool" "$tests" "$passed" "$failed" "$pending" "$skipped" "$other"
  [ "$failed" -eq 0 ]
}

ORACLE="$SRC/build/ft_oracle"
CINDER_ORACLE="$SRC/build/cinder_oracle"
EXPECTED_CHECKS=39
for o in "$ORACLE" "$CINDER_ORACLE"; do
  [ -x "$o" ] || { echo "missing $o (mayhem/build.sh should have built it)" >&2; emit_ctrf "ft-oracle" 0 1; exit 1; }
done

# Two oracles, each ending in a DONE line: FreeType (22 checks) and Cinder's own parsers (17).
ft_out="$("$ORACLE" "$SRC/samples/Kaleidoscope/resources/Asap-Regular.ttf" 2>&1)"
cinder_out="$("$CINDER_ORACLE" 2>&1)"
echo "$ft_out"
echo "$cinder_out"
out="$ft_out"$'\n'"$cinder_out"

passed=$(printf '%s\n' "$out" | grep -c '^PASS ')
failed=$(printf '%s\n' "$out" | grep -c '^FAIL ')
# A neutered or truncated oracle prints too few PASS lines and no DONE marker: count the gap as failures.
for o in "$ft_out" "$cinder_out"; do
  printf '%s\n' "$o" | grep -qx 'DONE' || failed=$(( failed + 1 ))
done
if [ "$passed" -lt "$EXPECTED_CHECKS" ]; then failed=$(( failed + EXPECTED_CHECKS - passed )); fi

emit_ctrf "ft-oracle" "$passed" "$failed"
