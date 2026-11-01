# xnu/optional

Patches that are NOT in any series (the build scripts and CI only read `patches/`, `patches-11215/`, `patches-12377/`).

* `0086-xcode16-iig-iorpc.patch` - from TigerShota/ios-sweet (MIT). `git apply --check` passes on the xnu-8796 tree.
  Makes the macOS 13.5 (iOS 16) kernel buildable with Xcode 16's iig, i.e. on GitHub's `macos-15` runner
  (`macos-14` is retired on 2026-11-02). Not enabled: the local iOS 16 builds use Xcode 15.2 and are verified on
  hardware. To use it: copy it into `xnu/patches/` (e.g. as `0226-...`) and switch the 13.x runner in
  `.github/workflows/xnu.yml` to `macos-15` with `BUILD_WERROR=0` (see TigerShota's xnu.yml).
