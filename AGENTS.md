# AGENTS.md

## Project context

- This repository is an embedded automotive project targeting STM32F103 microcontrollers.
- It includes at least:
  - A bootloader / flashing component (FBL) and an application (APP).
  - Modules that resemble AUTOSAR-style BSW/ASW components (e.g. EcuM, Dcm, Dem, Nvm, etc.).
  - CAN, UDS and ISO-TP style diagnostics for flashing and runtime diagnostics.
- Primary goals for AI agents:
  - **Static, text-based analysis only**, focused on AUTOSAR, UDS, ISO-TP, ISO 26262, and MISRA C aspects.
  - Produce long-form technical reports in Markdown that help a human engineer understand the architecture and gaps.
  - Do **not** modify files, commit, push, or open PRs unless explicitly asked in a future task.

## Dev environment notes

- Tooling:
  - Code is generated and built with STM32CubeIDE / GCC ARM.
  - The build environment is not guaranteed to be available in an agent sandbox.
- When a task mentions "build" or "run tests":
  - You may try obvious commands (`make -j`, `cmake --build`, or existing project scripts) but:
    - If the toolchain is missing or the build fails for environment reasons, stop trying to fix it.
    - Continue with **static analysis of the source code only**.
- Never waste time installing IDEs or heavy toolchains; focus on reading and reasoning about the code.

## Where to look in this repo

When performing architecture or standards analysis:

- Start with:
  - `Core/`, `Src/`, `Inc/`, `Drivers/` (STM32Cube-style structure).
  - Any `APP`, `FBL`, `BSW`, `Diag`, `EcuM`, `Dcm`, `Dem`, `Nvm`, `Can*`, and ISO-TP related folders/files.
- Pay special attention to:
  - Bootloader / flashing logic (FBL, ISO-TP, UDS services 0x34/0x36/0x37, 0x31, etc.).
  - Runtime diagnostics (UDS services like 0x10/0x11/0x22/0x23/0x27/0x28/0x2E/0x19).
  - Safety mechanisms (CRC checks, watchdogs, RAM tests, plausibility checks).
  - Any comments or docs referencing ASIL, ISO 26262, MISRA, AUTOSAR, or similar.

## Code style / constraints

- C code is intended to be "MISRA-leaning", embedded-safe:
  - No dynamic memory allocation is expected in production paths.
  - Prefer small, testable functions, strong typing, and explicit conversions.
  - Watchdog, CRC, and low-level hardware access must be handled deterministically.
- When suggesting improvements:
  - Respect the constraints of a small Cortex-M3 microcontroller (flash/RAM limits, hard real-time constraints).
  - Avoid suggestions that require OS-level features not available on bare metal.

## Task: Standards-oriented static review

When the user asks for a standards-related review (AUTOSAR, UDS, ISO-TP, ISO 26262, MISRA C), follow this procedure:

1. **Scope**
   - Treat the task as **read-only**. Do not:
     - Edit files.
     - Create new files.
     - Run `git commit`, `git push`, or open PRs.
   - All output should be **one or more long Markdown reports in the chat**, unless explicitly told otherwise.

2. **Repository scan**
   - Map the high-level architecture:
     - Identify major components (APP/FBL, BSW-like modules, drivers).
     - Identify where diagnostics and communication stacks live (CAN, ISO-TP, UDS).
   - Note any existing documentation or comments relevant to safety, AUTOSAR, or diagnostics.

3. **AUTOSAR perspective**
   - Identify:
     - Any modules that correspond to AUTOSAR concepts (EcuM, Dem, Dcm, CanIf, CanTp, Nvm, etc.).
     - The layering pattern: MCU/MCAL, BSW, Services, Application.
   - For AUTOSAR-style analysis:
     - Describe which parts of the repo roughly align with AUTOSAR layers and module responsibilities.
     - Highlight obvious layering violations (e.g. application code directly touching HAL or hardware that should be abstracted).
     - Summarize configuration-like code vs. “pure logic”.
   - Output for AUTOSAR:
     - **What is present** (architecture aspects that look AUTOSAR-like).
     - **What is missing/not aligned** (expected AUTOSAR elements that don’t exist or are mixed together).
     - **Risks / impact** of the gaps.
     - **Concrete improvement suggestions**, but only as text (no code edits).

4. **UDS perspective**
   - Locate all UDS-related code:
     - Service handlers, DID/RID tables, session/security state machines, NRC handling.
   - For each implemented service (0x10, 0x11, 0x22, 0x23, 0x27, 0x28, 0x2E, 0x31, 0x34/0x36/0x37, 0x19, etc.):
     - Explain how the request is processed and how the response is generated.
     - Note session and security preconditions and any timing constraints (e.g. P2/S3).
   - Output for UDS:
     - **List of implemented services and DIDs/RIDs**.
     - **What is present** (correct flows, proper NRC usage, clear separation of concern).
     - **What is missing or weak** (missing NRCs, missing sessions/security checks, missing services expected for flashing/diagnostics).
     - **Risks / impact** and **improvement suggestions**.

5. **ISO-TP perspective**
   - Find the ISO-TP or custom transport layer implementation.
   - Analyze:
     - TX/RX state machines (SF/FF/CF/FC).
     - Handling of SN (sequence number), block size (BS), STmin, timeouts, and buffer limits.
   - Output for ISO-TP:
     - **Describe the state machines and message flow in detail**.
     - **What is present** (correct handling of segments, flow control, length fields).
     - **What is missing / risky** (edge cases, timeouts, path for abort, unusual sizes like 4095 bytes).
     - **Improvement suggestions** for robustness and clarity.

6. **ISO 26262 perspective (Part 2 / management and basic safety mechanisms)**
   - From code and docs in this repo, identify:
     - Safety mechanisms (watchdogs, CRC over flash, RAM tests, plausibility checks, graceful degradation, etc.).
     - Any hints of ASIL decomposition, safety goals, or safety requirements (even if informal).
   - Output for ISO 26262:
     - **What safety-related mechanisms are present in code** and how they are used.
     - **Which typical artefacts are missing from the repo** from a process perspective (safety plan, safety case, traceability, etc.) – list them explicitly as gaps.
     - Make it clear that this is a **code-level, best-effort engineering review**, not a formal compliance statement.

7. **MISRA C perspective**
   - Perform a **static, heuristic** MISRA-style review of the C code:
     - Identify obvious rule violations or risk patterns (implicit conversions, magic numbers, deep nesting, mixed responsibilities, non-const globals, unsafe casts, etc.).
     - Suggest safer patterns and refactor ideas that move the code closer to MISRA.
   - If the repo includes any static analysis reports (e.g. cppcheck, commercial tools), summarize them and connect them to the code.
   - Output for MISRA:
     - **What is present** already aligned with MISRA-friendly practices.
     - **What is missing / violating common MISRA rules**.
     - **Concrete suggestions** to improve, but no actual patch or PR.

8. **Cross-standard summary**
   - End the report with a **combined summary**:
     - Table or bullet list of:
       - AUTOSAR, UDS, ISO-TP, ISO 26262, MISRA C.
       - For each: “present”, “partial”, and “missing” aspects.
     - Short prioritized list of **top 5–10 improvements** that would give the biggest benefit.

## Output format

- Use **Markdown** for reports.
- Structure the report with clear headings:
  - `# Overview`
  - `## AUTOSAR`
  - `## UDS`
  - `## ISO-TP`
  - `## ISO 26262`
  - `## MISRA C`
  - `## Cross-standard summary & recommendations`
- There is **no strict length limit**: be as long and detailed as needed to fully explain the repository’s behavior and gaps to an experienced embedded automotive engineer.
- Do not assume the user has the standards open while reading; include enough textual context so the report is understandable on its own.
