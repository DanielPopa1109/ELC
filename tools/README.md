# Local validation tools

Run from the ELC root in PowerShell:

```
python tools/build_firmware.py
python tools/verify_firmware.py
python tools/run_host_tests.py
python tools/audit_sources.py
git diff --check
```

The build searches ARM_GCC_BIN, otherwise `.tools/xpack-arm-none-eabi-gcc-15.2.1-1.1/bin`. A local xPack GNU Arm Embedded15.2.1 compiler was downloaded because STM32CubeIDE/bundled Cortex-M compilers were not found in the installed ST locations. The existing AArch64 Linux compiler is not suitable for these images. Vendor archive: https://github.com/xpack-dev-tools/arm-none-eabi-gcc-xpack/releases/download/v15.2.1-1.1/xpack-arm-none-eabi-gcc-15.2.1-1.1-win32-x64.zip . No system toolchain was installed.

The native runner uses `.tools/native/ziglang/zig.exe` (Zig0.16.0), obtainable with `python -m pip install --target .tools/native ziglang==0.16.0`. It sets both Zig caches under build/tests, compiles with assertions and runtime bounds checks, then executes five Windows binaries: `fee` (Fee journal), `cantp` (ISO 15765-2), `bsw_stack` (Can through Dcm/Dem/NvM, UDS and the XCP/telemetry paths), and `boot_1` / `boot_2` (the resident and SRAM instances of `boot_protocol.h`). Restricted desktop process environments can prevent Zig from locating its own executable; executing the local compiler outside that process restriction was required here. Hardware and application input samples are mocks, while the BSW implementation and both programming-core instances are real source.

Build output is ignored under build/. It includes ELF, MAP, disassembly, size/stack-usage files, HEX/BIN and a SHA-256 manifest per image. APP.program.bin is padded to the programming span excluding the CRC footer. APP.bootable.bin includes the computed footer for independent debugger/recovery use and must not be supplied whole to RequestDownload; the resident programmer owns that word. No output has been deployed or flashed.

`audit_sources.py` writes `docs/modernization/SOURCE_INVENTORY.json` and fails the run when the architecture rules are broken: RTE/DET/MemMap or legacy API names, a CAN transmit/receive HAL call outside `Can.c`, a Flash erase/program outside `Fls.c`, a forbidden upward include between BSW modules, application data (`SMon_`, `EcuM_`) in `Dcm.c`, a raw CAN identifier outside `CanIf_Cfg.c`, a `*_main` function name in BSW, or a library/HAL/function-pointer dependency in the boot core. It also checks that the APP's F101 FBL-version constant matches the resident FBL version. Rules and rationale: [AUTOSAR-lab alignment](../docs/autosar/AUTOSAR_LAB_ALIGNMENT.md). Run it last so the inventory hashes match the final sources. `git diff --check` passed in the 2026-10-24 continuation.

The FBL ELF has a deliberate read/write/execute load segment for its copied SRAM code/data image; relocation verification checks its actual dependencies. Native tests do not execute real Cortex-M instructions, MSP/VTOR handover or physical Flash programming. Source inventory is a reproducible map, not a claim of hardware validation.
