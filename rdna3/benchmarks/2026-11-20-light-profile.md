# Light profile of the adopted baseline (2026-11-20)

A single-load diagnostic on the adopted template binary (`528530d9…`, fixed-zero PCIe graph on): `STRATA_VERIFY_PROFILE=1` + `STRATA_VERIFY_PROFILE_LIGHT=1` (stamps on, fusion gates kept), frozen requests (adaptation and suffix drafting off), driven through the same server path as the scored campaigns. Seven piped requests (128 warm-up, zh/code 384 twice each, zh/code 1024). Production was stopped for 2 min 53 s and restored; 0.1.43 verified loaded afterwards.

## Reproducibility

The repeated 384-token requests reproduced their window counts, rollback counts, total wall time and every stage bucket to 0.01 ms (3915 vs 3918 ms over 197 windows) — the frozen determinism seen in the switch campaigns holds under instrumentation. Instrumented windows run ~1.8 ms slower than the unprofiled frozen arms (19.87 vs 18.07 ms/window), so bucket values are ~9% inflated; shares and comparisons remain valid.

## Stage tables (zh 1024, 519 windows, 20.10 ms/window instrumented)

| bucket (ms/window) | stage 0 (CUDA0, layers 0–25) | stage 1 (CUDA1, layers 26–47) |
|---|---:|---:|
| VRAM hits (resident experts) | 1.53 | 1.31 |
| HC read (norm+down+up, both halves) | 1.67 | 1.22 |
| waitA (host plan) | 1.00 | 0.41 |
| waitCPU (CPU experts) | 0.57 | 1.67 |
| copy+combine | 0.35 | 0.49 |
| head | — | 0.86 |
| waitB / PCIe grp | 0.05 | 0.04 |
| router+plan+bell | 0.61 | 0.53 |
| other compute (gemv/conv/z/rec/out-proj) | 2.22 | 1.87 |

The code 1024 request (415 windows, 19.84 ms/window) shows the same shape: waitA 1.09/0.61, waitCPU 1.20/1.68, HC 1.71/1.20. At equal window width the two prompt families cost the same per window; the scored code-single lanes differ by carrying ~3× more tokens per window (acceptance), not by dearer windows.

## Findings

1. **The adopted fixed-zero graph measures as removed**: waitB and the PCIe group interval are down to 0.04–0.05 ms (stamp boundaries only).
2. **The surviving wait chain is ~2.4 ms/window instrumented (~2.2 real)**, now precisely split: stage-0 windows wait ~1.0 ms for the host to publish the plan (waitA) and stage-1 windows wait ~1.7 ms for the CPU experts (waitCPU). Both are host-pool service latency on the serve thread — the four-verifier competition described in rounds 40–41 — not GPU compute.
3. **HC reads cost ~2.9 ms/window combined** at this width; R33 put the dev-vs-legacy HC gap at +0.38 ms (two launches vs the legacy one-launch fused A8 read, unmeasured since).
4. The remaining solo-lane gap to legacy (~5 ms per code window) is consistent with the wait chain plus the HC residual plus the commit/emit/draft host time that rounds 38 measured outside the verify interval.

## What this ranks next

1. The pool serve model (publish earlier, serve per stage, or pre-run across windows) — the only item that can recover the full ~2 ms wait chain; the tested 4+4 dual-pool partition failed, so this needs a design that keeps one pool but changes when work is queued.
2. The HC fused A8 single-launch port — bounded by the legacy fixed-work measurement recommended earlier (~0.4–1 ms at large windows).
3. Everything else (kernels, copy+combine, router) is already at or better than legacy levels per R33 and this table.

Raw per-request tables are in the [structured record](2026-11-20-light-profile.json); the engine log is archived with the campaign evidence.
