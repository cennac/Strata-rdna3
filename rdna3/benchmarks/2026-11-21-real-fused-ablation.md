# Fused-vs-split A8 HC ablation on the real production binary

**Decision: the HC fused-A8 single-launch port to the development engine is rejected on measured grounds.** On the real production binary (`58854970…`, engine 0.1.43, its own runtime config), `STRATA_GR_FUSED=0` costs ~0.3 ms/window on Chinese and nothing on code. That is below the port's risk threshold (a grid barrier in the development engine's overlapping graphs needs co-residency proof), and it supersedes the earlier "inconclusive" record, which had unknowingly tested an upstream 0.1.62 package whose legacy HC kernels do not exist.

## Method

Four fresh loads of the real binary — fused, split, split, fused (bracketed pairs) — from the service's own runtime config (`strata-runtime/4d87c5fab2dae93e.json`): the R8 A8 configuration (`STRATA_GR_RDNA=1`, `_Q8=1`, `_A8=1`, `_INJ_UP=1`, `GR_FUSED` toggled per arm), pool 8, adaptation on, caches off, the shared profile seed, the standard prompts and sampling. 1,024 tokens per request, four lanes per load, per-window decode timing on. Every arm's engine hash asserted (`58854970…`).

## Results

| Workload | Fused (both loads) | Split (both loads) | Split − fused |
|---|---:|---:|---:|
| Chinese single | 131.5 / 139.4 | 130.5 / 129.4 | **−4.0%** (median) |
| Chinese dual | 187.3 / 186.2 | 183.5 / 183.2 | −1.8% |
| Code single | 181.1 / 173.3 | 176.8 / 179.3 | +0.5% |
| Code dual | 236.8 / 237.3 | 237.4 / 234.4 | −0.5% |

Per-window timing isolates the mechanism: the Chinese single-request windows ran at 14.10 / 13.92 ms fused vs 14.26 / 14.27 ms split (**~0.3 ms/window**); the code windows are 13.2 vs 13.2–13.3 (nothing). The effect is real but small and Chinese-only.

Incidental same-day reference (not methodology-matched to the dev campaign's own template): the real production binary under this protocol measured zh 130–139 / 183–187, code 173–181 / 234–237, consistent with its historical service numbers.

## Why this closes the question

The development engine's HC read costs ~2.9 ms/window (light profile), but most of that is the BF16-quantization bandwidth difference already screened separately (local Q8/A8 lost on service throughput). The fusion itself — one launch plus grid barrier instead of two launches — is what this ablation isolates on the only binary that has the kernel, and it is worth at most ~2% on one lane. Porting a cross-workgroup spin barrier into overlapping development graphs for that is not justified. The earlier [blocked record](2026-11-20-legacy-fused-ablation.md) is superseded; its "inconclusive" verdict stood only because that campaign's preflight pointed at the upstream package.

All requests reached full budget; production 0.1.43 was restored and verified loaded after the window. Per-load data: [structured record](2026-11-21-real-fused-ablation.json).
