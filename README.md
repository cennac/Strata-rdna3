# Strata-rdna3 — dev/0.2.16

当前检出的是正式版0.2.16迁移开发分支。稳定基线在 [`main`](https://github.com/cennac/Strata-rdna3/tree/main)。

我在这里维护双 RX 7900 XTX 的 Strata 优化，重点是单请求速度、双并发吞吐，以及可复现的源码和配置。

- **`main`：现役优化版**，Strata v0.1.43-rdna3.1 / engine 0.1.43，作为首版性能基线。
- **`dev/0.2.16`：正式版迁移研发**，版本为 v0.2.16-rdna3.1，基于上游 Strata v0.2.16 / engine 0.1.62。达到现役性能并完成正确性验证后再合入主分支。

这是一份独立维护的优化项目。上游为 [Niko1221/Strata](https://github.com/Niko1221/Strata)，RDNA3 参考实现为 [nicejunjie/Strata-rdna3](https://github.com/nicejunjie/Strata-rdna3)。保留原作者署名与许可证。

## 本机与测试口径

MSI MPG Z490 GAMING CARBON WIFI、10900T ES、96GB DDR4，两张 24GB RX 7900 XTX：白金与涡轮。测试保留白金272W、涡轮231W功耗上限，不更改风扇和频率策略。实际成绩见 [测试记录](rdna3/benchmarks/2026-11-19.md)。

模型为 **Qwen3.8-Flash-Next，GSQ-RCO IQ3_XXS**。测试中的单路是一个请求使用两张卡；双路是两请求同时运行。每路容量131072不代表本轮输入了128K提示词。

| 同机实测，中位数 tok/s | 现役优化版 | 0.2.16开发优化版 | 0.2.16正式版 |
|---|---:|---:|---:|
| 中文单路生成 | 139.1 | 113.2 | 60.3 |
| 中文双路整批吞吐 | 185.8 | 156.9 | 132.0 |
| 代码单路生成 | 195.5 | 150.9 | 75.3 |
| 代码双路整批吞吐 | 235.8 | 208.3 | 174.4 |

各项3次，presence penalty=1.5，关闭提示/会话缓存，输出1024。单路是生成阶段速度，双路是包含首字等待的整批吞吐。开发版已有收益，仍落后于现役，暂不合并。

公开源码全量重编也已实测：中文单路 **142.4**、双路合计 **182.9** tok/s；代码单路 **193.3**、双路合计 **236.4** tok/s。每项3次，达到现役量级。

后续筛选与失败结果另存：[融合／行分组](rdna3/benchmarks/2026-11-19-round2.md)、[HC Q8/A8](rdna3/benchmarks/2026-11-19-round3.md)、[精确稠密矩阵路径](rdna3/benchmarks/2026-11-19-round5.md)。这些实验没有改变上表的默认推荐组合；开关、适用形状和数值探针见[实验说明](rdna3/experiments/README.md)。

三次复测见[第六轮](rdna3/benchmarks/2026-11-19-round6.md)与[Q5_K输出头实验](rdna3/benchmarks/2026-11-19-round7.md)。输出头局部耗时改善没有形成明确整机收益；这些默认关闭的实验不代表上表成绩再次提高。

## 现役版包含什么

- RDNA3 HC read、预填充与量化专家计算核。
- MTP 与单请求流水线，在双并发服务中保留单请求快速路径。
- 双请求调度、草稿设备与槽位状态处理。
- 专家驻留刷新、RAM专家层、PLE RAM 与会话复制优化。

首版将实际使用的源码覆盖层汇总到同一源码树；构建脚本不链接本机遗留的静态库。二进制、源码验证状态与依赖来源见 [来源记录](rdna3/PROVENANCE.md)。

## 从源码构建

当前目标是 Linux / HIP / gfx1100。先准备 ROCm 开发环境、CMake、C++编译器和 Python。配置中的 `--rocm` 指向你的 ROCm 安装根目录。

```bash
git clone https://github.com/cennac/Strata-rdna3.git
cd Strata-rdna3
git switch dev/0.2.16
python3 rdna3/build.py --rocm /opt/rocm --jobs 4
```

`dev/0.2.16` 使用正式版自带的计算核和依赖；`main` 携带历史构建对应的GGML与rocWMMA源码。模型 GGUF、pack、tokenizer 和 MTP runtime 由使用者单独准备，量化及模型家族必须匹配。

## 生成配置并运行

以下为96GB内存、双XTX的开发配置模板。`--gpu` 使用 **HIP序号**，不使用容易随插卡变化的 Vulkan 序号。先确认两张目标显卡，再填入参数。

```bash
python3 rdna3/configure.py \
  --rocm /opt/rocm \
  --pack /path/to/iq3_xxs/pack \
  --native /path/to/model-00001-of-00002.gguf \
  --ple-gguf /path/to/model-00002-of-00002.gguf \
  --mtp /path/to/mtp/runtime \
  --gpu 0,1
```

模板保留MTP、26/22分层、两请求及每路131072容量；默认只监听本机。配置写到 `state/`，不自动启动、不改系统服务。专家画像未附带，首次加载与预热结果会受画像和文件缓存影响。

使用控制层所需的 Python 环境，从 `terminal/` 启动：

```bash
python3 -m venv .venv
.venv/bin/python -m pip install -r terminal/requirements.txt
cd terminal
../.venv/bin/python -s serve/server.py --engine strata \
  --config ../state/dual-xxs.json --host 127.0.0.1 --port 8100
```

也可以在自己的应用程序中导入生成的配置管理生命周期。视觉读取器属于单独的可选组件，运行模板默认不启动；本机对照记录保留已启用的视觉配置，显存布局会有差异。

## 复测

先关闭服务配置中的会话缓存和提示缓存，再独立加载、预热。公开脚本使用同一套中文与代码提示，presence penalty为1.5，每项默认3次、输出上限1024：

```bash
python3 rdna3/bench.py --url http://127.0.0.1:8100 \
  --model Qwen3.8-Flash-Next-IQ3_XXS --output state/bench.json
```

脚本保留每请求输出、首字时间、引擎速度和整批吞吐。提前EOS单独标记，不能混作固定长度成绩。测试速度不能代替质量结论。

后续开发与合并规则见 [DEVELOPMENT.md](rdna3/DEVELOPMENT.md)，改动记录见 [CHANGELOG.md](rdna3/CHANGELOG.md)。

上游完整说明保留于 [UPSTREAM_README.md](UPSTREAM_README.md)。当前分支尚未达到主分支性能，日常服务仍用主分支基线。

版本规则：`v<上游版本>-rdna3.<修订号>`。现役0.1.43基底包含后续MTP与调度回移，控制层也较新；源码版本核对记录见 [来源](rdna3/PROVENANCE.md)。
