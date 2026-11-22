# Progress log

Newest entries at the top. Add an entry at the end of every working session.

## Template

```
## YYYY-MM-DD — <short title>

**Done:**
- ...

**Working / verified:**
- ...

**Unverified or broken:**
- ... (include what Dave should test in the sim and the expected result)

**Next step:**
- ...
```

---

## 2026-11-21 — Design complete, repo created

**Done:**
- Product design worked out with Dave on a whiteboard and written up in `docs/design.md`.
- Recorder spec written in `docs/recorder.md`.
- Initial decisions recorded in `docs/decisions.md` (D-001 onward).

**Working / verified:**
- Nothing built yet.

**Next step:**
- Recorder milestone 1: create `/recorder` with a CMake + vcpkg C++ project, locate the MSFS 2020 SimConnect SDK,
  connect to the sim, read a few SimVars (parking brake, on-ground, fuel weight, position), and log them.
  Write `/recorder/README.md` with build and run steps.
