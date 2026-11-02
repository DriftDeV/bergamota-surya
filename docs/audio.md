# Audio sul Poco X3 NFC (surya): hardware, stato, piano

Legenda: **[V]** verificato (file/riga, patch o misura) · **[I]** inferenza · **[?]** da verificare.
Base da arricchire con piani e stato (sezioni 3 e 7). Documenti gemelli: `gpu.md`, `wifi.md`, `bluetooth.md`, `cameras.md`.
Modello di riferimento per boot di un processore remoto con PAS e firmware nell'ADT: `docs/GPU_A630_BRINGUP.md` (zap) e il lavoro modem/MBA di `docs/WIFI_BT_PLAN.md`.

> **Correzione a quanto detto in chat:** il codec **non** è un WCD9335 «Tavil» su SLIMbus. Il nodo `tavil_codec` esiste nel FDT ma ha
> `status = "disabled"` (`dtb/surya/live.dts:909-950`), come l'intero `slim@62dc0000` (`live.dts:883-890`). La scheda audio attiva è
> `qcom,model = "sm6150-wcd9375-snd-card"` (`live.dts:17960`): codec **WCD937x (WCD9375)** dietro i macro LPASS «bolero» e
> **SoundWire**. L'unico SLIMbus attivo (`slim@62e40000`) è del Bluetooth WCN3990 [V].

## 1. Hardware sul Poco X3 NFC (surya)

| Blocco | Dato | Fonte |
|---|---|---|
| Scheda audio | `sm6150_snd` compatible `qcom,sm6150-asoc-snd`, modello `sm6150-wcd9375-snd-card`, `qcom,wcn-btfm = 1`, `qcom,mi2s-audio-intf`, `qcom,auxpcm-audio-intf`, `qcom,wsa-max-devs = 0` (**nessun WSA881x**) | live.dts:17948-17985 [V] |
| Contenitore | `qcom,msm-audio-apr` → `qcom,q6core-audio` → `bolero-cdc` (`qcom,bolero-codec`, 3 macro) | live.dts:17811-17840 [V] |
| Macro LPASS | `va-macro@62f20000`, `tx-macro@62ec0000`, `rx-macro@62ee0000`, `wsa-macro@62f00000` | live.dts:17839-17912 [V] |
| SoundWire master | `swr2` TX (swrm 0x62ed0000, id 3, IRQ SPI 0x128 + wake 0x1f4), `swr1` RX (swrm 0x62ef0000, id 2, IRQ SPI 0x129), `swr0` WSA (IRQ 0x127) | live.dts:17856, 17895; aliases 20360-20362 [V] |
| Codec | `wcd937x-codec` (`qcom,wcd937x-codec`), slave TX `reg 0x00 0x1170223`, slave RX `reg 0x00 0x1170224`, regolatori `ldo-rxtx`, `vddpx-1`, `buck`, `mic-bias`, 3 micbias a 2700 mV, gpio reset (`qcom,wcd-rst-gpio-node`), DMIC su gpio dedicati | live.dts:17916-17945 [V] |
| Altoparlante | **ampli I²C TI `tas256x`**, bus `i2c@0xa8c000` (QUP SE9), indirizzi 0x4c (destro) / 0x4d (sinistro), 2 canali, `ti,reset-gpio`/`reset-gpio2`/`irq-gpio`/`irq-gpio2`, `#sound-dai-cells` | live.dts:9326, 9353-9367 [V] |
| Switch USB-C audio | `fsa4480@43` sullo stesso bus I²C (`qcom,fsa4480-i2c`), `fsa4480-i2c-handle` nel sound card | live.dts:9343, 17958 [V] |
| Haptics | `awinic,aw8624_haptic` / `ti,drv2624` (vibrazione) | live.dts:8901 [V] |
| ADSP (LPASS) | `qcom,lpass@62400000` (`pil-tz-generic`), PIL `qcom,pas-id = 1`, `firmware-name = "adsp"`, `memory-region = pil_adsp_mem`, `qcom,smem-id 0x1a7`, `qcom,sysmon-id 1` | live.dts:4645-4680 [V] |
| Memoria ADSP | `pil_adsp_region@95700000`, 0x1e00000 (30 MiB) no-map; `adsp_region` shared-dma-pool 0xc00000 | live.dts:20499, 20555 [V] |
| Trasporto ADSP | GLINK edge `adsp` (`qcom,glink-label = "lpass"`, remote pid 2, SMEM), canali `IPCRTR`, **`apr_audio_svc`**, `glink_ssr`, `g_glink_audio_data`, fastrpc | live.dts:4087-4130 [V] |
| DAI/PCM | `msm-pcm-dsp`, `msm-pcm-routing`, `msm-pcm-voice`, MI2S 0-4, AUXPCM, TDM, `cdc-dma` (45056…45118), `bt_sco_rx/tx`, SB-9 | live.dts:17269-17725 [V] |
| Firmware ADSP | nella partizione `modem` (FAT): `IMAGE/ADSP.MDT` (8860 B) + `ADSP.B00..B09` (948; 7912; 80588; 98052; 1891100; 1707428; 2936; 6140; 25908; 170516 B), `ADSPR.JSN`, `ADSPS.JSN`, `ADSPUA.JSN`, `QDSP6M.QDB` (5240816 B). Backup: `backup/surya/modem.img`. Solo nomi/dimensioni | listato in sola lettura [V] |
| Partizione `dsp` | `backup/surya/dsp.img` (32 MiB), **non FAT** (formato da identificare) | [V non FAT; contenuto ?] |

## 2. Cosa sappiamo (verificato)

* **Nulla di audio gira oggi su Heartwood** (xnu-8796 / 11215 / 12377): nessuna patch audio/slim/adsp/apr nel repo
  (`ls xnu/patches*`: solo patch BT/Wi-Fi). Nelle note del Poco F1 l'audio è fuori perimetro (`PORT_SURYA.md` (radice) §3, `README.md`) [V].
* L'avvio dell'ADSP per il F1 (SDM845) non è stato fatto. Per surya il blocco è un normale **PAS** (stesso schema dello zap della GPU):
  `qcom,pil-tz-generic`-style con `pas-id 1` [V DTB]. Il codice PAS/SCM già scritto per la GPU (`sweet_scm.c`: INIT_IMAGE, MEM_SETUP,
  AUTH_AND_RESET, SHUTDOWN con status 0 sullo zap) è la base riusabile [V per la GPU; I per l'ADSP].
* Il lato iOS non ha un driver audio Qualcomm. `mediaserverd`/`coreaudiod` parlano con un'uscita IOAudio/`AppleAudioDevice` Apple [I]. Un'uscita finta
  è l'unico modo realistico per far partire il suono (nessuna ricerca nel repo ancora) [?].
* Sul F1 il Bluetooth con iOS 18 è stato portato fino al *connect* (`docs/WIFI_BT_IOS18.md` §2.10-2.12): l'audio BT (A2DP) richiede il pipe
  Skywalk `com.apple.bluetooth.AudioPipe` e il suo HAL; **non è stato provato** [V].

## 3. Ipotesi di implementazione su Heartwood/XNU + iOS, a fasi

| Fase | Obiettivo | Come | Note |
|---|---|---|---|
| A0 | Mappa hardware | completare da `live.dts`: GPIO (reset WCD, SWR, DMIC, I²S), clock LPASS (`lpass_core_hw_vote`), regolatori, SMMU LPASS | solo lettura |
| A1 | Avviare l'ADSP | PAS id 1: leggere `ADSP.MDT`+`B00..B09` dalla partizione `modem` (come `sweet_ufs_read_part` in WIFI_BT_PLAN), mappare in 0x95700000, INIT_IMAGE/MEM_SETUP/AUTH_AND_RESET, voti RPMh; SMEM + SMP2P («slave-kernel», `qcom,smem-states`) | stesso rischio della GPU |
| A2 | Canale con l'ADSP | GLINK (SMEM) edge `adsp` → `apr_audio_svc`; APR/AFE (servizi `q6core`, `q6afe`, `q6adm`, `q6asm`) | richiede la pila GLINK/QRTR che il lavoro Wi-Fi F1 ha già in parte (`1113-beryllium-mpss-rmtfs-qrtr-glink`) |
| A3 | Codec e SoundWire | QUP SE9 I²C per `tas256x` (0x4c/0x4d), GPIO di reset; SoundWire master RX/TX (swrm 0x62ef0000/0x62ed0000), macro bolero, WCD9375 (reset, regolatori, clock MCLK 9.6 MHz) | molte tabelle di registri Qualcomm |
| A4 | Un tono di prova | percorso fisso: PCM → ADSP → MI2S/TDM verso `tas256x` (altoparlante) | primo suono udibile |
| A5 | Driver iOS | classe IOKit che esponga un'unica uscita al CoreAudio (`mediaserverd`), con buffer condiviso verso l'ADSP | nessun precedente nel repo |
| A6 | Completare | microfoni (DMIC/AMIC), cuffie (`mbhc`, `fsa4480`), volume/mute, BT SCO, chiamate | mesi |

Stima grezza (solo per priorità) [I]: A1 qualche giorno, A2-A4 settimane, A5 settimane con esito incerto, A6 mesi.

## 4. Ostacoli e rischi

1. **L'ADSP è un sottosistema completo.** Senza firmware, SMEM, GLINK e APR non c'è nemmeno il «segnale» per accendere il codec.
2. **Reset del SoC** per accessi a blocchi spenti (LPASS, SWR, macro) o a SMMU sbagliati: stessa famiglia dei rischi GPU (`docs/GPU_A630_BRINGUP.md` §Rischi).
3. **TrustZone/XPU**: l'immagine ADSP è firmata; PAS con parametri sbagliati può bloccare il sottosistema.
4. **Lato iOS**: nessuna interfaccia compatibile; ogni app audio passa da `mediaserverd`. Un'uscita finta potrebbe essere rifiutata se manca un set di proprietà
   IOKit (come per il Wi-Fi, `docs/WIFI_IOS26.md` §1) [I].
5. **Licenze**: il firmware ADSP è Qualcomm/Xiaomi: solo nomi e dimensioni in git, mai i contenuti.
6. **Amplificatore intelligente TAS256x**: richiede una configurazione di I²C e un blocco DSP interno prima di suonare [I].
7. **Dipendenza dal modem**: l'ADSP e il modem condividono SMEM/GLINK; ordine di avvio non verificato [?].

## 5. Lavoro della community e riferimenti utili

* **Linux mainline** (`sm6150`/`sdm845`): `qcom,apr`, `q6afe`/`q6asm`, `wcd937x`, `lpass-macro`, `soundwire-qcom`, `tas2562`/`tas256x` (driver TI) [I, da leggere].
* **Sorgenti Xiaomi/Qualcomm downstream** del telefono (`MiCode/Xiaomi_Kernel_OpenSource`, ramo `surya-q-oss`): `sound/soc/codecs/wcd937x`, `bolero`, `asoc/sm6150.c`, `msm-audio-effects` [?].
* **ios-topaz (SM6225)**: usa PAS per modem; non ha audio [V `docs/TOPAZ_MERGE.md`].
* **ry-diffusion/Bergamota (F1)**: audio fuori perimetro; BT audio non provato.
* Nessuna altra fonte della community con audio funzionante su iOS-su-Qualcomm trovata [V, censimento branch/fork 2026-11-02].

## 6. Domande aperte / prossimi passi

1. Verificare nel DTB: quali GPIO/pin per SWR, DMIC e reset WCD; quale DAI usa il TAS256x (MI2S/TDM) [?].
2. Formato di `backup/surya/dsp.img` (non FAT): è il `dsp` Android con librerie ADSP (ext4/squashfs)? [?]
3. L'ADSP parte con i soli file della partizione `modem`, o serve anche `dsp`? [?]
4. Esiste un modo di far suonare l'altoparlante senza l'ADSP (via I²C+I²S diretta dal kernel)? Probabilmente no (I²S è dell'LPASS) [I].
5. Quale classe/IOKit minimo vuole `mediaserverd` per accettare un'uscita (analisi di `coreaudiod`/`mediaserverd` iOS 18)? [?]

## 7. Stato

| Fase | Stato | Data |
|---|---|---|
| A0 mappa hardware dal DTB | in questo documento (parziale) | 2026-11-02 |
| A1 ADSP via PAS | da fare | |
| A2 GLINK/APR | da fare | |
| A3 codec/SoundWire | da fare | |
| A4 tono di prova | da fare | |
| A5 driver iOS | da fare | |
