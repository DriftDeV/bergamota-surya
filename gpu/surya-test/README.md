# surya-test: test userland del client GPU Adreno 618 (Poco X3 NFC)

`adreno618_test` apre il servizio IOKit `SweetAdreno618` da un normale processo iOS e ripete dallo spazio utente
quello che il kernel fa nei propri self-test: un BO da 16 KiB, shader compute + descrittore UAV + target + command
stream scritti dalla CPU, `GEM_SUBMIT`, attesa del fence, lettura di `0xcafe0001`.
Modellato su `gpu/kernel-test/adreno_uc_test.c` (Poco F1). ABI: `xnu/gpu-abi/sweet_adreno_abi.h`,
`docs/GPU_KERNEL.md` (sezione 6, "Typical winsys sequence" e "Differences from Linux msm").

## Uso

```
gpu/surya-test/build.sh                      # arm64e + renegade strip --sign -> out/adreno618_test, poi selfcheck host
scp out/adreno618_test root@<ip-del-telefono>:/usr/local/bin/
adreno618_test --info                        # solo sonda: INFO + GET_PARAM, non sottomette nulla
adreno618_test [-v]                          # test completo (-v stampa il command stream)
adreno618_test --selfcheck                   # solo generatore, confronto con i golden (non serve il device)
```

`out/` e' ignorata da `gpu/surya-test/.gitignore`. Il build.sh compila anche una variante macOS
(`-DHOST_SELFCHECK`, senza IOKit) e la esegue.

## Passi (una riga per passo; exit 0 solo se tutto passa)

1. `IOServiceMatching("SweetAdreno618")` (se manca, prova `SweetAdreno630` solo per stampare un messaggio),
   proprieta' di registry `SweetGPUABIVersion`/`SweetGPUChipID`, `IOServiceOpen`, `INFO` (abi 1, flag ready),
   `GET_PARAM` GPU_ID 618, CHIP_ID 0x06010800, GMEM_SIZE 0x80000, VA_START, VA_SIZE.
2. `GEM_NEW` 16 KiB, `GEM_INFO` GET_IOVA e GET_OFFSET, `IOConnectMapMemory64` con `kIOMapAnywhere | kIOMapWriteCombineCache`.
3. Riempimento da CPU (dentro `CPU_PREP`/`CPU_FINI`): shader a +0x0000, descrittore UAV a +0x0400, target 16 byte a +0x0800
   (pre-riempito `0xdeadbee0..3`), timestamp a +0x0c00, command stream a +0x1000. Gli indirizzi GPU sono quelli reali del BO
   (shader e descrittore restano allineati a 64 byte).
4. `GEM_SUBMIT` (1 cmd tipo BUF, BO READ|WRITE, nessun reloc), `WAIT_FENCE` con timeout relativo di 2 s, `CPU_PREP`,
   controllo: target[0] == 0xcafe0001, target[1..3] invariati, timestamp == 0xc0de0001.
5. Il passo 4 viene ripetuto una seconda volta ri-avvelenando il target; poi `GEM_CLOSE` e uscita.

## Command stream

La sequenza registri/valori e' la stessa di `tools/ir3asm/csgen.c` (dispatch compute a618 di `first_store.asm`, stessa
sequenza di Mesa computerator), portata in una funzione che prende gli indirizzi come parametri; gli indirizzi a 64 bit
sono scritti come parole lo/hi. Gli offset dei registri sono scritti come numeri con il nome del registro nel commento
(gli header Mesa generati in `research/mesa-ir3/` sono ignorati da git e non vengono copiati). Lo shader e'
`tools/ir3asm/first_store_a618.bin` incorporato come array (32 dword).

`--selfcheck` genera lo stream con i VA fissi di csgen (0x40022000/0x40022400/0x40022800/0x40020160) e lo confronta con
97 dword di riferimento incorporate; nella variante host, se esiste `research/mesa-ir3/cs_out.h`, confronta anche con quello
(stream, descrittore e shader).

## Righe attese su un dispositivo funzionante

```
PASS  service SweetAdreno618 found
PASS  registry ABI version 1 (want 1), chip id 0x6010800 (want 0x6010800)
PASS  IOServiceOpen -> 0
PASS  INFO abi 1 ...   /   PASS  INFO flags 0x1 ...
PASS  GET_PARAM GPU_ID 618 / CHIP_ID 0x6010800 / GMEM_SIZE 0x80000
PASS  GEM_NEW 16384 -> handle N ...      PASS  IOConnectMapMemory64 -> 0x... size 16384
PASS  round 1: GEM_SUBMIT (97 dwords at BO+0x1000) -> fence F
PASS  round 1: WAIT_FENCE F (2 s)
      round 1: target 0xcafe0001 0xdeadbee1 0xdeadbee2 0xdeadbee3 timestamp 0xc0de0001
PASS  round 2: ...
PASS  GEM_CLOSE
PASSED: 0 failure(s)
```

## Ipotesi sul servizio (fase 2), non verificate

- Il nome di classe e' `SweetAdreno618`; se l'header definisce `SWEET_GPU_SERVICE_NAME_A618` il valore e' lo stesso.
- Selettori e strutture sono quelli msm di `sweet_adreno_abi.h`; `SWEET_GPU_SEL_INFO`, `GET_PARAM`, `GEM_*`, `WAIT_FENCE`,
  `CPU_PREP/FINI` funzionano come sul Poco F1 (timeout relativi, la mappatura usa l'handle come memoryType).
- La proprieta' di registry e' un CFNumber leggibile come SInt32; il BO ha iova allineato a 4 KiB e sta sotto 4 GiB o meno
  (il programma gestisce anche indirizzi a 64 bit).
- `SURFACE_BO` e `DEBUG` non sono usati.

## Fonti

- Linux `include/uapi/drm/msm_drm.h` (layout delle strutture msm): https://github.com/torvalds/linux/blob/master/include/uapi/drm/msm_drm.h
- Mesa, `src/freedreno/computerator/a6xx.cc` e header generati `a6xx.xml.h`/`pm4.xml.h` (registri, bit di parita' dei pacchetti PM4).
- Apple IOKit user-space: `IOServiceMatching`, `IOServiceOpen`, `IOConnectCallStructMethod`, `IOConnectMapMemory64`
  (IOKit/IOKitLib.h, apple-oss-distributions/IOKitUser).
