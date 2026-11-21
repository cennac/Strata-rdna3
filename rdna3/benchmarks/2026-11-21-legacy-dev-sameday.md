# Same-day interleaved legacy 0.1.43 vs dev baseline — production is degraded; dev leads every lane

**Finding: the production engine is running at roughly half its 2026-11-19 measured speed on this rig, and the development baseline now beats it on all four lanes.** Three interleaved fresh-load pairs plus a direct probe of the live service. The engine and template are unchanged by this campaign; this is a measurement and a production incident report.

## Throughput (1,024 tokens, three pairs, same day)

| Workload | Legacy 0.1.43 (median) | Dev baseline (median) | Dev vs legacy |
|---|---:|---:|---:|
| Chinese single | 60.3 | 124.6 | **+106.6%** |
| Chinese dual | 130.5 | 158.0 | +21.1% |
| Code single | 74.5 | 161.9 | **+117.3%** |
| Code dual | 171.5 | 216.7 | +26.4% |

Every legacy number in every pair sits far below its 2026-11-19 same-methodology values (139.1/185.8/195.5/235.8 medians then; 57–75/129–131/70–75/147–172 per-load now). A direct probe of the **live production service on port 8100** — untouched config, its own profile — measured 66.9 tok/s decode, matching the campaign's legacy arms, so this is not a test-harness artifact.

## Where the time goes

Both engines logged per-window decode timing in the same campaign:

- Legacy: 34.54 ms/window (2.08 tokens/window, 492 windows per zh request) — verify 32.11 of which GPU-reach wait 14.90 and ~16.3 ms sits outside every counter.
- Dev: 16.20 ms/window (2.05 tokens/window) — GPU-reach wait 12.26, host ~1.0, ~2.9 unattributed.

Acceptance is equivalent (2.08 vs 2.05 tokens/window), so the whole difference is per-window cost. On 2026-11-19 the same legacy engine ran 14.06 ms/window with ~1.2 ms unattributed; the ~15 ms of new unattributed verify time is where legacy's fused-A8 HC read runs — the one legacy kernel with a cross-workgroup grid barrier requiring all 168 workgroups co-resident.

Telemetry during the Chinese-single bursts is the discriminator: the legacy arms left the GPUs **45–54% busy at 92–108 W**, while the dev arms on the same cards minutes apart drove **57–84% busy at 169–241 W**. The legacy engine's GPUs are half idle while it decodes — spin-waiting, not computing.

## What is established and what is inferred

Established: the same-day numbers above; the live-service probe; the per-window timing split; the GPU busy/power gap; the historical 11-19 legacy values under the same methodology. Inferred (marked as such): the degradation mechanism — the grid-barrier kernel starving when co-residency is perturbed (a desktop session — gnome-shell, Xwayland — holds these cards, and an engine log this week recorded `GPU work paused by the graphics driver: 725 ms`); and the degradation window — between 2026-11-19 (last official-arm measurement, R26/R29 era) and now. Not identified: the trigger that changed in that window. No amdgpu errors in dmesg; no zombie GPU processes; thermals and power caps normal; production restarts do not recover it (it was freshly restarted when probed).

Legacy 0.1.43 is also text-nondeterministic under the frozen protocol on this rig ([11-20 record](2026-11-20-legacy-fused-ablation.md)), so no cross-engine frozen parity was attempted; all 36 scored requests reached full budget.

## Consequences

1. **Production incident**: the live service is at ~half speed. Worth investigating on the legacy side (its barrier kernel's co-residency assumptions on a desktop-shared GPU) or by scheduling a promotion.
2. **The dev branch is now the faster engine on this rig on every lane**, same-day, matched conditions — with the correctness evidence accumulated across this campaign series (frozen four-mode parity byte-identical in every screen, rollback checks, graph-node verifications). Whether to promote it to production is the owner's call; this record is the performance evidence.

Per-load numbers, engine logs and telemetry: [structured record](2026-11-21-legacy-dev-sameday.json). Production 0.1.43 was restored and verified loaded after the campaign; config hash unchanged; port 8101 closed; no monitor faults.
