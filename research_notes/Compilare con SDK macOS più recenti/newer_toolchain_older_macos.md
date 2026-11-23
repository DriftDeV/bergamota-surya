# Building with a newer Xcode / SDK than the host macOS officially supports (macOS 13.7.8 Intel, Xcode 15.2), for xnu-11215 / xnu-12377 and general apps

Date of research: 2026-11-02. Host: Darwin 22.6.0, macOS 13.7.8 (22H730), Intel x86_64, Xcode 15.2 (15C500b), SDKs MacOSX14.2 (Xcode) and MacOSX12/13 (CLT).

Legend. "LOCAL TEST" = I ran it on this Mac today; the Apple packages were downloaded from Apple's public software-update CDN (swcdn.apple.com, URLs taken from the public sucatalog, no Apple ID needed), expanded with `pkgutil --expand-full` into the session scratchpad (NOT installed, NOT in git): `/private/tmp/claude-501/-Users-luigicolucci-git-Bergamota/07ffb600-086d-4312-a16d-06644f390617/scratchpad/{clt162,clt265,t,cat}`. Catalog used: https://swscan.apple.com/content/catalogs/others/index-26-15-14-13-12-10.16-10.15-10.14-10.13-10.12-10.11-10.10-10.9-mountainlion-lion-snowleopard-leopard.merged-1.sucatalog.gz

---

## 0. What the repo's xnu build actually needs from Xcode

### Takeaway
The build uses: Apple clang/clang++, ld, ar/ranlib/strip/libtool (all via `xcrun -find`), `iig` (IOKit DriverKit IDL, only inside Xcode.app), `xcodebuild` (to build mig, libfirehose_kernel, ctf tools), the macOS SDK (headers), and (for 12377) KDK headers. ctfconvert/ctfmerge/ctfdump, mig/migcom and availability.pl are NOT taken from Xcode: dxb builds them from Apple's open-source repos.

### Cited Findings
- `.github/workflows/xnu.yml` picks Xcode 16.2 on `macos-15` for 15.x sources (xnu-11215), Xcode 26.1 on `macos-26` for 26.x (xnu-12377), oldest Xcode on `macos-14` for 13.x/14.x; it runs `xcrun --find iig` as a sanity check; header says "macos-14 still ships Xcode 15.x, whose iig matches the xnu-8796 IORPC ABI (Xcode 16's iig emits IORPC.kernelContent, which that xnu lacks)". — /Users/luigicolucci/git/Bergamota/.github/workflows/xnu.yml
- dxb `build.sh` takes `CC/CXX/LD/AR/RANLIB/STRIP/LIBTOOL` from `xcrun -find`, honours `DEVELOPER_DIR`, builds mig, ctfconvert/ctfmerge/ctfdump (dtrace repo) and libfirehose_kernel with `xcodebuild install -sdk macosx`, then runs `make install ... SDKROOT=macosx ... KDKROOT=...`. — /Users/luigicolucci/git/Bergamota/research/xnu-local/dxb/build.sh (lines 41-78, 506-524, 620, 713-781)
- `docs/CI_CACHE.md`: host tools cached = mig/migcom, ctfconvert/ctfmerge/ctfdump, availability.pl (built from bootstrap_cmds, dtrace, AvailabilityVersions); ccache wraps only KCC/KC++. — /Users/luigicolucci/git/Bergamota/docs/CI_CACHE.md
- `tools/xnu-local-build.sh` says the toolchain is "Xcode 26.x (DEVELOPER_DIR=...)", that the Xcode 27 SDK no longer ships Kernel.framework's private headers (TrustCache/API.h ...) so 27 only works with USE_KDK=1, and that the local build uses `BOUND_CHECKS=0`, `sccache`. — /Users/luigicolucci/git/Bergamota/tools/xnu-local-build.sh
- `docs/XNU11215_MIGRATION.md`: 11215 CI = macos-15 + Xcode 16.2, KDK 15.3 (24D60) downloaded by dxb. — /Users/luigicolucci/git/Bergamota/docs/XNU11215_MIGRATION.md
- LOCAL TEST: neither CLT 16.2 nor CLT 26.5 contains `iig`, `ctfconvert` or `xcodebuild`; their SDKs (MacOSX15.2.sdk, MacOSX26.5.sdk) contain `Kernel.framework/Headers` but no `PrivateHeaders`, and there is no `System/DriverKit`. In Xcode 15.2 `iig` lives at `Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/iig` (universal x86_64+arm64, minos 13.0), and DriverKit SDKs under `Platforms/DriverKit.platform`. So the CLT alone cannot do an xnu build; full Xcode.app (for iig + DriverKit SDK) is still required.
- Official xnu README: only says the xnu version is derived from the SDK/KDK CFBundleVersion and `SDKROOT` is "path to macOS SDK on disk"; it states no required Xcode or host macOS. — https://raw.githubusercontent.com/apple-oss-distributions/xnu/main/README.md
- blacktop/darwin-xnu-build README: macOS 12.5 to 26.5 listed as "Compiles"; Xcode listed as a dependency without a version; `--lib` mode "macOS 26.0+"; no statement about older build hosts. — https://github.com/blacktop/darwin-xnu-build

### Inferences
- The only components that must come from a newer Xcode are clang/ld/ar family (obtainable from the public CLT), the SDK (obtainable from the CLT SDK pkgs) and iig + DriverKit SDK (only in Xcode.app xip, which needs an Apple ID).

### Gaps
- Exactly which SDK paths xnu-11215/12377 read besides Kernel.framework (e.g. `usr/local/include`, DriverKit headers): not traced; the CI is the reference.

---

## 1. Minimum macOS to RUN Xcode 16.2, 26.1, current Xcode; can the toolchain binaries run on macOS 13?

### Takeaway
Officially: Xcode 16.2 needs macOS 14.5, Xcode 26.0-26.3 need 15.6, 26.4-26.6 need Tahoe 26.2, Xcode 27.x needs Tahoe 26.6 and Apple silicon only. None of that is a hard requirement of the command line binaries: I verified on macOS 13.7.8 Intel that Apple clang 16.0.0 (CLT 16.2, = Xcode 16.2's compiler), Swift 6.0.3, ld, ar, libtool, dsymutil all run, and so do Apple clang 21.0.0 (CLT 26.5) and its ld; only CLT 26.5's `swift-frontend` fails (missing libswiftCore symbols).

### Cited Findings
- Apple system requirements table: Xcode 16 / 16.1 / 16.2 = macOS Sonoma 14.5+; 16.3 = Sequoia 15.2+; 16.4 = Sequoia 15.3+; 26 / 26.0.1 / 26.1 / 26.1.1 / 26.2 / 26.3 = Sequoia 15.6+; 26.4.1 / 26.5 / 26.6 = Tahoe 26.2+; 27 / 27.1 / 27.2 = Tahoe 26.6+; Xcode 15.0-15.2 = Ventura 13.5 - Sonoma 14.x. SDKs: 16.2 -> macOS 15.2, 26.1 -> macOS 26.1, 26.5/26.6 -> 26.5. — https://developer.apple.com/xcode/system-requirements
- Xcode 27 installs only on Apple silicon Macs (third-party summary of Apple's page). — https://mungomash.com/software/xcode/versions/
- xcodereleases data (compiler per release): Xcode 16.0-16.2 = clang 16.0.0 (requires 14.5), 16.3/16.4/26.0-26.3 = clang 17.0.0, 26.4 to 27.2 = clang 21.0.0; Xcode 26.x have a "Universal" xip up to 26.6, 27.x only Apple silicon. Downloads live at download.developer.apple.com/Developer_Tools/ and redirect (302) to a login without an Apple ID session. — https://xcodereleases.com/data.json ; HEAD check of https://download.developer.apple.com/Developer_Tools/Xcode_16.2/Xcode_16.2.xip returned 302
- The macOS Tahoe point: an Xcode that is too old for the OS is refused by the OS (Xcode 16.4 will not launch on macOS 26.2). The reverse (new Xcode on older macOS) is blocked by the Xcode.app launch check/installer, not by the CLI binaries. — https://dev.classmethod.jp/en/articles/xcode-macos-compatibility-check/
- Apple's own CLT installer metadata: CLT for Xcode 16.2 (CLTools_Executables version 16.2.0.0.1.1733547573) has `<os-version before="16.0" min="14.5"/>`; CLT 16.4 min 15.3; CLT for Xcode 26.5 / 26.6 min 26.2 (before 27.0). So the .pkg installer refuses Ventura, but the payload can be expanded manually. — https://swdist.apple.com/content/downloads/61/45/072-44426-A_16242I3TPF/74lkhd4yt26hcrwgpibrqtltf7yth9kc5w/072-44426.English.dist (16.2) ; https://swdist.apple.com/content/downloads/09/08/047-91568-A_Y1CFZWQCD4/4xekpyz43i26dbp4enxfro8eb1q7wiujh5/047-91568.English.dist (26.5)
- LOCAL TEST: CLT 16.2 (public pkg https://swcdn.apple.com/content/downloads/61/45/072-44426-A_16242I3TPF/74lkhd4yt26hcrwgpibrqtltf7yth9kc5w/CLTools_Executables.pkg, 604 MB) expands with `pkgutil --expand-full` on Ventura in 13 s. Mach-O universal x86_64+arm64; `LC_BUILD_VERSION` minos 13.0 (sdk 15.0) for clang, ld, ar, libtool, ranlib, strip, nm, otool, lipo, dsymutil. `clang --version` -> "Apple clang version 16.0.0 (clang-1600.0.26.6), Target: x86_64-apple-darwin22.6.0"; `swift-frontend --version` -> "Apple Swift version 6.0.3 (swiftlang-6.0.3.1.10 clang-1600.0.30.1)", Target x86_64-apple-macosx13.0. All run.
- LOCAL TEST: CLT 26.5 (https://swcdn.apple.com/content/downloads/09/08/047-91568-A_Y1CFZWQCD4/4xekpyz43i26dbp4enxfro8eb1q7wiujh5/CLTools_Executables_Universal.pkg, 776 MB). Binaries are minos 14.0 / sdk 26.4. `clang --version` -> "Apple clang version 21.0.0 (clang-2100.1.1.101)" runs on 13.7.8; ld, ar, libtool, strip, nm, dsymutil run; clang links the system `/usr/lib/libc++.1.dylib` and works anyway. dyld does NOT enforce minos 14.0 for these. `swift-frontend` from CLT 26.5 FAILS: "dyld: Symbol not found: _$sSa034_makeUniqueAndReserveCapacityIfNotB0yyFyXl_Ts5 ... (built for macOS 14.0 which is newer than running OS) Expected in /usr/lib/swift/libswiftCore.dylib" (the OS Swift runtime is missing a symbol).
- LOCAL TEST: end to end on 13.7.8 with Swift 6.0.3 from CLT 16.2: `swiftc -sdk MacOSX15.2.sdk -target x86_64-apple-macosx13.0 s.swift` compiles and the binary runs ("swift hello Version 13.7.8").
- Third-party report: Xcode 15.4 requires macOS 14, but Xcode 15.2 works on Ventura (Bitcoin Core discussion). — https://mirror.b10c.me/bitcoin-bitcoin/30949/
- Apple forum precedent for old Xcode on new macOS via extracted app + `xcode-select` (Xcode 13.2.1 on Ventura). — https://developer.apple.com/forums/thread/707626
- A known failure class for newer toolchain binaries on older OS: dyld "Symbol not found" when the binary links Swift runtime/Foundation symbols that exist only in the newer OS (swift-frontend in the test above is an instance; Swift forum threads report the same for dev toolchains). — https://forums.swift.org/t/missing-symbol-makeuniqueandreservecapacityifnot-in-libswiftcore-dylib/81439 ; https://developer.apple.com/forums/thread/743055

### Inferences
- Toolchains compiled in C/C++ only (clang, ld, ar, libtool, dsymutil, probably iig) tend to run on Ventura because they use only libSystem/libc++ symbols present there; anything linked against the Swift runtime or recent Foundation (swift-frontend, swift-driver users, sourcekit, notarytool) is the fragile part. Xcode 16.2's tool set was built with minos 13.0 (all of CLT 16.2), so Xcode 16.2's `iig` very likely also runs on Ventura (not verified: iig is only in the xip). Xcode 26.x's toolchain is minos 14.0 in CLT 26.5; the 26.1 CLT is not in the public catalog (only the current one per OS is), so 26.1's clang-17 is not testable without the xip; clang 21 from 26.5 works.
- Full Xcode.app 16.2 / 26.x GUI is not a realistic target (launch is blocked and frameworks like DVTFoundation, SwiftUI, IDEFoundation require newer OS); but xcodebuild/xcrun from them is untested.

### Gaps
- Whether `iig`, `xcodebuild`, `xcrun`, `ibtool`, `actool` from Xcode 16.2/26.1 start on Ventura: needs the xip (Apple ID). No published report found.
- Whether Xcode 16.2 `xcodebuild` itself runs on 13 (its frameworks need macOS 14.5 SDK APIs?): untested.

---

## 2. Newer SDK with an older compiler (Xcode 15.2 clang, Apple clang 16, clang 21): what compiles, what breaks, license

### Takeaway
LOCAL TEST matrix on Ventura: C and Objective-C (Foundation, os/log, dispatch, mach) compile, link and RUN for every compiler x SDK combination, including Xcode 15.2's clang 15.0.0 with the macOS 26.5 SDK. C++ with the SDK's libc++ headers is where it breaks: the macOS 26.5 SDK's libc++ headers need a modern clang, and clang 15 and 16 both fail on `<vector>`. The Xcode license does not restrict by OS version, but does restrict to Apple-branded computers and forbids redistribution.

### Cited Findings
- LOCAL TEST (`-mmacosx-version-min=13.0`, `-fobjc-arc`, `#import <Foundation/Foundation.h>` + `<os/log.h>` + `<dispatch/dispatch.h>` + `<mach/mach.h>`): {Xcode 15.2 clang 15.0.0, CLT16.2 clang 16.0.0, CLT26.5 clang 21.0.0} x {MacOSX14.2, MacOSX15.2, MacOSX26.5 SDK} = 9/9 build and run on 13.7.8. Plain C hello world with `<sys/sysctl.h>`: builds and runs with 16/15.2 and 21/26.5; LC_BUILD_VERSION is `minos 13.0 sdk 15.2` / `sdk 26.5`.
- LOCAL TEST (C++20 `<vector> <string> <iostream> <stdexcept>`, `clang++`, min 13.0): all three compilers x SDK 14.2 and 15.2 = OK; clang 15.0.0 and clang 16.0.0 with the MacOSX26.5 SDK = FAIL at `MacOSX26.5.sdk/usr/include/c++/v1/__type_traits/is_convertible.h:32: error: '_Tp' does not refer to a value`; clang 21.0.0 + 26.5 SDK = OK. So: SDK 26.x C++ headers need clang 19+ (LLVM-based) builtins.
- LOCAL TEST: with clang 21 + SDK 26.5, `#include <print>` at `-mmacosx-version-min=13.0` fails with "'to_chars' is unavailable: introduced in macOS 13.3" (and similar availability errors); at `-mmacosx-version-min=14.0` it compiles, links and runs on 13.7.8 ("hello 42"). Availability annotations in the new libc++ gate APIs on the deployment target as intended; running a minos-14 binary on 13 worked here because the needed libc++ symbols exist.
- Note on my first C++ attempt: using `clang` instead of `clang++` fails to link (`std::__1::locale::use_facet` undefined) because `clang` does not add `-lc++`; not an SDK issue.
- Mismatch that matters for xnu: kernel code does not use the SDK's libc++ (xnu uses its own libkern C++ and `-nostdinc++`-style flags, an assumption not verified here), so the C++ header breakage above is irrelevant for kernel TUs, but relevant for host tools and apps.
- SDK pkgs are publicly downloadable inside the CLT bundle: `CLTools_macOSNMOS_SDK.pkg` (macOS 15 SDK for CLT 16.2: contains MacOSX15.2.sdk + MacOSX15.sdk) https://swcdn.apple.com/content/downloads/61/45/072-44426-A_16242I3TPF/74lkhd4yt26hcrwgpibrqtltf7yth9kc5w/CLTools_macOSNMOS_SDK.pkg (57 MB); for CLT 26.5 contains MacOSX26.5.sdk + MacOSX26.sdk https://swcdn.apple.com/content/downloads/09/08/047-91568-A_Y1CFZWQCD4/4xekpyz43i26dbp4enxfro8eb1q7wiujh5/CLTools_macOSNMOS_SDK.pkg (62 MB). SDKSettings.json of MacOSX15.2.sdk: DisplayName "macOS 15.2".
- CLT SDKs lack `Kernel.framework/PrivateHeaders`, `usr/local/include` and DriverKit (LOCAL TEST, see section 0). The repo notes the Xcode 27 SDK dropped the Kernel.framework private headers so the KDK is needed for 26.x/27. — /Users/luigicolucci/git/Bergamota/tools/xnu-local-build.sh
- License (read from the Apple "MACOS SDK AND XCODE AGREEMENT" embedded in the CLT dist file): section 2.A "you are granted a limited, non-exclusive license to use the Developer Software on Apple-branded computers to develop and test application and other software"; section 2.B "you agree not to, install, use or run the Developer Software on any non-Apple-branded computer ... You may not rent, lease, lend, sell, sublicense or otherwise redistribute the Developer Software"; internal copies only to your own employees. Applications built must be for Apple-branded products (per search summaries of the agreement). — https://swdist.apple.com/content/downloads/61/45/072-44426-A_16242I3TPF/74lkhd4yt26hcrwgpibrqtltf7yth9kc5w/072-44426.English.dist ; also https://apple.com.cn/legal/sla/docs/xcode.pdf (PDF not machine-readable here)
- Third-party read of the agreement: use limited to Apple-branded products. — https://forums.swift.org/t/xtool-cross-platform-xcode-replacement-build-ios-apps-on-linux-and-more/79803/2 (one commenter's reading, not a legal ruling)

### Inferences
- Because the host is an Intel Mac (Apple-branded) and Apple itself publishes the CLT/SDK pkgs on a public CDN, downloading and unpacking them on this Mac for own use is within the license text read; hosting them for the CI or copying them to a non-Mac or sharing is not. The license does not tie use to an OS version; the macOS version gate lives in the installer's `allowed-os-versions`.
- SDK-only reuse (new SDK + old Xcode clang) is fine for C/ObjC and old SDK-targets; for C++ and Swift the SDK and compiler must be from the same era (SDK 26.x needs clang 19+, which in practice means Apple clang 21 from CLT 26.4+, or Homebrew/upstream LLVM 19+ for pure C++; Swift modules in a new SDK need the matching or newer swift-frontend, see section 5).

### Gaps
- Exact legal status of unpacking the CLT pkg on an OS the installer refuses: no source found; the license text is silent about OS version.
- C++ and Swift with newer SDKs under older swift-frontend (e.g. Swift 5.9 from Xcode 15.2 against macOS 15.2 SDK swiftmodules): not tested.

---

## 3. Deployment target vs SDK (apps, kernel)

### Takeaway
Compiling against SDK 15.2/26.5 with `-mmacosx-version-min=13.0` works and the result runs on 13.7.8 (tested for C, ObjC/Foundation, C++20, Swift 6.0.3). Availability checks are enforced by the compiler (`'x' is unavailable: introduced in macOS 13.3`); APIs newer than the target must be weak-linked/guarded. For the kernel, deployment target is irrelevant (arm64 freestanding build for iOS-class kernel; host version only affects host tools).

### Cited Findings
- LOCAL TEST: builds with `-mmacosx-version-min=13.0` against SDK 15.2 / 26.5 emit `LC_BUILD_VERSION minos 13.0, sdk 15.2` / `sdk 26.5` and run on 13.7.8 (see section 2).
- LOCAL TEST: Xcode 15.2's `iig` is minos 13.0 / sdk 14.0; all CLT 16.2 tools are minos 13.0 / sdk 15.0, i.e. Apple builds the Xcode 16.2 toolchain with a macOS 13 deployment target. CLT 26.5 tools are minos 14.0 / sdk 26.4.
- Forum reports of the failure class: building with Xcode 16 beta and running on macOS 14.5.1 crashed on a missing Foundation symbol; a dependency targeted at macOS 14.0 caused missing-symbol crashes. Also: "With Xcode 16.4 and prior, system frameworks would get automatically linked weakly ... Later Xcode betas changed this, which can cause launch crashes on older OS versions" (Xcode 26 era). — https://developer.apple.com/forums/thread/762711 ; https://developer.apple.com/forums/thread/743055 ; https://forums.swift.org/t/missing-symbol-makeuniqueandreservecapacityifnot-in-libswiftcore-dylib/81439
- Apple lists deployment-target ranges per Xcode for App Store uploads: Xcode 16.x macOS 10.13-15; 26.x macOS 11-26.x; 27.x macOS 12-27. So Xcode 26 and 27 cannot target macOS 10.x anymore, but macOS 13 is still inside the range. — https://developer.apple.com/xcode/system-requirements
- The repo's workflow builds xnu as plain arm64 with `RC_DARWIN_KERNEL_VERSION=24.3.0` (15.3) and the macOS SDK only for headers/`xcrun`; host tools (mig, ctf*) use `xcodebuild ... ARCHS="arm64 x86_64"`. — /Users/luigicolucci/git/Bergamota/docs/XNU11215_MIGRATION.md ; /Users/luigicolucci/git/Bergamota/research/xnu-local/dxb/build.sh

### Inferences
- For apps: new SDK + low deployment target is the normal Apple workflow and works on 13 as long as the dev avoids unguarded newer APIs; Swift concurrency/Swift 6.2+ features can pull in runtime symbols not in the macOS 13 OS Swift runtime (back-deployment libs `libswift_Concurrency` etc. are bundled in CLT `CLTools_SwiftBackDeploy.pkg`, 48 MB in the catalog; untested).
- For the kernel: link/deployment target of host helper tools (mig, ctf*) built with xcodebuild must be satisfiable on Ventura: set `MACOSX_DEPLOYMENT_TARGET=13.0` (Xcode 15.2 does this already).

### Gaps
- Behaviour of Swift 6.2 / 26.x swiftmodules built for min 13 on a real macOS 13 deployment: not tested.

---

## 4. swift.org toolchains, Homebrew/upstream LLVM, Apple's llvm-project forks: replacement for Xcode 16.2's clang for xnu? (bounds-safety, arm64e, Apple-only flags)

### Takeaway
Upstream/Homebrew LLVM cannot replace Apple clang for xnu: xnu-11215/12377 rely on Apple-fork features (`-fbounds-safety` and `__counted_by`-style attributes in the Apple fork, Apple-specific `-target`/TBI/arm64e flags, `-mkernel` quirks). swift.org toolchains use swiftlang/llvm-project (a different branch than the Apple clang in Xcode). The realistic "newer clang on Ventura" is Apple's own binaries from the CLT payload (verified to run).

### Cited Findings
- `-fbounds-safety` is documented in upstream Clang docs as a design document: "This is a design document and the feature is not available for users yet." — https://clang.llvm.org/docs/BoundsSafety.html
- The Clang bounds-safety adoption guide says the preview implementation lives in a fork of llvm-project on GitHub (swiftlang/llvm-project, branch `stable/20240723`) while upstreaming is in progress. — https://lists.llvm.org/pipermail/cfe-commits/Week-of-Mon-20250113/663672.html ; https://github.com/swiftlang/llvm-project/commit/07f057b10a51976b5aa3edfa921ceec7e304cbbd
- LOCAL TEST: Apple clang 16.0.0 rejects `-fbounds-safety` ("unknown argument"); Apple clang 21.0.0 accepts the flag (it errored only because my test lacked `<ptrcheck.h>` / `__counted_by` definition). The repo disables the checks anyway: `BOUND_CHECKS=0` in `tools/xnu-local-build.sh` and the CI build. — /Users/luigicolucci/git/Bergamota/tools/xnu-local-build.sh
- Repo notes: "New-clang fixes in xnu/patches" (patch 0001 drops a removed `-enable-trivial-auto-var-init...` flag for new clang) and local kernels built with the Xcode 27 clang 21 boot. — /Users/luigicolucci/git/Bergamota/NOTES.md (lines 216, 781); /Users/luigicolucci/git/Bergamota/docs/XNU11215_MIGRATION.md (patch table)
- swift.org installer: the .pkg installs an `.xctoolchain` into `~/Library/Developer/Toolchains/`; "Xcode is not required to run the package installer or use an installed toolchain" but SwiftPM functionality may be limited without Xcode; command-line selection via `xcrun --toolchain swift` or `TOOLCHAINS`. The stated macOS requirements were not in the fetched text. — https://swift.org/install/macos/package_installer/
- Homebrew: formula `swift` 6.2 has bottles for Tahoe/Sequoia/Sonoma (Apple silicon) and Sonoma (Intel); no Ventura bottle. Homebrew support tiers snapshot (Nov 2025): Ventura is Tier 3 on Apple silicon and the timeline predicts Ventura drops out of supported tiers from Sep 2026; Homebrew 7.0 moves Intel Macs to Tier 3. Local observation in the repo: "brew's ipsw/ccache need llvm+rust from source on macOS 13: abandoned". — https://formulae.brew.sh/formula/swift ; https://docs.brew.sh/Support-Tiers ; /Users/luigicolucci/git/Bergamota/NOTES.md (line 20)
- Upstream LLVM prebuilt binaries for macOS x86_64: a March 2025 llvm-bugs report asks "Is prebuilt binary for macos x64 no longer available?"; answer not retrieved. — https://lists.llvm.org/pipermail/llvm-bugs/2025-March/127783.html
- LOCAL TEST: the toolchain override via `~/Library/Developer/Toolchains/*.xctoolchain` + `TOOLCHAINS=<id>` + `xcrun` did NOT pick up my hand-made xctoolchain wrapping CLT 16.2 (xcrun still returned Xcode 15.2's clang). A symlink-farm clone of Xcode.app with the CLT 16.2 binaries swapped in also failed: `xcrun`/`xcodebuild` resolve their own realpath and fall back to the real Xcode.app (and xcodebuild reported SDK cannot be located). Both are my quick attempts with a minimal Info.plist, not conclusive.

### Inferences
- A swiftlang/Apple-fork clang from swift.org is not a drop-in for Xcode's clang version (different branch/date); only worth trying if exact `-fbounds-safety` features are needed, and swift.org's own macOS floor is unverified. Upstream clang cannot build 11215 with BOUND_CHECKS on, and even with BOUND_CHECKS=0 may choke on Apple-only options (e.g. patch 0001 shows flag drift even between Apple versions).
- Simplest mechanism for dxb: `PATH`/`DEVELOPER_DIR` games are fragile; the lower-risk approach is a REAL copy of Xcode 15.2's Developer dir (cp -R, not symlinks) with `Toolchains/XcodeDefault.xctoolchain/usr/{bin,lib}` clang/ld/ar/... overwritten from CLT 16.2 and the 15.2 SDK added to `Platforms/MacOSX.platform/Developer/SDKs` (untested), or invoking the make step directly with `CC=... LD=... SDKROOT=<path>` (xnu's makedefs accept explicit tools; the repo's `--remake` path already calls make directly).

### Gaps
- swift.org toolchain's minimum macOS and whether its clang runs on Ventura: not verified (page text lacked the requirement; no download tested).
- Whether xnu-11215 with Apple clang 15.0.0 (Xcode 15.2's) can compile at all: previous repo notes imply xnu-8796 builds with Xcode 15.2; for 11215 the workflow insists on Xcode 16.2; not tested here (no 11215 tree on disk).
- Official apple/llvm-project branch mapping to Xcode 16.2 (clang-1600) vs Xcode 26.1 (clang-1700): not retrieved.

---

## 5. `xcrun --toolchain`, `DEVELOPER_DIR`, `xcodes`, CLT installer constraints, `pkgutil --expand` on Ventura

### Takeaway
`pkgutil --expand-full` of newer CLT/SDK .pkg files works on Ventura (verified); the CLT installer and `softwareupdate` enforce a minimum macOS through the distribution's `allowed-os-versions`; `DEVELOPER_DIR` must point to a path shaped like `*.app/Contents/Developer` and works only with a REAL Xcode layout; `xcodes` needs an Apple ID and the Xcode 16.2+ xip cannot be installed on 13 by Apple's rules (but extraction is possible).

### Cited Findings
- LOCAL TEST: `pkgutil --expand-full CLTools_Executables.pkg exp` on macOS 13.7.8: 13 s, produces `Payload/Library/Developer/CommandLineTools/usr/{bin,lib,...}`; same for `CLTools_macOSNMOS_SDK.pkg` (SDK) and CLT 26.5 Universal.
- CLT 16.2 / 26.5 public catalog entries: allowed-os-versions `min="14.5" before="16.0"` and `min="26.2" before="27.0"`; the catalog only lists the latest CLT per OS release, so older versions (CLT 26.1) are not available from the catalog; CLT dmgs on developer.apple.com need an Apple ID. — https://swscan.apple.com (catalog URL above)
- LOCAL TEST: `DEVELOPER_DIR=<CLT dir> xcrun --find clang` fails on Ventura with "unable to locate a suitable SDK for the system"/"SDK macosx cannot be located" when the CLT directory has no `SDKs/MacOSX.sdk`; with an SDK directory next to it plus `SDKROOT=/path/MacOSX15.2.sdk` direct invocations of the extracted clang work (see section 2 tests).
- LOCAL TEST: `DEVELOPER_DIR=<dir>/Contents/Developer` where the parent is not named `*.app` fails ("unable to find Xcode installation from active developer path"); renamed to `XcodeH.app/Contents/Developer` xcrun accepted it but resolved symlinked `usr/bin/xcrun` to the real Xcode.
- The repo's CI makes a fake `xcodes` and relies on preinstalled Xcode; dxb only checks `xcodes` exists. — /Users/luigicolucci/git/Bergamota/.github/workflows/xnu.yml (Deps step)
- Homebrew: formula-based tools (`gum`, `jq`, ...) already installed here (`/usr/local/Cellar`) on Homebrew 7.0.11 despite Ventura being Tier 3; sccache was not installed. — local `brew --version`, `ls /usr/local/Cellar`
- Apple support table also says "footnote: iOS 15 and watchOS 8 Simulators not supported on macOS Sonoma 14.x (Xcode 16.2)" (irrelevant to xnu). — https://developer.apple.com/xcode/system-requirements

### Inferences
- The robust way to pick the newer toolchain without a VM is to bypass xcrun for the compile step: call the extracted `clang`, `ld`, `ar`, `libtool` explicitly with `SDKROOT`/`-isysroot` (works on Ventura), and keep Xcode 15.2's `xcodebuild`/`xcrun` for the mig/ctf/libfirehose host-tool builds (these are built from open-source Apple repos and cached).
- iig is the only unavoidable Xcode.app-only piece for 11215/12377.

### Gaps
- Whether Xcode 16.2's xip can be expanded on Ventura with Apple's `xip` command (Xcode 16 xips use a newer format; dxb uses `xcodes --experimental-unxip`): untested; needs Apple ID.
- How KDK .pkg for 15.3/26.5 installs on Ventura (installer min OS check): untested.

---

## 6. Compat layers / "Xcode on unsupported macOS" patches / Hackintosh-free approaches: reliability and risk

### Takeaway
There is no Rosetta-like layer for running newer macOS userspace on older macOS on Intel. The only credible "unsupported OS" approaches are (a) running specific toolchain binaries (CLT payload) directly, which works for C/C++/ObjC but not for Swift-heavy tools, and (b) patching Xcode.app's version check (only documented for older releases). I found no reliable, documented patch for running full Xcode 16/26 on Ventura.

### Cited Findings
- Documented case of using an extracted old Xcode with `xcode-select` on a newer macOS (Xcode 13.2.1 on Ventura). — https://developer.apple.com/forums/thread/707626
- Blog about getting Xcode 15 to launch on macOS 12 by editing the version string, with warnings that features needing newer OS APIs will not work. — surfaced by search (dxb query), unverified: no fetch of the blog
- The Swift forum shows dev toolchains failing on older macOS with missing Foundation/Swift symbols (Swift 6 dev snapshot on macOS 12). — https://forums.swift.org/t/swift-6-0-dev-snapshot-on-macos-12-symbol-not-found-s10foundation10nsnotfoundsivg/73956 ; https://forums.swift.org/t/cannot-use-swift-6-0-dev-toolchain-on-maos-12-monterey/71574
- Xcode tools licensing: use only on Apple-branded computers (no restriction on OS version) — see section 2.
- Apple ended security updates for Ventura on 2025-11-30 (macOS 13 end of support). — https://macos-tahoe.com/blog/macos-ventura-end-of-support-december-2025-migration-guide/ (third-party)

### Inferences
- Risk summary: running unsupported-OS CLT binaries is low-risk (no system changes, nothing installed) but unsupported and can break on a given tool; patching/hacking Xcode.app is high risk and not necessary for the xnu build.
- The Intel host cannot move to macOS 15/26 unless the Mac model is supported (model not checked): the user said no newer macOS.

### Gaps
- No reliable community patch for Xcode 16.x/26.x on Ventura found; searches returned nothing specific.

---

## 7. Community reports of building xnu on older macOS and which toolchain they used

### Takeaway
I found no published report of building xnu-11xxx / 12xxx on an older macOS host; community guidance is "match Xcode to the macOS release you are building" and dxb's CI uses the matching macOS+Xcode runner. Historical guides (PureDarwin XNU 16.7) built on the matching old OS+Xcode. In this repo, only xnu-8796 + Xcode 15.2 locally on macOS 13.5/13.7 is the known-working local path and the new tags have been built in CI (macos-15 Xcode 16.2; macos-26 Xcode 26.1) or locally with Xcode 27.

### Cited Findings
- General guidance in iossec's xnu README: match Xcode to the macOS version you build (e.g. Xcode 15.4 for macOS 15.4), install the matching KDK, select Xcode with `xcode-select -s`. — https://cs.iossec.tech/xnu/latest/source/README.md
- PureDarwin's guide to build XNU 16.7 requires macOS 10.12.6 and Xcode 8.3.3 (matching host). — https://github.com/PureDarwin/Building-XNU-16.7-Kernel-Guide/
- blacktop/darwin-xnu-build: macOS 12.5-26.5 "Compiles" (CI-driven, macOS 14 and later runners per the repo's own workflow comments); `--lib` mode needs macOS 26.0+. — https://github.com/blacktop/darwin-xnu-build
- Repo history: xnu-8796 (iOS 16) local on macOS 13.7.8 + Xcode 15.2 is the verified path; the run was "DENIED by the auto-mode classifier: NOT RUN" at the time, the tree is prepared in `research/xnu-local/dxb` (dxb @663906528, xnu-8796.141.3). `macos-14` runners were retired on 2026-11-02 (their run 41319372456 was green), so new CI for 13.x sources must move to `macos-15` with Xcode 16 whose iig needs the 0086 back-port patch. — /Users/luigicolucci/git/Bergamota/NOTES.md (lines 9, 20)

### Inferences
- Nobody appears to have published an older-macOS-host build of the new xnu; this repo would be a first, so expect to debug rather than follow a recipe.

### Gaps
- No issue/thread of "xnu-11215 build on macOS 13/14 host" found in dxb issues (search engine returned none; GitHub issues tab not fetched).
- Whether the local `research/ios26`, `research/ios18` dirs contain earlier toolchain logs: not inspected.

---

## 8. Recommendation: xnu-11215 on macOS 13.7.8 Intel (and xnu-12377 later)

### Takeaway
Keep CI (macos-15 + Xcode 16.2; macos-26 + Xcode 26.1) as the authoritative builder and use local builds only for fast iteration via a "hybrid" setup: Xcode 15.2 for `xcodebuild`/mig/ctf/libfirehose host tools, Apple clang 16.0.0 + ld + libtool from the CLT 16.2 payload and the MacOSX15.2.sdk for the kernel compile. Tools verified to run on 13.7.8; the missing proven piece is `iig` (only in Xcode.app) and the KDK headers. For xnu-12377 the verified-runnable CLT 26.5 clang 21 (same compiler family as the Xcode 27 clang 21 that already built it locally per the repo notes) is the practical local path, with the KDK.

### Cited Findings
- Components verified to run on 13.7.8: CLT 16.2 clang 16.0.0 (clang-1600.0.26.6), ld, ar, libtool, dsymutil, swiftc 6.0.3; CLT 26.5 clang 21.0.0 (clang-2100.1.1.101), ld, ar, libtool, dsymutil (not swift-frontend). LOCAL TEST, details in sections 1-2.
- clang 16 + MacOSX15.2.sdk and clang 21 + MacOSX26.5.sdk compile `-target arm64-apple-macos13 -ffreestanding -mkernel` objects (`Mach-O 64-bit object arm64`). LOCAL TEST (trivial file, not an xnu TU).
- The exact compiler of CI's Xcode 16.2 is clang-1600.0.26.6 = what the CLT 16.2 ships (xcodereleases: Xcode 16.2 compiler clang 16.0.0). Xcode 26.1's compiler is clang 17.0.0 (not the 21 in CLT 26.5). — https://xcodereleases.com/data.json
- ccache keys use `CCACHE_COMPILERCHECK=content`, so a different clang binary means a different cache namespace (cache entries from CI Xcode 16.2 clang are valid for the CLT 16.2 clang only if the binaries are byte-identical, which they likely are, uncertain). — /Users/luigicolucci/git/Bergamota/docs/CI_CACHE.md

### Inferences
- Realistic ranking for xnu-11215 on Ventura:
  1. CI (macos-15, Xcode 16.2): works today, no local setup (the repo's `runner` input handles pool capacity).
  2. Hybrid local: Xcode 15.2 (xcodebuild, host tools, iig of 15.2 will probably NOT work for 11215 because 16's iig emits IORPC.kernelContent that 11215 uses, and 15's does not: the reverse direction of the 0086 note) + CLT 16.2 compiler. Blocked on getting Xcode 16.2's `iig` (xip with Apple ID, then copy only `iig` and the DriverKit SDK; check that it starts on 13.7.8). Medium effort, plausible.
  3. Newer SDK with Xcode 15.2's clang: C parts compile (tested), but xnu 11215 uses clang 16-era flags/builtins (patch 0001 history), so expect many flag/builtin errors; low value.
  4. Upstream/Homebrew LLVM or swift.org clang: not recommended (no -fbounds-safety, Apple-only flags, Homebrew has no Ventura bottles, building LLVM from source fails per repo note).
  5. Full Xcode 16.2/26.x GUI app on Ventura: not realistic.
- For xnu-12377 on Ventura: same hybrid, using CLT 26.5 clang 21 + MacOSX26.5.sdk + KDK 26.5 headers + `USE_KDK=1` (the Xcode 27 SDK is not needed). Caveat: CI uses 26.1 (clang 17), local notes show clang 21 builds worked with Xcode 27; Xcode 26.4+ also is clang 21, but the SDK lacks `Kernel.framework/PrivateHeaders`, so KDK is mandatory for 26.x.
- For general apps: use the newest SDK that your Xcode 15.2 clang can parse for C/ObjC only (works for 26.5 in my test); for C++ and Swift install the matching compiler from the CLT payload (clang 21 for SDK 26.5; Swift 6.0.3 from CLT 16.2 works with SDK 15.2; Swift 26.x frontend does not start on 13.7.8).

### Gaps
- Not done: full hybrid dxb run, `iig` from Xcode 16.2/26.1 on Ventura, KDK installation on Ventura, ccache binary-identity check between CI Xcode 16.2 clang and CLT 16.2 clang, any xnu TU compile with CLT clang. These need either the Xcode 16.2 xip (Apple ID) or time.
- Left on disk (scratchpad, untracked, Apple software, do not commit): /private/tmp/claude-501/-Users-luigicolucci-git-Bergamota/07ffb600-086d-4312-a16d-06644f390617/scratchpad/clt162 (CLT 16.2 expanded + MacOSX15.2.sdk), /private/tmp/claude-501/-Users-luigicolucci-git-Bergamota/07ffb600-086d-4312-a16d-06644f390617/scratchpad/clt265 (CLT 26.5 expanded + MacOSX26.5.sdk), tests in .../scratchpad/t.
