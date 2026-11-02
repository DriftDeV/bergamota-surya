# Poco X3 NFC (surya): nucleo GPU del kernel e API interna `sweet_a618_*` (Adreno 618)

Stato 2026-11-02: **scritto, compilato solo a livello di sintassi su stub (clang -fsyntax-only), mai eseguito su hardware.**
Fase 1 della controparte surya di `GPU_KERNEL.md` (Poco F1, Adreno 630). Codice: `xnu/patches-11215/0301-surya-gpu-a618-scm-zap.patch`
(file `bsd/dev/sweet_gpu.c`, sezione "sweetgpu=11: permanent GPU core and the kernel-internal API").

Tag delle evidenze: **[M]** misurato su hardware (NOTES.md, `SURYA_STATUS.md`), **[V]** letto nelle fonti, **[I]** inferenza,
**[U]** non testato su hardware.

## 1. Cosa e' e come si attiva

Boot-arg **`sweetgpu=11`** (default: spento). Esegue tutti gli stadi di `sweetgpu=10` (GMU, HFI, GX acceso, zap firmato
autenticato da TrustZone, SQE avviato, `CP_ME_INIT`, `CP_SET_SECURE_MODE 0`) e poi **lascia la GPU accesa per sempre**:
niente thread di spegnimento, GX tenuto da `OOB_GPU`, zap lasciato in esecuzione (`sweet_scm_zap_keep` forzato a 1), SQE in
esecuzione, anello inattivo. Su questo stato espone l'API kernel-interna `sweet_a618_*` (13 funzioni, firme identiche a quelle
`sweet_gpu_*` del Poco F1) e lancia un thread di **autotest in-kernel** (senza IOKit).

* Senza `sweetgpu=11` il codice nuovo non gira: gli stadi `sweetgpu=1..10` e `sweetgpukeep=1` si comportano come prima [I: le uniche
  modifiche ai percorsi esistenti sono il parametro `perm` di `gpu_gx_test` e il ramo `mode >= 11` dopo `gpu_kept`].
* `sweetgpukeep` non serve con `sweetgpu=11` (la modalita' permanente lo include).
* Solo surya (`sweet_board_surya()`); il Poco F1 vive nello stesso binario ma con i simboli `sweet_gpu_*` di `sweet_a630.c`.
* Serializzazione: **a carico del chiamante** (un solo `IOLock` nello strato IOKit). Il nucleo non prende lock.
* Nessun interrupt: si fa solo polling del registro/parola di fence.

## 2. Flusso di boot con `sweetgpu=11`

1. `sweet_gpu_run` -> stadi 1..10 invariati; `gpu_gx_test(..., perm=true)` tiene OOB_GPU e non spegne lo zap.
2. `a618_core_start`: mappa GPU e SMMU, verifica che il contesto SMMU CB1 sia intatto (`gpu_cp_context_intact`), alloca il pool
   delle tabelle di pagina (1028 pagine da 4 KiB = 4 MiB, WCOMB; se fallisce ripiega su 260 pagine = 512 MiB di spazio), azzera la
   parola di fence, inizializza la free list (`0x80000000..0x100000000`), imposta `ready`.
3. Parte il thread di autotest. Se **PASSED**: scatta una sola volta l'hook di pronto (sezione 4). Se **FAILED**: `ready` torna
   falso, l'hook non scatta, la GPU resta accesa fino al reset.

Stato di partenza identico a quello **gia' provato** con `sweetgpukeep=1` [M]: `OOB_BOOT_SLUMBER` resta impostato e il registro
`GMU_PWR_COL_KEEPALIVE` **non** viene scritto (il Poco F1 fa entrambe le cose; qui non sono state replicate perche' non
verificate su a618, vedi rischi).

## 3. API (firme congelate)

```c
bool     sweet_a618_ready(void);
bool     sweet_a618_param(uint32_t which, uint64_t *value);
bool     sweet_a618_iova_alloc(uint64_t size, uint64_t *iova, uint64_t *taken);
bool     sweet_a618_iova_reserve(uint64_t iova, uint64_t size);
void     sweet_a618_iova_free(uint64_t iova, uint64_t size);
bool     sweet_a618_vm_map(uint64_t iova, uint64_t pa, bool readonly);
void     sweet_a618_vm_unmap(uint64_t iova, uint64_t size);
void     sweet_a618_cache_sync(void *va, size_t len);
uint32_t sweet_a618_submit(const uint64_t *ib_iova, const uint32_t *ib_dwords, unsigned count);
bool     sweet_a618_fence_done(uint32_t fence);
uint32_t sweet_a618_fence_last(void);
void     sweet_a618_mark_lost(void);
void     sweet_a618_report(const char *why);

extern void (*sweet_a618_ready_hook)(void);
void     sweet_a618_set_ready_hook(void (*hook)(void));   /* extra: setter senza corsa, vedi sezione 4 */
```

| Funzione | Semantica |
|---|---|
| `ready` | `true` se il nucleo e' inizializzato e nessuna sottomissione e' fallita. Non usare le altre funzioni prima. Diventa vera appena il pool e' pronto, **prima** della fine dell'autotest: i client aspettano l'hook (sezione 4), non fanno polling su `ready`. |
| `param` | Numeri `MSM_PARAM_*` (tabella in sezione 6). `false` se non pronto o parametro sconosciuto. |
| `iova_alloc` | First-fit nella finestra client `[0x80000000, 0x100000000)`. `size` multiplo di 4 KiB, non zero. Prende `size` + **una pagina di guardia non mappata**; `*iova` e' l'indirizzo, `*taken` = `size + 0x1000` e va restituito a `iova_free`. |
| `iova_reserve` | Toglie esattamente `[iova, iova+size)` dalla free list (client che sceglie il proprio indirizzo, `MSM_INFO_SET_IOVA`). |
| `iova_free` | Restituisce un intervallo (`iova`, `taken`), fonde con i vicini. Lista piena (4096 intervalli): l'intervallo si perde (spazio di indirizzi perso, mai corrotto). Nessun controllo di doppia liberazione. |
| `vm_map` | **Una pagina GPU da 4 KiB** (`iova` e `pa` allineati a 4 KiB, `pa < 64 GiB`); `readonly` imposta AP[2]. `false` se il pool di tabelle e' esaurito. Attributo pagina: normale non-cacheable (attr 0 del MAIR). Nessuna invalidazione TLB qui. |
| `vm_unmap` | Azzera le PTE di `[iova, iova+size)` e fa TLBIALL + TLBSYNC del contesto CB1. |
| `cache_sync` | `dc civac` per VA (`sweet_dma_sync_va`): da chiamare dopo che la CPU ha scritto dati che la GPU legge, e prima di leggere dati scritti dalla GPU. |
| `submit` | Vedi sezione 5. Ritorna il numero di sequenza (fence), **0 se rifiutato**. |
| `fence_done` | `true` se la parola di fence in scratch (letta dopo `dc civac`) e' >= `fence` (confronto con segno a 32 bit, tollera il wrap). |
| `fence_last` | Ultimo numero di sequenza sottomesso. |
| `mark_lost` | Segna la GPU persa (una volta sola), conta un hang e stampa `report`. Dopo: `ready()` e' falso, `submit` ritorna 0. Nessun recupero. |
| `report` | Stampa a log (con `sweet_log_sync`) sequenze, RPTR/WPTR, stato CP/RBBM (solo se GX e' acceso e il fence AHB del GMU e' aperto: altrimenti le letture bloccherebbero il bus) e FSR/FAR di CB1. |

Tutte le BO dei client devono essere **write-combined e residenti**: la memoria la alloca e ne imposta gli attributi
(`VM_WIMG_WCOMB`) lo strato IOKit; qui arrivano solo indirizzi fisici. Il page walker dell'SMMU legge le tabelle senza coerenza.

### Spazio di indirizzi e tabelle (4 KiB!)

* Contesto SMMU CB1, ASID 2, SMR 2 (gia' in uso per anello/SQE/scratch) [M]. `TCR` con T0SZ 32: la tabella L1 statica di
  `gpu_cp_memory_setup` ha 4 voci; le voci 2 e 3 (`0x80000000..0xffffffff`) vengono create qui.
* L2/L3 prese on-demand da un pool WCOMB (nessuna tabella viene mai restituita). Una PTE per pagina GPU da 4 KiB: **mai
  `PAGE_SIZE`** (16 KiB in xnu), lezione della patch 0185 del Poco F1.
* Indirizzi fissi gia' usati dal CP e fuori dalla finestra client: anello `0x40000000` (32 KiB), SQE `0x40010000` (sola lettura),
  scratch `0x40020000` (0x4000 byte). La parola di fence e' `0x40020200` (scratch + `0x200`).

## 4. Hook di pronto

`void (*sweet_a618_ready_hook)(void)` e' `NULL` di default e viene chiamato **una sola volta**, dal thread di autotest, quando
l'autotest e' **PASSED** (cioe' il nucleo e' davvero usabile e nessun altro lo sta usando). Fase 2 pubblica il servizio IOKit li'.

Poiche' il nucleo parte in un thread del boot e IOKit puo' arrivare dopo, assegnare direttamente la variabile puo' essere troppo
tardi. **Consigliato:** `sweet_a618_set_ready_hook(fn)`: imposta il puntatore e, se l'autotest e' gia' passato, chiama `fn`
subito nel contesto del chiamante; altrimenti verra' chiamata dal thread di autotest. Senza lock (builtin `__atomic`, schema
tipo Dekker): `fn` parte esattamente una volta in entrambi gli ordini. Il hook e' eseguito in contesto thread kernel, non in
interrupt, e puo' bloccarsi.

Fino al hook **nessun client deve chiamare l'API**: l'autotest usa lo stesso anello e lo stesso pool senza lock.

## 5. Sottomissione e fence

`sweet_a618_submit(ib_iova[], ib_dwords[], count)` con `1 <= count <= 128`:

1. Rifiuta (ritorna 0, nessun effetto) se: non pronto/perso, argomenti nulli, un IB non allineato a 4 byte, fuori dalla finestra
   client (mai anello/SQE/scratch), con `dwords` nullo o >= 2^20.
2. Rifiuta con `report` se GX e' spento. Se e' presente uno **SMMU fault** su CB1 (FSR & `0xc00001fe`): log, FSR
   azzerato, GPU dichiarata persa, ritorna 0. I bit di stato residui (0x400 dal boot) vengono solo azzerati.
3. Se il fence AHB del GMU non e' ALLOW lo riporta a 0 (come il F1).
4. Attende spazio nell'anello (letto da `CP_RB_RPTR`; `NO_UPDATE` e' attivo, niente shadow) fino a 2 s; senza spazio:
   `mark_lost`, ritorna 0. Servono `16 + 4*count` dword.
5. **TLBIALL + sync di CB1** (perche' `vm_map` non invalida: cosi' le mappe appena create sono sempre visibili al primo submit).
6. Incrementa il numero di sequenza (mai 0), scrive nell'anello: 2 `CP_EVENT_WRITE` (`PC_CCU_INVALIDATE_DEPTH` 24,
   `PC_CCU_INVALIDATE_COLOR` 25), per ogni IB un `CP_INDIRECT_BUFFER` (indirizzo lo/hi, dword), poi `CP_EVENT_WRITE`
   `CACHE_FLUSH_TS | IRQ` che scrive la sequenza in `0x40020200`; flush dell'anello e scrittura di `CP_RB_WPTR`.

Il fence e' il numero di sequenza; `fence_done` lo confronta con la parola in scratch. Il bit IRQ e' impostato come nel test
funzionante ma nessun interrupt e' gestito (polling).

## 6. Parametri (`which` = `MSM_PARAM_*`)

Numeri da `include/uapi/drm/msm_drm.h` (torvalds/linux, righe 74-92) [V].

| `which` | Nome | Valore a618 | Fonte / tag |
|---|---|---|---|
| 1 | GPU_ID | 618 | [M] chip id |
| 2 | GMEM_SIZE | `0x80000` (512 KiB) | specifica fase 1 [V] |
| 3 | CHIP_ID | `0x06010800` | [M] |
| 4 | MAX_FREQ | livello massimo della tabella DT del bin fuse (`gx_khz[ultimo] * 1000`), es. 825 MHz bin 0/0xac; `0` se la tabella DCVS non e' stata costruita | tabella dalla DT del telefono [M], confermata da `sm7150.dtsi` v7.2 (opp 825/800/700/650/610/565/430/355/267/180 MHz) [V]. La GPU resta al livello di boot (180 MHz), niente DCVS |
| 5 | TIMESTAMP | contatore always-on del GMU (19.2 MHz), registri `0x1f888/0x1f889` | [I]: offset dedotto da a6xx_gmu.xml `0x5088` + `0x1a800` come per gli altri registri GMU; non letto prima su surya [U] |
| 6 | GMEM_BASE | `0x100000` | specifica fase 1 |
| 7 | PRIORITIES | 1 | come F1 |
| 8 | PP_PGTABLE | 0 | deprecato |
| 9 | FAULTS | numero di hang/fault dichiarati | |
| 10 | SUSPENDS | 0 | la GPU non va mai in sospensione |
| 14 | VA_START | `0x80000000` | |
| 15 | VA_SIZE | `0x80000000` (fino a `0xfffff000` inclusa l'ultima pagina) | come F1 |
| 16 | HIGHEST_BANK_BIT | 14 | `ubwc_config.c` righe 35-38 e 149 (sc7180 -> `ubwc_2_0_hbb14`) [V] |
| 18 | UBWC_SWIZZLE | 6 | [I] default Linux a6xx, non verificato per sc7180 |
| 19 | MACROTILE_MODE | 0 | [I] |

## 7. Autotest in-kernel

Thread `a618_selftest_thread`, senza IOKit. Alloca 32 KiB contigui WCOMB (due pagine kernel da 16 KiB), li mappa con `iova_alloc`
+ `vm_map` (4 PTE per pagina kernel): pagina "codice" con shader `first_store` (+0), descrittore UAV (+0x400) e command stream
(+0x1000); pagina "dati" con il target (dword 0) e la parola di timestamp (+0x100). Lo stream e' `cs_ib` di
`gpu_cp_dispatch_test` **rilocato a runtime** (nessuna rigenerazione): le costanti `0x40022000` (shader, 2 occorrenze),
`0x40022400` (descrittore, 2) e `0x40020160` (timestamp, 1) vengono sostituite con i nuovi indirizzi, il dword 4 del descrittore
punta al target. Se il numero di occorrenze non torna l'autotest fallisce (stream rigenerato con altri VA).

Fa **2 dispatch** con `sweet_a618_submit` + polling di `sweet_a618_fence_done` (timeout 1 s ciascuno), controlla
`target[0] == 0xcafe0001` e `ts == 0xc0de0001`, che i due fence siano consecutivi, poi `vm_unmap` + `iova_free` e verifica che la
free list sia tornata a un solo intervallo `0x80000000..0x100000000` e `bytes_mapped == 0`.

Le pagine di test restano allocate (`KMA_PERMANENT`, 32 KiB) e mappate WCOMB.

### Righe di log attese (tutte iniziano con `SWEET GPU:`)

```
SWEET GPU: API core: GPU kept up (OOB_GPU held), ring 8192 dwords at RPTR=WPTR=<n>, client VA 0x80000000-0x100000000, 1028 table pages
SWEET GPU: API selftest: starting (2 x 97-dword compute dispatch through the sweet_a618_* API)
SWEET GPU: API selftest: round 1 fence 1 done, target 0xcafe0001 (want 0xcafe0001), ts 0xc0de0001 (want 0xc0de0001)
SWEET GPU: API selftest: round 2 fence 2 done, target 0xcafe0001 (want 0xcafe0001), ts 0xc0de0001 (want 0xc0de0001)
SWEET GPU: API selftest PASSED: 2 dispatches through iova_alloc/vm_map/submit/fence_done, fences 1 2, unmap and free clean
SWEET GPU: API core ready: calling the ready hook        (solo se un hook e' impostato)
```

In caso di errore: `SWEET GPU: API selftest FAILED: <motivo> (fences .. .., target .., ts .., mapped .., free ranges ..)`, preceduta da
`report` (`API selftest: fence timeout`) per timeout e fault. Motivi possibili: no memory, iova_alloc, vm_map, relocation,
submit refused, fence timeout, target/ts wrong, fence numbers, unmap/free.

## 8. Differenze dall'implementazione del Poco F1

* Prefisso `sweet_a618_` (stesso binario). Parola di fence a `0x40020200` (scratch di surya a `0x40020000`).
* GMEM 512 KiB, GPU_ID 618, CHIP_ID `0x06010800`, UBWC hbb 14.
* Accesso ai registri col mapping `ml_io_map` e `gpu_rd/gpu_wr` di 0301 (nessun gate DCVS/sospensione/recupero: **non** ci sono
  `lock/unlock`, `vm_flush`, `check_fault` esportati, ne' governor).
* `vm_map` non invalida il TLB (il F1 esportava `sweet_gpu_vm_flush`): lo fa `submit` prima di scrivere l'anello, e `vm_unmap`.
* `submit` valida gli IB (finestra client, 20 bit di dimensione): scelta di sicurezza, il F1 non lo faceva.
* Pool di tabelle da 1028 pagine come la patch 0237 del F1, con ripiego a 260.
* Hook di pronto al posto della chiamata diretta `SweetAdrenoPublish`, piu' un setter senza corsa.
* Nessun `OOB_BOOT_SLUMBER` clear e nessuna scrittura di `GMU_PWR_COL_KEEPALIVE` (il F1 le fa): mantiene lo stato gia' provato.

## 9. Non verificato / rischi

* **Mai eseguito su hardware.** Tutto il percorso `sweet_a618_*` e' nuovo; solo anello, SQE, IB e dispatch con indirizzi fissi
  sono stati misurati [M]. Rischi principali: (a) la rilocazione dello stream (indirizzi non piu' nello scratch) e shader/descrittore
  in pagine mappate dal pool; (b) i due eventi `PC_CCU_INVALIDATE_*` prima dell'IB, mai eseguiti su a618; (c) la mappa del
  pool WCOMB da 4 MiB (`kmem_alloc_contig` contigua: puo' fallire dopo il boot, ripiego 1 MiB); (d) offset `0x1f888` del timestamp.
* Senza `GMU_PWR_COL_KEEPALIVE` e con `OOB_BOOT_SLUMBER` ancora impostato il GMU non dovrebbe collassare GX (provato 90 s con
  `sweetgpukeep=1` [M]) ma non e' provato a lungo termine; se servisse, la riga da aggiungere e' `GMU_PWR_COL_KEEPALIVE` =
  `0x1f8c3` [I] (offset F1 `0x50c3` + `0x1a800`).
* Niente recupero: dopo `mark_lost` o un fault SMMU il CP resta incastrato; serve reset del telefono.
* `fence_done(0)` e' vero come nel F1 (confronto con segno); `submit` non ritorna mai 0 con successo.
* Nessuna protezione contro doppia `iova_free`/doppia `vm_map` della stessa pagina.
* Dopo un fallimento dell'autotest la GPU resta accesa ma `ready` e' falso.

## 10. Fonti

* Linux `drivers/gpu/drm/msm/adreno/a6xx_gpu.c`, `a6xx_submit` (righe 360-433): sequenza `PC_CCU_INVALIDATE_DEPTH/COLOR`,
  `CP_INDIRECT_BUFFER` con `A5XX_CP_INDIRECT_BUFFER_2_IB_SIZE`, `CACHE_FLUSH_TS`.
  https://github.com/torvalds/linux/blob/master/drivers/gpu/drm/msm/adreno/a6xx_gpu.c
* Linux `include/uapi/drm/msm_drm.h` righe 74-92 (numeri `MSM_PARAM_*`).
  https://github.com/torvalds/linux/blob/master/include/uapi/drm/msm_drm.h
* Linux `drivers/soc/qcom/ubwc_config.c` righe 35-38 (`ubwc_2_0_hbb14`) e 149 (`qcom,sc7180`).
  https://github.com/torvalds/linux/blob/master/drivers/soc/qcom/ubwc_config.c
* `arch/arm64/boot/dts/qcom/sm7150.dtsi` (branch v7.2), `gpu_opp_table` (righe ~2872-2930).
  https://github.com/sm7150-mainline/linux/blob/v7.2/arch/arm64/boot/dts/qcom/sm7150.dtsi
* Repository: `xnu/patches-11215/0181`, `0182`, `0185`, `0186`, `0237` (controparte F1), `docs/GPU_KERNEL.md` sezioni 5-7,
  `docs/SURYA_STATUS.md`, `tools/ir3asm/` (generatore dello stream). Codice proprio; dalle fonti esterne sono usati solo
  numeri di registro e sequenze come fatti.
