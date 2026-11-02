/*
 * adreno618_test: userland test of the SweetAdreno618 user client (Poco X3 NFC, Adreno 618, SM7150).
 * Repeats from a normal process what the kernel self-test does: one 16 KiB BO holds a compute shader, a UAV buffer
 * descriptor, a 16-byte target and a command stream; GEM_SUBMIT runs it and the shader stores 0xcafe0001 to the target.
 * Modelled on gpu/kernel-test/adreno_uc_test.c (Poco F1). Own code. ABI: xnu/gpu-abi/sweet_adreno_abi.h.
 *
 *   adreno618_test            full test (info, BO, submit twice, check, close)
 *   adreno618_test --info     service probe only (INFO + GET_PARAM), submits nothing
 *   adreno618_test --selfcheck  build the command stream with the original csgen VAs and compare it with the golden
 *                             arrays (host only, needs no device); also the only mode of the -DHOST_SELFCHECK build
 *   -v                        raw numbers
 *
 * The command stream is the one of tools/ir3asm/csgen.c (a618 compute dispatch of first_store.asm, same register/value
 * sequence as Mesa computerator), ported to take the GPU addresses as parameters. Register offsets are hard-coded
 * (names in comments) as csgen.c produces them from Mesa's generated a6xx.xml.h / pm4.xml.h.
 */
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#ifndef HOST_SELFCHECK
#include <IOKit/IOKitLib.h>
#include <CoreFoundation/CoreFoundation.h>
#include <mach/mach.h>
#include <unistd.h>
#include "../../xnu/gpu-abi/sweet_adreno_abi.h"
#endif

#define EXPECT_ABI_VERSION      1
#define EXPECT_GPU_ID           618
#define EXPECT_CHIP_ID          0x06010800u
#define EXPECT_GMEM_SIZE        0x80000u

#define BO_SIZE         16384
#define OFF_SHADER      0x0000  /* 64-byte aligned */
#define OFF_DESC        0x0400  /* 64-byte aligned */
#define OFF_TARGET      0x0800
#define OFF_TS          0x0c00
#define OFF_CS          0x1000
#define TS_SEQ          0xc0de0001u
#define RESULT_WORD     0xcafe0001u
#define MAX_CS_DWORDS   128

/* Compiled shader of tools/ir3asm/first_store.asm (a618): stores 0xcafe0001 to the buffer bound as UAV 0. */
static const uint32_t cs_shader[32] = {

	0xcafe0001, 0x204cc000, 0x00000000, 0x204cc001, 0x00000000, 0x00000500,
	0x01674000, 0xc0260000, 0x00000000, 0x03000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000, 0x00000000,
	0x00000000, 0x00000000,
};

/* ---- command stream generator (port of tools/ir3asm/csgen.c) ---- */

static uint32_t cs[MAX_CS_DWORDS];
static unsigned cs_n;

/* odd-parity bit as in Mesa pm4_odd_parity_bit() */
static uint32_t
parity(uint32_t v)
{
	return (0x9669u >> (0xfu & (v ^ (v >> 4) ^ (v >> 8) ^ (v >> 12) ^ (v >> 16) ^ (v >> 20) ^ (v >> 24) ^ (v >> 28)))) & 1u;
}

static void
pkt4(uint32_t reg, uint32_t val)
{
	cs[cs_n++] = 0x40000000u | 1u | (parity(1) << 7) | (reg << 8) | (parity(reg) << 27);
	cs[cs_n++] = val;
}

static void
pkt7(uint32_t op, uint32_t cnt)
{
	cs[cs_n++] = 0x70000000u | cnt | (parity(cnt) << 15) | ((op & 0x7fu) << 16) | (parity(op) << 23);
}

/* CP_CONTEXT_REG_BUNCH payload: (register offset, value) pairs */
static void
crb(uint32_t reg, uint32_t val)
{
	cs[cs_n++] = reg;
	cs[cs_n++] = val;
}

#define LO(x) ((uint32_t)(uint64_t)(x))
#define HI(x) ((uint32_t)((uint64_t)(x) >> 32))

/* Dispatch of one workgroup of one thread. Returns the number of dwords. */
static unsigned
build_stream(uint64_t shader_va, uint64_t desc_va, uint64_t ts_va)
{
	const uint32_t instrlen = 1, nuav = 1;

	cs_n = 0;
	/* restore state: non-context registers (pkt4) */
	pkt4(0xae0f, 0x20);             /* SP_PERFCTR_SHADER_MASK = CS */
	pkt4(0xae04, 0);                /* SP_NC_MODE_CNTL_2 */
	pkt4(0x9804, 0x1f);             /* PC_MODE_CNTL */
	pkt4(0x9805, 0);                /* PC_POWER_CNTL */
	pkt4(0xa0f8, 0);                /* VFD_POWER_CNTL */
	pkt4(0xb600, 0x00108000);       /* TPL1_DBG_ECO_CNTL */
	pkt4(0x8600, 0x00000880);       /* GRAS_DBG_ECO_CNTL */
	pkt4(0xae03, 0x00000430);       /* SP_CHICKEN_BITS */
	pkt4(0xae00, 0);                /* SP_DBG_ECO_CNTL */
	pkt4(0xbe04, 0x00080000);       /* HLSQ_DBG_ECO_CNTL */
	pkt4(0x9600, 0);                /* VPC_DBG_ECO_CNTL */
	pkt4(0x0e12, 1);                /* UCHE_UNKNOWN_0E12 */

	/* program registers: CP_CONTEXT_REG_BUNCH (opcode 0x5c), 15 pairs */
	pkt7(0x5c, 30);
	crb(0xab00, 0x5);               /* SP_MODE_CNTL = CONSTANT_DEMOTION_ENABLE | ISAMMODE(GL) */
	crb(0xbb08, 0xbf);              /* SP_UPDATE_CNTL = all *_STATE bits | GFX_UAV */
	crb(0xb987, 0x100);             /* SP_CS_CONST_CONFIG = CONSTLEN(0) | ENABLED */
	crb(0xa9bb, 0x400100);          /* SP_CS_CONFIG = ENABLED | NUAV(1) */
	crb(0xa9bc, instrlen);          /* SP_CS_INSTR_SIZE */
	crb(0xa9b0, 0x80100080);        /* SP_CS_CNTL_0 = FULLREGFOOTPRINT(1) | THREADSIZE(128) | MERGEDREGS */
	crb(0xa9b1, 0x1);               /* SP_CS_CNTL_1 = SHARED_SIZE(1) | CONSTANTRAMMODE(128) */
	crb(0xb997, 0xfcfcfcfc);        /* SP_CS_CONST_CONFIG_0 = invalid ids */
	crb(0xb998, 0x2fc);             /* SP_CS_WGE_CNTL = LINEARLOCALIDREGID(invalid) | THREADSIZE(128) */
	crb(0xa9b4, LO(shader_va));     /* SP_CS_BASE lo */
	crb(0xa9b5, HI(shader_va));     /* SP_CS_BASE hi */
	crb(0xa9bc, instrlen);          /* SP_CS_INSTR_SIZE */
	crb(0xa9f2, LO(desc_va));       /* SP_CS_UAV_BASE lo */
	crb(0xa9f3, HI(desc_va));       /* SP_CS_UAV_BASE hi */
	crb(0xaa00, nuav);              /* SP_CS_USIZE */

	/* shader preload: CP_LOAD_STATE6_FRAG (opcode 0x34), STATE_TYPE ST6_SHADER | SRC INDIRECT | BLOCK SB6_CS_SHADER | NUM_UNIT */
	pkt7(0x34, 3);
	cs[cs_n++] = 0x00360000u | (instrlen < 64 ? instrlen : 64) << 22;   /* NUM_UNIT in bits 22+; base = ST6_SHADER | INDIRECT | SB6_CS_SHADER */
	cs[cs_n++] = LO(shader_va);
	cs[cs_n++] = HI(shader_va);
	/* UAV descriptor: ST6_UAV | INDIRECT | SB6_CS_SHADER | NUM_UNIT(nuav) */
	pkt7(0x34, 3);
	cs[cs_n++] = 0x0036c000u | nuav << 22;
	cs[cs_n++] = LO(desc_va);
	cs[cs_n++] = HI(desc_va);
	pkt7(0x65, 1);                  /* CP_SET_MARKER */
	cs[cs_n++] = 0x8;               /* MODE = RM6_COMPUTE */

	/* NDRANGE: CP_CONTEXT_REG_BUNCH, 10 pairs */
	pkt7(0x5c, 20);
	crb(0xb990, 0x3);               /* SP_CS_NDRANGE_0 = KERNELDIM(3) | LOCALSIZE(1,1,1) */
	crb(0xb991, 0x1);               /* SP_CS_NDRANGE_1 = GLOBALSIZE_X(1) */
	crb(0xb992, 0);                 /* SP_CS_NDRANGE_2 */
	crb(0xb993, 0x1);               /* SP_CS_NDRANGE_3 = GLOBALSIZE_Y(1) */
	crb(0xb994, 0);                 /* SP_CS_NDRANGE_4 */
	crb(0xb995, 0x1);               /* SP_CS_NDRANGE_5 = GLOBALSIZE_Z(1) */
	crb(0xb996, 0);                 /* SP_CS_NDRANGE_6 */
	crb(0xb999, 1);                 /* SP_CS_KERNEL_GROUP_X */
	crb(0xb99a, 1);                 /* SP_CS_KERNEL_GROUP_Y */
	crb(0xb99b, 1);                 /* SP_CS_KERNEL_GROUP_Z */

	pkt7(0x33, 4);                  /* CP_EXEC_CS */
	cs[cs_n++] = 0;
	cs[cs_n++] = 1;
	cs[cs_n++] = 1;
	cs[cs_n++] = 1;
	pkt7(0x26, 0);                  /* CP_WAIT_FOR_IDLE */
	/* completion timestamp: CP_EVENT_WRITE (0x46), CACHE_FLUSH_TS | IRQ(bit31) */
	pkt7(0x46, 4);
	cs[cs_n++] = 4 | 0x80000000u;
	cs[cs_n++] = LO(ts_va);
	cs[cs_n++] = HI(ts_va);
	cs[cs_n++] = TS_SEQ;
	return cs_n;
}

/* UAV buffer descriptor (R32_UINT, linear buffer view, 4 elements) for the target at target_va */
static void
build_desc(uint32_t desc[16], uint64_t target_va)
{
	memset(desc, 0, 16 * sizeof(uint32_t));
	desc[0] = 0x12c06880;           /* A6XX_TEX_MEMOBJ_0: TILE_MODE linear, FMT 32_UINT, swizzle XYZW */
	desc[1] = 0x00000004;           /* A6XX_TEX_MEMOBJ_1: WIDTH(4 elements) */
	desc[2] = 0x80000000;           /* A6XX_TEX_MEMOBJ_2: TYPE = A6XX_TEX_BUFFER */
	desc[4] = LO(target_va);
	desc[5] = HI(target_va);
}

/* ---- self check against the csgen output (research/mesa-ir3/cs_out.h) ---- */

/* csgen fixed VAs: shader 0x40022000, UAV descriptor 0x40022400, target 0x40022800, timestamp 0x40020160 */
static const uint32_t golden_ib[97] = {
	0x40ae0f01, 0x00000020, 0x48ae0401, 0x00000000, 0x48980401, 0x0000001f,
	0x40980501, 0x00000000, 0x40a0f801, 0x00000000, 0x40b60001, 0x00108000,
	0x40860001, 0x00000880, 0x40ae0301, 0x00000430, 0x40ae0001, 0x00000000,
	0x40be0401, 0x00080000, 0x48960001, 0x00000000, 0x400e1201, 0x00000001,
	0x70dc801e, 0x0000ab00, 0x00000005, 0x0000bb08, 0x000000bf, 0x0000b987,
	0x00000100, 0x0000a9bb, 0x00400100, 0x0000a9bc, 0x00000001, 0x0000a9b0,
	0x80100080, 0x0000a9b1, 0x00000001, 0x0000b997, 0xfcfcfcfc, 0x0000b998,
	0x000002fc, 0x0000a9b4, 0x40022000, 0x0000a9b5, 0x00000000, 0x0000a9bc,
	0x00000001, 0x0000a9f2, 0x40022400, 0x0000a9f3, 0x00000000, 0x0000aa00,
	0x00000001, 0x70348003, 0x00760000, 0x40022000, 0x00000000, 0x70348003,
	0x0076c000, 0x40022400, 0x00000000, 0x70e50001, 0x00000008, 0x70dc8014,
	0x0000b990, 0x00000003, 0x0000b991, 0x00000001, 0x0000b992, 0x00000000,
	0x0000b993, 0x00000001, 0x0000b994, 0x00000000, 0x0000b995, 0x00000001,
	0x0000b996, 0x00000000, 0x0000b999, 0x00000001, 0x0000b99a, 0x00000001,
	0x0000b99b, 0x00000001, 0x70b30004, 0x00000000, 0x00000001, 0x00000001,
	0x00000001, 0x70268000, 0x70460004, 0x80000004, 0x40020160, 0x00000000,
	0xc0de0001,
};
#if defined(HOST_SELFCHECK) && defined(__has_include)
#if __has_include("../../research/mesa-ir3/cs_out.h")
#define HAVE_CS_OUT 1
#define cs_ib ref_ib
#define cs_shader ref_shader
#define cs_uav_desc ref_desc
#include "../../research/mesa-ir3/cs_out.h"
#undef cs_ib
#undef cs_shader
#undef cs_uav_desc
#endif
#endif
static const uint32_t golden_desc[16] = { 0x12c06880, 0x00000004, 0x80000000, 0x00000000, 0x40022800 };

static int
selfcheck(void)
{
	int bad = 0;
	unsigned n = build_stream(0x40022000, 0x40022400, 0x40020160);
	uint32_t desc[16];
	build_desc(desc, 0x40022800);
	if (n != 97) {
		printf("selfcheck FAIL: stream has %u dwords, golden 97\n", n);
		bad++;
	}
	for (unsigned i = 0; i < n && i < 97; i++) {
		if (cs[i] != golden_ib[i]) {
			printf("selfcheck FAIL: cs[%u] = %#010x, golden %#010x\n", i, cs[i], golden_ib[i]);
			bad++;
		}
	}
	for (unsigned i = 0; i < 16; i++) {
		if (desc[i] != golden_desc[i]) {
			printf("selfcheck FAIL: desc[%u] = %#010x, golden %#010x\n", i, desc[i], golden_desc[i]);
			bad++;
		}
	}
	printf("selfcheck: stream %u dwords, descriptor 16 dwords vs embedded golden: %s\n", n, bad ? "MISMATCH" : "identical");
#ifdef HAVE_CS_OUT
	/* also against the live csgen output when the (git-ignored) research tree is present */
	int bad2 = 0;
	for (unsigned i = 0; i < 97; i++) {
		bad2 += cs[i] != ref_ib[i];
	}
	for (unsigned i = 0; i < 16; i++) {
		bad2 += desc[i] != ref_desc[i];
	}
	for (unsigned i = 0; i < 32; i++) {
		bad2 += (i < sizeof(ref_shader) / 4 ? ref_shader[i] : 0) != cs_shader[i];
	}
	printf("selfcheck: vs research/mesa-ir3/cs_out.h (ib 97, desc 16, shader 32 dwords): %s\n", bad2 ? "MISMATCH" : "identical");
	bad += bad2;
#endif
	/* the stream for a real BO address: addresses must show up as lo/hi words */
	n = build_stream(0x1234567890000ull, 0x1234567890400ull, 0x1234567890c00ull);
	int found = 0;
	for (unsigned i = 0; i + 1 < n; i++) {
		found += cs[i] == 0xa9b4 && cs[i + 1] == 0x67890000u && cs[i + 2] == 0xa9b5 && cs[i + 3] == 0x00012345u;
	}
	printf("selfcheck: 64-bit shader address split into lo/hi words: %s\n", found == 1 ? "ok" : "FAIL");
	bad += found != 1;
	printf("selfcheck: %s\n", bad ? "FAILED" : "PASSED");
	return bad != 0;
}

#ifdef HOST_SELFCHECK
int
main(int argc, char **argv)
{
	(void)argc; (void)argv;
	return selfcheck();
}
#else

/* ---- device test ---- */

#ifndef SWEET_GPU_SERVICE_NAME_A618
#define SWEET_GPU_SERVICE_NAME_A618 "SweetAdreno618"
#endif

static int g_fail, g_verbose;
static unsigned g_cs_dwords;
#define CHECK(cond, ...) do { if (cond) { printf("PASS  "); } else { printf("FAIL  "); g_fail++; } printf(__VA_ARGS__); printf("\n"); } while (0)

static io_connect_t g_conn;

static kern_return_t
call(uint32_t sel, void *s, size_t in, size_t out)
{
	size_t o = out;
	return IOConnectCallStructMethod(g_conn, sel, s, in, out ? s : NULL, out ? &o : NULL);
}

static kern_return_t
get_param(uint32_t p, uint64_t *v)
{
	struct sweet_msm_param a = { .pipe = SWEET_MSM_PIPE_3D0, .param = p };
	kern_return_t kr = call(SWEET_GPU_SEL_GET_PARAM, &a, sizeof(a), sizeof(a));
	*v = kr == 0 ? a.value : 0;
	return kr;
}

static struct sweet_msm_timespec
rel(int64_t sec, int64_t nsec)
{
	struct sweet_msm_timespec t = { sec, nsec };
	return t;
}

/* Step 1: open the service, INFO and GET_PARAMs. Returns 0 when the service is usable. */
static int
step_info(void)
{
	io_service_t svc = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching(SWEET_GPU_SERVICE_NAME_A618));
	if (svc == IO_OBJECT_NULL) {
		printf("FAIL  service %s not found\n", SWEET_GPU_SERVICE_NAME_A618);
		io_service_t old = IOServiceGetMatchingService(kIOMainPortDefault, IOServiceMatching("SweetAdreno630"));
		if (old != IO_OBJECT_NULL) {
			printf("      SweetAdreno630 exists: this is a Poco F1 kernel, not surya; use gpu/kernel-test/adreno_uc_test\n");
			IOObjectRelease(old);
		} else {
			printf("      is this the surya kernel with the GPU enabled and did the log say that the A618 service was published?\n");
		}
		g_fail++;
		return 1;
	}
	printf("PASS  service %s found\n", SWEET_GPU_SERVICE_NAME_A618);
	CFTypeRef p = IORegistryEntryCreateCFProperty(svc, CFSTR("SweetGPUABIVersion"), kCFAllocatorDefault, 0);
	CFTypeRef c = IORegistryEntryCreateCFProperty(svc, CFSTR("SweetGPUChipID"), kCFAllocatorDefault, 0);
	int32_t rv = -1, rc = -1;
	if (p) {
		CFNumberGetValue(p, kCFNumberSInt32Type, &rv);
		CFRelease(p);
	}
	if (c) {
		CFNumberGetValue(c, kCFNumberSInt32Type, &rc);
		CFRelease(c);
	}
	printf("      registry: SweetGPUABIVersion %d SweetGPUChipID %#x\n", rv, (uint32_t)rc);
	CHECK(rv == EXPECT_ABI_VERSION && (uint32_t)rc == EXPECT_CHIP_ID, "registry ABI version %d (want %d), chip id %#x (want %#x)",
	    rv, EXPECT_ABI_VERSION, (uint32_t)rc, EXPECT_CHIP_ID);
	kern_return_t kr = IOServiceOpen(svc, mach_task_self(), 0, &g_conn);
	IOObjectRelease(svc);
	CHECK(kr == KERN_SUCCESS, "IOServiceOpen -> %#x", kr);
	if (kr != KERN_SUCCESS) {
		return 1;
	}

	struct sweet_gpu_info info;
	memset(&info, 0, sizeof(info));
	kr = call(SWEET_GPU_SEL_INFO, &info, 0, sizeof(info));
	CHECK(kr == 0 && info.abi_version == EXPECT_ABI_VERSION, "INFO abi %u (want %u) minor %u kr %#x", info.abi_version, EXPECT_ABI_VERSION, info.abi_minor, kr);
	CHECK((info.flags & 3) == 1, "INFO flags %#x (bit0 ready, bit1 lost) gpu %u chip %#x va %#llx+%#llx last_fence %u",
	    info.flags, info.gpu_id, info.chip_id, (unsigned long long)info.va_start, (unsigned long long)info.va_size, info.last_fence);
	if ((info.flags & 3) != 1) {
		return 1;
	}
	uint64_t v = 0;
	kr = get_param(SWEET_MSM_PARAM_GPU_ID, &v);
	CHECK(kr == 0 && v == EXPECT_GPU_ID, "GET_PARAM GPU_ID %llu (want %d) kr %#x", (unsigned long long)v, EXPECT_GPU_ID, kr);
	kr = get_param(SWEET_MSM_PARAM_CHIP_ID, &v);
	CHECK(kr == 0 && v == EXPECT_CHIP_ID, "GET_PARAM CHIP_ID %#llx (want %#x) kr %#x", (unsigned long long)v, EXPECT_CHIP_ID, kr);
	kr = get_param(SWEET_MSM_PARAM_GMEM_SIZE, &v);
	CHECK(kr == 0 && v == EXPECT_GMEM_SIZE, "GET_PARAM GMEM_SIZE %#llx (want %#x) kr %#x", (unsigned long long)v, EXPECT_GMEM_SIZE, kr);
	kr = get_param(SWEET_MSM_PARAM_VA_START, &v);
	printf("%s  GET_PARAM VA_START %#llx kr %#x\n", kr == 0 ? "PASS" : "FAIL", (unsigned long long)v, kr);
	g_fail += kr != 0;
	kr = get_param(SWEET_MSM_PARAM_VA_SIZE, &v);
	printf("%s  GET_PARAM VA_SIZE %#llx kr %#x\n", kr == 0 ? "PASS" : "FAIL", (unsigned long long)v, kr);
	g_fail += kr != 0;
	return 0;
}

/* Fill the BO: shader, descriptor, poisoned target, zero timestamp, command stream. Caller holds CPU_PREP(WRITE). */
static void
poison_target(volatile uint32_t *bo)
{
	for (unsigned i = 0; i < 4; i++) {
		bo[OFF_TARGET / 4 + i] = 0xdeadbee0u + i;
	}
	bo[OFF_TS / 4] = 0;
}

static int
run_once(volatile uint32_t *bo, uint32_t handle, int round)
{
	struct sweet_msm_gem_cpu_prep cp = { .handle = handle, .op = SWEET_MSM_PREP_WRITE, .timeout = rel(1, 0) };
	struct sweet_msm_gem_cpu_fini cf = { .handle = handle };
	kern_return_t kr = call(SWEET_GPU_SEL_GEM_CPU_PREP, &cp, sizeof(cp), 0);
	CHECK(kr == 0, "round %d: CPU_PREP write kr %#x", round, kr);
	poison_target(bo);
	kr = call(SWEET_GPU_SEL_GEM_CPU_FINI, &cf, sizeof(cf), 0);
	CHECK(kr == 0, "round %d: CPU_FINI kr %#x", round, kr);

	struct sweet_msm_gem_submit_bo sb = { .flags = SWEET_MSM_SUBMIT_BO_READ | SWEET_MSM_SUBMIT_BO_WRITE, .handle = handle };
	struct sweet_msm_gem_submit_cmd sc = { .type = SWEET_MSM_SUBMIT_CMD_BUF, .submit_idx = 0, .submit_offset = OFF_CS,
		.size = g_cs_dwords * 4 };
	struct sweet_msm_gem_submit sub = { .nr_bos = 1, .nr_cmds = 1, .bos = (uint64_t)(uintptr_t)&sb, .cmds = (uint64_t)(uintptr_t)&sc };
	kr = call(SWEET_GPU_SEL_GEM_SUBMIT, &sub, sizeof(sub), sizeof(sub));
	CHECK(kr == 0 && sub.fence != 0, "round %d: GEM_SUBMIT (%u dwords at BO+%#x) -> fence %u kr %#x", round, g_cs_dwords, OFF_CS, sub.fence, kr);
	if (kr != 0) {
		return 1;
	}
	struct sweet_msm_wait_fence wf = { .fence = sub.fence, .timeout = rel(2, 0) };
	kr = call(SWEET_GPU_SEL_WAIT_FENCE, &wf, sizeof(wf), 0);
	CHECK(kr == 0, "round %d: WAIT_FENCE %u (2 s) kr %#x", round, sub.fence, kr);
	cp.op = SWEET_MSM_PREP_READ;
	kr = call(SWEET_GPU_SEL_GEM_CPU_PREP, &cp, sizeof(cp), 0);
	CHECK(kr == 0, "round %d: CPU_PREP read kr %#x", round, kr);
	uint32_t t[4], ts = bo[OFF_TS / 4];
	for (unsigned i = 0; i < 4; i++) {
		t[i] = bo[OFF_TARGET / 4 + i];
	}
	(void)call(SWEET_GPU_SEL_GEM_CPU_FINI, &cf, sizeof(cf), 0);
	printf("      round %d: target %#x %#x %#x %#x timestamp %#x\n", round, t[0], t[1], t[2], t[3], ts);
	CHECK(t[0] == RESULT_WORD, "round %d: target[0] = %#x (want %#x)", round, t[0], RESULT_WORD);
	CHECK(t[1] == 0xdeadbee1u && t[2] == 0xdeadbee2u && t[3] == 0xdeadbee3u, "round %d: target[1..3] unchanged", round);
	CHECK(ts == TS_SEQ, "round %d: timestamp %#x (want %#x)", round, ts, TS_SEQ);
	return kr != 0 || t[0] != RESULT_WORD || ts != TS_SEQ;
}

int
main(int argc, char **argv)
{
	int info_only = 0;
	for (int i = 1; i < argc; i++) {
		if (!strcmp(argv[i], "-v")) {
			g_verbose = 1;
		} else if (!strcmp(argv[i], "--info")) {
			info_only = 1;
		} else if (!strcmp(argv[i], "--selfcheck")) {
			return selfcheck();
		} else {
			printf("usage: %s [--info | --selfcheck] [-v]\n", argv[0]);
			return 2;
		}
	}

	/* 1 */
	if (step_info()) {
		return 1;
	}
	if (info_only) {
		IOServiceClose(g_conn);
		printf("%s: %d failure(s) (--info: nothing submitted)\n", g_fail ? "FAILED" : "PASSED", g_fail);
		return g_fail != 0;
	}

	/* 2: one 16 KiB BO */
	struct sweet_msm_gem_new nb = { .size = BO_SIZE, .flags = 0 };
	kern_return_t kr = call(SWEET_GPU_SEL_GEM_NEW, &nb, sizeof(nb), sizeof(nb));
	CHECK(kr == 0 && nb.handle != 0, "GEM_NEW %u -> handle %u kr %#x", BO_SIZE, nb.handle, kr);
	if (kr != 0) {
		return 1;
	}
	struct sweet_msm_gem_info gi = { .handle = nb.handle, .info = SWEET_MSM_INFO_GET_IOVA };
	kr = call(SWEET_GPU_SEL_GEM_INFO, &gi, sizeof(gi), sizeof(gi));
	uint64_t iova = gi.value;
	CHECK(kr == 0 && iova != 0 && !(iova & 0xfff), "GEM_INFO GET_IOVA %#llx kr %#x (must be non-zero and 4 KiB aligned)", (unsigned long long)iova, kr);
	struct sweet_msm_gem_info go = { .handle = nb.handle, .info = SWEET_MSM_INFO_GET_OFFSET };
	kr |= call(SWEET_GPU_SEL_GEM_INFO, &go, sizeof(go), sizeof(go));
	CHECK(kr == 0, "GEM_INFO GET_OFFSET %llu (= memoryType for the map)", (unsigned long long)go.value);
	if (kr != 0 || iova == 0 || (iova & 0xfff)) {
		return 1;
	}
	mach_vm_address_t addr = 0;
	mach_vm_size_t size = 0;
	kr = IOConnectMapMemory64(g_conn, (uint32_t)go.value, mach_task_self(), &addr, &size, kIOMapAnywhere | kIOMapWriteCombineCache);
	CHECK(kr == 0 && addr != 0 && size >= BO_SIZE, "IOConnectMapMemory64 -> %#llx size %llu kr %#x", (unsigned long long)addr, (unsigned long long)size, kr);
	if (kr != 0) {
		return 1;
	}
	volatile uint32_t *bo = (volatile uint32_t *)(uintptr_t)addr;

	/* 3: fill the BO from userland (CPU_PREP write ... CPU_FINI around the CPU stores) */
	struct sweet_msm_gem_cpu_prep cp = { .handle = nb.handle, .op = SWEET_MSM_PREP_WRITE, .timeout = rel(1, 0) };
	struct sweet_msm_gem_cpu_fini cf = { .handle = nb.handle };
	kr = call(SWEET_GPU_SEL_GEM_CPU_PREP, &cp, sizeof(cp), 0);
	CHECK(kr == 0, "CPU_PREP write (fill) kr %#x", kr);
	for (unsigned i = 0; i < BO_SIZE / 4; i++) {
		bo[i] = 0;
	}
	for (unsigned i = 0; i < 32; i++) {
		bo[OFF_SHADER / 4 + i] = cs_shader[i];
	}
	uint32_t desc[16];
	build_desc(desc, iova + OFF_TARGET);
	for (unsigned i = 0; i < 16; i++) {
		bo[OFF_DESC / 4 + i] = desc[i];
	}
	g_cs_dwords = build_stream(iova + OFF_SHADER, iova + OFF_DESC, iova + OFF_TS);
	for (unsigned i = 0; i < g_cs_dwords; i++) {
		bo[OFF_CS / 4 + i] = cs[i];
	}
	poison_target(bo);
	kr = call(SWEET_GPU_SEL_GEM_CPU_FINI, &cf, sizeof(cf), 0);
	CHECK(kr == 0, "CPU_FINI kr %#x; BO filled: shader +%#x desc +%#x target +%#x ts +%#x stream +%#x (%u dwords), iova %#llx",
	    kr, OFF_SHADER, OFF_DESC, OFF_TARGET, OFF_TS, OFF_CS, g_cs_dwords, (unsigned long long)iova);
	if (g_verbose) {
		for (unsigned i = 0; i < g_cs_dwords; i++) {
			printf("%s%08x", (i % 8) ? " " : "\n  cs ", cs[i]);
		}
		printf("\n");
	}

	/* 4, 5: submit / wait / check, twice (the second time the target is re-poisoned first) */
	for (int round = 1; round <= 2; round++) {
		if (run_once(bo, nb.handle, round)) {
			break;
		}
	}

	IOConnectUnmapMemory64(g_conn, (uint32_t)go.value, mach_task_self(), addr);
	struct sweet_gem_close gc = { .handle = nb.handle };
	kr = call(SWEET_GPU_SEL_GEM_CLOSE, &gc, sizeof(gc), 0);
	CHECK(kr == 0, "GEM_CLOSE kr %#x", kr);
	IOServiceClose(g_conn);
	printf("%s: %d failure(s)\n", g_fail ? "FAILED" : "PASSED", g_fail);
	return g_fail != 0;
}
#endif /* HOST_SELFCHECK */
