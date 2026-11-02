# GPU Adreno 618 (Poco X3 NFC, surya): hardware, stato misurato, piano

Legenda: **[V]** verificato (file/riga, patch o misura sul telefono) · **[I]** inferenza · **[?]** da verificare.
Questo documento è la base: piani dettagliati e stato vanno aggiunti nelle sezioni 3 e 7.
Documenti gemelli: `docs/audio.md`, `docs/wifi.md`, `docs/bluetooth.md`, `docs/cameras.md`.
Riferimento diretto per il kernel: `docs/GPU_A630_BRINGUP.md` (Poco F1, Adreno 630), `docs/GPU_KERNEL.md`,
`docs/GPU_RESEARCH.md`, `docs/METAL_ADRENO630.md`, `docs/GPU_EAGL.md`.

## 1. Hardware sul Poco X3 NFC (surya)

Tutti i valori sono dal FDT vivo (ABL Android) in `dtb/surya/live.dts`, salvo diversa indicazione.

| Voce | Valore | Fonte |
|---|---|---|
| SoC | Snapdragon 732G, `qcom,sdmmagpie` (SM7150) | live.dts:7-8 [V] |
| GPU | Adreno 618, `qcom,chipid = <0x06010800>`, nodo `kgsl-3d0@5000000` | live.dts:18464 [V] |
| Registri GPU | `kgsl_3d0_reg_memory` 0x5000000 (0x40000), `cx_dbgc` 0x5061000, `cx_misc` 0x509e000, `qfprom` 0x780000 (speed bin) | live.dts:18464 [V] |
| IRQ GPU | SPI 0x12c (300) | live.dts [V] |
| GMU | `qcom,gmu@506a000` (0x31000), PDC cfg 0xb290000 (0x10000), PDC seq 0xb490000 (0x10000), IRQ HFI SPI 0x130, GMU SPI 0x131 | live.dts:18937 [V] |
| Clock controller GPU | `qcom,gpucc-sdmmagpie` 0x5090000 (0x9000) | live.dts:~649 [V] |
| Quirk kgsl | `qcom,gpu-quirk-hfi-use-reg`, **`qcom,gpu-quirk-secvid-set-once`**, `qcom,no-nap`, `qcom,ubwc-mode = 2`, `qcom,highest-bank-bit = 0x0e`, `qcom,min-access-length = 0x20` | live.dts:18464 [V] |
| SMMU GPU | `arm,smmu-kgsl@5040000`; context bank `gfx3d_user` (stream 0x00, `qcom,gpu-offset = 0x48000`), `gfx3d_secure` (stream 0x02); GMU: `gmu_user` (stream 0x04) | live.dts:13841, 18922 [V] |
| Zap shader | `a615_zap` (mdt + b00..b02), PAS id **13** | NOTES.md 2026-11-01 [V] |
| Firmware (solo nomi/dimensioni) | `a618_gmu.bin` 32768 B, `a630_sqe.fw` 32136 B (nome Linux, usato anche dall'A618), `a615_zap.mdt` 6860 B, `.b00` 148 B, `.b01` 6712 B, `.b02` 1984 B | `research/surya-fw/` (git-ignored) [V] |
| Origine firmware | `/vendor/firmware` del telefono, letti in sola lettura da OrangeFox. Il firmware è Qualcomm e **non va in git** | NOTES.md [V] |
| Scrittura firmware nell'ADT | nodo `/sweet-gpu` (`tools/adt-gpufw.py`): `gmu-firmware`, `sqe-firmware`, `zap-mdt`, `zap-b00..b02` | tools/adt-gpufw.py [V] |
| Memoria riservata zap | `pil_gpu_mem`: 0x97715000, 0x2000 (no-map) | live.dts:20534 [V] |

Differenza chiave con il Poco F1 (Adreno 630): l'A618 usa **GBIF** (non VBIF), ha un GMU con quirk `hfi-use-reg`, e la sua
tabella dei livelli di potenza ha 8 livelli fino a 800 MHz [V, HFI]. La GMEM e le tabelle UBWC sono diverse [I].

## 2. Cosa sappiamo (verificato sul telefono, avvio dalla RAM)

Codice: `bsd/dev/sweet_gpu.c`, `sweet_scm.c`, `sweet_rpmh.h` in `xnu/patches-11215/0301-surya-gpu-a618-scm-zap.patch`
(e `xnu/patches/0224`/`0225` per xnu-8796). Si attiva solo se `sweet_board_surya()`. Boot-arg `sweetgpu=N`
(stadio), `sweetscm=N` (SCM/PAS), `sweetgpudelay=N`. Origine: fork `mxtpasontop/ios-sweet` ramo `gpu` (patch 0078-0098),
**senza licenza** (vedi sezione 4).

| Stadio | Cosa | Esito misurato |
|---|---|---|
| ABL | GPU spenta, GDSC CX/GX collassati, CBCR spenti tranne `cxo_aon` | [V] NOTES 2026-11-01 |
| 2 | CX GDSC acceso (gdscr 0xf8228000), RCG GMU a 200 MHz (cfg 0x602), clock ahb/cxo/cx_gmu/memnoc/ddrss_gpu_axi; registri GMU leggibili (CM3_SYSRESET = 1); SMMU GPU: 5 SMR / 5 CB tutti invalidi (IDR0 0x48016605); ITCM/DTCM scrittura+lettura ok | OK [V] |
| 4 | Boot del firmware GMU: **0xBABEFACE in 55 µs**; HFI: tutti gli ACK; fw GMU 1.17; 8 livelli di potenza fino a 800 MHz | OK [V] |
| 6 | GX acceso, `RBBM_STATUS = 0x1`, poi slumber | OK [V] |
| SCM | query SCM (versione, `is_call_available` ecc.) | OK [V] |
| `sweetscm=4` + `sweetgpu=7` | voti RPMh BCM per il crypto (CE0, SN11, SN0, SH0, MC0, ACV) e **zap `a615_zap` firmato, autenticato da TrustZone**: INIT_IMAGE, MEM_SETUP, AUTH_AND_RESET, SHUTDOWN tutti con status 0 | OK [V] |
| 9 (`sweetgpu=9`, `sweetscm=4`) | memoria CP: ring VA 0x40000000 (PA 0x104e18000, 32 KiB), SQE VA 0x40010000 read-only (32132 B, versione 0x16ee19a, patch 0x1190001), scratch VA 0x40020000; SMMU CB1 (ASID 2, SID 0, SMR2 0x80000000, S2CR 0x301) con tabelle percorse via software (20 pagine + 6 guardie non mappate) verificate; GX acceso a 180 MHz, SPTPRAC acceso, HFI ACK, OOB gpu ack, `RBBM_STATUS 0x1`; AHB fence ALLOW ok (ctrl 0); clock SP 0x8ff1 ok | OK fino a qui [V] |
| 9, prima scrittura al CP | **`CP reg[0x8c2] wrote=0x10000c0 read=0` → "CP cold configuration readback mismatch"**; `CP_STATUS 0x1000000` | **FALLISCE** [V] (stesso muro della patch 0098 del fork) |

Il log completo: `logs/surya-gpu-9.txt`, immagine `research/surya-out/enxerto-boot-scm9.img` (git-ignored).
Il registro `0x8c2` è `CP_ROQ_THRESHOLDS_2` [V: stesso offset nell'A630, 0103].

### 2.1 Il confronto con l'A630 che funziona (Poco F1)

Sul Poco F1 la stessa famiglia a6xx arriva a "CP NOP test PASSED" (`NOTES.md` 2026-10-23, `docs/GPU_A630_BRINGUP.md`, patch
`0103-beryllium-gpu-a630-bringup`). Ordine dello **stadio 7 dell'A630** (`gpu_cp_hw_init` + `gpu_cp_start` + `gpu_gx_test`) [V, 0103]:

1. OOB `gpu` (GX acceso), AHB fence ALLOW, clock SP.
2. `hw_init`, in ordine: `*_ADDR_MODE_CNTL = 1` per CP, VSC, GRAS, RB, PC, HLSQ, VFD, VPC, UCHE, SP, TPL1 (indirizzi a 64 bit);
   `RBBM_VBIF_CLIENT_QOS_CNTL = 3`; `VBIF_GATE_OFF_WRREQ_EN = 9` (solo a630); `RBBM_PERFCTR_GPU_BUSY_MASKED`;
   UCHE (write range, trap base, write-thru base, GMEM range, `FILTER_CNTL 0x804`, `CACHE_WAYS 4`);
   **`CP_ROQ_THRESHOLDS_2 = 0x010000c0`**, `ROQ_THRESHOLDS_1`, `MEM_POOL_SIZE`, `PC_DBG_ECO_CNTL`, `CP_AHB_CNTL`,
   `RBBM_INTERFACE_HANG_INT_CNTL`, `UCHE_CLIENT_PF`; poi base del ring/SQE.
3. `CP_SQE_CNTL = 1` (mai altri valori a freddo), `CP_ME_INIT` (8 dword), attesa ring idle.
4. **Solo dopo**: caricamento dello zap (PAS), poi `CP_SET_SECURE_MODE 0` dal ring.
5. Test: NOP + `CP_MEM_WRITE` + `CP_EVENT_WRITE CACHE_FLUSH_TS`; lettura dalla memoria condivisa.

Il nostro stadio 9 sull'A618 (da `mxtpasontop/ios-sweet`): scrive per **primo** `CP_ROQ_THRESHOLDS_2` (e poche altre CP), senza `ADDR_MODE_CNTL`/UCHE/QoS prima, **e carica lo zap prima del CP** («zap before CP»). Il documento dell'A630 dice che quel fork non è mai
uscito da quello stato (`docs/GPU_A630_BRINGUP.md`, rischio 6, e riga «Stage 7 order follows Linux»).

### 2.2 Ipotesi sul blocco (la scrittura non resta) [I]

| Ipotesi | Perché è plausibile | Cosa la distingue |
|---|---|---|
| H1: la GPU esce dal reset in **modalità sicura** e il CP ignora la configurazione finché lo zap non la scatta | stesso ordine di Linux; il DTB ha `gpu-quirk-secvid-set-once` | la sonda (vedi 2.3); lettura di `RBBM_SECVID_TRUST_CNTL` (rischiosa) |
| H2: ordine delle scritture: mancano `ADDR_MODE_CNTL`, QoS/GBIF, UCHE prima del CP | l'A630 li scrive per primi | la sonda scrive proprio quelli |
| H3: gating dei registri del blocco CP (clock HWCG/SP, AHB fence per range) | fence a range, clock SP appena acceso | `STICKS` su blocchi non-CP, `IGNORED` solo CP |
| H4: GBIF non sbloccato (l'A618 ha GBIF, non VBIF) | differenza A618/A630 | scrittura `RBBM_GBIF_CLIENT_QOS_CNTL` (0x3da) nella sonda |
| H5: scritture da CPU in finestra non-coerente/cache (device vs normal) | gli altri registri GPU si leggono bene | scrittura+lettura su un registro "innocuo" |

### 2.3 La sonda di scrittura (commit `49acab0`, patch 0301)

Prima della tabella dei registri CP, `gpu_cp_configure_cold()` scrive 13 registri **non-CP** dell'A630 `hw_init` (stessi valori
Linux), rilegge ognuno e stampa `STICKS` o `IGNORED`, **senza fermarsi**, e a fine giro il conteggio e lo stato della fence AHB.
Non avvia il CP e non tocca i registri sicuri (SECVID). Registri: `CP_ADDR_MODE_CNTL` 0x842, `VSC_ADDR_MODE_CNTL` 0xc01,
`GRAS_ADDR_MODE_CNTL` 0x8601, `RB_ADDR_MODE_CNTL` 0x8e05, `UCHE_ADDR_MODE_CNTL` 0xe00, `RBBM_CLIENT_QOS_CNTL` 0x3da,
`RBBM_PERFCTR_GPU_BUSY_MASKED` 0x50b, `UCHE_WRITE_THRU_BASE` 0xe07, `UCHE_GMEM_RANGE_MIN` 0xe0b, `UCHE_FILTER_CNTL` 0xe18,
`UCHE_CACHE_WAYS` 0xe17, `PC_DBG_ECO_CNTL` 0x9e00, `RBBM_INTERFACE_HANG_INT_CNTL` 0x1f [V, patch].

| Esito | Significato | Passo successivo |
|---|---|---|
| tutti `IGNORED` | l'intero spazio registri GPU ignora le scritture: problema a monte (fence AHB, stato sicuro, blocco da accendere) | rivedere GMU/OOB/fence; confrontare con `a6xx_gmu` di Linux; H1/H3 |
| solo i registri CP `IGNORED` (gli altri `STICKS`) | solo il CP è bloccato: stato sicuro del CP o ordine di avvio SQE | riordinare come l'A630 (SQE_CNTL → ME_INIT → zap → `SET_SECURE_MODE 0`); H1 |
| quasi tutti `STICKS` | il guasto era nella tabella iniziale: manca la preparazione `hw_init` | portare tutta `gpu_cp_hw_init` dell'A630 con i valori A618 |
| esito misto per blocco | gating per blocco (clock HWCG, SP) | tabella HWCG e clock per blocco (non ancora portata) |

Stato: patch scritta e pushata, CI in corso (run 41364980555 al momento della stesura). Test sul telefono: `[?]` da fare.

## 3. Ipotesi di implementazione, a fasi

| Fase | Obiettivo | Come | Ostacolo principale |
|---|---|---|---|
| G0 | Sbloccare il CP | esito della sonda → riordino stadio 9 secondo A630 (`gpu_cp_hw_init`, `SQE_CNTL=1`, `ME_INIT`, zap, `SET_SECURE_MODE 0`) | modalità sicura; rischio di reset |
| G1 | Primo comando eseguito | NOP + `CP_MEM_WRITE` + `CACHE_FLUSH_TS` letto da CPU (test NOP) | SMMU CB1, cache del ring |
| G2 | Client IOKit nel kernel | adattare la patch **0182** (`beryllium-gpu-adreno-iokit-user-client`) e l'ABI `xnu/gpu-abi/sweet_adreno_abi.h` all'A618: GEM, submit, fence, mappatura | tabelle SMMU per processo, sandbox (classe user client ammessa) |
| G3 | Import IOSurface | patch 0187 (`gpu-adreno-iosurface-import`), 0214/0217 (alias scrivibile), 0215 | memoria client, coerenza cache |
| G4 | Userland: Vulkan/GL | `gpu/freedreno-sweet/` (backend DRM per Turnip/freedreno con sweet_drm.c), Mesa con A618 (chip 0x06010800 è una a6xx supportata) [I] | tabelle chip A618 nel backend, UBWC/GMEM |
| G5 | Metal su Vulkan | `gpu/metal/` = **Chlorophyll** (Metal→Vulkan su Turnip, AIR→NIR) caricata da `gpu/eagl` (shim OpenGL ES) e dall'inject su backboardd/SpringBoard | AIR/Metal incompleto, avvio lento, memoria, jetsam |
| G6 | CoreAnimation compone con la GPU | iOS 18: `gpu/eagl` (EAGL su Mesa); iOS 26: Chlorophyll su Turnip (milestone del F1) | glifi/icone (`IOSurface` multipiano), 30 Hz del pannello |
| G7 | DCVS, potenza, recupero | patch 0186/0188/0189 (metrics, governor, DCVS OOB) | recovery dopo hang |

Nota di ordine: iOS 16 (kernel xnu-8796) ha solo la parte kernel; il lato Metal/GL è documentato per iOS 18/26 (`docs/METAL_ADRENO630.md`).
Un driver Metal per iOS 16 sarebbe nuovo (dimensione grande) [I].

## 4. Ostacoli e rischi

1. **Reset del SoC senza panic** per accessi a blocchi spenti, SMR/SMMU sbagliati, o violazioni XPU (registri sicuri `RBBM_SECVID_*`). La
   sonda evita i SECVID. Le fasi G0-G1 sono le più pericolose [V: `docs/GPU_A630_BRINGUP.md` rischi 1-2].
2. **Prime letture dei registri GPU/GMU.** Nota della community (TigerShota/ios-sweet, Redmi/Poco "gauguin" SM6350, Adreno 619, MIT): **la prima lettura di registri del core GPU/GMU bloccava un core CPU**, e la loro patch 0085 le ha tolte (`NOTES.md` 2026-11-02). Sul nostro A618 le letture dopo GX acceso funzionano (stadi 2-6) [V]; resta la regola: leggere solo con GX/CX acceso, clock attivi, dentro l'OOB.
3. **Modalità sicura / zap.** Un AUTH_AND_RESET con zap sbagliato o carve-out errato può bloccare la GPU fino al reset. Lo zap è firmato per dispositivo [V].
4. **Licenza del codice del fork A618** (`mxtpasontop/ios-sweet`, ramo `gpu`, patch 0078-0098): nessun file LICENSE. Attribuzione tenuta nei soggetti delle patch e nel README. Decisione dell'utente (tenere/rimuovere nel repo pubblico) **ancora aperta** [?].
5. **Dipendenza dal firmware Qualcomm** (non redistribuibile): resta fuori da git, letto dal telefono.
6. **iOS non ha Metal "vero" qui.** Vedi `docs/METAL_ADRENO630.md` (a): non registrare un device Metal incompleto; il percorso consigliato per la composizione è GLES (EAGL su Mesa) o un Metal completo.
7. **Carico CPU.** Con la GPU spenta il rendering è su CPU: i widget e `spotlightknowledged` saturano i core per minuti dopo il boot [V, avvio iOS 18].
8. **Cicli di compilazione.** Ogni modifica kernel = CI (~15 min) + riavvio dalla RAM.

## 5. Lavoro della community e riferimenti

* **ry-diffusion/Bergamota (Poco F1, Adreno 630):** CP che esegue comandi (`NOTES.md` 2026-10-23 «ADRENO 630 RUNS COMMANDS»); patch 0103, 0170, 0180-0189, 0212-0217, 0229-0251 in `xnu/patches-11215`; Chlorophyll su Turnip su iOS 26; EAGL su Mesa su iOS 18. Già nel nostro branch.
* **mxtpasontop/ios-sweet, ramo `gpu` (SM7150/A618):** base del nostro `sweet_gpu.c`; si ferma prima di eseguire comandi sul CP (senza licenza).
* **TigerShota/ios-sweet (gauguin, A619):** vedi rischio 2; i loro 0084/0085 non sono integrati.
* **oosexgang/hakios-metal** (MIT, altro progetto, Adreno 7c+ Gen3): driver Metal in C++ senza Mesa. **Solo riferimento, non copiare**; annotato in NOTES.md.
* **UniXNU** (BSD-2-Clause-Patent): architettura di riferimento citata in `docs/GPU_RESEARCH.md`.
* **Mesa (freedreno/turnip), Linux `a6xx`/`msm`:** ordine di `a6xx_hw_init`, quirk A618 (`a618`), GBIF.
* **Lavoro altrui su Poco X3 (surya):** `arbuxiktech/iOS-on-surya` è fermo e senza lavoro proprio; nessun A618 funzionante noto [V, censimento del 2026-11-02].

### 5.1 Riferimento: Snapintosh (macOS 27 su Snapdragon X2 Elite, Adreno X2-90)

Segnalato dall'utente il 2026-11-02. Progetto di terzi, non un fork di Bergamota: **riferimento di lettura, nessun codice copiato** finché non decidiamo cosa riusare e con quale attribuzione. Censimento fatto con `gh api` in sola lettura [V: README e licenze dei repo].

| Componente | Cosa fa | Licenza | Rilevanza per noi |
|---|---|---|---|
| `Snapintosh/SnapMetal` (`AdrenoMetal.bundle`) | Driver Metal come plug-in: device, command buffer, encoder, comandi PM4 Adreno scritti a mano, parla con la kext via IOGPU. Include harness che esegue il plug-in contro il Metal del Mac host per confronto | MIT [V] | Alta come riferimento di architettura: Metal→Adreno senza Mesa, come `hakios-metal`, ma su una GPU Qualcomm e su un sistema Apple vero |
| `Snapintosh/air2ir3` | Compilatore AIR (IR di Metal) → ir3 (IR di Mesa per Adreno) | MIT [V] | Alta: stessa funzione del nostro `libbgair` (AIR→NIR→ir3) in Chlorophyll; utile per confrontare copertura delle istruzioni AIR e tempi di traduzione |
| `Snapintosh/SnapAdreno` (AdrenoX2 + AdrenoDisplay) | Kext IOGPU e display per l'Adreno X2-90 | MIT [V] | Media: mostra come si espone una GPU Adreno all'IOGPU/Metal di Apple, ma su famiglia a8xx |
| `Snapintosh/Snapintosh` (ombrello) | Monitor EL2 (SPTM/TXM, tabelle a 16 KiB), tool, docs | MIT con componenti GPL-2.0 (`NOASSERTION` su GitHub) [V] | Bassa: l'hypervisor non ci serve |

**Perché è pertinente [I].** Apple riusa il modello Metal/IOGPU tra macOS e iOS, quindi un driver Metal per Adreno scritto per macOS è concettualmente vicino a quello che ci serve su iOS (oggi coperto da Chlorophyll = Metal su Vulkan/Turnip, `gpu/metal`, `gpu/freedreno-sweet`).

**Limiti [V/I].**
* GPU di generazione diversa: l'X2-90 è a8xx, la nostra A618 è a6xx. Registri, tabelle e parte dei pacchetti PM4 cambiano; ir3 e le basi di PM4 sono comuni [I].
* È pensato per macOS 27: il caricamento del bundle e il servizio IOKit non sono quelli di iOS, dove oggi iniettiamo il driver con `bgi.dylib` [I].
* Nessun esito per l'audio, la camera e il Bluetooth: il loro README li dà "non supportati" [V].
* Nessuna prova che giri sulla nostra GPU: il blocco di base resta far eseguire un comando al CP dell'A618 (sezione 2).

**Uso previsto, a fasi.**
1. Leggere SnapMetal e air2ir3 per confrontare: come costruiscono i command buffer PM4, come mappano le risorse, copertura AIR, caching dei shader.
2. Se il nostro traduttore AIR dà problemi di copertura o di lentezza (vedi le note sui 40 s al primo frame in `NOTES.md`), valutare `air2ir3` come alternativa o come fonte di patch, con attribuzione MIT.
3. Solo dopo che l'A618 esegue un comando e il client IOKit funziona (fasi 2-3 della sezione 3): valutare di riusare parti di SnapMetal per un percorso Metal diretto, senza Vulkan.

## 6. Domande aperte / prossimi passi

1. Esito della sonda sul telefono (`sweetgpu=9 sweetscm=4`): quale riga della matrice 2.3?
2. `RBBM_SECVID_TRUST_CNTL` si può leggere in sicurezza? Il quirk `secvid-set-once` del DTB dice che kgsl lo imposta una volta sola [?].
3. Serve la tabella HWCG per far attaccare i registri di alcuni blocchi? [?]
4. GBIF: quali registri vanno impostati prima di `CP_ME_INIT` sull'A618 [?]
5. Licenza: tenere o rimuovere il codice derivato da `mxtpasontop/ios-sweet`? (decisione utente).
6. Un secondo stadio: provare il riordino A630 completo (G0) subito dopo la sonda, nella stessa immagine.

## 7. Stato

| Fase | Stato | Data |
|---|---|---|
| Stadi 2, 4, 6 (clock, GMU, HFI, GX) | OK su hardware | 2026-11-01 |
| SCM + zap firmato (sweetscm=4) | OK su hardware | 2026-11-01 |
| Stadio 9, prima scrittura CP | **FALLISCE** (`0x8c2` read 0) | 2026-11-01 |
| Sonda di scrittura (13 registri non-CP) | scritta, CI in corso, non ancora provata | 2026-11-02 |
| G0 riordino stadio 9 | da fare | |
| G1 NOP | da fare | |
| G2-G7 | da fare | |
