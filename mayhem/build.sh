#!/usr/bin/env bash
#
# mayhem/build.sh — build (a) Cinder's own GL-free core (src/cinder: DataSource, ObjLoader, JsonTree,
# the Radiance and QOI image sources, ...) and (b) its bundled FreeType (src/freetype) with sanitizers,
# link the fuzz harnesses against them, and build known-answer oracles.
# The window/app/GL layer needs GL/X11/GLFW/audio and is not built; the parsers fuzzed here only need
# mayhem/cinder/platform_headless.cc in place of src/cinder/app/linux/PlatformLinux.cpp.
# Runs inside the commit image as `mayhem` in /mayhem; re-runnable and needs no network.
set -euo pipefail

[ -n "${SOURCE_DATE_EPOCH:-}" ] || unset SOURCE_DATE_EPOCH

: "${SANITIZER_FLAGS=-fsanitize=address,undefined -fno-sanitize-recover=all -fno-omit-frame-pointer}"
: "${DEBUG_FLAGS:=-g -gdwarf-3}"
: "${CC:=clang}" ; : "${CXX:=clang++}" ; : "${LIB_FUZZING_ENGINE:=-fsanitize=fuzzer}"
: "${MAYHEM_JOBS:=$(nproc)}"
: "${COVERAGE_FLAGS=}"
: "${SRC:=/mayhem}"
export SANITIZER_FLAGS DEBUG_FLAGS CC CXX LIB_FUZZING_ENGINE MAYHEM_JOBS COVERAGE_FLAGS

cd "$SRC"

FT="$SRC/src/freetype"
HARNESS="$FT/tools/ftfuzzer"
INCS="-I$SRC/include -I$SRC/include/freetype"
DEFS="-DFT2_BUILD_LIBRARY"

# The module set Cinder's include/freetype/config/ftmodule.h registers (cinder's own CMake list,
# minus the validators/bzip2 that no registered module uses).
FT_SOURCES="
  base/ftbase.c base/ftbbox.c base/ftbdf.c base/ftbitmap.c base/ftcid.c base/ftdebug.c
  base/ftfstype.c base/ftgasp.c base/ftglyph.c base/ftgxval.c base/ftinit.c base/ftmm.c
  base/ftotval.c base/ftpatent.c base/ftpfr.c base/ftstroke.c base/ftsynth.c base/ftsystem.c
  base/fttype1.c base/ftwinfnt.c
  bdf/bdf.c cff/cff.c pcf/pcf.c pfr/pfr.c sfnt/sfnt.c truetype/truetype.c type1/type1.c
  type42/type42.c winfonts/winfnt.c raster/raster.c smooth/smooth.c autofit/autofit.c
  cache/ftcache.c gzip/ftgzip.c lzw/ftlzw.c psaux/psaux.c pshinter/pshinter.c
  psnames/psnames.c cid/type1cid.c
"

# build_ft <outdir> <cflags...> : compile every FreeType source into <outdir>/libft.a (skips fresh objects).
build_ft() {
  local out="$1"; shift
  mkdir -p "$out"
  local objs=() stale=() f o
  for f in $FT_SOURCES; do
    o="$out/${f//\//_}.o"
    objs+=("$o")
    if [ ! -f "$o" ] || [ "$FT/$f" -nt "$o" ]; then stale+=("$f"); fi
  done
  if [ "${#stale[@]}" -gt 0 ]; then
    printf '%s\0' "${stale[@]}" | OUT="$out" FT="$FT" CC="$CC" CFLAGS_ALL="$INCS $DEFS $*" \
      xargs -0 -P "$MAYHEM_JOBS" -n 1 bash -c \
      'f="$1"; $CC $CFLAGS_ALL -c "$FT/$f" -o "$OUT/${f//\//_}.o"' _
  fi
  rm -f "$out/libft.a"
  ar rcs "$out/libft.a" "${objs[@]}"
}

# build_cinder <outdir> <cflags...> : compile Cinder's GL-free core into <outdir>/libcinder-core.a.
# CINDER_JSON_RELAX is added to the bundled jsoncpp.cpp compile only.
CINDER_SOURCES="
  src/cinder/Area.cpp src/cinder/Base64.cpp src/cinder/BSpline.cpp src/cinder/Buffer.cpp
  src/cinder/Camera.cpp src/cinder/Channel.cpp src/cinder/CinderMath.cpp src/cinder/Color.cpp
  src/cinder/DataSource.cpp src/cinder/DataTarget.cpp src/cinder/Exception.cpp
  src/cinder/Filesystem.cpp src/cinder/GeomIo.cpp src/cinder/ImageIo.cpp
  src/cinder/ImageSourceFileQoi.cpp src/cinder/ImageSourceFileRadiance.cpp src/cinder/JsonTree.cpp
  src/cinder/Log.cpp src/cinder/Matrix.cpp src/cinder/ObjLoader.cpp src/cinder/Path2d.cpp
  src/cinder/Perlin.cpp src/cinder/Rect.cpp src/cinder/Sphere.cpp src/cinder/Stream.cpp
  src/cinder/Surface.cpp src/cinder/Triangulate.cpp src/cinder/TriMesh.cpp src/cinder/Url.cpp
  src/cinder/UrlImplCurl.cpp src/cinder/Utilities.cpp src/cinder/app/Platform.cpp
  src/cinder/ip/Fill.cpp src/jsoncpp/jsoncpp.cpp mayhem/cinder/platform_headless.cc
  src/libtess2/bucketalloc.c src/libtess2/dict.c src/libtess2/geom.c src/libtess2/mesh.c
  src/libtess2/priorityq.c src/libtess2/sweep.c src/libtess2/tess.c
"
CINDER_INCS="-I$SRC/include -I$SRC/include/jsoncpp -I$SRC/src"
CINDER_DEFS="-DCINDER_LINUX -DCINDER_HEADLESS -DNDEBUG"
build_cinder() {
  local out="$1"; shift
  mkdir -p "$out"
  local objs=() stale=() f o
  for f in $CINDER_SOURCES; do
    o="$out/${f//\//_}.o"
    objs+=("$o")
    if [ ! -f "$o" ] || [ "$SRC/$f" -nt "$o" ]; then stale+=("$f"); fi
  done
  if [ "${#stale[@]}" -gt 0 ]; then
    printf '%s\0' "${stale[@]}" | OUT="$out" SRC="$SRC" CC="$CC" CXX="$CXX" JSON_RELAX="${CINDER_JSON_RELAX:-}" \
      CFLAGS_ALL="$CINDER_INCS $CINDER_DEFS $*" \
      xargs -0 -P "$MAYHEM_JOBS" -n 1 bash -c '
        f="$1"; extra=""
        case "$f" in src/jsoncpp/jsoncpp.cpp) extra="$JSON_RELAX";; esac
        case "$f" in
          *.c) $CC $CFLAGS_ALL $extra -c "$SRC/$f" -o "$OUT/${f//\//_}.o" ;;
          *)   $CXX -std=c++17 $CFLAGS_ALL $extra -c "$SRC/$f" -o "$OUT/${f//\//_}.o" ;;
        esac' _
  fi
  rm -f "$out/libcinder-core.a"
  ar rcs "$out/libcinder-core.a" "${objs[@]}"
}

# 1) Sanitized + DWARF-3 FreeType for the fuzz harnesses. Only UBSan's `function` check is relaxed:
#    FreeType 2.9.1 initialises modules through FT_Module_Constructor-typed pointers that point at
#    functions with a narrower signature (ftobjs.c:4806 ps_hinter_init), which trips on every FT_Init_FreeType.
#    ASan and all other UBSan checks stay on and halting.
#    -fsanitize=fuzzer-no-link adds libFuzzer coverage to the library itself: the rasterfuzzer harness has
#    almost no branches of its own, so without it Mayhem sees 0 edges.
# shellcheck disable=SC2086
build_ft "$SRC/build/ft-san" -O1 $SANITIZER_FLAGS -fsanitize=fuzzer-no-link -fno-sanitize=function $DEBUG_FLAGS

# 2) Harnesses: libFuzzer binary + standalone reproducer, LSan off at build time.
# shellcheck disable=SC2086
$CXX $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/lsan_off.cc" -o "$SRC/build/lsan_off.o"
# shellcheck disable=SC2086
$CC $SANITIZER_FLAGS $DEBUG_FLAGS -c "$STANDALONE_FUZZ_MAIN" -o "$SRC/build/standalone_main.o"

# rasterfuzzer.cc rewrites its input in place, which libFuzzer aborts on (overwrites-const-input);
# mayhem/rasterfuzzer_copy.cc is the entry point and passes the upstream code a private copy.
for h in ftfuzzer rasterfuzzer; do
  if [ "$h" = rasterfuzzer ]; then
    # shellcheck disable=SC2086
    $CXX -std=c++11 -O1 $SANITIZER_FLAGS $DEBUG_FLAGS $INCS -DLLVMFuzzerTestOneInput=RasterFuzzerOneInput \
        -c "$HARNESS/$h.cc" -o "$SRC/build/$h.o"
    # shellcheck disable=SC2086
    $CXX -std=c++11 -O1 $SANITIZER_FLAGS $DEBUG_FLAGS -c "$SRC/mayhem/rasterfuzzer_copy.cc" -o "$SRC/build/$h-copy.o"
    srcs=("$SRC/build/$h.o" "$SRC/build/$h-copy.o")
  else
    srcs=("$HARNESS/$h.cc")
  fi
  # shellcheck disable=SC2086
  $CXX -std=c++11 -O1 $SANITIZER_FLAGS $DEBUG_FLAGS $INCS $LIB_FUZZING_ENGINE \
      "${srcs[@]}" "$SRC/build/lsan_off.o" "$SRC/build/ft-san/libft.a" -larchive -o "$SRC/$h"
  # shellcheck disable=SC2086
  $CXX -std=c++11 -O1 $SANITIZER_FLAGS $DEBUG_FLAGS $INCS \
      "${srcs[@]}" "$SRC/build/standalone_main.o" "$SRC/build/lsan_off.o" \
      "$SRC/build/ft-san/libft.a" -larchive -o "$SRC/$h-standalone"
done

# 3) Cinder's own parsers, through its public API (ci::ObjLoader, ci::JsonTree, the Radiance and QOI
#    image sources). The bundled jsoncpp negates LLONG_MIN in Json::Reader::decodeNumber on every
#    negative integer (jsoncpp.cpp:722), which UBSan's signed-integer-overflow check halts on at the
#    first negative number; that check alone is relaxed on that one file. Everything else stays halting.
# shellcheck disable=SC2086
CINDER_JSON_RELAX="-fno-sanitize=signed-integer-overflow" \
  build_cinder "$SRC/build/cinder-san" -O1 $SANITIZER_FLAGS -fsanitize=fuzzer-no-link $DEBUG_FLAGS
for t in obj json radiance qoi; do
  h="cinder_${t}_fuzzer"
  # shellcheck disable=SC2086
  $CXX -std=c++17 -O1 $SANITIZER_FLAGS $DEBUG_FLAGS $CINDER_INCS $CINDER_DEFS -c \
      "$SRC/mayhem/cinder/${t}_fuzzer.cc" -o "$SRC/build/$h.o"
  # shellcheck disable=SC2086
  $CXX -O1 $SANITIZER_FLAGS $DEBUG_FLAGS $LIB_FUZZING_ENGINE "$SRC/build/$h.o" "$SRC/build/lsan_off.o" \
      "$SRC/build/cinder-san/libcinder-core.a" -lz -lcurl -o "$SRC/cinder_$t"
  # shellcheck disable=SC2086
  $CXX -O1 $SANITIZER_FLAGS $DEBUG_FLAGS "$SRC/build/$h.o" "$SRC/build/standalone_main.o" "$SRC/build/lsan_off.o" \
      "$SRC/build/cinder-san/libcinder-core.a" -lz -lcurl -o "$SRC/cinder_$t-standalone"
done

# 4) Known-answer oracles with the project's NORMAL flags (no sanitizers), run by mayhem/test.sh.
# shellcheck disable=SC2086
build_ft "$SRC/build/ft-test" -O2 $COVERAGE_FLAGS
# shellcheck disable=SC2086
$CC -O2 $COVERAGE_FLAGS $INCS "$SRC/mayhem/ft_oracle.c" "$SRC/build/ft-test/libft.a" -lm -o "$SRC/build/ft_oracle"
# shellcheck disable=SC2086
build_cinder "$SRC/build/cinder-test" -O2 $COVERAGE_FLAGS
# shellcheck disable=SC2086
$CXX -std=c++17 -O2 $COVERAGE_FLAGS $CINDER_INCS $CINDER_DEFS "$SRC/mayhem/cinder/cinder_oracle.cc" \
    "$SRC/build/cinder-test/libcinder-core.a" -lz -lcurl -o "$SRC/build/cinder_oracle"
