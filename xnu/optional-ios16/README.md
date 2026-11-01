Patches for the iOS 16 userland on the xnu-11215 kernel (goose-archives/Bergamota). NOT in the iOS 18 series:
0310 serves the pre-xnu-10002 out-of-line mach_ports_register/lookup ABI; with an iOS 18 root launchd dies with
mach_ports_lookup() failed: -300 (boot 1, 2026-11-02).
0311 accepts the iOS 16.5 0x46c IOMFB swap record.
