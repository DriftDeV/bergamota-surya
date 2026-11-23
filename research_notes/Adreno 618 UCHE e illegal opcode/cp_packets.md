# Adreno 6xx CP/SQE PM4 packets and CP_INTERRUPT_STATUS bit 0x80

Source base (all Linux refs are tag v6.12; line numbers are from the copies fetched via the GitHub contents API, so they may be off by a few lines on a different tree):
- L_GPU = https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/a6xx_gpu.c
- L_H = https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/adreno_gpu.h
- L_A6XML = https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/registers/adreno/a6xx.xml
- L_PM4 = https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/registers/adreno/adreno_pm4.xml
- L_CAT = https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/a6xx_catalog.c
- PATCH = /Users/luigicolucci/git/Bergamota/xnu/patches-11215/0301-surya-gpu-a618-scm-zap.patch

Not found: Mesa XML and downstream kgsl sources (gitlab.freedesktop.org returned an Anubis access-denied page; raw.githubusercontent.com returned 429; no kgsl source was fetched). Everything below is from mainline Linux plus a read of the PATCH.

## Q1. Type-7 packet header encoding

### Takeaway
The PATCH's `cp_pkt7()` and `cp_parity()` are bit-for-bit identical to Linux's `OUT_PKT7` / `PM4_PARITY`. The header encoding is not the cause of the failure.

### Cited Findings
- Linux header: `CP_TYPE7_PKT | (cnt << 0) | (PM4_PARITY(cnt) << 15) | ((opcode & 0x7F) << 16) | (PM4_PARITY(opcode) << 23)` — [L_H#L660-L665](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/adreno_gpu.h#L660)
- `PM4_PARITY(val)` = `(0x9669 >> (0xF & (val ^ val>>4 ^ val>>8 ^ val>>12 ^ val>>16 ^ val>>20 ^ val>>24 ^ val>>28))) & 1` — [L_H#L637-L643](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/adreno_gpu.h#L637)
- `CP_TYPE7_PKT = 0x70000000` — [L_PM4#L234](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/registers/adreno/adreno_pm4.xml#L234)
- PATCH lines ~1946-1955: `0x70000000u | cnt | (cp_parity(cnt) << 15) | ((opcode & 0x7f) << 16) | (cp_parity(opcode) << 23)`, same parity function. Identical.
- Count is the number of payload dwords after the header (header not counted), 14 bits at [13:0]; opcode 7 bits at [22:16]; parity bits at 15 and 23 (from the formula above).
- Worked examples (computed with the formula above, I ran a Python check):
  - CP_NOP (0x10) cnt 1: `0x70100001`; cnt 0: `0x70108000`
  - CP_MEM_WRITE (0x3d) cnt 3: `0x703d8003`; cnt 4: `0x703d0004`
  - CP_EVENT_WRITE (0x46) cnt 4: `0x70460004`
  - CP_ME_INIT (0x48) cnt 8: `0x70c80008`
  - CP_SET_SECURE_MODE (0x66) cnt 1: `0x70e60001`
  - CP_WAIT_FOR_IDLE (0x26) cnt 0: `0x70268000`
  - CP_REG_WRITE (0x6d) cnt 2: `0x706d0002`
- Parity bit rule: the bit is 1 when the field has an even popcount, so the field plus its parity bit always has an odd number of ones (derived from the 0x9669 table: bit n of 0x9669 is 1 exactly when popcount(n) is even).

### Inferences
- If the PATCH's parity function were wrong, ME_INIT (0x70c80008) would also have failed; it drained, which is consistent with the encoding being right.

### Gaps
- Mesa `fd_pkt7_hdr` not fetched (access denied), so no second source for the encoding.

## Q2. Opcodes and payloads; which are valid on a615/a618 SQE

### Takeaway
All three test packets match what Linux emits on a6xx. CP_MEM_WRITE with `addr_lo, addr_hi, data...` is valid and variable length. The PATCH's packets are correct on paper.

### Cited Findings
- Opcode values: CP_NOP 0x10, CP_WAIT_FOR_IDLE 0x26, CP_MEM_WRITE 0x3d, CP_EVENT_WRITE 0x46 (variants A2XX-A6XX), CP_ME_INIT 0x48, CP_SET_SECURE_MODE 0x66, CP_REG_WRITE 0x6d (variant A6XX) — [L_PM4#L248-L606](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/registers/adreno/adreno_pm4.xml#L248)
- CP_MEM_WRITE layout: dword0 ADDR_LO, dword1 ADDR_HI, "followed by the DWORDs to write" — [L_PM4#L1373-L1381](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/registers/adreno/adreno_pm4.xml#L1373). So cnt = 2 + number of data dwords; the PATCH's cnt 3 with one data dword is valid.
- Linux uses CP_MEM_WRITE on a6xx in `a6xx_set_pagetable`: `OUT_PKT7(ring, CP_MEM_WRITE, 4)` then ADDR_LO, ADDR_HI, `lower_32_bits(ttbr)`, `(asid << 16) | upper_32_bits(ttbr)` (two data dwords) — [L_GPU#L152-L157](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/a6xx_gpu.c#L152)
- CACHE_FLUSH_TS on a6xx in `a6xx_submit`: `OUT_PKT7(ring, CP_EVENT_WRITE, 4)`; dword0 `CP_EVENT_WRITE_0_EVENT(CACHE_FLUSH_TS) | CP_EVENT_WRITE_0_IRQ`; dword1 lower 32 of the fence IOVA; dword2 upper 32; dword3 seqno — [L_GPU#L262-L276](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/a6xx_gpu.c#L262). The PATCH's sequence (`4 | BIT31`, lo, hi=0, seq) is the same.
- CP_EVENT_WRITE dword0 fields: EVENT [7:0], TIMESTAMP bit 30 ("when set, write back timestamp instead of value from packet"), IRQ bit 31 — [L_PM4#L1614-L1620](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/registers/adreno/adreno_pm4.xml#L1614). CACHE_FLUSH_TS = 4 — [L_PM4#L17](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/registers/adreno/adreno_pm4.xml#L17). Note the Linux a6xx_submit does NOT set bit 30; only EVENT|IRQ. Whether bit 30 is needed for the written value to be the seqno is not established by the sources I read.
- a6xx_cp_init emits `CP_ME_INIT, 8` then `0x0000002f, 0x00000003, 0x20000000, 0,0,0,0,0` (8 payload dwords), then flush and idle wait — [L_GPU#L612-L639](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/a6xx_gpu.c#L612). PATCH gpu_cp_start matches exactly.
- CP_SET_SECURE_MODE: `OUT_PKT7(CP_SET_SECURE_MODE, 1); OUT_RING(0)`, flush, idle wait, only when the zap loaded — [L_GPU#L1164-L1171](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/a6xx_gpu.c#L1164). PATCH matches.
- Linux also uses on a6xx: CP_WHERE_AM_I (cnt 2), CP_REG_TO_MEM (cnt 3), CP_WAIT_TIMESTAMP, CP_SMMU_TABLE_UPDATE (cnt 4), CP_THREAD_CONTROL, CP_SET_MARKER, CP_INDIRECT_BUFFER_PFE, CP_SET_PROTECTED_MODE, CP_WAIT_REG_MEM, CP_EVENT_WRITE — [L_GPU grep lines 63-309](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/a6xx_gpu.c#L63). So these exist in the firmware family used by a618. CP_WHERE_AM_I only exists in the patched/new a630 SQE: Linux requires a version nibble 0xa with patch >= 1, or `(buf[0] & 0xfff) >= 0x190`, to set `has_whereami` — [L_GPU#L720-L736](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/a6xx_gpu.c#L720).
- a618 (0x06010800, revn 618) and a615 both use `a630_sqe.fw`; a618 has no ADRENO_QUIRK_HAS_HW_APRIV (only CACHED_COHERENT) — [L_CAT#L763-L773](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/a6xx_catalog.c#L763). Observed in your logs: SQE version 0x16ee19a has low byte 0x9a and `(version & 0xfff) = 0x19a`, which is >= 0x190, and low nibble 0xa: it is the patched/new a630 SQE, so WHERE_AM_I-era firmware.

### Inferences
- NOP, MEM_WRITE (cnt 3) and EVENT_WRITE (cnt 4) are all opcodes this firmware family runs under Linux. I found no source listing opcodes unknown to the a615/a618 SQE; I could not confirm any opcode is unsupported.
- A single 32-bit CP_MEM_WRITE (cnt 3) should work; if you want to mirror Linux exactly use cnt 4 with a 64-bit pair of data dwords (`0x703d0004, lo, hi, d0, d1`).
- Because Linux has_whereami is true on this firmware and a618 has no hw_apriv, real Linux a6xx_submit also emits CP_WHERE_AM_I; not needed for a bare test.

### Gaps
- Mesa adreno_pm4.xml (opcode `variants` per chip) and downstream kgsl adreno_pm4types.h not fetched.
- No documentation found on which opcodes a615/a618 SQE rejects.

## Q3. Does the memory need to be mapped through the GPU SMMU; fault vs illegal instruction

### Takeaway
Yes: Linux gives the CP GPU-virtual addresses (IOVAs) from the GPU aspace for the ring, the SQE and the fence. The PATCH already does this (CB1, GPU VA 0x40000000). I found no source saying what the CP does on an unmapped IOVA; do not assume it shows up as bit 7.

### Cited Findings
- Linux writes `gpu->rb[0]->iova` to CP_RB_BASE and `a6xx_gpu->sqe_iova` to CP_SQE_INSTR_BASE, both IOVAs in `gpu->aspace` — [L_GPU#L1118-L1121](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/a6xx_gpu.c#L1118)
- Fence address in EVENT_WRITE is `rbmemptr(ring, fence)`, i.e. an IOVA — [L_GPU#L268-L273](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/a6xx_gpu.c#L268)
- SQE firmware buffer: `adreno_fw_create_bo` allocates `fw->size - 4` bytes and copies `fw->data[4..]` (skips the first dword, the file header), with `MSM_BO_WC | MSM_BO_GPU_READONLY` — https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/adreno_gpu.c#L554-L566
- PATCH: ring VA 0x40000000, SQE 0x40010000 (readonly), scratch 0x40020000, mapped through SMMU CB1 with TTBR built in `cp_build_tables`; the SQE is copied from `fw + 4` (lines ~1397-1420, ~1560). So no physical addresses go into CP_RB_BASE (it gets cp_buffers[0].va) — [PATCH setup[] table ~line 1855](file:///Users/luigicolucci/git/Bergamota/xnu/patches-11215/0301-surya-gpu-a618-scm-zap.patch).
- PATCH PTEs: `PTE_PAGE_NC = 3 | AF | SH_OS(2<<8) | AP_UNPRIV(1<<6) | nG(1<<11)`, readonly adds bit 7. SQE page is AP[2:1]=11 (RO, unprivileged-accessible) which is also readable by privileged accesses on ARM stage 1.

### Inferences
- A fetch from an unmapped/faulting IOVA should set an SMMU context bank fault (FSR/FSYNR0/FAR; your `gpu_smmu_cb_faults` only prints CB GMU_CB, not CB1 = CP_CB). I could not find a source stating how the CP firmware reports it. Check FSR of CB1 (SMMU_CB(1)) after the failure.
- Because Linux maps the SQE with MSM_BO_GPU_READONLY only, readonly-ness is fine.

### Gaps
- No source on CP behaviour for an unmapped IOVA (fault vs illegal instruction).
- No source on the SMMU AP-bit expectations for CP_APRIV_CNTL = 0 vs privileged transactions.

## Q4. CP_INTERRUPT_STATUS bits; Linux decode; diagnostic registers

### Takeaway
0x80 is bit 7 = CP_ILLEGAL_INSTR_ERROR, not bit 0 (CP_OPCODE_ERROR). Linux just prints "CP illegal instruction error" for it and gives no further decode, so bit 7 is distinct from a PM4 opcode problem.

### Cited Findings
- A6XX_CP_INT bits (a6xx.xml): bit0 CP_OPCODE_ERROR, bit1 CP_UCODE_ERROR, bit2 CP_HW_FAULT_ERROR, bit4 CP_REGISTER_PROTECTION_ERROR, bit5 CP_AHB_ERROR, bit6 CP_VSD_PARITY_ERROR, bit7 CP_ILLEGAL_INSTR_ERROR; bits 8-17 are A7XX-only LPAC/BV variants. Bit 3 is undefined — [L_A6XML#L2261-L2279](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/registers/adreno/a6xx.xml#L2261)
- Registers: CP_RB_BASE 0x800, CP_RB_CNTL 0x802, CP_RB_RPTR_ADDR 0x804, CP_RB_WPTR 0x807, CP_SQE_CNTL 0x808, CP_HW_FAULT 0x821, CP_INTERRUPT_STATUS 0x823, CP_PROTECT_STATUS 0x824, CP_STATUS_1 0x825, CP_SQE_INSTR_BASE 0x830, CP_APRIV_CNTL 0x844, CP_MEM_POOL_SIZE 0x8C3, CP_SQE_STAT_ADDR 0x908, CP_SQE_STAT_DATA 0x909, CP_SQE_UCODE_DBG_ADDR 0x910, CP_SQE_UCODE_DBG_DATA 0x911, CP_AHB_CNTL 0x98D — [L_A6XML#L2281-L2446](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/registers/adreno/a6xx.xml#L2281)
- `a6xx_cp_hw_err_irq`: on OPCODE_ERROR it writes CP_SQE_STAT_ADDR=1 and reads CP_SQE_STAT_DATA ("possible opcode=0x%8.8X"); HW_FAULT prints CP_HW_FAULT; PROTECTION_ERROR decodes CP_PROTECT_STATUS (bit 20 = READ, [17:0] = addr); ILLEGAL_INSTR prints only "CP illegal instruction error" — [L_GPU#L1434-L1473](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/a6xx_gpu.c#L1434)
- A mainline web search found logs from the a630 where "CP Illegal instruction error" appears in bursts with a following GPU page fault and hang detection (kgsl) — [github.com/worthdoingbadly/patch-a630](https://github.com/worthdoingbadly/patch-a630) (search-result snippet only, I did not open it).
- Linux "halt SQE" writes CP_SQE_CNTL = 3 in recovery — [L_GPU#L1245-L1247](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/a6xx_gpu.c#L1245). PATCH comments (line ~1626) record that cold SQE_CNTL=3 already raised CP_IRQ bit 7 in an earlier boot (0097) before ROQ setup. Observation from your own tree, not from an external source.

### Inferences
- Since Linux's names bit 0 as the PM4 opcode error and gives it the CP_SQE_STAT dump, while bit 7 is a separate "illegal instruction", bit 7 most plausibly means the SQE microcode engine itself hit an invalid instruction (bad/garbled microcode fetch, wrong ICACHE contents, or state corruption), not that a PM4 opcode was unknown. This is my reading of the naming, not a documented fact.
- Your earlier observation that SQE_CNTL=3 (halted) alone raises bit 7 suggests bit 7 can be a side effect of SQE start/halt state rather than the specific packet. Check whether bit 7 was already set after "SQE enabled" or after ME_INIT (your status lines print CP_IRQ each time; compare), since CP_INTERRUPT_STATUS may be sticky: you read it only after the test block.
- Diagnostics to add (all in the xml): read CP_SQE_STAT_ADDR=1 / CP_SQE_STAT_DATA, CP_SQE_UCODE_DBG_ADDR/DATA, CP_HW_FAULT, CP_PROTECT_STATUS, CP_STATUS_1, and SMMU CB1 FSR/FSYNR0/FAR. Log CP_INTERRUPT_STATUS after each packet and clear it via RBBM_INT_CLEAR_CMD (CP_IRQ is RBBM_INT_0 bit; not verified here).

### Gaps
- No official documentation of what CP_ILLEGAL_INSTR_ERROR means; only the name.
- RBBM interrupt clear register/bit for CP_HW_ERROR not looked up.

## Q5. Endianness, ring units, WPTR ordering, ring base IOVA

### Takeaway
Ring contents are little-endian 32-bit dwords, CP_RB_WPTR counts dwords (wrap at ring size/4), and the ring base must be a GPU IOVA. PATCH already does all three.

### Cited Findings
- `get_wptr()` = `(ring->cur - ring->start) % (MSM_GPU_RINGBUFFER_SZ >> 2)`: dword index — [L_H#L673-L676](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/adreno_gpu.h#L673)
- `a6xx_flush`: finish all ring writes, `mb()`, then `gpu_write(CP_RB_WPTR, wptr)` — [L_GPU#L69-L90](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/a6xx_gpu.c#L69)
- `MSM_GPU_RB_CNTL_DEFAULT = BUFSZ(ilog2(RINGSZ/8)) | BLKSZ(ilog2(32/8))`, and a618 (no hw_apriv) ORs in `AXXX_CP_RB_CNTL_NO_UPDATE` — [msm_gpu.h#L314](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/msm_gpu.h#L314), [L_GPU#L1127-L1132](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/a6xx_gpu.c#L1127). For a 32 KiB ring: BUFSZ=12, BLKSZ=2<<8 → `0x200c`, plus NO_UPDATE. PATCH `CP_RING_CNTL = (1<<27)|(2<<8)|12` = `0x0800200c`. Matches, assuming AXXX_CP_RB_CNTL_NO_UPDATE is bit 27 (as the PATCH uses; I did not open the a2xx header to confirm).
- PATCH ring: 0x8000 bytes, wptr in dwords, `dsb sy` before WPTR write — matches (lines ~1938-1970).

### Inferences
- PATCH's ring-flush path is equivalent to Linux; WPTR ordering is not the problem.

### Gaps
- Endianness is not stated in any source I read; everything on arm64 is little-endian and Linux uses plain OUT_RING stores.

## Q6. Register setup required before the SQE accepts packets

### Takeaway
Linux's order is: protection/ROQ/MEM_POOL/AHB config, interrupt mask, then CP_SQE_INSTR_BASE, CP_RB_BASE, CP_RB_CNTL (+optional RPTR_ADDR), and finally CP_SQE_CNTL=1 followed by ME_INIT. The PATCH covers most of this but omits CP_PROTECT, RBBM_INT_0_MASK and (on parts with whereami) the shadow. The remaining differences are candidates, not proven causes.

### Cited Findings
- CP_PROTECT_CNTL = ACCESS_PROT_EN | ACCESS_FAULT_ON_VIOL_EN | LAST_SPAN_INF_RANGE, then CP_PROTECT(i) table, last entry infinite — [L_GPU#L490-L501](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/a6xx_gpu.c#L490)
- Non-a640/a650/a610/a7xx parts (a615/a618/a630): CP_ROQ_THRESHOLDS_2 = 0x010000c0, CP_ROQ_THRESHOLDS_1 = 0x8040362c; CP_MEM_POOL_SIZE = 128 (a6xx other than a610/a702); CP_AHB_CNTL = 1 — [L_GPU#L977-L1007](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/a6xx_gpu.c#L977). PATCH writes the same values (setup[] ~1848-1851).
- UCHE_FILTER_CNTL 0x804, UCHE_CACHE_WAYS 0x4 for non-a640 parts — [L_GPU#L970-L973](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/a6xx_gpu.c#L970). PATCH probes these.
- RBBM_INT_0_MASK enabled before the CP starts — [L_GPU#L1103-L1105](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/a6xx_gpu.c#L1103)
- CP_APRIV_CNTL is only written when `gpu->hw_apriv` (`BIT(6)|BIT(5)|BIT(3)|BIT(2)|BIT(1)`); a618 has no HW_APRIV quirk so Linux leaves it at its reset value — [L_GPU#L1085-L1097](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/a6xx_gpu.c#L1085), [L_CAT#L763-L773](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/a6xx_catalog.c#L763). CP_APRIV_CNTL bit meanings: 0 ICACHE, 1 RBFETCH, 2 RBPRIVLEVEL, 3 RBRPWB, 5 CDREAD, 6 CDWRITE — [L_A6XML#L2296-L2312](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/registers/adreno/a6xx.xml#L2296)
- Ordering: ROQ/MEM_POOL/AHB, then `adreno_hw_init`, then CP_SQE_INSTR_BASE, CP_RB_BASE, CP_RB_CNTL, RPTR shadow, then `CP_SQE_CNTL = 1`, then `a6xx_cp_init`, then zap + CP_SET_SECURE_MODE — [L_GPU#L1116-L1171](https://github.com/torvalds/linux/blob/v6.12/drivers/gpu/drm/msm/adreno/a6xx_gpu.c#L1116)
- PATCH observation (comment ~1849): CP_ROQ_THRESHOLDS_2 (0x8c2) reads back 0 after the write and is marked "not required". PATCH also does not write CP_PROTECT_CNTL / CP_PROTECT(n) or RBBM_INT_0_MASK in the cold setup (grep of PATCH found no PROTECT_CNTL write).

### Inferences
- Hypothesis (unverified): if CP_ROQ_THRESHOLDS_2 really did not take the value (reads 0), the ROQ partitioning is wrong and ring/IB data could be mis-sliced, producing an illegal-instruction trap on a later, larger fetch than ME_INIT. ME_INIT (9 dwords) and SECURE_MODE (2 dwords) drained; the NOP+MEM_WRITE+EVENT_WRITE block is 2+4+5=11 dwords in the one-shot variant. This is a guess that fits your observation that bit 7 appeared with the larger block and earlier at SQE_CNTL=3.
- Hypothesis (unverified): CP_PROTECT unconfigured is not an illegal-instruction source (that is bit 4 CP_REGISTER_PROTECTION_ERROR), so it is unlikely the cause but should be set to match Linux.
- Hypothesis (unverified): CP_APRIV_CNTL reset value may require privileged-vs-unprivileged page attributes to line up; your PTEs are AP_UNPRIV so either privilege level is permitted.
- Suggested corrected sequences for the ring (dwords, cnt excludes header):
  - NOP: `0x70100001, 0x00000000`
  - MEM_WRITE, one 32-bit value: `0x703d8003, addr_lo, addr_hi, value`; Linux-style 64-bit form: `0x703d0004, addr_lo, addr_hi, value_lo, value_hi`
  - CACHE_FLUSH_TS: `0x70460004, 0x80000004, addr_lo, addr_hi, seqno` (Linux form, EVENT=4 | IRQ). Variant to try if the seqno is not written: `0xC0000004` (adds TIMESTAMP bit 30, per pm4.xml), untested.
  - ME_INIT: `0x70c80008, 0x0000002f, 0x00000003, 0x20000000, 0,0,0,0,0`
  - SET_SECURE_MODE 0: `0x70e60001, 0x00000000`

### Gaps
- Did not verify the RBBM interrupt bit/clear register used with CP_HW_ERROR.
- No source on whether cold-start CP registers behave differently under the signed-zap secure world (CP_ROQ_THRESHOLDS_2 reading 0 may reflect that; not established).
- Mesa freedreno and downstream kgsl comparisons missing.
