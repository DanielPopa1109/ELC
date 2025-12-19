# AGENTS.md

## Project context

- This repository is an embedded automotive project targeting **STM32F103** microcontrollers.
- It includes at least:
  - A bootloader / flashing component (FBL) and an application (APP).
  - Modules that resemble AUTOSAR-style BSW/ASW components (e.g. EcuM, Dcm, Dem, Nvm, etc.).
  - CAN, UDS and ISO-TP style diagnostics for flashing and runtime diagnostics.

## Agent permissions for this branch

On this branch, the agent is explicitly allowed to **edit source files** and to use **git** to create and publish changes.

### Allowed git operations

The agent may run (and is encouraged to use when appropriate):

- `git status`, `git diff`, `git log`, `git show`
- `git fetch`, `git pull`
- `git add`, `git commit`
- `git push` (see safety rules below)

### Git safety rules (must follow)

- **Never** use `--force` / `--force-with-lease` unless the user explicitly requests it.
- **Never** push directly to protected branches (e.g. `main`, `master`, `release/*`) unless the user explicitly requests it.
- Prefer working on the **current branch** (or create a feature branch if needed).
- Keep commits **small and reviewable**:
  - one functional change per commit when possible,
  - use clear commit messages (module + intent), e.g. `Dcm: fix NRC handling for 0x27`.
- Before pushing:
  - ensure `git status` is clean (only intended files staged/committed),
  - include a short summary of what was changed and why.

> Note: Depending on the Codex runtime/sandbox settings, networked git operations (fetch/pull/push) may require explicit approval or enabled network access.

## Primary goals for the agent

The agent may perform **both**:
1) standards-oriented analysis (reports), and
2) implementation work (code changes) to improve alignment with automotive standards.

When implementing changes, prioritize:
- AUTOSAR-like layering and module responsibilities
- UDS (ISO 14229) correctness (services, NRCs, session/security rules, timing behavior)
- ISO-TP (ISO 15765-2) correctness (segmentation, SN/BS/STmin, timeouts, buffer limits)
- MISRA C friendliness (determinism, explicit typing, minimized undefined behavior risk)
- ISO 26262-aligned safety mechanisms and engineering hygiene (defensive checks, diagnostics, watchdog/CRC/RAM-test integration, clear fault handling)

## Dev environment notes

- Tooling:
  - Code is generated and built with STM32CubeIDE / GCC ARM.
  - The build environment may not be present in the agent sandbox.
- When a task mentions "build", "run tests", or "verify":
  - Try obvious commands if available (`make -j`, `cmake --build`, project scripts).
  - If the toolchain is missing or the build fails due to environment limitations:
    - stop chasing environment issues,
    - continue with **static reasoning** + targeted code fixes that are low-risk and well-contained,
    - document what could not be verified.

## Where to look in this repo

- Start with:
  - `Core/`, `Src/`, `Inc/`, `Drivers/` (STM32Cube-style structure).
  - Any `APP`, `FBL`, `BSW`, `Diag`, `EcuM`, `Dcm`, `Dem`, `Nvm`, `Can*`, and ISO-TP related folders/files.
- Pay special attention to:
  - Bootloader / flashing logic (FBL, ISO-TP, UDS services 0x34/0x36/0x37, 0x31, etc.).
  - Runtime diagnostics (UDS services like 0x10/0x11/0x22/0x23/0x27/0x28/0x2E/0x19).
  - Safety mechanisms (CRC checks, watchdogs, RAM tests, plausibility checks, graceful degradation).
  - Any comments or docs referencing ASIL, ISO 26262, MISRA, AUTOSAR, or similar.

## Code style / constraints

- Embedded-safe constraints:
  - No dynamic allocation in production paths unless explicitly justified.
  - Prefer small, testable functions, strong typing, and explicit conversions.
  - Hardware access must be deterministic; handle watchdog/CRC/low-level access explicitly.
- STM32Cube-generated code:
  - Avoid editing generated sections unless necessary.
  - Prefer user-code sections or project-owned modules for changes.

## Implementation workflow (when making code changes)

1. **Plan**
   - Identify the standard requirement/behavior being targeted (AUTOSAR/UDS/ISO-TP/MISRA/ISO 26262 mechanism).
   - Propose a minimal change set and file list.

2. **Implement**
   - Make the code change.
   - Add/adjust comments that clarify intent, preconditions, and safety assumptions.
   - If a behavioral change is made in diagnostics/transport:
     - update any DID/RID/service tables accordingly.

3. **Verify (best effort)**
   - Run available build/tests or at minimum perform a local consistency check:
     - compile-likelihood (includes, prototypes, linkage),
     - buffer bounds and integer range checks,
     - state machine completeness (timeouts, abort paths).

4. **Document**
   - In the final output, include:
     - what changed,
     - why (standard-alignment motivation),
     - what was verified,
     - remaining risks / TODOs.

5. **Commit & push (allowed)**
   - Stage only intended files.
   - Commit with a descriptive message.
   - Push to the current branch (or a feature branch) following the safety rules above.

## Standards-oriented review mode (when asked for a report)

If the user asks for a standards report instead of (or before) changes, produce a Markdown report with:

- `# Overview`
- `## AUTOSAR`
- `## UDS`
- `## ISO-TP`
- `## ISO 26262`
- `## MISRA C`
- `## Cross-standard summary & recommendations`

## Compliance disclaimer (must keep honest)

- The agent can **improve alignment** with these standards, but cannot truthfully claim “full compliance” or “certification” from code edits alone.
- ISO 26262 compliance depends on process artefacts (plans, reviews, traceability, verification evidence, safety case) and tool qualification, which may not exist in this repo.
