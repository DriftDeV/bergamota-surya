# ir3asm

Assembles ir3 assembly text into an Adreno a6xx shader binary, offline on the Mac, for a618 (chip id `0x06010800`) by default.
Own code (`ir3asm.c`); it links Mesa 26.2.5's ir3 libraries (Mesa is MIT licensed). Build: `tools/ir3asm/build.sh`
(clones Mesa into the git-ignored `research/mesa-ir3`, needs `brew install bison`).

    research/mesa-ir3/ir3asm tools/ir3asm/first_store.asm out.bin [gpu_id [chip_id_hex]]

Prints the binary size, the register and const counts the dispatch state needs, the kernel buffer table and the dwords.

- `first_store.asm`: the first shader for surya. One invocation writes `0xcafe0001` to dword 0 of the UAV buffer 0.
- `first_store_a618.bin`: its assembled form for a618 (128 bytes: `mov`, `mov`, `nop`, `stib`, `end`, `nop`).
- Other assembly examples to start from: `src/freedreno/computerator/examples/*.asm` in the Mesa tree.
- Limits: this is the assembler only. Dispatching the shader still needs the compute state (SP/HLSQ registers, `CP_EXEC_CS`) in a
  command stream, as `computerator` (`src/freedreno/computerator/a6xx.cc`) builds it.
