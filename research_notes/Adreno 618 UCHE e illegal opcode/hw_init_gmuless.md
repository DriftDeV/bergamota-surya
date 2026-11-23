# Linux a6xx hw_init order, GMU-less path, and gaps in patch 0301 (Adreno 618, SM7150)

Sources used (all fetched this session):
- LX = Linux v6.12 `drivers/gpu/drm/msm/adreno/a6xx_gpu.c` / `a6xx_gmu.c`: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/drivers/gpu/drm/msm/adreno/a6xx_gpu.c?h=v6.12 (and `a6xx_gmu.c`, same path). `hw_init()` starts at line 861; `a6xx_set_hwcg` 408; `a6xx_set_cp_protect` 479; `a6xx_calc_ubwc_config` ~535; `a6xx_set_ubwc_config` 568; `a6xx_cp_init` ~600.
- CAT = Linux v6.14 `a6xx_catalog.c`: https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/drivers/gpu/drm/msm/adreno/a6xx_catalog.c?h=v6.14
- XML = Mesa 24.3 register database: https://gitlab.freedesktop.org/mesa/mesa/-/raw/mesa-24.3.0/src/freedreno/registers/adreno/a6xx.xml (offsets are dwords)
- P0301 = /Users/luigicolucci/git/Bergamota/xnu/patches-11215/0301-surya-gpu-a618-scm-zap.patch, P0103 = .../0103-beryllium-gpu-a630-bringup.patch

Limits: I did not get the downstream kgsl (msm-4.14/4.19) `adreno_a6xx.c` or the surya device tree (not fetched; GitHub raw was rate limited). Anything about downstream is flagged as not verified. I did not check GMU-wrapper resume beyond `a6xx_pm_resume`.

## Q1. Ordered a6xx_hw_init register writes before the CP starts (a618 / a615 / a630)

### Takeaway
In Linux the order is: GMU OOB (GX on) -> GBIF halt clear -> SECVID TSB -> ADDR_MODE_CNTL (x12) -> HWCG -> QoS -> perf mask -> UCHE (64-bit) -> ROQ/pool/ECO/AHB -> perf counters -> UBWC -> hang-detect -> UCHE_CLIENT_PF -> CP_PROTECT -> RBBM_INT_0_MASK -> SQE base, RB base, RB_CNTL, RPTR_ADDR -> SQE_CNTL=1 -> CP_ME_INIT -> zap -> CP_SET_SECURE_MODE 0. a615, a618 and a630 differ only in HWCG table/clock_cntl_on, GBIF vs VBIF halt block, GMEM size, UBWC HBB and a630-only VBIF_GATE_OFF.

### Cited Findings
Per-chip values; "same" means a615 = a618 = a630. Register dword offsets from XML. "Needs" column: GX = GX power + clocks on (GMU OOB GPU_SET in mainline), GMU = GMU register range, SMMU = value is an IOVA only used on first fetch, ZAP = secure.

| # | Register (dword off) | Value | Needs | LX |
|---|---|---|---|---|
| 0 | GMU_OOB_GPU_SET (GMU) | set; keeps GX on | GMU | hw_init start, ~869 |
| 1 | GBIF_HALT 0x3c45 = 0, RBBM_GBIF_HALT 0x16 = 0 (a618, a615: `a6xx_has_gbif` is true for everything but a630) | 0 | GX | ~876-889; `a6xx_has_gbif` in a6xx_gpu.h:71 |
|   | a630: no GBIF clear (VBIF) | - | | a6xx_gpu.h:71 |
| 2 | RBBM_SECVID_TSB_CNTL 0xF803 | 0 | GX, secure-range reg | ~892 |
| 3 | RBBM_SECVID_TSB_TRUSTED_BASE 0xF800 (64b), TRUSTED_SIZE 0xF802 | 0 | GX, secure-range reg | ~900 |
| 4 | ADDR_MODE_CNTL = 1 for CP 0x842, VSC 0xC01, GRAS 0x8601, RB 0x8E05, PC, HLSQ, VFD, VPC, UCHE 0xE00, SP, TPL1, RBBM_SECVID_TSB 0xF810 | 1 (same) | GX | ~907-920 |
| 5 | HWCG (`a6xx_set_hwcg`): GMU_AO_GMU_CGC_MODE/DELAY/HYST (GMU range); clear SPTPRAC_CLOCK_CONTROL bit0 (GMU); write table; set bit0; RBBM_CLOCK_CNTL 0xAE | a618/a615: `a615_hwcg`, clock_cntl_on 0x8aa8aa82, CGC mode 0x222; a615 delay 0x111 hyst 0x555, a618 delay 0x10111 hyst 0x5555. a630: `a630_hwcg`, clock_cntl_on 0x8aa8aa02, CGC mode 0x20202 | GX + GMU | LX 408-477; CAT a615_hwcg 65-183, a630_hwcg 185-; gmu_cgc_mode CAT ~751/899 |
| 6 | QoS: a618/a615/a630 use the `else` branch: RBBM_VBIF_CLIENT_QOS_CNTL 0x10 = 3 (GBIF_QSB_SIDE0-3 = 0x71620 + RBBM_GBIF_CLIENT_QOS_CNTL 0x11 = 3 only for a610/a640/a650/a7xx) | 3 | GX | LX ~924-936 |
| 7 | a630 only: VBIF_GATE_OFF_WRREQ_EN 0x302A | 9 | GX | ~938 |
| 8 | RBBM_PERFCTR_GPU_BUSY_MASKED 0x50B | 0xffffffff | GX | ~944 |
| 9 | UCHE_WRITE_RANGE_MAX 0xE05 (64b) | 0x0001ffff_ffffffc0 | GX | ~947-951 |
|   | UCHE_TRAP_BASE 0xE09 (64b) | 0x0001ffff_fffff000 | GX | same |
|   | UCHE_WRITE_THRU_BASE 0xE07 (64b) | 0x0001ffff_fffff000 | GX | same |
| 10 | UCHE_GMEM_RANGE_MIN 0xE0B (64b) = 0x100000; GMEM_RANGE_MAX 0xE0D (64b) = 0x100000 + gmem - 1 | a618/a615 gmem = 512K (CAT `.gmem = SZ_512K`, entries at ~706, ~738, ~763): MAX = 0x17ffff. a630 gmem 1M (CAT ~890): MAX = 0x1fffff | GX | LX ~953-960 |
| 11 | UCHE_FILTER_CNTL 0xE18 = 0x804; UCHE_CACHE_WAYS 0xE17 = 0x4 | same | GX | ~962-967 |
| 12 | CP_ROQ_THRESHOLDS_2 0x8C2 / _1 0x8C1 | a618/a615/a630: 0x010000c0 / 0x8040362c (a640/a650: 0x02000140 / same _1; a610: 0x800060 / 0x40201b16) | GX | ~969-977 |
| 13 | CP_MEM_POOL_SIZE 0x8C3 | 128 (a610 48, a702 64) | GX | ~983-990 |
| 14 | PC_DBG_ECO_CNTL 0x9E00 | `prim_fifo_threshold`: a618 0x00180000, a615 0x0018000 (sic, CAT line ~725 has 7 digits), a630 0x00180000 | GX | LX ~994; CAT |
| 15 | CP_AHB_CNTL 0x98D | 1 | GX | ~998 |
| 16 | RBBM_PERFCTR_CNTL 0x500 | 1 | GX | ~1001 |
| 17 | CP_PERFCTR_CP_SEL(0) 0x8D0 | PERF_CP_ALWAYS_COUNT | GX | ~1010 |
| 18 | `a6xx_set_ubwc_config`: RB_NC_MODE_CNTL, TPL1_NC_MODE_CNTL, SP_NC_MODE_CNTL, UCHE_MODE_CNTL 0xE01, RBBM_NC_MODE_CNTL 0x534 | a618: highest_bank_bit 14 -> hbb=1 (hbb_lo=1, hbb_hi=0), swizzle 0x6, min_acc_len 0, amsbc 0, macrotile 0; a615 (and a630) highest_bank_bit 15 -> hbb=2 (hbb_lo=2) | GX | LX calc ~535-565, set 568-612 (HBB is 14 only for a618 in `a6xx_calc_ubwc_config`) |
|    | resulting values (computed from the LX expressions): a618: RB_NC 0x2000+0x0+...: level2_swizzling_dis=!(6&2)=0, ubwc_mode=0, so RB_NC = hbb_lo<<1 = 0x2; TPL1_NC = 0x2; SP_NC = 0x2; UCHE_MODE = 1<<21 = 0x200000; RBBM_NC = 0. a615/a630: hbb_lo=2: NC regs 0x4, UCHE_MODE 0x400000 | | computed, not copied |
| 19 | RBBM_INTERFACE_HANG_INT_CNTL 0x1F | (1<<30) \| 0x1fffff (a615/a618/a630) | GX | ~1015-1025 |
| 20 | UCHE_CLIENT_PF 0xE19 | BIT(7) \| 1 | GX | ~1027 |
| 21 | GMU_AO_GPU_CX_BUSY_MASK, GMU_CX_GMU_POWER_COUNTER_SELECT_0 / ENABLE | 0xff000000; rmw 0xff, BIT(5); 1 | GMU | ~1045-1050 |
| 22 | `a6xx_set_cp_protect`: CP_PROTECT_CNTL 0x84F = ACCESS_PROT_EN \| FAULT_ON_VIOL_EN \| LAST_SPAN_INF_RANGE, then CP_PROTECT(i) | `a630_protect_regs` (32 entries), shared by a615/a618/a630 (CAT 509-544) | GX | LX 479-501 |
| 23 | RBBM_INT_0_MASK 0x38 | A6XX_INT_MASK (CP_AHB_ERROR, ATB_ASYNCFIFO_OVERFLOW, CP_HW_ERROR, CP_IB2, CP_IB1, CP_RB, CACHE_FLUSH_TS, ATB_BUS_OVERFLOW, HANG_DETECT, UCHE_OOB_ACCESS, UCHE_TRAP_INTR) | GX | ~811-821 mask def; written ~1085 |
| 24 | `adreno_hw_init` (generic ring reset, shadow setup) | - | | ~1088 |
| 25 | CP_SQE_INSTR_BASE 0x830 (64b) | SQE IOVA | SMMU | ~1091 |
| 26 | CP_RB_BASE 0x800 (64b) | ring IOVA (`gpu->rb[0]->iova`) | SMMU | ~1094 |
| 27 | CP_RB_CNTL 0x802 | MSM_GPU_RB_CNTL_DEFAULT \| NO_UPDATE when no hw_apriv (a618/a615/a630: no hw_apriv so NO_UPDATE is set) | GX | ~1100 |
| 28 | CP_RB_RPTR_ADDR 0x804 (64b) | shadow IOVA, only if `shadow_bo` (has_whereami: needs SQE >= 1.90 (0x190) for "a630_sqe.fw"; version check at LX ~730-745) | SMMU | ~1109 |
| 29 | CP_SQE_CNTL 0x808 = 1 | 1 (never 3 in init; 3 is the halt value used by recovery) | GX | ~1120 |
| 30 | CP_ME_INIT (8 dwords: 0x2f, 3, 0x20000000, 0 x5) | | CP + SMMU | `a6xx_cp_init` ~600-625 |
| 31 | zap load (PAS id 13), then CP_SET_SECURE_MODE 0 through the ring | | ZAP | ~1135-1145 |
| 32 | no-zap path: RBBM_SECVID_TRUST_CNTL 0xF400 = 0 | | secure | ~1150 |

Facts supporting the table:
- hw_init waits on `a6xx_gmu_set_oob(GMU_OOB_GPU_SET)` for non-wrapper GPUs so the GMU keeps GX on during setup — [LX](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/drivers/gpu/drm/msm/adreno/a6xx_gpu.c?h=v6.12) hw_init
- a618, a615 and a630 each list `.gmem` 512K / 512K / 1M, SQE "a630_sqe.fw", GMU "a630_gmu.bin"; a618 and a615 use zap "a615_zap.mbn/mdt" and `a615_hwcg`, all three `a630_protect` — [CAT](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/drivers/gpu/drm/msm/adreno/a6xx_catalog.c?h=v6.14)
- SM7150 (chip 0x06010800) has its own catalog entry with `.machine = "qcom,sm7150"`, 512K gmem, `prim_fifo_threshold = 0x00180000`, plus a second 0x06010800 entry with `.revn = 618` — [CAT](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/drivers/gpu/drm/msm/adreno/a6xx_catalog.c?h=v6.14)
- Register offsets as listed (UCHE 0xE00-0xE19, CP 0x800-0x98D, RBBM_VBIF_CLIENT_QOS_CNTL 0x10, RBBM_GBIF_HALT 0x16, GBIF_HALT 0x3c45) — [XML](https://gitlab.freedesktop.org/mesa/mesa/-/raw/mesa-24.3.0/src/freedreno/registers/adreno/a6xx.xml)
- CP_INTERRUPT_STATUS bit 7 is CP_ILLEGAL_INSTR_ERROR; bit 0 CP_OPCODE_ERROR; bit 1 CP_UCODE_ERROR; bit 2 CP_HW_FAULT_ERROR; bit 4 CP_REGISTER_PROTECTION_ERROR; bit 5 CP_AHB_ERROR; bit 6 CP_VSD_PARITY_ERROR — [XML](https://gitlab.freedesktop.org/mesa/mesa/-/raw/mesa-24.3.0/src/freedreno/registers/adreno/a6xx.xml) lines 2275-2282
- A630-era ubwc code: `highest_bank_bit = 15` default, `if a618: = 14`; written as hbb-13 split into hbb_lo / hbb_hi — LX `a6xx_calc_ubwc_config` / `a6xx_set_ubwc_config`

### Inferences
- The patch's cold CP stage (P0301 ~1784-1880) runs ahead of the ring programming with the right values for ROQ (0x010000c0/0x8040362c), pool 128, AHB 1, GMEM 512K (MAX 0x17ffff — matches `0x100000 + 0x80000 - 1`), but skips the pieces listed in Q-final below.
- The UBWC values are irrelevant for a CP NOP test but are needed before any real rendering; a618 HBB is 14 (not the a630 default 15).

### Gaps
- Exact line numbers for rows after ~900 are approximate (I read the code in a stream; the function boundaries and ordering are exact). Re-grep before quoting a line.
- I could not fetch downstream kgsl `a6xx_start()` to confirm the vendor driver writes the same values for sm7150 (e.g. whether it also writes GBIF/QoS or `A6XX_UCHE_GBIF_GX_CONFIG`). Not verified.

## Q2. Registers that need GX/GMU, registers that hang or reset the SoC, GMU-range registers in the patch

### Takeaway
Every GPU-block register (0x0-0xFFFF dwords at 0x05000000) is GX-domain except a few CX-side ones (RBBM clock/hang/GBIF are GX-wrapped on a6xx and need GX clocks too); a write with GX off raises an AHB timeout / NoC error and can reset the SoC. The patch holds the GMU OOB (GPU_SET) around the register work, so GX should be on; its GMU-range accesses are GMU_GX_SPTPRAC_CLOCK_CONTROL, the AHB fence registers and the HFI/boot registers.

### Cited Findings
- `a6xx_gmu_gx_is_on` reads REG_A6XX_GMU_SPTPRAC_PWR_CLK_STATUS and tests GX_HM_GDSC_POWER_OFF and GX_HM_CLK_OFF — [LX a6xx_gmu.c](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/drivers/gpu/drm/msm/adreno/a6xx_gmu.c?h=v6.12) line 93-105. Mainline checks it before any GPU register dump (a6xx_gpu_state) and before in `a6xx_gmu_*` register-touching paths (line 191).
- The registers hw_init writes in the GMU range (not the GPU range): `REG_A6XX_GPU_GMU_AO_GMU_CGC_{MODE,DELAY,HYST}_CNTL`, `REG_A6XX_GPU_GMU_GX_SPTPRAC_CLOCK_CONTROL`, `REG_A6XX_GPU_GMU_AO_GPU_CX_BUSY_MASK`, `REG_A6XX_GMU_CX_GMU_POWER_COUNTER_SELECT_0/ENABLE` — LX hw_init and `a6xx_set_hwcg` (lines 408-477).
- `a6xx_set_hwcg` clears bit0 of GPU_GMU_GX_SPTPRAC_CLOCK_CONTROL before the HWCG table and sets it afterwards (a615-a640 family, not a610/a7xx) — LX line ~455-470.
- In hw_init, GBIF_HALT / RBBM_GBIF_HALT are cleared "in case GX domain was not collapsed" — LX hw_init comment ~876.
- Patch P0301 touches in the GMU range: GMU_GX_SPTPRAC_CLOCK_CONTROL (GMU_REG(0x1a880)), GMU_AHB_FENCE_RANGE_0/1 (0x23b11/12) and GMU_AHB_FENCE_STATUS (0x23b13) — P0301 lines 641-654, 1700-1715 (cp SP clock), 1827 probe printout. GMU base 0x0506a000, GPU base 0x05000000, GPU window 0x40000 (P0301 570-574).
- No source I found documents which specific a6xx registers reset the SoC when GX is off. Mainline guards with `a6xx_gmu_gx_is_on` and OOB; the a630 bring-up P0103 does the same with GMU OOB.

### Inferences
- The "reset without a panic" seen after the 64-bit UCHE block (see Q-final) is not a GX-power problem: the earlier 32-bit probe writes in the same boot window (UCHE_ADDR_MODE_CNTL, UCHE_WRITE_THRU_BASE lo, GMEM_RANGE_MIN lo, UCHE_FILTER_CNTL, ...) and 0xe19 were accepted ("STICKS"), so GX was on. Hypothesis, not confirmed: the hi words (0x1ffff) switch on UCHE write-through/trap regions with GBIF still halted or in an unknown state (see below), the first UCHE/memory traffic hangs and the SoC watchdog/NoC error resets it.
- The AHB fence (GMU_AHB_FENCE_*) is GMU-side and not in mainline hw_init; it is the downstream-style gate the patch already handles ("dropped-write" evidence). Not verified against vendor source.

### Gaps
- The Poco X3 SM7150 GX/RPMh vote list (GPU_GX GDSC, GMU_GX sync) was not read from the downstream dts.
- No documented per-register "causes SoC reset" list exists in public sources I found.

## Q3. GMU-less operation, gmu_wrapper, and whether a630_gmu.bin is mandatory

### Takeaway
Mainline's GMU-less path (`gmu_is_wrapper`) exists only for SoCs without RPMh GMU firmware (A610, A619 holi, A702); the GMU registers are still mapped ("gmu" wrapper region) and cx/gx power domains + GPU clocks are enabled via genpd. For a618/a615/a630 mainline always loads GMU firmware (a630_gmu.bin); no source I found shows a615/a618 running without it.

### Cited Findings
- `adreno_has_gmu_wrapper()` returns `gpu->gmu_is_wrapper`; `adreno_is_a619_holi` = a619 && wrapper — [adreno_gpu.h v6.14](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/drivers/gpu/drm/msm/adreno/adreno_gpu.h?h=v6.14) lines 304-307, 435-437.
- `a6xx_gmu_wrapper_init`: maps the "gmu" MMIO, attaches genpd "cx" and "gx", sets `gmu->legacy = true` for manual SPTPRAC control — [LX a6xx_gmu.c](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/drivers/gpu/drm/msm/adreno/a6xx_gmu.c?h=v6.12) 1597-1660.
- Wrapper `a6xx_pm_resume`: sets OPP, `pm_runtime_resume_and_get` on the GMU wrapper device and `gxpd`, `clk_bulk_prepare_enable` for the GPU clocks, and for holi calls `a6xx_sptprac_enable` — [LX a6xx_gpu.c](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/drivers/gpu/drm/msm/adreno/a6xx_gpu.c?h=v6.12) 1840-1880.
- In wrapper mode hw_init skips GMU OOB, and a recover path resets via `a6xx_bus_clear_pending_transactions` and `a6xx_gpu_sw_reset` — LX 869, 1188, 1261-1270. LLCC slices are skipped (no RPMh) — LX 1671-1690.
- a618/a615/a630 catalog entries all name `a630_gmu.bin` — [CAT](https://git.kernel.org/pub/scm/linux/kernel/git/torvalds/linux.git/plain/drivers/gpu/drm/msm/adreno/a6xx_catalog.c?h=v6.14). Upstream SM7150 review: Konrad Dybcio asked for a630_gmu.bin / a619_gmu.bin to be tried instead of the vendor a618_gmu.bin — [lkml thread](https://lkml.iu.edu/hypermail/linux/kernel/2309.1/06831.html) (search snippet, outcome not seen).
- Linux sets `priv->gpu_clamp_to_idle = true` for a618 — LX ~2306.

### Inferences
- The wrapper path is not a template for a618: it relies on a GMU that is absent, whereas a618 on SM7150 has RPMh and GMU. The patch (sweetgpu=3..9) already boots GMU firmware, so it is not GMU-less; it follows the full-GMU path, with the GMU HFI cold boot done in firmware ("legacy" queue IDs for a630/a615-family, P0301 gpu_hfi_table).
- The GMU firmware is required for GX rail and SPTPRAC handling on the full path (`a6xx_gmu_gfx_rail_on`, `a6xx_sptprac_enable`, LX a6xx_gmu.c 365-440, 882-889).

### Gaps
- No evidence found of anyone running a615/a618/a630 without GMU firmware. Downstream kgsl for sm7150 requires GMU (not verified from source in this session).

## Q4. GPU SMMU, ring base (physical vs IOVA), and fault behaviour

### Takeaway
CP_RB_BASE, CP_SQE_INSTR_BASE and CP_RB_RPTR_ADDR are 64-bit GPU virtual addresses (IOVAs) that go through the GPU SMMU context bank; with a stage-1 bypass/identity context they would be physical addresses, but Linux never uses that. P0301 maps them through a translated context bank (CP_CB, TTBR0 = cp_memory_pa), so IOVAs (cp_buffers[].va) are the correct values.

### Cited Findings
- hw_init writes `gpu_write64(CP_SQE_INSTR_BASE, a6xx_gpu->sqe_iova)`, `gpu_write64(CP_RB_BASE, gpu->rb[0]->iova)`, `gpu_write64(CP_RB_RPTR_ADDR, shadowptr(...))` — LX hw_init ~1091-1110. `sqe_iova` is from `adreno_fw_create_bo(...&sqe_iova)`, `shadow_iova` from `msm_gem_kernel_new(... gpu->aspace ...)` (LX `a6xx_ucode_load`), i.e. addresses in `gpu->aspace`.
- The address space is an adreno_smmu one (`a6xx_create_address_space` -> `adreno_iommu_create_address_space`), private per-process tables are switched by CP_SMMU_TABLE_UPDATE (TTBR0, ASID, CONTEXTIDR 0, CONTEXTBANK 0) in the ring; global buffers live in the default (TTBR1) table — LX 140-147, 2039-2065.
- In wrapper mode the same: `a6xx_gmu_wrapper` uses the same `a6xx_create_address_space`. No physical-address path exists in hw_init.
- ring `CP_RB_CNTL` sets NO_UPDATE when no hw_apriv / whereami, so RPTR is read via register rather than the shadow — LX ~1098-1105 (a618 has no hw_apriv in CAT entries I read).
- SMMU fault behavior (stall vs terminate): not in the sources I fetched. Mainline registers a pagefault handler (`a6xx_fault_handler` ~1418) and the adreno_smmu "set_stall" hooks.

### Inferences
- A fault on the SQE microcode fetch (instr base) or on ring fetch would show up as CP_HW_FAULT / ucode error or hang rather than ILLEGAL_INSTR; ILLEGAL_INSTR (bit 7) most plausibly comes from a packet the SQE microcode decodes and rejects (bad header parity, opcode not in this ucode, or an opcode needing privilege) rather than garbage fetch. Garbage-from-fault typically yields all-zero or stale words; a zero dword is a type-0 packet with count 0, not an illegal instruction. This is a guess.
- The patch's CP_ME_INIT draining (RPTR=WPTR) proves ring fetch through the SMMU works (it fetched and parsed the 9-dword packet, and the SQE firmware itself came through CP_SQE_INSTR_BASE).

### Gaps
- Behaviour of the QCOM SMMU in fault/stall mode for CP fetches: no source found.

## Q5. a618 vs a630 vs a615 differences, and what the A630-derived patch should change

### Takeaway
The patch's a630-derived values are right for ROQ/pool/AHB/QOS value/hang mask; a618 differs in GMEM (512K), HBB (14), HWCG table (a615_hwcg, clock_cntl_on 0x8aa8aa82, CGC delay/hyst), no VBIF_GATE_OFF write, and GBIF halt clear.

### Cited Findings
- GMEM: a618 512K, a615 512K, a630 1M — CAT.
- HWCG table: a615 and a618 use `a615_hwcg` ("For a615 family (a615, a616, a618 and a619)"); a630 `a630_hwcg`. `clock_cntl_on`: a630 0x8aa8aa02, a610 0xaaa8aa82, a702 0xaaaaaa82, everything else (a615/a618) 0x8aa8aa82. CGC delay/hyst: a615 family 0x111/0x555 (the helper tests `adreno_is_a615_family`; whether a618 counts as a615_family I did not confirm), others 0x10111/0x5555 — LX 408-440; gmu_cgc_mode a615/a618 0x222, a630 0x20202 — CAT.
- CP_PROTECT: all three use `a630_protect_regs` (32 entries) — CAT 509-544.
- UBWC: a618 HBB 14, a615/a630 15; others 0 — LX `a6xx_calc_ubwc_config`.
- VBIF_GATE_OFF_WRREQ_EN = 9 only `if adreno_is_a630` — LX ~938.
- GBIF vs VBIF: `a6xx_has_gbif` = !a630 — a6xx_gpu.h:71; but the QSB_SIDE/RBBM_GBIF_CLIENT_QOS branch is only for a610/a640/a650/a7xx; a615/a618 use RBBM_VBIF_CLIENT_QOS_CNTL=3 — LX ~924-936.

### Inferences
Comparison of P0301 vs Linux (what is missing or wrong) -- patch lines refer to P0301 cold-register stage (1784-1900) and P0103 `gpu_cp_hw_init` (2033-2105):
1. WRONG REGISTER in the 32-bit probe: `{ "RBBM_CLIENT_QOS_CNTL", 0x3da, 3 }` (P0301 ~1802). The a6xx register is RBBM_VBIF_CLIENT_QOS_CNTL at 0x10 (P0103 line 227 has 0x0010 and writes `gpu_w(A6XX_RBBM_VBIF_CLIENT_QOS_CNTL, 3)`, P0103 2059). 0x3da is not a QoS register in the XML; it is a stray write into an undocumented GX register. Fix: 0x10.
2. MISSING GBIF halt clear: Linux writes GBIF_HALT 0x3c45 = 0 and RBBM_GBIF_HALT 0x16 = 0 first (a618/a615 have GBIF). P0301 does neither (and P0103 targets a630/VBIF so it skipped them correctly there). Read GBIF_HALT 0x3c45, GBIF_HALT_ACK 0x3c46, RBBM_GBIF_HALT 0x16 and RBBM_GPR0_CNTL 0x18 first; the SoC reset after the UCHE/memory stage could be a halted GBIF (hypothesis, unverified).
3. MISSING RBBM_SECVID_TSB_CNTL / TRUSTED_BASE / TRUSTED_SIZE / TSB ADDR_MODE (0xF803, 0xF800, 0xF802, 0xF810) writes (Linux step 2-4). P0301 and P0103 both skip secure registers deliberately; if the zap is loaded these are normally harmless but not needed for a CP NOP test. ADDR_MODE_CNTL for HLSQ/VFD/VPC/SP/TPL1/PC are in P0103 and skipped in P0301's probe (only CP, VSC, GRAS, RB, UCHE written); do all twelve before the CP runs because a 32-bit ADDR_MODE block next to 64-bit UCHE is inconsistent.
4. ORDER: P0301 writes UCHE_WRITE_THRU_BASE lo 0xfffff000 first with hi = 0, and GMEM_RANGE_MIN lo without MAX, before the 64-bit pass (probe ~1804-1807), leaving a state Linux never has (WRITE_THRU_BASE 0x00000000_fffff000 while GMEM_RANGE_MAX = 0). Linux (and P0103) write each 64-bit value as a pair in the order WRITE_RANGE_MAX, TRAP_BASE, WRITE_THRU_BASE, GMEM_RANGE_MIN, GMEM_RANGE_MAX, then FILTER_CNTL, CACHE_WAYS. Write the pairs only once, in that order, and do not do the probe's lo-only writes.
5. UCHE_CLIENT_PF 0xE19 is written in P0301 immediately after the UCHE ranges (~1842); Linux writes it much later, after HANG_INT_CNTL, ubwc and before CP_PROTECT. Move it, and write HANG_INT_CNTL (0x1F) once (the probe already writes it at 0x1f with (1<<30)|0x1fffff, correct).
6. a618-specific values P0301 should use that it does not (or does only partially): UBWC (RB_NC 0x2, TPL1_NC 0x2, SP_NC 0x2, UCHE_MODE 0x200000, RBBM_NC 0) , `a615_hwcg` table + RBBM_CLOCK_CNTL 0x8aa8aa82 (P0301 only sets SPTPRAC clock bit and reads 0xae; patch comment "Do not change HWCG tables"), PC_DBG_ECO_CNTL 0x00180000 (present), CP_PROTECT table (absent; on-purpose?), RBBM_INT_0_MASK (absent; P0103 leaves it 0 and polls, same here).
7. SQE_CNTL and ring: P0301 writes ring/SQE base/RB_CNTL while SQE_CNTL = 0 then SQE_CNTL=1 (gpu_cp_start). Same as Linux. RB_CNTL used is `CP_RING_CNTL = (1<<27)|(2<<8)|12` (bit 27 = NO_UPDATE; block size/ring size encoded) vs Linux MSM_GPU_RB_CNTL_DEFAULT | NO_UPDATE, equivalent form (value of the default macro not read; check against drm/msm msm_ringbuffer.h).
8. Test-block packets: all CP_PKT7 headers use the parity function (OK). One thing to check for the ILLEGAL_INSTR (0x80) after CP_SET_SECURE_MODE: Linux's own order is ME_INIT -> zap load -> `CP_SET_SECURE_MODE 0` -> idle. CP_NOP / CP_MEM_WRITE / CACHE_FLUSH_TS are accepted by a630-class ucode in Linux. a6xx CP_MEM_WRITE in Linux's own driver is CP_MEM_WRITE (opcode 0x3D) and CP_EVENT_WRITE (0x46) with CACHE_FLUSH_TS event 4 and the fence address as 64-bit, which the patch matches. Linux uses CP_EVENT_WRITE with 4 dwords after header: event|IRQ, addr lo, addr hi, seqno — same. The packet that is illegal is therefore more likely CP_MEM_WRITE (a6xx Linux never emits it; mainline uses CP_MEMPTR/CP_EVENT_WRITE/CP_WAIT_MEM_WRITES and PKT4 scratch writes) or CP_NOP with count (the SQE a618 ucode 0x16ee19a may not implement opcode 61). This is a hypothesis; the packet-by-packet test already in P0301 will say which.
   - Note: the A630 patch P0103 used the same three packets and the NOP test is reported working on a630, so a different SQE revision (0x16ee19a vs a630's) is a candidate. SQE firmware: Linux requires >= 0x190 for "a630_sqe.fw" (LX ~737); 0x16ee19a has low 12 bits 0x19a (>= 0x190) so passes.

### Gaps
- Whether `adreno_is_a615_family` includes a618 (changes the CGC delay/hyst: 0x111/0x555 vs 0x10111/0x5555) was not checked in adreno_gpu.h.
- Downstream surya values (kgsl a6xx_start) not compared.
- Cause of the SoC reset after the 64-bit UCHE writes: not determinable from these sources. Suggested bisect on device (cheap, ordered): (a) read-only dump of GBIF_HALT 0x3c45/ACK 0x3c46, RBBM_GBIF_HALT 0x16, RBBM_GPR0_CNTL 0x18, RBBM_STATUS before the UCHE writes; clear GBIF halts as Linux does; (b) fix 0x3da -> 0x10 and drop the stray 32-bit-only writes; (c) write the UCHE pairs one register per boot (RANGE_MAX, TRAP_BASE, WRITE_THRU_BASE, GMEM_MIN, GMEM_MAX) logging with `sweet_log_sync()` before each write to find the register that triggers the reset; (d) try TRAP_BASE/WRITE_THRU_BASE hi = 0 first.
