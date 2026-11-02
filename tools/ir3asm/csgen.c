/* csgen: host-side generator of the a618 compute dispatch command stream used by the surya kernel test.
 * Builds the same register sequence as Mesa's computerator (src/freedreno/computerator/a6xx.cc, a6xx variant) for one
 * assembled ir3 kernel, with fixed GPU virtual addresses, and prints C arrays for the kernel patch.
 * Own code; the register layouts come from Mesa's generated headers (MIT). */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <assert.h>
static inline uint32_t fui(float f){uint32_t u; memcpy(&u,&f,4); return u;}
#include "adreno_common.h"
#include "enums.xml.h"
#include "a7xx_enums.h"
#include "a8xx_enums.h"
#include "pm4.xml.h"
#include "a6xx.xml.h"
#include "desc.xml.h"

#define SCRATCH_VA   0x40020000u
#define SHADER_VA    (SCRATCH_VA + 0x2000)
#define DESC_VA      (SCRATCH_VA + 0x2400)
#define TARGET_VA    (SCRATCH_VA + 0x2800)
#define TS_VA        (SCRATCH_VA + 0x160)
#define TS_SEQ       0xc0de0001u
#define INVALID_REG  0xfcu

static uint32_t cs[512];
static unsigned n;

static uint32_t parity(uint32_t v)
{
   return (0x9669u >> (0xfu & (v ^ (v >> 4) ^ (v >> 8) ^ (v >> 12) ^ (v >> 16) ^ (v >> 20) ^ (v >> 24) ^ (v >> 28)))) & 1u;
}
static void pkt4(uint32_t reg, uint32_t val)
{
   cs[n++] = 0x40000000u | 1u | (parity(1) << 7) | (reg << 8) | (parity(reg) << 27);
   cs[n++] = val;
}
static unsigned pkt7_begin(unsigned op, unsigned cnt)
{
   cs[n++] = 0x70000000u | cnt | (parity(cnt) << 15) | ((op & 0x7fu) << 16) | (parity(op) << 23);
   return n;
}
/* CP_CONTEXT_REG_BUNCH: pairs of (register offset, value) */
static unsigned bunch_start, bunch_pairs;
static void crb_begin(unsigned pairs) { bunch_start = pkt7_begin(CP_CONTEXT_REG_BUNCH, pairs * 2); bunch_pairs = pairs; }
static void crb(uint32_t reg, uint32_t val) { cs[n++] = reg; cs[n++] = val; }
static void crb_end(void) { assert(n - bunch_start == bunch_pairs * 2); }

int main(int argc, char **argv)
{
   /* kernel facts from ir3asm for first_store.asm (a618): */
   const unsigned instrlen = argc > 1 ? atoi(argv[1]) : 1, max_reg = 0, max_half_reg = (unsigned)-1;
   const bool merged = true, double_threadsize = true, early_preamble = false;
   const unsigned branchstack = 0, constlen = 0, nuav = 1, localsize = 1, groups = 1;
   const unsigned thrsz = double_threadsize ? THREAD128 : THREAD64;
   (void)max_half_reg;

   /* UAV buffer descriptor for the 16-byte target (R32_UINT, linear, buffer view), as fdl6_buffer_view_init does */
   uint32_t desc[16] = {0};
   unsigned elements = 4;
   desc[0] = A6XX_TEX_MEMOBJ_0_TILE_MODE(TILE6_LINEAR) | A6XX_TEX_MEMOBJ_0_SWAP(0 /* WZYX */) |
             A6XX_TEX_MEMOBJ_0_FMT(FMT6_32_UINT) | A6XX_TEX_MEMOBJ_0_MIPLVLS(0) |
             A6XX_TEX_MEMOBJ_0_SWIZ_X(A6XX_TEX_X) | A6XX_TEX_MEMOBJ_0_SWIZ_Y(A6XX_TEX_Y) |
             A6XX_TEX_MEMOBJ_0_SWIZ_Z(A6XX_TEX_Z) | A6XX_TEX_MEMOBJ_0_SWIZ_W(A6XX_TEX_W);
   desc[1] = A6XX_TEX_MEMOBJ_1_WIDTH(elements & ((1 << 15) - 1)) | A6XX_TEX_MEMOBJ_1_HEIGHT(elements >> 15);
   desc[2] = A6XX_TEX_MEMOBJ_2_STRUCTSIZETEXELS(0) | A6XX_TEX_MEMOBJ_2_STARTOFFSETTEXELS(0) | A6XX_TEX_MEMOBJ_2_TYPE(A6XX_TEX_BUFFER);
   desc[4] = TARGET_VA;
   desc[5] = 0;

   /* restore state: non-context registers (pkt4) */
   pkt4(REG_A6XX_SP_PERFCTR_SHADER_MASK, A6XX_SP_PERFCTR_SHADER_MASK_CS);
   pkt4(REG_A6XX_SP_NC_MODE_CNTL_2, 0);
   pkt4(REG_A6XX_PC_MODE_CNTL, 0x1f);
   pkt4(REG_A6XX_PC_POWER_CNTL, 0);
   pkt4(REG_A6XX_VFD_POWER_CNTL, 0);
   pkt4(REG_A6XX_TPL1_DBG_ECO_CNTL, 0x00108000);
   pkt4(REG_A6XX_GRAS_DBG_ECO_CNTL, 0x00000880);
   pkt4(REG_A6XX_SP_CHICKEN_BITS, 0x00000430);
   pkt4(REG_A6XX_SP_DBG_ECO_CNTL, 0);
   pkt4(REG_A6XX_HLSQ_DBG_ECO_CNTL, 0x00080000);
   pkt4(REG_A6XX_VPC_DBG_ECO_CNTL, 0);
   pkt4(REG_A6XX_UCHE_UNKNOWN_0E12, 0x1);

   /* program registers */
   unsigned c = 0;
   unsigned constlen4 = (constlen + 3) & ~3u;
   crb_begin(15);
   crb(REG_A6XX_SP_MODE_CNTL, A6XX_SP_MODE_CNTL_CONSTANT_DEMOTION_ENABLE | A6XX_SP_MODE_CNTL_ISAMMODE(ISAMMODE_GL)); c++;
   crb(REG_A6XX_SP_UPDATE_CNTL, A6XX_SP_UPDATE_CNTL_VS_STATE | A6XX_SP_UPDATE_CNTL_HS_STATE | A6XX_SP_UPDATE_CNTL_DS_STATE |
        A6XX_SP_UPDATE_CNTL_GS_STATE | A6XX_SP_UPDATE_CNTL_FS_STATE | A6XX_SP_UPDATE_CNTL_CS_STATE | A6XX_SP_UPDATE_CNTL_GFX_UAV); c++;
   crb(REG_A6XX_SP_CS_CONST_CONFIG, A6XX_SP_CS_CONST_CONFIG_CONSTLEN(constlen4) | A6XX_SP_CS_CONST_CONFIG_ENABLED); c++;
   crb(REG_A6XX_SP_CS_CONFIG, A6XX_SP_CS_CONFIG_ENABLED | A6XX_SP_CS_CONFIG_NTEX(0) | A6XX_SP_CS_CONFIG_NSAMP(0) | A6XX_SP_CS_CONFIG_NUAV(nuav)); c++;
   crb(REG_A6XX_SP_CS_INSTR_SIZE, instrlen); c++;
   crb(REG_A6XX_SP_CS_CNTL_0, A6XX_SP_CS_CNTL_0_HALFREGFOOTPRINT(0) | A6XX_SP_CS_CNTL_0_FULLREGFOOTPRINT(max_reg + 1) |
        A6XX_SP_CS_CNTL_0_BRANCHSTACK(branchstack) | A6XX_SP_CS_CNTL_0_THREADSIZE(thrsz) |
        (early_preamble ? A6XX_SP_CS_CNTL_0_EARLYPREAMBLE : 0) | (merged ? A6XX_SP_CS_CNTL_0_MERGEDREGS : 0)); c++;
   crb(REG_A6XX_SP_CS_CNTL_1, A6XX_SP_CS_CNTL_1_SHARED_SIZE(1) | A6XX_SP_CS_CNTL_1_CONSTANTRAMMODE(CONSTLEN_128)); c++;
   crb(REG_A6XX_SP_CS_CONST_CONFIG_0, A6XX_SP_CS_CONST_CONFIG_0_WGIDCONSTID(INVALID_REG) | A6XX_SP_CS_CONST_CONFIG_0_WGSIZECONSTID(INVALID_REG) |
        A6XX_SP_CS_CONST_CONFIG_0_WGOFFSETCONSTID(INVALID_REG) | A6XX_SP_CS_CONST_CONFIG_0_LOCALIDREGID(INVALID_REG)); c++;
   crb(REG_A6XX_SP_CS_WGE_CNTL, A6XX_SP_CS_WGE_CNTL_LINEARLOCALIDREGID(INVALID_REG) | A6XX_SP_CS_WGE_CNTL_THREADSIZE(thrsz)); c++;
   crb(REG_A6XX_SP_CS_BASE, SHADER_VA); c++;
   crb(REG_A6XX_SP_CS_BASE + 1, 0); c++;
   crb(REG_A6XX_SP_CS_INSTR_SIZE, instrlen); c++;
   crb(REG_A6XX_SP_CS_UAV_BASE, DESC_VA); c++;
   crb(REG_A6XX_SP_CS_UAV_BASE + 1, 0); c++;
   crb(REG_A6XX_SP_CS_USIZE, nuav); c++;
   crb_end();
   assert(c == 15);

   /* shader preload */
   pkt7_begin(CP_LOAD_STATE6_FRAG, 3);
   cs[n++] = CP_LOAD_STATE6_0_STATE_TYPE(ST6_SHADER) | CP_LOAD_STATE6_0_STATE_SRC(SS6_INDIRECT) |
             CP_LOAD_STATE6_0_STATE_BLOCK(SB6_CS_SHADER) | CP_LOAD_STATE6_0_NUM_UNIT(instrlen < 64 ? instrlen : 64);
   cs[n++] = SHADER_VA; cs[n++] = 0;
   /* UAV descriptor */
   pkt7_begin(CP_LOAD_STATE6_FRAG, 3);
   cs[n++] = CP_LOAD_STATE6_0_STATE_TYPE(ST6_UAV) | CP_LOAD_STATE6_0_STATE_SRC(SS6_INDIRECT) |
             CP_LOAD_STATE6_0_STATE_BLOCK(SB6_CS_SHADER) | CP_LOAD_STATE6_0_NUM_UNIT(nuav);
   cs[n++] = DESC_VA; cs[n++] = 0;
   pkt7_begin(CP_SET_MARKER, 1);
   cs[n++] = A6XX_CP_SET_MARKER_0_MODE(RM6_COMPUTE);

   /* NDRANGE */
   crb_begin(10);
   crb(REG_A6XX_SP_CS_NDRANGE_0, A6XX_SP_CS_NDRANGE_0_KERNELDIM(3) | A6XX_SP_CS_NDRANGE_0_LOCALSIZEX(localsize - 1) |
        A6XX_SP_CS_NDRANGE_0_LOCALSIZEY(0) | A6XX_SP_CS_NDRANGE_0_LOCALSIZEZ(0));
   crb(REG_A6XX_SP_CS_NDRANGE_1, A6XX_SP_CS_NDRANGE_1_GLOBALSIZE_X(localsize * groups));
   crb(REG_A6XX_SP_CS_NDRANGE_2, 0);
   crb(REG_A6XX_SP_CS_NDRANGE_3, A6XX_SP_CS_NDRANGE_3_GLOBALSIZE_Y(1));
   crb(REG_A6XX_SP_CS_NDRANGE_4, 0);
   crb(REG_A6XX_SP_CS_NDRANGE_5, A6XX_SP_CS_NDRANGE_5_GLOBALSIZE_Z(1));
   crb(REG_A6XX_SP_CS_NDRANGE_6, 0);
   crb(REG_A6XX_SP_CS_KERNEL_GROUP_X, 1);
   crb(REG_A6XX_SP_CS_KERNEL_GROUP_Y, 1);
   crb(REG_A6XX_SP_CS_KERNEL_GROUP_Z, 1);
   crb_end();

   pkt7_begin(CP_EXEC_CS, 4);
   cs[n++] = 0; cs[n++] = groups; cs[n++] = 1; cs[n++] = 1;
   pkt7_begin(CP_WAIT_FOR_IDLE, 0);
   /* completion timestamp */
   pkt7_begin(CP_EVENT_WRITE, 4);
   cs[n++] = 4 /* CACHE_FLUSH_TS */ | 0x80000000u;
   cs[n++] = TS_VA; cs[n++] = 0; cs[n++] = TS_SEQ;

   printf("/* generated by research/mesa-ir3/csgen: a618 compute dispatch of first_store.asm. VAs: shader %#x, UAV descriptor %#x, target %#x, timestamp %#x (seq %#x). */\n",
          SHADER_VA, DESC_VA, TARGET_VA, TS_VA, TS_SEQ);
   printf("static const uint32_t cs_ib[%u] = {", n);
   for (unsigned i = 0; i < n; i++) printf("%s0x%08x,", (i % 6) ? " " : "\n\t", cs[i]);
   printf("\n};\nstatic const uint32_t cs_shader[] = {");
   if (argc > 2) {
      FILE *f = fopen(argv[2], "rb");
      uint32_t w[256]; size_t k = f ? fread(w, 4, 256, f) : 0;
      for (size_t i = 0; i < k; i++) printf("%s0x%08x,", (i % 6) ? " " : "\n\t", w[i]);
      if (f) fclose(f);
   }
   printf("\n};\nstatic const uint32_t cs_uav_desc[16] = {");
   for (unsigned i = 0; i < 16; i++) printf("%s0x%08x,", (i % 6) ? " " : "\n\t", desc[i]);
   printf("\n};\n");
   return 0;
}
