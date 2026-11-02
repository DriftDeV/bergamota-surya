#!/bin/bash
# Builds ir3asm (ir3 assembly -> a6xx shader binary, default a618) against a Mesa 26.2.5 tree, on macOS, no GPU needed.
#   tools/ir3asm/build.sh            # clone + configure + build into research/mesa-ir3 (git-ignored), then ir3asm there
#   research/mesa-ir3/ir3asm tools/ir3asm/first_store.asm out.bin [gpu_id [chip_id_hex]]
# Needs: python3, git, cmake, flex and a bison >= 3.x (Apple's /usr/bin/bison is 2.3: `brew install bison`).
# Mesa is cloned and built only for its ir3 libraries; libnir is a stub in this tools-only build, so ir3asm links the
# unused NIR symbols as zeroed stubs (see build-ir3asm.py logic below).
set -euo pipefail
ROOT=$(cd "$(dirname "$0")/../.." && pwd)
W=$ROOT/research/mesa-ir3
TAG=${MESA_TAG:-mesa-26.2.5}
mkdir -p "$W"
[ -d "$W/venv" ] || python3 -m venv "$W/venv"
"$W/venv/bin/pip" install --quiet meson ninja mako pyyaml packaging ply
export PATH=/usr/local/opt/bison/bin:/opt/homebrew/opt/bison/bin:$W/venv/bin:$PATH
bison --version | head -1 | grep -qE ' [3-9]\.' || { echo "need bison >= 3 (brew install bison)"; exit 1; }
[ -d "$W/src" ] || git clone --depth 1 --branch "$TAG" https://gitlab.freedesktop.org/mesa/mesa.git "$W/src"
if [ ! -d "$W/build" ]; then
  (cd "$W/src" && meson setup ../build -Dgallium-drivers= -Dvulkan-drivers= -Dtools=freedreno -Dllvm=disabled \
    -Dglx=disabled -Degl=disabled -Dgles1=disabled -Dgles2=disabled -Dopengl=false -Dplatforms= -Dgbm=disabled \
    -Dvalgrind=disabled -Dlibunwind=disabled -Dzstd=disabled -Dbuild-tests=false)
fi
# Only the static libraries: the stock ir3_disasm target fails to link because libnir is a stub.
ninja -C "$W/build" src/freedreno/ir3/libfreedreno_ir3.a src/freedreno/isa/libir3encode.a src/freedreno/isa/libir3decode.a \
  src/util/libmesa_util.a src/util/libmesa_util_clflush.a src/util/libmesa_util_clflushopt.a src/util/libmesa_util_simd.a \
  src/util/blake3/libblake3.a src/c11/impl/libmesa_util_c11.a src/compiler/nir/libnir.a src/compiler/libcompiler.a \
  src/freedreno/common/libfreedreno_common.a src/compiler/isaspec/libisaspec.a src/freedreno/ir3/ir3_disasm.p/tests_disasm.c.o
cp "$ROOT/tools/ir3asm/ir3asm.c" "$W/ir3asm.c"
python3 - "$W" <<'PY'
import json, os, shlex, subprocess, sys
w = sys.argv[1]
os.chdir(os.path.join(w, 'build'))
cc = json.load(open('compile_commands.json'))
cmd = next(e['command'] for e in cc if e['file'].endswith('freedreno/ir3/tests/disasm.c'))
keep, skip = [], 0
for p in shlex.split(cmd)[1:]:
    if skip:
        skip -= 1
        continue
    if p in ('-o', '-MF', '-MQ'):
        skip = 1
        continue
    if p in ('-MD', '-c') or p.endswith('disasm.c') or p.startswith('-fdiagnostics') or p.startswith('-Werror'):
        continue
    keep.append(p)
libs = ("src/freedreno/ir3/libfreedreno_ir3.a src/freedreno/isa/libir3encode.a src/util/libmesa_util.a "
        "src/util/libmesa_util_clflush.a src/util/libmesa_util_clflushopt.a src/util/libmesa_util_simd.a "
        "src/util/blake3/libblake3.a src/c11/impl/libmesa_util_c11.a src/compiler/nir/libnir.a src/compiler/libcompiler.a "
        "src/freedreno/common/libfreedreno_common.a src/freedreno/isa/libir3decode.a src/compiler/isaspec/libisaspec.a").split()
base = ['cc'] + keep + ['-w', '../ir3asm.c', '-o', '../ir3asm'] + libs + ['-lz', '-lm', '-lc++']
subprocess.run(base + ['-Wl,-undefined,dynamic_lookup'], check=True)
syms = sorted(x for x in subprocess.run(['nm', '-u', '../ir3asm'], capture_output=True, text=True).stdout.split()
              if x.startswith(('_nir_', '_glsl_', '_ir3_nir')))
open('../nirstubs.s', 'w').write(''.join('.globl %s\n.zerofill __DATA,__bss,%s,65536,4\n' % (x, x) for x in syms))
subprocess.run(['cc'] + keep + ['-w', '../ir3asm.c', '../nirstubs.s', '-o', '../ir3asm'] + libs + ['-lz', '-lm', '-lc++'], check=True)
print('built', os.path.join(w, 'ir3asm'), 'with', len(syms), 'NIR stubs')
PY

# csgen: host-side generator of the compute dispatch command stream (kernel patch 0301 embeds its output).
#   research/mesa-ir3/csgen INSTRLEN first_store_a618.bin > cs_out.h
(cd "$W/src/src/freedreno/registers" && for x in a6xx:a6xx.xml adreno_pm4:pm4.xml a6xx_descriptors:desc.xml a6xx_enums:enums.xml; do
   "$W/venv/bin/python" gen_header.py --rnn . --xml adreno/${x%%:*}.xml c-defines > "$W/${x##*:}.h"; done
 for x in adreno_common a7xx_enums a8xx_enums; do "$W/venv/bin/python" gen_header.py --rnn . --xml adreno/$x.xml c-defines > "$W/$x.h"; done)
cp "$ROOT/tools/ir3asm/csgen.c" "$W/csgen.c"
(cd "$W" && cc -w -o csgen csgen.c) && echo "built $W/csgen"
