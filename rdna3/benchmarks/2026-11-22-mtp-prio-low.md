# MTP draft stream at the lowest priority: the solo lane pays, the screen fails (b451)

**Decision: keep `STRATA_MTP_HIP_PRIO` unset — the default priority is optimal on gfx1100.** The first code-level screen on the 0.2.18 line closed the priority axis from the other end: b421 had measured the raised arm (+1.08 ms/window on frozen loads, zh single −2.63%); this round built and measured the never-expressible **lowest** arm and it does not help either. No workload improved in all three pairs, and the solo windows got marginally slower. With raised, default and lowered all measured, no stream-priority setting on this rig beats the default.

## What the patch did

The engine parsed `STRATA_MTP_HIP_PRIO` as a boolean (any nonzero → raise to the queried `hi`). A 7-line overlay on `engine/src/core/mtp.cpp` (dev/0.2.18 `8c3a9ea` plus overlay, binary SHA-256 `b78af7f2…`) turned it into a level — negative maps to the range's lowest end — and made the per-load announcement say `lowered`/`raised`. gfx1100's range is `1..-1` (live probe, b421). The patch was **reverted after the screen**: not adopted, so the tree carries no engine change; the exact base/patched hashes and the reverted diff are in the [structured record](2026-11-22-mtp-prio-low.json).

One harness lesson cost the first window: this template loads **three drafters** (the solo one plus the two `--batch-mtp` slot drafters), so a healthy on-arm log carries the announcement three times, and the gate had allowed one. The fix requires three identical `lowered` lines; the aborted run's off-arm had already reproduced the pinned reference bitwise (rolls 30/22), so the window still paid for the overlay's off-arm integration proof.

## Result (3 interleaved fresh-load pairs, 1,024 tokens, 36 scored requests)

| Workload | off → on per pair (tok/s) | Median | All pairs faster |
|---|---|---:|---|
| Chinese single | 125.1→121.2 · 128.4→123.3 · 113.8→125.0 | **−1.44%** | no |
| Chinese dual | 160.4→160.1 · 162.8→165.9 · 159.5→163.6 | +2.00% | no |
| Code single | 167.6→162.8 · 160.7→160.8 · 144.8→155.8 | +0.06% | no |
| Code dual | 222.2→224.2 · 179.1→175.2 · 198.5→219.3 | +10.52% | no |

TTFT medians all within ±0.8%. Every quality gate passed before scoring: both arms bitwise-reproduce the pinned b442 reference (all 14 raw + 12 sampled probes, forced rolls 30/22), objective v7 unchanged, and — restored for this round because a priority moves neither weight nor capacity — **graph signatures identical between arms**.

## The mechanism, measured on frozen loads

`STRATA_DECODE_TIMING` rode the raw probes (one line per request, both arms identically):

| probe | off ms/window | on ms/window | Δ |
|---|---:|---:|---:|
| zh pw2 | 18.35 | 18.48 | +0.13 |
| zh forced | 18.68 | 18.80 | +0.12 |
| code pw2 | 16.08 | 16.15 | +0.07 |
| code forced | 17.90 | 17.92 | +0.02 |

Lowering the draft stream's priority makes the solo windows **slower, not faster** — the b421 mirror hypothesis is refuted. The contention is asymmetric: raising gave the chain a hardware queue of its own and let its rounds preempt window work (+1.08 ms); lowering defers the chain, and the next stage-0 launch **waits for the chain to finish** (mtp.cpp: `--pipeline-windows 2` comment), so the solo pipeline pays the deferral (~+0.1 ms/window) while gaining nothing back — at the default priority the interleaving was already near-free. The dual lanes tell the same story from the other side: batch windows care about window throughput more than chain latency, and both dual medians improved (code +10.5%) — but each lost one pair, below the predeclared all-pairs bar, and the primary solo lane would regress.

## Notes

- The second window aborted on a **first-of-its-kind event**: a pair-2 off-arm zh solo request stopped naturally at 395 tokens (`finish=stop`, no transport or engine error, 99.8% cache hit, 649 adaptive swaps in that request). The scored phase runs with live adaptation (`--adapt-every 1 --adapt-swaps 48`), the deliberate deployed trade-off; the no-reroll rule failed the window closed. Archived as `run-short-abort` in the harness bundle.
- Production 0.1.43 was restored and verified after every window (running executable hash `58854970…` unchanged, config untouched, port 8101 closed, zero monitor faults).
- Harness bundle: `E:\AIPorject\66\artifacts\strata-mtp-prio-low-20261122-b451` (offline tests 19/19, replaying the real b450 logs plus the aborted on-arm announcement log); sanitized export `public-results.json`.

## Where this leaves the campaign

The priority axis is closed with all three levels measured. Code-level speed work continues to point at the heavier items: the fused-HC redesign (b436's ~0.3 ms/window zh value, needs a non-grid-barrier order-preserving form) or the QSA K/V projection pair (−6 nodes, coverage-risk). Output-changing placement/capacity paths remain behind the explicit quality deliberation.
