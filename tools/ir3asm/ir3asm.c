/* ir3asm: assemble ir3 assembly text into an a6xx shader binary for a chosen GPU (default a618, chip id 0x06010800).
 * Own code written for the Bergamota surya port; links against a Mesa build tree (MIT licensed Mesa headers/libs). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "util/macros.h"
#include "ir3.h"
#include "ir3_assembler.h"
#include "ir3_shader.h"
#include "freedreno/common/freedreno_dev_info.h"

int
main(int argc, char **argv)
{
   if (argc < 3) {
      fprintf(stderr, "usage: ir3asm IN.asm OUT.bin [gpu_id [chip_id_hex]]\n");
      return 2;
   }
   struct fd_dev_id id = { .gpu_id = argc > 3 ? atoi(argv[3]) : 618, .chip_id = argc > 4 ? strtoull(argv[4], NULL, 16) : 0x06010800ull };
   const struct fd_dev_info *info = fd_dev_info_raw(&id);
   if (!info) {
      fprintf(stderr, "unknown gpu %u chip %#llx\n", id.gpu_id, (unsigned long long)id.chip_id);
      return 1;
   }
   struct ir3_compiler *c = ir3_compiler_create(NULL, &id, info, &(struct ir3_compiler_options){ .disable_cache = true });
   FILE *in = fopen(argv[1], "r");
   if (!in) { perror(argv[1]); return 1; }
   struct ir3_kernel_info k = {};
   struct ir3_shader *s = ir3_parse_asm(c, &k, in);
   fclose(in);
   if (!s) { fprintf(stderr, "assembler failed\n"); return 1; }
   struct ir3_shader_variant *v = s->variants;
   FILE *out = fopen(argv[2], "wb");
   fwrite(v->bin, 1, v->info.size, out);
   fclose(out);
   printf("gpu %u chip %#llx: %u bytes (%u dwords of instructions), max_reg %d max_half_reg %d constlen %u max_waves %d\n"
          "kernel: bufs %u, numwg reg %u, wgid reg %u, localsize %u,%u,%u\n",
          id.gpu_id, (unsigned long long)id.chip_id, v->info.size, v->info.sizedwords, v->info.max_reg, v->info.max_half_reg,
          v->info.constlen, v->info.max_waves, k.num_bufs, k.numwg, k.wgid, v->local_size[0], v->local_size[1], v->local_size[2]);
   for (unsigned i = 0; i < k.num_bufs; i++)
      printf("buf %u: %s size %u dwords addr_reg %u\n", i, k.buf_types[i] == KERNEL_BUF_UAV ? "UAV" : "UBO", k.buf_sizes[i], k.buf_addr_regs[i]);
   for (unsigned i = 0; i < v->info.sizedwords; i += 2)
      printf("%08x_%08x\n", v->bin[i + 1], v->bin[i]);
   return 0;
}

/* NIR data symbols the ir3 library references but the assembler never uses (libnir is a stub in this tools-only build). */
uint32_t nir_debug;
bool nir_debug_print_shader[MESA_SHADER_KERNEL + 1];
