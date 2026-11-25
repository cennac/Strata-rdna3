# Strata-rdna3

This is Cennac's independent RDNA3 optimization repository.

- `main` is the measured legacy-20261110 baseline, engine 0.1.43. Keep its serving behavior stable.
- `dev/0.2.21` integrates upstream v0.2.21, engine 0.1.67. Keep its migrated profile experimental until the performance and correctness gates pass.
- Compare the same weights, expert-profile seed, prompts, penalties, cache policy, output length, power limits and hardware. Record tokens/s and time to first token for one and two simultaneous requests.
- A request on two GPUs is not a single physical GPU result. Combined throughput must use total tokens divided by batch wall time.
- Keep early EOS, failures and unsuccessful experiments in the evidence. Do not call a capacity setting an actual long-input measurement.
- Check penalty/history handling, rollback and numerical changes before promotion. Speed alone is insufficient.
- API model names contain the model and quantization only. Put version and profile in deployment metadata.
- Keep weights, credentials, machine-specific deployment files, private prompts and logs out of Git. Public examples use placeholders and public benchmark prompts.
- Keep upstream licenses and attribution. Make local changes opt-in where practical.
- Update `rdna3/CHANGELOG.md`, the reproduction instructions and benchmark records with meaningful changes.
- Merge development into `main` only after the measured performance and correctness criteria are met; record the evidence.

Release versions follow upstream: `v<upstream-version>-rdna3.<revision>`. Stable first release is v0.1.43-rdna3.1 (legacy engine plus later backports); development is v0.2.21-rdna3.1. Keep the upstream engine version separate from the public release version.
