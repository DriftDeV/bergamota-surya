# Poco X3 NFC (surya, SM7150): cosa è fatto

Aggiornato il 2026-11-02. **[fatto]** = misurato sul telefono o verificato nel repo; **[ipotesi]** = non provato.
Dettagli cronologici e log: `NOTES.md`. Piano di quanto resta: `docs/SURYA_ROADMAP.md`.

## Hardware e regole
- Poco X3 NFC (`surya`), SM7150, 6 GB, UFS, bootloader sbloccato, A-only. `userdata` = sda16, 104,83 GiB (LUN 0 di 114,9 GiB).
- Regole del progetto (AGENTS.md): nessun trailer di co-autore nei commit; backup delle partizioni in `backup/surya/` intatti; dump di dispositivo, seriali e artefatti Apple fuori da git; ogni scrittura distruttiva con OK esplicito dell'utente.
- Scritture fatte finora sul telefono (tutte con OK esplicito): `userdata` (iOS 16 il 2026-11-01, iOS 18 il 2026-11-01 e di nuovo il 2026-11-02) e `boot` (Enxerto, il 2026-11-02). Nessuna GPT modificata. I test dei kernel sono sempre `fastboot boot` da RAM.

## Bootloader e avvio
- **[fatto] Enxerto per surya:** `BOARD_SURYA` in `enxerto/board.h`, IMEM restart_reason `0x146aa65c`, fastboot di Bergamota (`bergamota0001`) funzionante, scansione GPT, lettura UFS, schermata caricatore.
- **[fatto] Enxerto nella partizione `boot`** (2026-11-02, immagine `research/surya-out/enxerto-boot-partition-surya.img`, cmdline `bergamota.fastboot=1 bergamota.charge=0`, verificata a hash). Backup Android in `backup/surya/boot.img` (checksum verificato). Un reset o crash porta in Bergamota fastboot da solo; Android non parte più da `boot`.
- **[fatto] Ciclo di test automatico:** `tools/surya-test.sh IMG NOME SECONDI` aspetta qualsiasi fastboot, avvia la cattura UDP del log, fa `fastboot boot`, attende `en6`. `fastboot -s bergamota0001 boot IMG` avvia le immagini di test (prima volta su surya il 2026-11-02).
- **[fatto] `enxerto/build.sh CHARGE=0`** (`bergamota.charge=0`): la schermata caricatore non prende più il sopravvento quando ABL aggiunge `androidboot.mode=charger`.
- Power + Volume Giù porta ancora al fastboot di Xiaomi.

## Sistema operativo
- **[fatto] iOS 16.7.16** avviato e usabile (2026-11-01): touch, USB ethernet, SSH.
- **[fatto] iOS 18.7.8** (radice creata con `RENEGADE_TARGET=ios18 tools/renegade/renegade root`, scritta in `userdata`): schermata di blocco, home con icone e widget, tocco, tasti laterali, rendering con CPU. Kernel xnu-11215 dalla CI (patch 0300 touch Novatek, 0301 GPU, 0312, 0315 log USB). Boot circa 35 s fino al ping; SSH dropbear a chiave dopo 2-3 minuti.
- **[fatto]** Patch 0310 e 0311 (ABI mach_ports di iOS 16) e le dipendenti 0313/0314/0316/0317 spostate in `xnu/optional-ios16/` perché rompevano iOS 18.
- **[fatto] Display:** ADT 414x896 a 2x (`surya-xr-2x-touch-gpu.adt`). **Resta** la barra di stato rossa `rdar:45025538` (UIKit non trova un provider per quella dimensione).
- 2026-11-02: la radice iOS 18 aveva corruzione HFS (causa probabile: reset bruschi con radice montata in scrittura; ipotesi dell'utente: Android che riscrive `userdata`); riscritta e verificata.

## Compilazione del kernel
- **[fatto]** CI GitHub `xnu.yml` (dispatch manuale, `-f macos_version=15.3 -f machine_config=SWEET`). Il pool macOS arm64 spesso non ha capacità: **parametro `runner`** aggiunto; `-f runner=macos-15-intel` compila in 16,5 minuti.
- **[fatto]** Ricerca su come compilare con SDK più recenti dal Mac Intel con macOS 13.7.8: `reports/Compilare con SDK macOS più recenti.md` (raccomandazione: prova con VMware Fusion 13.6.4 e guest Sequoia).

## GPU Adreno 618 (chip id `0x06010800`)
- **[fatto]** Accensione GMU/RPMh/HFI, GX acceso, registri GPU leggibili e scrivibili (stadi `sweetgpu=2..9`), 64-bit UCHE, firmware `a630_sqe.fw`/`a630_gmu.bin` più `a615_zap.mbn` dal vendor del telefono.
- **[fatto] Zap firmato autenticato da TrustZone** (PAS id 13, tutti gli stati 0).
- **[fatto, 2026-11-02] Il CP esegue comandi e scrive in memoria** (Run D, `sweetgpu=10 sweetzapfixed=1`): `CP_NOP`, `CP_MEM_WRITE` (rilegge `0x5ee70001`) e `CACHE_FLUSH_TS` (timestamp `0x600d0001`) con `CP_IRQ` 0, passando dalla SMMU della GPU; `RBBM_SECVID_TRUST_CNTL` torna a 0; il telefono resta acceso e interattivo. Chiave: lo zap va caricato nella regione riservata `0x97715000` (dal devicetree di sm7150-mainline/postmarketOS), non in RAM kernel qualsiasi. Il meccanismo preciso è un'ipotesi.
- **[fatto, 2026-11-02] GPU tenuta accesa dopo il test** (`sweetgpukeep=1`, log `logs/surya-keep.txt`): GX, GMU, zap e SQE restano accesi; sei giri consecutivi (15 s l'uno) di `NOP` + `CP_MEM_WRITE` + `CACHE_FLUSH_TS` passano tutti con `CP_IRQ` 0, `RBBM_SECVID_TRUST_CNTL` 0 e stato RPMh attivo; poi spegnimento ordinato (zap `SHUTDOWN` completo, slumber). Il telefono resta acceso, senza panic né reset.
- **[fatto]** Errori precedenti spiegati a posteriori: `CP_IRQ 0x80` (illegal instruction), scritture che non arrivavano e i reset senza panic erano legati allo zap in RAM non riservata (ipotesi molto probabile).
- Documenti: `docs/gpu.md` (sezioni 5.1 Snapintosh, 5.2 piano di debug, 5.3 postmarketOS, 5.4 metal2vulkan).

## Documentazione e ricerche prodotte
- `docs/gpu.md`, `docs/audio.md`, `docs/wifi.md`, `docs/bluetooth.md`, `docs/cameras.md` (stato, piani, rischi per ogni blocco hardware).
- `docs/dual-boot.md` (idea futura Android + iOS, rischio GPT, variante solo `userdata`, riferimento woa-vayu).
- `reports/Adreno 618 UCHE e illegal opcode.md` e `reports/Compilare con SDK macOS più recenti.md` (note in `research_notes/`, non committate).

## Cose escluse da git di proposito
`logs/`, `POCO_X3_CAMERE_ISP.md` (contiene dati personali), dump e artefatti Apple, `research/`.
