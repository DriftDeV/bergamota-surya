# Adreno 6xx UCHE register programming (a615/a618/a630) and reset risk

Source note: Linux files were fetched from torvalds/linux master on 2026-11-02 (no commit sha recorded; line numbers refer to that fetch). Downstream files are from LineageOS/android_kernel_xiaomi_sm6150, a Qualcomm msm-4.14 derivative and the same SoC family as the Poco X3 NFC (sm7150).

## 1. Exact UCHE writes and values (Linux mainline and downstream kgsl)

### Takeaway
The patch's UCHE offsets, lo/hi split and values are identical to what Linux and downstream kgsl program for a618/a615/a630. The only difference is UCHE_CLIENT_PF (patch and Linux 0x81, downstream 0x1), and that is harmless because the field is a perf-counter select. The UCHE values themselves are not wrong or dangerous.

### Cited Findings
- All offsets are dword offsets; the 64-bit registers are lo at N and hi at N+1:
  - UCHE_ADDR_MODE_CNTL 0x0E00
  - UCHE_MODE_CNTL 0x0E01
  - UCHE_WRITE_RANGE_MAX 0x0E05/0x0E06
  - UCHE_WRITE_THRU_BASE 0x0E07/0x0E08
  - UCHE_TRAP_BASE 0x0E09/0x0E0A
  - UCHE_GMEM_RANGE_MIN 0x0E0B/0x0E0C
  - UCHE_GMEM_RANGE_MAX 0x0E0D/0x0E0E
  - UCHE_CACHE_WAYS 0x0E17
  - UCHE_FILTER_CNTL 0x0E18
  - UCHE_CLIENT_PF 0x0E19
  - Sources: [a6xx.xml lines 1202-1214 (variants A6XX-A7XX)](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/msm/registers/adreno/a6xx.xml); [downstream a6xx_reg.h lines 728-743](https://github.com/LineageOS/android_kernel_xiaomi_sm6150/blob/HEAD/drivers/gpu/msm/a6xx_reg.h) lists the explicit _LO/_HI pairs.
- UCHE_CLIENT_PF has only a PERFSEL bitfield, bits 7:0 (a6xx.xml line 1214-1216). The downstream header masks the low 3 bits as the client id when it reads the register after a fault (CLIENT_ID_MASK 0x7, a6xx_reg.h line 743). It is a perf-select/fault-client register, not a control register. — [a6xx.xml](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/msm/registers/adreno/a6xx.xml)
- Mainline a6xx_hw_init, for non-a7xx parts that include a618/a615/a630 (a6xx_gpu.c):
  - Line 1233: `UCHE_ADDR_MODE_CNTL = 1`, in the group of all *_ADDR_MODE_CNTL writes (CP, VSC, GRAS, RB, PC, HLSQ, VFD, VPC, UCHE, SP, TPL1, SECVID_TSB), lines 1224-1236.
  - Lines 1269-1277: `gpu_write64(UCHE_WRITE_RANGE_MAX, uche_trap_base + 0xfc0)`, `gpu_write64(UCHE_TRAP_BASE, uche_trap_base)`, `gpu_write64(UCHE_WRITE_THRU_BASE, uche_trap_base)`, under the comment "Disable L2 bypass in the UCHE".
  - Lines 1279-1290: GMEM range MIN = SZ_1M, MAX = SZ_1M + info->gmem - 1, as gpu_write64. It is skipped for a650-family, a702 and a730. For a740-family and a722 the min is SZ_16M.
  - Lines 1292-1296: non-a7xx writes `UCHE_FILTER_CNTL = 0x804` and `UCHE_CACHE_WAYS = 0x4`.
  - Line 1361: `UCHE_CLIENT_PF = BIT(7) | 0x1` (0x81).
  - Source: [a6xx_gpu.c](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/msm/adreno/a6xx_gpu.c)
- `adreno_gpu->uche_trap_base = 0x1fffffffff000ull` is set in a6xx_gpu_init (a6xx_gpu.c line 2781). That is 0x0001_ffff_ffff_f000, so trap base is hi=0x0001ffff, lo=0xfffff000, and WRITE_RANGE_MAX = +0xfc0 is hi=0x0001ffff, lo=0xffffffc0. — [a6xx_gpu.c](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/msm/adreno/a6xx_gpu.c)
- Catalog: the a618 entry has chip id 0x06010800, revn 618, gmem SZ_512K and quirk ADRENO_QUIRK_4GB_VA (a6xx_catalog.c ~lines 847-862). The a615 entry with machine qcom,sm7150 and the same chip id 0x06010800 is at ~lines 821-845 (gmem 512K, zap a615_zap.mbn). So GMEM range is 0x100000..0x17ffff. — [a6xx_catalog.c](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/msm/adreno/a6xx_catalog.c)
- Downstream kgsl a6xx_start (adreno_a6xx.c lines 812-831) writes the same values, as separate lo/hi writes:
  - WRITE_RANGE_MAX_LO 0xffffffc0 / HI 0x0001ffff
  - TRAP_BASE_LO 0xfffff000 / HI 0x0001ffff
  - WRITE_THRU_BASE_LO 0xfffff000 / HI 0x0001ffff
  - GMEM_RANGE_MIN_LO = uche_gmem_base / HI 0
  - GMEM_RANGE_MAX_LO = uche_gmem_base + gmem_size - 1 / HI 0
  - FILTER_CNTL 0x804, CACHE_WAYS 0x4
  - Source: [adreno_a6xx.c](https://github.com/LineageOS/android_kernel_xiaomi_sm6150/blob/HEAD/drivers/gpu/msm/adreno_a6xx.c)
- Downstream `uche_gmem_base = ALIGN(gmem_size, SZ_1M)`, which is 0x100000 for 512K (adreno.c line 849). The a615 and a618 entries both have `.gmem_size = SZ_512K` (adreno-gpulist.h ~lines 340-380). Downstream writes `UCHE_CLIENT_PF = 1` (line 943), not 0x81; it also writes `UCHE_MODE_CNTL = (mal<<23)|(bit<<21)` (line 929, from UBWC config) and RBBM_INTERFACE_HANG_INT_CNTL (1<<30)|0xcfffff. — [adreno_a6xx.c](https://github.com/LineageOS/android_kernel_xiaomi_sm6150/blob/HEAD/drivers/gpu/msm/adreno_a6xx.c)
- Comparison with the patch (`xnu/patches-11215/0301-surya-gpu-a618-scm-zap.patch`, u64[] table around lines 1825-1843, commit b94e414):
  - Register offsets 0xe05, 0xe07, 0xe09, 0xe0b, 0xe0d: match.
  - Hi word written to reg+1: match.
  - WRITE_RANGE_MAX 0x0001ffff:ffffffc0, TRAP_BASE 0x0001ffff:fffff000, WRITE_THRU 0x0001ffff:fffff000: match.
  - GMEM_RANGE_MIN 0x100000 and MAX 0x17ffff, hi 0: match.
  - UCHE_CLIENT_PF 0xe19 = 0x81: matches mainline, differs from downstream only in bit 7.
  - FILTER_CNTL 0x804 at 0xe18, CACHE_WAYS 4 at 0xe17: match (written earlier in the probe).
  - The A630 patch 0103 (lines 2062-2075) writes the same values, and that path is documented in NOTES.md as working on beryllium (sdm845).
- Hi/lo order in the patch is correct (lo first, then hi), and Linux's gpu_write64 also writes lo then hi.

### Inferences
- The 0x1ffff:xxxx pattern is not derived from the SMMU address space in the Linux driver; it is a fixed constant (trap base 0x1_ffff_ffff_f000) chosen to sit above any GPU VA the driver hands out. a618 has the 4GB_VA quirk, so VAs stay below 4 GiB and the trap base is far above them. The 0x1ffff constant is a 49-bit value, which matches a 48-bit-plus-one VA window. The patch does not need to change it.
- Alignment: lo values 0xfffff000 (4 KiB) and 0xffffffc0 (64 B) are the same as Linux. GMEM min 1 MiB-aligned. Nothing is misaligned.

### Gaps
- I did not fetch the Mesa a6xx.xml separately; the mainline copy is the same file Mesa uses (the Linux tree imports it from Mesa). I did not check whether msm-4.14 has a different UCHE_CLIENT_PF encoding beyond the header and a6xx_start.

## 2. Does a wrong UCHE value hang the bus or reset the SoC?

### Takeaway
I found no public evidence that wrong UCHE range or trap values cause a bus hang, an XPU violation or a watchdog reset. The documented effect is a UCHE interrupt (UCHE_OOB_ACCESS, bit 24, or UCHE_TRAP_INTR, bit 25, in RBBM_INT_0), which is a GPU-side fault. The patch's values are the correct ones anyway, so this is unlikely to be the cause of the reset.

### Cited Findings
- Linux defines the UCHE faults as RBBM interrupts and only logs them: `UCHE | Out of bounds access` (a6xx_gpu.c line 1943-1944; bits at a6xx.xml lines 60-61; mask lines 1152-1166). They are reported by the GPU, not routed to a bus error. — [a6xx_gpu.c](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/msm/adreno/a6xx_gpu.c)
- Unclocked GMU register access was confirmed by testing on sc7180 to be a problem ("reading the GMU registers needs the GMU clocks to be enabled"), which the commit that stopped gpu_busy from reading them describes. — [LKML backport](https://lkml.iu.edu/hypermail/linux/kernel/2208.1/08993.html) (summary of the commit text via search result; I did not read the full commit)
- On GMU targets the GMU owns the GX GDSC after init; Linux added a refcount on the GX power domain before GMU init so nobody can collapse it. — [LKML 2007.1/07299](https://lkml.iu.edu/hypermail/linux/kernel/2007.1/07299.html) (from search snippet only)
- Project history: NOTES.md records an SoC reset after a masked SMR read in an earlier SMMU experiment ("0083 on sweet: a masked SMR read back 0 and the SoC reset after the GMU boot", comment at 0103 line ~1063). That is the same class of symptom (instant reset, no panic) on SMMU registers, not on the GPU. — local file `xnu/patches-11215/0103-beryllium-gpu-a630-bringup.patch`
- Searches for TigerShota/ios-sweet notes on GPU register writes hanging the SoC returned nothing relevant. The repo exists (`gh api repos/TigerShota/ios-sweet` returned it) but I did not read its contents (the gh code search printed no hits for UCHE_WRITE_RANGE_MAX).

### Inferences
- A reset with no panic is consistent with a secure-world or hardware watchdog (an XPU or bus-hang bite), not with a GPU fault interrupt. UCHE misprogramming should produce a GPU interrupt instead.
- Writes alone are unlikely to matter here: the earlier probe run (11 of 13 writes stuck, no reset) already wrote UCHE_ADDR_MODE_CNTL, WRITE_THRU_BASE lo, GMEM_RANGE_MIN lo, FILTER_CNTL and CACHE_WAYS successfully.

### Gaps
- No public document states what an XPU violation or bus hang on the GPU register aperture looks like on sm7150. I found no source listing which GPU register offsets are XPU-protected on SM7150.
- No evidence either way on whether writing UCHE_TRAP_BASE hi = 0x1ffff can itself make later GPU accesses trap; this was not tested anywhere I could find.

## 3. Which other parts of the patch change could cause the reset

### Takeaway
Comparing commit b94e414 with its parent, the diff contains exactly two changes: the new UCHE u64[] block (plus UCHE_CLIENT_PF) and the packet-by-packet test. The UCHE block adds writes to 0xe06, 0xe08, 0xe0a, 0xe0c, 0xe0e and 0xe05/0xe09/0xe0d lo plus 0xe19 and reads of all of them; all are documented UCHE registers. The packet test is the more plausible trigger, but this is a guess I could not verify.

### Cited Findings
- The packet test changes from one ring flush (NOP, MEM_WRITE, EVENT_WRITE together) to three flushes with `cp_ring_wait_idle` and `cp_status_line` (CP_INTERRUPT_STATUS reads) after each (diff of commit b94e414, `git show b94e414`). The patch's own CP_EVENT_WRITE uses `CACHE_FLUSH_TS | CP_EVENT_WRITE_IRQ`, which makes the GPU write a timestamp and raise an IRQ.
- Linux hw_init writes UCHE after enabling hardware clock gating and after the GBIF/VBIF QoS config, and before CP setup (a6xx_gpu.c lines 1224-1296 versus 1384). The patch's probe writes 0x3da (named RBBM_CLIENT_QOS_CNTL), which the NOTES.md handoff says does not exist on a618. Mainline uses REG_A6XX_RBBM_VBIF_CLIENT_QOS_CNTL = 3 for a630/a615/a618 and RBBM_GBIF_CLIENT_QOS_CNTL for a610/a640/a650 (a6xx_gpu.c lines ~1247-1256). I did not verify the offsets of those two registers, so the 0x3da name is unchecked. This write was already in the previous run that did not reset.
- Registers in the earlier probe that need GX or the GMU on a GMU-managed GPU: all of CP, RB, GRAS, VSC, UCHE and PC live in the GX domain. Linux writes them with GX on, so the earlier non-reset run shows that GX is powered (the writes stuck) in the patch's configuration. — [a6xx_gpu.c](https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/msm/adreno/a6xx_gpu.c) hw_init runs under the GMU lock after gmu resume (lines 1549-1558).

### Inferences
- Because UCHE_TRAP_BASE, WRITE_THRU_BASE and GMEM_RANGE are now set to Linux values, memory traffic from the CP and UCHE behaves differently for the first time: before, trap base hi was left at its reset value. A change in which GPU accesses reach the bus (for example the CACHE_FLUSH_TS timestamp write or the CP_MEM_WRITE target address) is a plausible way for the new run to touch memory the first run did not. This is a guess. Check whether CP_SCRATCH_VA is below the 4 GiB limit and whether its SMMU mapping is valid, since a bad GPU-side write through the SMMU normally gives an SMMU fault (FSR) and not a reset.
- To bisect without another guess: re-run with only the UCHE block (no packet split), then with only the packet split (no UCHE hi writes). Flush the log (`sweet_log_sync`) after every single register write in the UCHE block so the last line before the reset identifies the register, since only one `sweet_log_sync` follows the whole loop today.

### Gaps
- Which of the two changes triggers the reset is not established. The previous log has no line after the last printed message, so I cannot tell from the repo whether the reset happens inside the UCHE loop (the only sync is after the loop). I did not read the logs/ directory.
