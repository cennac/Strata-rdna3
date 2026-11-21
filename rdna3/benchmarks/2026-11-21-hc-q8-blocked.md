# Local HC Q8 confirmation attempt — blocked by a flaky deadlock

**Outcome: not confirmed; the candidate is blocked.** The planned three-pair confirmation of the local Q8 hyper-connection read (`STRATA_HC_Q8=1` + `STRATA_LAB_RDNA_HC_Q8=1`, activations BF16) on the adopted zero-PCIe baseline could not complete: two of the campaign's Q8 loads deadlocked in the serve path and were killed by the 180-second watchdog, both with an identical signature.

## The deadlock

Both deaths print, at the second load's pipelined warmup:

```
strata serve: no progress for 180 s during a request (verify window (pipelined): the CPU experts of layer 24)
    - stopping the engine so the server starts it again (issue #29)
```

The serve thread stalls inside the layer-24 CPU-expert dispatch; the pool's own stall detectors never fire, so the block sits in the dispatch path, not in the pool's phase waits. The pattern across attempts: campaign loads hung on the 2nd Q8 load of one run and the 1st (following a BF16 load) of the next; single targeted reproductions — a fresh Q8 load, a BF16-then-Q8 two-load sequence in one unit, and an exact-copy of the campaign warmup request body (the full 120-item Chinese prompt with presence penalty 1.5) — all ran clean. That makes it a flaky race at roughly a quarter to a half of Q8 loads under campaign conditions, not a deterministic input path.

The binary is the branch build `3e419e1d…` (zero-PCIe patch, pool-queue code present but default off — verified off in every arm's config; the earlier `pool compute queue` marker is absent from all logs). `STRATA_MTP_HIP_PRIO` unset; look-ahead on; the Q8 arms' loader marker (`GiB of Q8_0 hyper-connection projections`) present, the BF16 arms' absent, as asserted.

## What throughput data exists

One complete Q8 pair ran before the first deadlock (single loads, not a median):

| Workload | BF16 | Q8 | Change |
|---|---:|---:|---:|
| Chinese single | 126.08 | 118.36 | −6.1% |
| Chinese dual | 161.10 | 164.18 | +1.9% |
| Code single | 155.58 | 157.60 | +1.3% |
| Code dual | 221.37 | 221.47 | +0.0% |

A single pair never decides adoption; the earlier one-load screen's +3.55% Chinese-dual remains unconfirmed.

## Standing decision

Local Q8 stays off (BF16 remains the template default). Any future confirmation requires first root-causing the layer-24 deadlock — the productive next step is a gdb attach onto a hanging campaign-style load (the watch­dog leaves a 180-second window), which three simplified reproductions failed to trigger; reproducing it may need the full campaign harness. Production 0.1.43 was restored and verified after every window. Attempt records: [campaign b429/b430 evidence and diagnostics](2026-11-21-hc-q8-blocked.json).
> **Resolved (later the same day):** the deadlock was root-caused and fixed (`res_put` stream-ordered), the confirmation ran, and local Q8 was rejected on throughput. See [the resolved record](2026-11-21-hc-q8-resolved.md).
