# Building macOS/XNU software from non-macOS hosts (Linux, containers, VMs) to get around macOS-version limits

Research date: 2026-11-02. Host in question: Intel i7-7820HQ Mac, macOS 13.7.8, Xcode 15.2, 16 GB RAM. Target: xnu-11215.81.4 (macOS 15.3) and xnu-12377.121.6 (macOS 26.5), built with the dxb kit (blacktop/darwin-xnu-build at SHA 663906528d).

Method note: several GitHub pages were read through a summarising fetch tool; where a claim is load-bearing I cross-checked it against the raw files through the `gh` API (UniXNU scripts, crossxnu docs). Where only a summary was available it is marked as such.

## 1. What the repo's own build actually needs from Apple (the requirement list a Linux build must replace)

### Takeaway
The dxb/CI build is Apple-toolchain-centric: Xcode's clang, `xcrun`, `xcodebuild` for four sub-projects (mig, ctf tools, libsystem/libsyscall headers, libfirehose_kernel), a macOS SDK, a KDK (macOS 15 and optionally 26), and BSD userland. It does not use codesign, dsymutil, kmutil or iig for the plain kernel (kmutil only with `--kc`). The repo already disables bounds checking (`BOUND_CHECKS=0`), LTO (`BUILD_LTO=0`) and the KDK on 26.x.

### Cited Findings
- dxb `build.sh` calls `xcode-select`, `xcrun -find` (exports CC, CXX, LD, AR, RANLIB, STRIP, LIBTOOL), `sw_vers`, `hdiutil`/`installer` (KDK install), `ditto`, BSD `sed -i ''`, `brew`, `xcodes`, `gum`, `jq`, `python3`. `kmutil create` and `ipsw ... kmutil inspect` run only inside `build_kc` (the `--kc` option). `codesign` and `dsymutil` are not called by the script itself (only `CODE_SIGN_IDENTITY="-"` is passed to xcodebuild). — [dxb build.sh @663906528d](https://raw.githubusercontent.com/blacktop/darwin-xnu-build/663906528d1cd5ea8fde7de6f16de73db9511549/build.sh) (read via summarising fetch)
- dxb clones Apple repos from apple-oss-distributions: `xnu`, `bootstrap_cmds`, `dtrace`, `AvailabilityVersions`, `Libsystem`, `libplatform`, `libdispatch`. Tags come from `release.json` of `distribution-macOS`. — same build.sh
- xcodebuild invocations: `bootstrap_cmds` (mig.xcodeproj, ARCHS "arm64 x86_64"), `dtrace` (targets ctfconvert, ctfdump, ctfmerge), `Libsystem` installhdrs, libsyscall installhdrs, `libdispatch` target `libfirehose_kernel` (ARCHS x86_64 arm64e; this repo patches it to add arm64). xnu itself is built with plain `make install ... SDKROOT=macosx KDKROOT=...`. — same build.sh; repo's `.github/workflows/xnu.yml` sed on ARCHS (`file:///Users/luigicolucci/git/Bergamota/.github/workflows/xnu.yml`)
- The KDK is auto-downloaded from dortania/KdkSupportPkg, mounted with hdiutil and installed with `installer`. `build_xnu` aborts if `KDKROOT` is missing. — dxb build.sh
- dxb's patch dirs: `patches/15.0` (entitlements, iokit, skywalk, link_kdk, remove_tightbeam, restore_files.sh) and `patches/26.0` (00_setup_coreentitlements.sh, add_iboot_header, link_kdk, dsymutil_no_process_substitution, simplify_san_lipo, disable_bti_vmapple, ...). — [dxb patches tree](https://github.com/blacktop/darwin-xnu-build/tree/663906528d1cd5ea8fde7de6f16de73db9511549/patches) (file names taken from the README summary, list may be incomplete)
- The repo's workflow comments: 13.x/14.x need Xcode 15.x because "its iig matches the xnu-8796 IORPC ABI (Xcode 16's iig emits IORPC.kernelContent, which that xnu lacks)"; 15.x builds use macos-15 with Xcode 16.2; 26.x uses macos-26 with Xcode 26.1 (what dxb's own CI uses). `xcrun --find iig` is checked. — `file:///Users/luigicolucci/git/Bergamota/.github/workflows/xnu.yml`
- Repo's local script says the Xcode 27 SDK "no longer ships Kernel.framework's private headers (TrustCache/API.h, ...)", so Xcode 27 only gets as far as pmap_cs.h unless the KDK is used. The no-KDK 26.x mode uses a CoreEntitlements V2 stub; "SWEET needs nothing from the KDK (link_kdk only force-loads closed libs for Apple SoCs, kernel collections are not built)". Compile cache is via `CCACHE=sccache`. — `file:///Users/luigicolucci/git/Bergamota/tools/xnu-local-build.sh`, `.github/workflows/xnu.yml`
- xnu `makedefs/MakeInc.def` (xnu-11215.81.4): `KCC = $(CCACHE) $(CC)`, `KC++ = $(CCACHE) $(CXX)`, `S_KCC = $(CC)` (assembly not cached); `-fbounds-safety` probed with `$(CC) -E -fbounds-safety` and `BOUND_CHECKS := 0` on x86_64, 1 elsewhere; `-mkernel` used for x86_64 and ARM64; `DO_CTFMERGE` gate (CTF built from DWARF). — [xnu MakeInc.def @xnu-11215.81.4](https://github.com/apple-oss-distributions/xnu/blob/xnu-11215.81.4/makedefs/MakeInc.def) (summarising fetch; file truncated, so CTFCONVERT invocation lines not seen)
- CI timings on `macos-14`: Build step 5:06 and 7:23 uncached, 8:31/5:42 cold-cache, 4:43/3:22 warm (1077 ccache misses on a cold run). Fixed costs not cached: KDK download ~1 min, `xnu_headers` ~75 s. Kernel not byte-identical across runs because `__CTF` (ctfmerge output) differs. — `file:///Users/luigicolucci/git/Bergamota/docs/CI_CACHE.md`
- Apple's xnu README: needs a macOS SDK (`SDKROOT`, example `macosx.internal`); `make SDKROOT=macosx.internal ARCH_CONFIGS=X86_64 KERNEL_CONFIGS=DEVELOPMENT`; options `BUILD_LTO=0`, `MAKEJOBS`, `REMOTEBUILD=user@host`. No minimum Xcode stated. (A BUILD.md URL returned 404.) — [apple-oss-distributions/xnu](https://github.com/apple-oss-distributions/xnu)
- dxb README: host is macOS (Xcode, Homebrew, kmutil, recoveryOS, Virtualization.framework); Linux is not mentioned; its CI is one "XNU CodeQL" workflow and a `.cirrus.yml`; lists "automatically building XNU with GitHub Actions" as an open TODO. — [blacktop/darwin-xnu-build](https://github.com/blacktop/darwin-xnu-build) (summarising fetch)

### Inferences
- Apple-binary-only pieces in the plain-kernel path: clang with Apple extensions (xnu type builtins, see section 5), `xcrun`, ctfconvert/ctfmerge (from `dtrace.xcodeproj`), `libfirehose_kernel` (xcodebuild of libdispatch), libcc_kext (Apple clang runtime), the KDK/SDK private headers (TrustCache, Tightbeam, CoreEntitlements), and ld (Apple ld64/ld_prime). mig/migcom and availability.pl are open source (APSL) and only build via Xcode projects.
- For SWEET (plain arm64, no SPTM/Tightbeam, kernel collection not built) the closed KDK libraries are not needed, which removes the biggest "Apple-binary-only" item that blocks Apple-SoC builds.

### Gaps
- Exact lines in `MakeInc.def`/`MakeInc.cmd` that call `xcrun` for ctfconvert/dsymutil were not seen (file truncated in the fetch).

## 2. osxcross: status, SDKs, SDK packaging on Linux, licence, VM/container feasibility

### Takeaway
osxcross is actively maintained (last commit 2026-07-24) and as of July 2026 knows SDKs up to macOS 26.6 and 27. It is a userland cross-compiler for Linux/BSD hosts, not a kernel build system, and its SDK path needs an Apple SDK extracted from Xcode.xip, which is the licence-sensitive step. It would help only for the compiler/SDK layer; xnu's host tools, mig, iig, ctf tools and libfirehose are outside its scope.

### Cited Findings
- Repo is GPL-2.0, created 2013, last push 2026-07-24, 3.4k stars. — GitHub API metadata for [tpoechtrager/osxcross](https://github.com/tpoechtrager/osxcross)
- Recent commits: "Support recent macOS SDKs" (2026-07-12, adds build.sh cases for macOS 26.3-26.6 mapped to Darwin 25.3-25.6, and macOS 27 mapped to Darwin 27; wrapper target.cpp updated); "Support macOS 26.2 SDK" (2025-11-24, per commit list); "build_clang.sh, build_apple_clang.sh: Add Apple Clang 23 and default to it" (2026-07-20); "Unify toolchains behind stable, latest, and llvm build flavors" (2026-07-19); "Add patch to fix libc++ math macros issues with macOS 27 SDKs and LLVM < 22" (2026-07-13). — [commit 121914f](https://github.com/tpoechtrager/osxcross/commit/121914f), [commit list](https://github.com/tpoechtrager/osxcross/commits/master)
- Three flavors: stable (cctools 986, ld64 711), latest (cctools 1030.6.3, ld64 956.6), llvm (LLVM tools + ld64.lld, recommended for new projects, supports arm64/arm64e/x86_64 but not i386/x86_64h). `build_clang.sh` builds upstream LLVM/LLD; `build_apple_clang.sh` builds Apple Clang. Hosts: Linux and *BSD on x86/x86_64/ARM/AArch64; targets arm64, arm64e, x86_64, i386. — [osxcross README](https://github.com/tpoechtrager/osxcross)
- SDK packaging on Linux: Method 1 for Xcode above 8.0: `./tools/gen_sdk_package_pbzx.sh <xcode>.xip`, needs clang, make and libssl/lzma/libxml2 dev packages, "up to 45 GB of free disk space, and an SSD is strongly recommended". Alternative: from Command Line Tools DMG with `gen_sdk_package_tools_dmg.sh`. The README.SDK.md text names no Xcode version above "above 8.0" (it does not mention Xcode 16/26). — [README.SDK.md](https://github.com/tpoechtrager/osxcross/blob/master/README.SDK.md)
- README.SDK.md only says: "Please ensure you have read and understood the Xcode license terms before continuing." The README's licence section covers only wrapper (GPLv2), cctools/ld64 (APSL 2.0) and xar (BSD); no statement about the SDK. — [README.SDK.md](https://github.com/tpoechtrager/osxcross/blob/master/README.SDK.md), [README](https://github.com/tpoechtrager/osxcross)
- Open issues are mostly old-SDK or distro issues (e.g. #486 of 2026-10-24: "malformed mach-o, symbol table not in __LINKEDIT" when linking frameworks against the MacOSX10.4u SDK; no maintainer reply). None of the first-page issues concern macOS 15/26 SDKs. — [osxcross issues](https://github.com/tpoechtrager/osxcross/issues), [#486](https://github.com/tpoechtrager/osxcross/issues/486)
- Docker Desktop dropped macOS 13: 4.48.0 (October 2025) announced the end of macOS 13 support; a secondary (French, Oct 2026) guide says current 4.96.0 needs macOS 14 and recommends Colima for Intel Macs on Ventura. — [releases.sh note](https://releases.sh/release/rel_4h-qp4sxELNzCRe9NkFut-macos-13-support-ending-pac-proxy-configuration-added), [SSD Nodes guide](https://www.ssdnodes.com/learn/lang/fr/install-docker-on-mac) (secondary source)

### Inferences
- osxcross can supply a Linux-hosted clang + SDK for userland; its compilers target `*-apple-darwin*` triples and could in principle be pointed at xnu's `-mkernel` flags, but it ships no mig, iig, ctf tools, kextsymboltool host build, or libfirehose, so by itself it does not build xnu. Both working Linux xnu builds found (section 4) do not use osxcross or an Apple SDK at all; they use upstream clang plus cctools-port.
- For an Intel Mac on macOS 13: Docker Desktop is no longer installable; Colima/Lima (HVF-accelerated x86_64 Linux VM) or UTM/QEMU are the realistic hosts (inference: Colima/Lima Intel support taken from the secondary guide only).
- The SDK extraction needs a Xcode 16/26 .xip (several GB download, 45 GB scratch) and can be done on Linux or in the VM; it cannot be extracted by Xcode 15.2 on macOS 13 simply by running newer Xcode, but `gen_sdk_package_pbzx.sh` only unpacks, so it works regardless of host macOS version (inference from README.SDK.md description of pbzx/xip unpacking).

### Gaps
- Could not confirm from a primary source that `gen_sdk_package_pbzx.sh` is verified with an Xcode 26 .xip (the commit lists show SDK 26.x/27 support in build.sh, which implies SDK 26 packages are used, but no explicit statement).
- Whether the llvm flavor produces binaries usable for kernel (-mkernel, -fapple-kext) compile: not documented; not tested here.

## 3. Darling

### Takeaway
Darling is active (GPL-3.0, last push 2026-10-28, preview release 0.1.20260608) but only reliably runs command-line software; it targets Intel x86-64 Linux with a real Linux kernel module and its docs do not show modern Xcode (15/16/26) toolchains working. It is not a credible route for running Apple's Xcode clang/ld or xnu host tools.

### Cited Findings
- Darling: ~13.7k stars, 392 open issues, GPL-3.0, last push 2026-10-28; latest releases v0.1.20260608 (2026-06-09), v0.1.20260222, v0.1.20251023. — GitHub API metadata and release list for [darlinghq/darling](https://github.com/darlinghq/darling)
- README: runs macOS apps on Linux without a VM; low-level components (XNU, libSystem, Security) from Apple open source; higher-level frameworks still being reimplemented; "many CLI tools already work", GUI in development. It documents installing Xcode from a DMG or XIP; the page says Apple's clang from the Xcode toolchain can compile a test program but that Xcode itself "doesn't run yet" (statement possibly dated). Examples use Xcode 7.2 and the 10.11 SDK. Architectures, supported macOS version and Docker/WSL support are not stated. — [darlinghq/darling](https://github.com/darlinghq/darling) (summarising fetch)
- Aggregator report: "Darling is currently in its early stages, with basic support for CLI apps and very limited support for GUI"; targets Intel macOS apps on x86-64 Linux; can run "Apple's Toolchain"; WSL status contradictory between sources. — [Wikipedia: Darling](https://en.wikipedia.org/wiki/Darling_(software)) and search aggregates (unverified)
- LWN 2019: Darling could not run the Xcode IDE. — [lwn.net/Articles/794871](https://lwn.net/Articles/794871) (only listed in a search result, not opened)

### Inferences
- Xcode 16/26 toolchain binaries are universal (arm64 + x86_64) and use chained fixups and recent dyld/libSystem features; no source found shows Darling's emulated macOS level covering them. I would treat "Darling runs Xcode 16 clang/ld" as unproven. Even if the clang binary ran, it would be Apple's binary on a non-Apple machine, so no licence benefit.
- Darling requires a real Linux kernel with a kernel module; inside a VM on the user's Intel Mac that means a full Linux VM first, so it adds a layer on top of option "Linux VM" with no gain.

### Gaps
- No 2026 source found that tests Darling with Xcode 15+/16/26 clang or ld. No Darling issue search was done.

## 4. Does anyone build xnu on Linux today? (and the Apple-tool replacements)

### Takeaway
Yes, as of late October 2026: two public projects build recent XNU on Linux without Xcode or an Apple SDK. UniXNU (merged Linux path 2026-10-24) builds arm64 XNU (iPadOS 17.x tag xnu-10063, 4 KiB page baseline) with a patched upstream clang, cctools-port ld64 956.6, bootstrap_cmds mig, an `iig` reimplementation and shims. crossxnu (2026-10-30) builds xnu-12377.121.6 for x86_64 on Linux/WSL2 and boots it in QEMU to launchd. Neither is demonstrated on xnu-11215 or on the SWEET arm64 machine config. Both are brand new (days to weeks old).

### Cited Findings
**UniXNU (iOS/macOS-on-non-Apple-ARM64 project)**
- Repo created 2026-10-19, last push 2026-11-02, 1777 stars. PR #100 "Build kernel on linux" (smeng9) was merged 2026-10-24: adds `scripts/linux/{setup-toolchain.py,xcrun,clang,clang++,cc,codesign,plutil,install,gm4,TargetConditionals.h,prepare-xnu.py,llvm-xnu-builtin-type.patch,iig-port-darwin-target.patch}`, `scripts/build-firehose.sh`, `scripts/build-corecrypto-14.py`, `docs/building.md`. PR text: "I am a linux user... I don't have a Mac." — [UniXNU](https://github.com/UniXNU/UniXNU), [PR #100](https://github.com/UniXNU/UniXNU/pull/100) (GitHub API)
- Docs: "On Linux the kernel step builds with upstream clang and an open source Mach-O toolchain instead of Xcode." Components: clang with `__builtin_xnu_type_*` ("that xnu's `kalloc.h` needs"; llvm-project + `llvm-xnu-builtin-type.patch`), ld64/libtool/nm/lipo from cctools-port `1030.6.3-ld64-956.6`, mig from `bootstrap_cmds-138`, `iig-port` as iig, shims for `xcrun`/`plutil`/`codesign`/Command Line Tools layout, lzfse. Supports apt, dnf, pacman, zypper on glibc Linux. "Building Clang takes most of the setup time." — [UniXNU docs/guide/requirements.md](https://github.com/UniXNU/UniXNU/blob/main/docs/guide/requirements.md) (fetched through `gh api`)
- `setup-toolchain.py` comments: the patched clang is built AArch64-only (`LLVM_TARGETS_TO_BUILD=AArch64`, assertions ON, `LLVM_PARALLEL_LINK_JOBS=1`) from upstream llvm-project commit `58c46bae...` (apt.llvm.org snapshot 24~++20260911); cctools-port's master (ld64-609) "corrupts the kernel link", hence the pinned 1030.6.3-ld64-956.6; libdispatch `swift-6.4.0-RELEASE` and BlocksRuntime built for host; iig-port commit `ea6d147d`. — [setup-toolchain.py](https://github.com/UniXNU/UniXNU/blob/main/scripts/linux/setup-toolchain.py)
- `xcrun` shim: `-find` resolves tools from toolchain dirs then PATH, "Unknown tools print nothing and exit 1, which is what makedefs expects for the optional ones (ctfconvert, scan-build, docc...)"; `-show-sdk-path` returns the build tree fakeroot (no Apple SDK). — [scripts/linux/xcrun](https://github.com/UniXNU/UniXNU/blob/main/scripts/linux/xcrun)
- `prepare-xnu.py` rewrites "the few xnu sources that assume Apple's clang or BSD tools (the SHA and AES assembler macros, the syscall table generator under mawk, `kextsymboltool` writing to `/dev/null`)", installs `TargetConditionals.h`, and builds `libcc_kext.a` from compiler-rt; `build-firehose.sh` compiles `libfirehose_kernel` without Xcode. — [docs/building.md](https://github.com/UniXNU/UniXNU/blob/main/docs/building.md) (Linux host section)
- The Linux build target is `SDKROOT=iphoneos ARCH_CONFIGS=ARM64 MACHINE_CONFIGS=BCM2837 KERNEL_CONFIGS=DEVELOPMENT`, sources `xnu-10063.141.1` (iPadOS 17.7.11 profile), kernel output named `...unstripped.noctf` (no CTF). OS profiles in repo: ipados-17.7.11, ipados-26.5.2 (xnu-12377.121.6, arm64e), ipados-27.0.1. Docs say device "nabu" source-kernel builds "run locally on Linux" and the sheng port uses the 26.5.2/27 profiles. — [docs/guide/build-kernel.md](https://github.com/UniXNU/UniXNU/blob/main/docs/guide/build-kernel.md), [profile.toml 26.5.2](https://github.com/UniXNU/UniXNU/blob/main/profiles/os/ipados-26.5.2-23F84/profile.toml), [nabu.md](https://github.com/UniXNU/UniXNU/blob/main/docs/devices/nabu.md)
- UniXNU's macOS path also exists: the workflow runs on `macos-26` with Xcode 26.6 default. — [docs/building.md](https://github.com/UniXNU/UniXNU/blob/main/docs/building.md)

**crossxnu (portmach)**
- Created 2026-10-30; builds `xnu-12377.121.6` (macOS 26.5) x86_64 RELEASE on Arch Linux/WSL2, clang 23.1.4; boots in QEMU (q35) to launchd as pid 1 and runs some userland commands; launchd then stalls/SIGILL; arm64 "не поддерживается (нужен KDK/Tightbeam/SPTM)"; LTO off; README says no Xcode, Apple SDK or Apple utilities needed. Linker is `ld64.lld`; MIG built from bootstrap_cmds; `iig-port` for DriverKit files (but see below); synthetic SDK from open headers. ~2 GB build tree. — [crossxnu README](https://github.com/portmach/crossxnu), [docs/status.md](https://github.com/portmach/crossxnu/blob/main/docs/status.md) (via `gh api`)
- crossxnu `docs/compatibility.md` (Russian) lists replacements and gaps: Apple clang to LLVM clang 23; cctools ld to `ld64.lld` (warns `-static` not implemented, may leave `LC_DYLD_CHAINED_FIXUPS`/`MH_DYLDLINK`: "main open risk" for the loader); `libtool` to `llvm-ar --format=darwin`; ctfconvert/ctfmerge disabled (`DO_CTFMERGE=0`), dsymutil disabled, codesign no-op; `libcc_kext` and `libfirehose_kernel` not built yet there; the three Apple-clang builtins `__builtin_xnu_type_summary/_signature/_types_compatible` absent from LLVM 23, replaced by fallbacks (signature = "", types_compatible = 1) which "do not change struct layout or vtables" but change kalloc_type hardening/telemetry; IIG "not in open source", DriverKit headers excluded so DriverKit services do not work in phase 1. — [docs/compatibility.md](https://github.com/portmach/crossxnu/blob/main/docs/compatibility.md)
- crossxnu licence doc: no Apple SDK/KDK/Xcode in the repo; synthetic SDK generated from open sources; scripts MIT, patches APSL-2.0. — [docs/legal.md](https://github.com/portmach/crossxnu/blob/main/docs/legal.md)

**Allenwdk/xnu-linux-toolchain**
- Created 2026-10-23, 7 stars, 3 commits; builds a UniXNU-uke arm64 kernel on Linux with cctools-port ld64, MIG, a Python iig reimplementation (RPC dispatch stubbed: `X_<M>_Invoke` returns `kIOReturnUnsupported`) and 57 xnu portability patches; says `ld64.lld` lacks `-image_base`, `-segaddr`, `-segment_order` and silently drops GNU `ar` archives. Self-reported cold build "205 seconds on 32 cores", one-line change rebuild 27 s; toolchain `bootstrap.sh` ~5-8 min; no boot test, builds not deterministic; README states it is not ready to take over CI kernel builds. `libfirehose_kernel` needs manually placed private Apple SDK headers (no script). — [Allenwdk/xnu-linux-toolchain](https://github.com/Allenwdk/xnu-linux-toolchain) (summarising fetch; README partly Chinese)

**Older / other**
- iig-port: "Portable XNU DriverKit IIG generator", MIT, created 2026-08-12, files `iig.cpp`, `tests/`; used by both UniXNU and (per README) crossxnu. — [Nyktis/iig-port](https://github.com/Nyktis/iig-port) (GitHub API)
- darwin-on-arm wiki: historical Linux cross-compile of an old XNU with clang-3.4 and a Darwin SDK built from source; old forks and a 2010 darwinbuild thread only. Not relevant to xnu-11215. — [Building Darwin](https://github.com/darwin-on-arm/wiki/wiki/Building-Darwin), [darwinbuild-dev thread](https://lists.macosforge.org/pipermail/darwinbuild-dev/2010-April/000632.html)
- PureDarwin (active, pushed 2026-10-30) is an OS-distribution project; crossxnu pins `PureDarwin/xnu-loader`. No PureDarwin recipe for building xnu-11215 on Linux found. — [PureDarwin/PureDarwin](https://github.com/PureDarwin/PureDarwin) (API metadata)
- MacPorts packages the dtrace repo's ctfconvert/ctfmerge/ctfdump (needs clang-16) and states the main dtrace tool is not built; Oracle's `libdtrace-ctf` is a separate, unrelated Linux CTF implementation. No Linux build of Apple's ctfconvert/ctfmerge found. — [MacPorts dtrace](https://ports.macports.org/api/v1/ports/dtrace), [Oracle blog](https://blogs.oracle.com/linux/?p=2480) (search-result snippets)
- Apple's `dtrace` repo contains `tools/ctfconvert` and an Xcode project (`dtrace.xcodeproj`); `bootstrap_cmds` has `mig.xcodeproj`, `migcom.tproj`, `xcodescripts` (both APSL 2.0). — [apple-oss-distributions/dtrace](https://github.com/apple-oss-distributions/dtrace), [bootstrap_cmds](https://github.com/apple-oss-distributions/bootstrap_cmds) (summarising fetch)
- UniXNU's Linux path and crossxnu both build `mig`/`migcom` from bootstrap_cmds on Linux (crossxnu status doc: "Apple MIG migcom + driver mig on Linux" checked, verified on `mach_host.defs`). — [crossxnu status.md](https://github.com/portmach/crossxnu/blob/main/docs/status.md)
- No dxb-on-Linux instructions exist; dxb is macOS-only (see section 1). Searches for "xnu-build docker" and "build xnu on linux" returned only old darwin-on-arm forks and the two projects above. — web searches, 2026-11-02

### Inferences
- Verdict for the user's question: **xnu-11215 on Linux is plausible but unverified**. No public source builds exactly xnu-11215 on Linux; the closest verified points are xnu-10063 arm64 (UniXNU) and xnu-12377 x86_64 (crossxnu). 11215 sits between them, so the same set of fixes (type builtins, libcc_kext, libfirehose_kernel, mig, linker flags, xcrun shim) should apply, but the repo's 100+ SWEET patches (xnu/patches-11215) were developed against dxb + Apple clang and would need a re-run/triage on a different compiler and linker. Expect extra errors from non-Apple clang warnings-as-errors (`-Werror` exceptions: UniXNU/Allenwdk keep a `werror-exceptions.txt`), asm macros, and link-time options.
- The path of least resistance would be to clone UniXNU's `scripts/linux` toolchain (it already targets arm64 with 4 KiB pages like SWEET) and add the 11215 tag, rather than osxcross or Darling.
- Specific hard problems for an arm64 plain (non-arm64e) SWEET build: (a) `__builtin_xnu_type_*` needs the UniXNU clang patch (or accepting crossxnu's weaker fallbacks); (b) the link needs cctools-port ld64 (`ld64.lld` lacks `-image_base`, `-segaddr`, `-segment_order` per Allenwdk) with kernel link address options; (c) libfirehose_kernel and libcc_kext must be built by script; (d) CTF skipped (kernel works without it; only the repo's SHA256SUMS reproducibility already excludes `__CTF`); (e) iig only matters for DriverKit headers; the Linux iig reimplementations do not generate working RPC dispatch, so DriverKit must be treated as unavailable (the repo's 15.x build already comes from Xcode 16.2 iig; check whether SWEET patches rely on DriverKit RPC dispatch in the kernel: unknown).
- Replacing the KDK on 15.x: dxb installs the 24D60 KDK via a macOS `installer` pkg. On Linux the KDK pkg can be unpacked with xar/pbzx tools (inference), or the no-KDK approach used for 26.x (stubs, plus headers from older tags such as UniXNU's `trustcache_tag = xnu-8796.101.5`) can be extended to 15.x.

### Gaps
- No evidence found of a Linux build of xnu-11215. No confirmation that UniXNU's Linux scripts work on 12377/arm64e profiles (docs show Linux for 17.x devices; the 26.5.2 and 27 profiles exist and `setup-toolchain.py` checks `arm64e-apple-macos26.0` builtins, but no statement that those profiles were built on Linux).
- UniXNU docs and the iig-port repo give no evidence about the effect of stubbed iig RPC on a non-DriverKit kernel such as SWEET.
- These projects are days to weeks old (Oct 2026), unreviewed, and single-author; stability unknown.
- Whether either project uses SDK headers copied from Apple: both state not (open headers / generated fakeroot), but `libfirehose_kernel` private headers: Allenwdk says "manually placed" with redistribution concerns.

## 5. LLVM/clang alone with a copied SDK: Apple-clang-only features xnu uses

### Takeaway
Plain upstream clang cannot build current xnu unchanged: xnu-12377 needs the Apple-only `__builtin_xnu_type_*` builtins (kalloc_type), and `-fbounds-safety` is still being upstreamed. `-fbounds-safety` is optional (`BOUND_CHECKS=0`, which this repo already sets), arm64e/ptrauth is not needed for the plain-arm64 SWEET kernel. The builtins have been reimplemented in a clang patch by UniXNU (2026-10).

### Cited Findings
- `__builtin_xnu_type_summary/_signature/_types_compatible` are not in LLVM 23; crossxnu stubs them (signature = "" and types_compatible = 1, which disables kalloc_type layout signatures/compile-time checks without changing struct layout). — [crossxnu compatibility.md](https://github.com/portmach/crossxnu/blob/main/docs/compatibility.md)
- UniXNU's `llvm-xnu-builtin-type.patch` "Implement[s] Apple xnu type traits in upstream clang": adds `__builtin_xnu_type_signature`, `__builtin_xnu_type_summary`, `__builtin_xnu_types_compatible` and `__attribute__((xnu_usage_semantics()))`, with `XnuTypeSignature.h` implementing the KT_GRANULE_* classification from `osfmk/kern/kalloc.h`; its note says "xnu's kalloc_type signatures have no fallback without them". — [llvm-xnu-builtin-type.patch](https://github.com/UniXNU/UniXNU/blob/main/scripts/linux/llvm-xnu-builtin-type.patch), [setup-toolchain.py](https://github.com/UniXNU/UniXNU/blob/main/scripts/linux/setup-toolchain.py)
- `-fbounds-safety`: not fully in upstream clang; upstreaming ongoing (GSoC 2026 project; pieces landed such as the SemaBoundsSafety split, docs, LLDB test scaffolding, API-notes PRs). Apple's fork has it. — [GSoC 2026 post](https://discourse.llvm.org/t/gsoc-2026-participating-in-upstreaming-fbounds-safety/89649), [llvm PR 169112](https://github.com/llvm/llvm-project/pull/169112), [LLVM devmtg 2025 slides](https://www.llvm.org/devmtg/2025-04/slides/tutorial/olsonn-stefaniski_fbounds.pdf) (search snippets; full status not confirmed)
- xnu makedefs probes `$(CC) -E -fbounds-safety /dev/null` before enabling it; x86_64 always 0, other arches 1 when supported; soft-trap variants use `-ftrap-function=ml_bound_chk_soft_trap`. Repo sets `BOUND_CHECKS=0` explicitly. — [MakeInc.def](https://github.com/apple-oss-distributions/xnu/blob/xnu-11215.81.4/makedefs/MakeInc.def), `file:///Users/luigicolucci/git/Bergamota/tools/xnu-local-build.sh`
- ptrauth/arm64e: `ARCH_FLAGS_ARM64 = -arch arm64e` appears in makedefs; this repo builds SWEET as plain arm64 (libfirehose_kernel patched to include arm64) and runs arm64e user code only via offline PAC stripping (docs: `docs/XNU11215_MIGRATION.md`). UniXNU docs: arm64e profiles need PAC hardware, iPadOS 26+ is arm64e-only, iPadOS 18 is the last arm64 version. — `file:///Users/luigicolucci/git/Bergamota/docs/XNU11215_MIGRATION.md`, [UniXNU arm64e.md](https://github.com/UniXNU/UniXNU/blob/main/docs/porting/arm64e.md)
- Apple's llvm fork has stable branches through `stable/20260609`. A code search for `__builtin_xnu_type_signature` in `apple/llvm-project` returned no hits. — GitHub API branch list and `gh search code` (code search covers only the default branch, so this is inconclusive)
- osxcross can build Apple Clang from source (`build_apple_clang.sh`, Apple Clang 23 default as of 2026-07-20) — [osxcross commits](https://github.com/tpoechtrager/osxcross/commits/master)

### Inferences
- Because xnu-11215 also uses kalloc_type with type signatures (the builtins were introduced before macOS 15; crossxnu says 12377 uses three), assume 11215 needs the same builtins. (Inference: not verified against 11215 source.)
- An Apple-clang built from apple/llvm-project (osxcross route) might include the builtins if the xnu-required support is in the public fork; the no-hit search is not conclusive. The UniXNU patch is the verified route.
- LLVM `-mkernel`, `-fapple-kext`, `-fno-rtti` etc. are available in upstream clang (crossxnu's ABI-flag table lists them as used on its LLVM 23 build).

### Gaps
- The exact list of Apple-clang-only features used by xnu-11215 was not enumerated from source (no full build log available). Other Apple-only flags/features (e.g. `-fbounds-safety-adoption-mode`, `__builtin_*` for sptm/pointer-auth) could still bite.
- Whether bounds-safety attributes (`__counted_by` and similar macros) compile cleanly when the feature is off: the repo already builds that way on Apple clang, so macros are expected to degrade, but not verified with non-Apple clang.

## 6. Efficiency: build time and RAM on Linux VM/container vs native, and offloading

### Takeaway
On the i7-7820HQ (4 cores/8 threads, 16 GB) a Linux VM with hardware acceleration would run the xnu compile itself at roughly native speed; the dominant one-time cost is building the patched clang (and ld64/mig toolchain), not the kernel. Compile caching (ccache/sccache) already works with xnu's makefile hooks and is the only offload mechanism clearly compatible with xnu; distributed compile (icecream) is possible in principle but unproven for this build.

### Cited Findings
- CI baseline: Build step 3:22-8:31 on `macos-14` runners; ccache/hosttools caches save about 1-3 min; KDK download, `xnu_headers` etc. add fixed cost. — `file:///Users/luigicolucci/git/Bergamota/docs/CI_CACHE.md`
- Allenwdk toolchain: cold UniXNU-uke build 205 s on 32 cores; rebuild after one-line change 27 s, patch change 111 s; toolchain bootstrap ~5-8 min (self-reported, same machine unspecified). — [Allenwdk/xnu-linux-toolchain](https://github.com/Allenwdk/xnu-linux-toolchain) (summarising fetch)
- UniXNU toolchain script builds clang for AArch64 only with `LLVM_PARALLEL_LINK_JOBS=1`, assertions on; docs: "Building Clang takes most of the setup time." — [setup-toolchain.py](https://github.com/UniXNU/UniXNU/blob/main/scripts/linux/setup-toolchain.py), [requirements.md](https://github.com/UniXNU/UniXNU/blob/main/docs/guide/requirements.md)
- crossxnu: build tree ~2 GB; warns to build on a Linux filesystem, not `/mnt/c` (9p) because it slows the build a lot. — [crossxnu README](https://github.com/portmach/crossxnu)
- Repo's local script: "Needs about 8 GB free" for the macOS build, default `-j8`. — `file:///Users/luigicolucci/git/Bergamota/tools/xnu-local-build.sh`
- xnu makedefs wrap KCC/KC++ with `$(CCACHE)`; the repo uses `CCACHE=sccache` locally and ccache in CI; assembly and link are not cached. — [MakeInc.def](https://github.com/apple-oss-distributions/xnu/blob/xnu-11215.81.4/makedefs/MakeInc.def), `file:///Users/luigicolucci/git/Bergamota/docs/CI_CACHE.md`
- xnu's own README lists `REMOTEBUILD=user@host`. — [apple-oss-distributions/xnu](https://github.com/apple-oss-distributions/xnu)
- sccache: wrapper cache for gcc/clang/rustc etc., local disk or S3 (and other remotes); separate distributed mode; icecream (SUSE) is easier to deploy across heterogeneous Linux machines and, with clang being a native cross-compiler, only needs a chroot with the toolchain; distcc requires identical toolchains on all nodes. These are blog/aggregator-level sources. — [sccache distributed blog](https://blog.davidedmundson.co.uk/blog/setting-up-distributed-compilations-with-sccache/), search aggregate of sccache/icecream/distcc pages (secondary)

### Inferences
- Intel Mac + HVF VM: x86_64 Linux guest on x86_64 host has no CPU emulation, so compile throughput should be within ~10-20% of native for CPU-bound work (inference: not measured). Pitfalls: disk I/O through shared folders (virtiofs/9p) is much slower, so build in the VM's own disk. RAM: allocate 10-12 GB to the VM, 6-8 cores; LLVM build (AArch64 only, assertions ON) may peak around 8-10 GB for link steps even with `PARALLEL_LINK_JOBS=1` (inference). Expect the clang build to take on the order of 1-2+ hours on 4c/8t (my estimate; the project only says "most of the setup time"), done once and cacheable as a prefix.
- The xnu build itself: CI's 1077 compile units run in 5-8 min on a 3-core M1-class runner (inference: macos-14 runners are 3-core M1); an i7-7820HQ with 8 threads should land in the same order (10-15 min cold, my estimate), then single-digit minutes warm with ccache.
- Offload options that fit: (1) a shared ccache/sccache remote (S3/GHA) because compile commands are deterministic per patched tree; (2) icecream/sccache-dist with the patched clang packaged identically on helper nodes. Both only offload C/C++ compile (the link and asm do not), and neither has a known xnu precedent. CI itself (GitHub macOS runners + the repo's caches) is already the offload.

### Gaps
- No benchmark of xnu builds on Linux VMs on Intel hardware found. No evidence for icecream/sccache-dist with xnu. Peak RAM for the AArch64-only clang build not found.

## 7. Legal/licensing aspects of using macOS SDK on Linux

### Takeaway
Apple's Xcode/SDK licence is widely read as restricting use of Xcode and SDKs to Apple-branded hardware; osxcross's own docs only tell users to read it. A Linux VM or container running on the user's own Mac is arguably still "Apple-branded hardware" (opinion, not a ruling). The new Linux xnu builds avoid the SDK entirely (open headers, synthetic SDK), which sidesteps the issue for the kernel but not for the KDK.

### Cited Findings
- Apple developer forum thread on cross-compiling on Linux: poster paraphrases Section 2.5 "Copies" as not allowing "any part of Xcode on anything but Apple-branded hardware"; accepted answer says wording is the same back to 2011, recommends having access to a Mac; a later reply says it is a legal question for a lawyer/Apple legal. — [Apple Developer Forums thread 114179](https://developer.apple.com/forums/thread/114179) (paraphrase, not verbatim licence text)
- Search results reproduce Xcode SLA wording limiting SDK installation/use to Apple-branded computers ("clause 2.6 No Other Permitted Uses" in one copy). Copies vary by date; official current text was not retrieved. — [Apple Xcode SLA PDF (apple.com.cn)](https://apple.com.cn/legal/sla/docs/xcode.pdf) (search snippet only)
- Commenters in the xtool discussion argue running Linux or a VM on Apple hardware is fine; others argue downloading Xcode.xip for Linux tooling may breach the terms; labelled as opinions. — [OSnews xtool story](https://www.osnews.com/story/142331/xtool-cross-platform-xcode-replacement-for-linux-windows-and-macos/) (search snippet)
- osxcross README: "Please ensure you have read and understood the Xcode license terms before continuing." — [README.SDK.md](https://github.com/tpoechtrager/osxcross/blob/master/README.SDK.md)
- crossxnu legal doc: no Apple SDK/KDK/Xcode in repo; "Project does not bypass code-signing, DRM and license limits of macOS; booting on real Apple hardware may violate the macOS licence; responsibility lies with the user." — [crossxnu legal.md](https://github.com/portmach/crossxnu/blob/main/docs/legal.md)
- Components used on Linux are open source: xnu, bootstrap_cmds, dtrace, AvailabilityVersions, cctools (APSL-2.0), libdispatch (Apache-2.0), iig-port (MIT), LLVM (Apache-2.0 with LLVM exceptions). — [crossxnu legal.md](https://github.com/portmach/crossxnu/blob/main/docs/legal.md)

### Inferences
- Risk summary: using Xcode.xip-derived SDK in a Linux VM on the user's own Mac is low practical risk (private use, Apple hardware) but its legality is untested; do not redistribute SDKs or KDK images; keep them out of git (consistent with AGENTS.md "Keep ... downloaded Apple artifacts out of git").
- The user's repo already downloads the KDK on GitHub macOS runners (Apple-branded hardware) so current CI is the cleanest legal posture.
- Not legal advice.

### Gaps
- Official current Xcode SLA text (2026) with section numbers not retrieved; the KDK licence terms not checked.

## 8. Verdict: can xnu-11215 be built off-macOS?

### Takeaway
Probably yes with significant work, but no one has demonstrated it for xnu-11215. Feasible stack: Linux x86_64 VM on the Intel Mac (Colima/Lima/UTM, since Docker Desktop no longer supports macOS 13) + UniXNU's `scripts/linux` toolchain (patched upstream clang, cctools-port ld64 956.6, mig, iig-port, shims) + the repo's patches re-applied. osxcross and Darling are not the right tools for the kernel. The existing GitHub macOS-runner CI remains the lowest-risk route and already solves the Xcode 16.2/26.1 requirement.

### Cited Findings
- Evidence for feasibility: UniXNU Linux kernel build for xnu-10063 arm64 (merged 2026-10-24) and crossxnu for xnu-12377 x86_64 (booting under QEMU, 2026-10-30); both without Xcode/SDK. — see section 4 sources
- Evidence against ease: crossxnu says arm64 is unsupported ("needs KDK/Tightbeam/SPTM"); Allenwdk says its toolchain "can't yet take over CI kernel builds", reproducibility and boot untested; `libfirehose_kernel` private-header issue. — [crossxnu status.md](https://github.com/portmach/crossxnu/blob/main/docs/status.md), [Allenwdk](https://github.com/Allenwdk/xnu-linux-toolchain)
- dxb itself supports only macOS hosts and its own TODO lists CI as unfinished. — [blacktop/darwin-xnu-build](https://github.com/blacktop/darwin-xnu-build)

### Inferences
- Effort estimate (my judgement): 1-3 days to get the toolchain built and one 11215 object set compiling in a VM; unknown number of days for the full SWEET patch set to link and give a bootable kernel. Largest risks: linker/`kextsymboltool`/segment options, `-Werror` exceptions, libfirehose_kernel, subtle codegen differences from non-Apple clang for a kernel that boots on real hardware (no byte-identical comparison to Apple clang possible; crossxnu says bit-for-bit match is not required or attainable).
- Cheaper alternatives (out of scope of this note, but relevant to the decision): keep CI on macOS runners (already 3-8 min with caches), or use the new UniXNU Linux scripts only as a fallback when runners are unavailable (the workflow already has a `runner` override because the macos-15 pool often lacks capacity).

### Gaps
- No measured result for xnu-11215 on Linux; recommending a time-boxed spike (build UniXNU toolchain in a Colima/Lima VM, compile ~20 representative files of xnu-11215 with SWEET flags, link) before any commitment.
