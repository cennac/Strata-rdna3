# 源码与构建来源

## main：v0.1.43-rdna3.1（历史运行标识legacy-20261110）

首版对应实际运行的 engine **0.1.43** 优化版。它是在Strata v0.1.43基底上的优化快照，带后续MTP、调度回移和较新的服务控制层。与上游v0.1.43逐文件核对，823个Git blob相同、63个同路径blob不同；核对摘要见 [legacy-version-audit.json](legacy-version-audit.json)。

现役二进制SHA-256：

```text
588549701c867dc5a4aafef8de48c1e77b3d2f0dff4fd8d6e3a71db72df50df2
```

原来的构建曾将不同阶段的对象文件与静态库链接到一起。本仓库将对应源码收拢为可全量构建的树，覆盖顺序为：

1. 现役RDNA3基础引擎与计算核。
2. 会话复制的pinned staging实现。
3. 会话状态、快照直接复制实现及对应头文件。
4. MTP/双路回移的verify实现及头文件。
5. 最终专家驻留刷新与调度的generate实现。

原始源快照及覆盖层文件哈希见 [source-snapshot.json](source-snapshot.json)。它用于追溯首版源文件；后续变更由Git提交记录。

GGML来自历史构建使用的llama.cpp源码；其量化布局参考记录为 `3cf03257f219afbe7334045ff7c6a06ac68c627d`，实际所用GGML文件已随源码保存，文件哈希见快照。rocWMMA头文件也随源码保存。各依赖保留自身许可证。运行库不纳入Git；构建时明确指定ROCm路径。

全量重编使用ROCm10.1、gfx1100，并不承诺不同编译器得到与历史二进制相同的文件哈希。构建与实测状态会在测试记录中分别注明。

## dev/0.2.16

基于[Niko1221/Strata v0.2.16](https://github.com/Niko1221/Strata/releases/tag/v0.2.16)，engine **0.1.62**。上游源码归档SHA-256：

```text
858715c6d5f9e6b912cfc31e49855974f64b91126cfe2260712930e1aa288066
```

本地改动为默认关闭的惩罚历史流水线，以及实验性的BF16 RDNA3 HC read。RDNA3参考实现来自[nicejunjie/Strata-rdna3](https://github.com/nicejunjie/Strata-rdna3)及历史本地移植，MTP与调度也基于上游及本地回移工作。不是将旧版静态库链接进新版。

后续实验增加了复用上游Q8_0权重的HC Q8/A8适配，以及保持上游求和顺序的稠密矩阵wave32及Q5_K输出头路径。它们均默认关闭；来源仍为上述参考实现、仓库main与上游计算布局。`source-snapshot.json`记录开发首版，后续实验的精确源码哈希另存于[2026-11-19实验清单](validation/2026-11-19-lab-source-sha256.json)，变更由Git追踪。

`rdna_kquant_lab.cu`直接迁自main的`rdna_mmvq.cu`（稳定提交`7d1c66ceac6e055ec76c285da4ba47f66630f08b`），保留原量化解释和归约布局，新增默认关闭开关、gfx1100设备门控及独立数值探针。量化表与布局继续使用随仓库保存的GGML依赖及许可证。该实验不承诺逐位等同上游。

源码、模板和公开提示词测试脚本可公开复用；模型权重、密钥、实际服务配置与私有调用记录不在此仓库中。

## 首版全量重建结果

从本仓库收拢的源码全量重编，没有复用历史项目静态库。ROCm10.1、gfx1100，构建SHA-256：`be601550af81514e987bc5da3ae7ffcfebd2595a625c8ab15a54d0cd910d4fec`。

| 版本 | 中文单路 tok/s | 中文双路整批 tok/s | 代码单路 tok/s | 代码双路整批 tok/s |
|---|---:|---:|---:|---:|
| 公开源码重建 | 142.40 | 182.86 | 193.30 | 236.44 |

同一公开提示、输出1024、每项3次，12轮均长度匹配。与原现役139.10 / 185.75 / 195.50 / 235.83相比，差异在约±2.4%以内，首版源码达到现役量级。单路、双路的统计口径同前表。


详细质量和恢复记录见 [2026-11-19测试记录](benchmarks/2026-11-19.md)。

## 版本命名

使用 `v<上游版本>-rdna3.<修订号>`：稳定首版 `v0.1.43-rdna3.1`，开发首版 `v0.2.16-rdna3.1`。`legacy-20261110`继续用于追溯原测试和运行快照；它不是新的上游版本号。

## dev/0.2.21

Imported upstream v0.2.21 commit `d46f5f268f8ed797e83a5c63a69bffbd7a581cf6` as vendor `2a2add3`; integration `2aeba59` merges the adopted dev/0.2.20 profile. Engine version is 0.1.67. The source archive SHA-256 is `e23078def6ce4f772cedd0fe86034159d1b4fd2fb8e7bf1f60de3213edee058f`; fresh HIP gfx1100 binary SHA-256 is `603175a85134e039290cadf0d42220ece7ff77ef1d3ce6550ae87981add1d2c5`. The [frozen adopted-profile reference](reference-0.1.66-rdnagr.json) retains the source record identity. All 26 probes and forced rollbacks agree in the [integration test](benchmarks/2026-11-26-v0221-integration.md). No production or main promotion.
