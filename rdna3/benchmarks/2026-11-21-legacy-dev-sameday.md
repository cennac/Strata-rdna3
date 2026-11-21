# Same-day interleaved comparison — CORRECTED: no production incident

**Correction.** This campaign's original conclusion ("production degraded to half speed; dev leads every lane") was wrong, for two reasons discovered while re-verifying: (1) the campaign preflight's `engine` path has pointed at the **2026-11-19 update package — an upstream 0.1.62 build (`bdec5453`), not the production binary** — since it was captured, so every "official/legacy" arm in this campaign series measured the stock upstream engine, which has always run ≈57–75 single-lane under this methodology (identical to the R26 table's stock row: 60.3/131.97/75.30/174.41); and (2) the initial live-service probe (66.9 tok/s) used a 192-token short request whose decode average is not comparable to the 1,024-token benchmark protocol. A proper four-lane probe of the **live production service** (real 0.1.43, `/data/strata-mtp-cache-20261110/strata`, sha `58854970…`, measured in place on port 8100 with its own config) gave:

| Lane | Live production 0.1.43 (decode tok/s) | Historical reference |
|---|---:|---:|
| Chinese single | **139.5** | 139.1 |
| Code single | **188.3** | 195.5 |
| Chinese dual (per lane) | 101.2 / 107.2 | 185.8 aggregate |
| Code dual (per lane) | 125.5 / 143.0 | 235.8 aggregate |

**Production is at its historical speed. There is no incident.** The GPU "45–54% busy" telemetry and the ~16 ms unattributed per-window time belong to the upstream 0.1.62 build, not to 0.1.43 — consistent with the R35/R38 analyses of stock pipelining, not evidence of a barrier regression.

## What the campaign actually measured

The interleaved pairs remain valid as a same-day comparison of **dev baseline vs stock upstream 0.1.62** (all matched: prompts, penalties, seed, caches off, 1,024 tokens, three fresh-load pairs):

| Workload | Upstream 0.1.62 (median) | Dev baseline (median) | Dev vs upstream |
|---|---:|---:|---:|
| Chinese single | 60.3 | 124.6 | +106.6% |
| Chinese dual | 130.5 | 158.0 | +21.1% |
| Code single | 74.5 | 161.9 | +117.3% |
| Code dual | 171.5 | 216.7 | +26.4% |

This matches the R26-era stock-vs-dev relationship and confirms the adopted combination's cumulative gains over stock; it says nothing about 0.1.43.

## Where dev actually stands against real production

Combining the same-day live production probe with the dev campaign medians: dev trails production by roughly **−11% (zh single), −14% (code single)** and by a wider margin on dual lanes measured this way (dev aggregate 158/217 vs production per-lane sums ≈204–208/251–286). Live-production numbers came through the service's own config (caches as configured), so the dual comparison is indicative, not methodology-matched; the singles are decode rates and comparable. The optimization target remains the same structural per-window gap already documented.

## Process corrections

- The preflight `engine` path must not be trusted for production-representative arms; any future "production" arm must resolve the binary from `strata.service`'s runtime config and record its hash (`58854970…` today).
- Short-prompt probes are not comparable to the benchmark protocol; the four-lane probe above is the fast health check to use.
- The earlier same-day README table and changelog entry claiming production degradation are superseded by this record.

All 36 campaign requests reached full budget; production was restored and verified after the window; the live probes were ordinary requests to the running service. Per-load data: [structured record](2026-11-21-legacy-dev-sameday.json).
