# 开发版验证

两张gfx1100分别通过160项HC read字段检查，T=1…8、pending write开/关、inject开/关。数值输出见同目录txt；relative L2 < 1e-4。构建与运行：

```sh
python3 rdna3/build.py --rocm /opt/rocm --jobs 4
cmake --build build --target rdna_hc_probe -j 4
LD_LIBRARY_PATH=/opt/rocm/lib:/opt/rocm/lib/llvm/lib build/rdna_hc_probe 0
LD_LIBRARY_PATH=/opt/rocm/lib:/opt/rocm/lib/llvm/lib build/rdna_hc_probe 1
```

惩罚流水线在冻结专家自适应和后缀草稿的条件下，完成中文与代码各384输出的串行重复、流水线、强制回滚对照，文本一致。完整采样/续聊覆盖尚未完成，不能作为上游全平台合并依据。

完整整机速度和1024预算的质量冒烟检查见[测试记录](../benchmarks/2026-11-19.md)。算术题仍未通过；默认开关关闭，开发模板显式开启本机验证组合。

## 后续实验

Q8/A8 HC 的独立标量参考、Q8工作区首次失败及修正理由见[第三轮](../benchmarks/2026-11-19-round3.md)。两张卡的完整探针输出为本目录 `round3-kernel-*.txt`。BF16/A8各检查160项，Q8检查128项已定义输出；另有图捕获重放。

稠密矩阵精确求和迁移每卡320组逐位比较及图重放通过，输出为 `round5-kernel-wave-gpu*.txt`，对应[第五轮](../benchmarks/2026-11-19-round5.md)。实验开关与探针命令见[实验说明](../experiments/README.md)。源码哈希见[本轮清单](2026-11-19-lab-source-sha256.json)。

数值通过只说明所测输入的计算满足参照条件；没有将微基准或少量格式题解释为完整质量通过。Q8/A8和精确MMVQ均保留独立开关，默认模板没有启用。

第七轮新增实际Q5_K头，当前MMVQ探针每卡400组。结果见 `round7-kernel-q5-gpu*.txt`。small-format与Q5_K头分别在冻结条件下通过中文、代码的串行重复／流水线／强制回滚文本对照；三次整机结果分别见[第六轮](../benchmarks/2026-11-19-round6.md)、[第七轮](../benchmarks/2026-11-19-round7.md)。

第十轮另增改变归约顺序的K-quant投影核，每卡400组容差检查通过，最大相对L2误差`1.69e-7`。适用格式的单列／多列、成对发射、图重放检查通过；输出为`round10-kernel-kquant-gpu*.txt`。冻结专家自适应与后缀草稿后，中文、代码各384输出的串行重复、流水线及强制回滚文本一致。这不表示其输出逐位等同未优化上游，也不表示完整质量通过。

第十一轮选择器验证为[round11-format-selector.json](round11-format-selector.json)：两GPU分别检查全部、IQ4_XS、IQ4_XS＋Q6_K，GPU0另检空值／非法／未知／末尾逗号，十进程各400组、各10组图重放通过。最大相对L2为`1.69e-7`；未选格式逐位回退。诊断日志包含图捕获注册，不是执行计数。

第十二轮正常路径的窗口计数见[round12-decode-windows.json](round12-decode-windows.json)，逐条匹配测速请求的token/s与草稿计数。它说明中文退化表现为验证窗口增多，不是固定token路由的算子对照。冻结惩罚／回滚文本一致性及三题冒烟见[整机记录](../benchmarks/2026-11-19-round12.json)；服务恢复核验见[round12-restoration.json](round12-restoration.json)。

第十三轮冻结插桩的逐阶段窗口对照见[round13记录](../benchmarks/2026-11-19-round13.md)：开发版每窗口慢约1.9ms，约45%在等待链（waitB/PCIe grp/waitCPU）、18%在HC读取残余差距，copy+combine开发版更好。插桩数字只用于归因。

第十四轮CPU分发对照见[round14记录](../benchmarks/2026-11-19-round14.md)：`STRATA_CPU_IQ_ALL_EXACT1`（上游收编版Codex IQ3S_EXACT1）在本机全量parity零失败（`round14-iq-avx2-parity.txt`，生效日志`round14-exact1-gate.txt`），但整机慢于`STRATA_IQ_MT_MIN=1`，不采纳。开关对照审计见[2026-11-19-switch-audit.md](2026-11-19-switch-audit.md)。

第十五至十七轮采纳`STRATA_SH_STREAM=1`＋`STRATA_AUX_CPUS=auto`（四路+0.7~6.9%，均为调度/放置类开关）。第十八至二十轮发现**冻结串行重复检查当日下午起对纯对照配置也失败**（热/冷加载共4组，6/6中文不一致），该检查在恢复可复现前不用于开关甄别；上午round-12同协议通过，诱因未定位，开放问题。见[round18-20记录](../benchmarks/2026-11-19-round18-20.md)。

第二十一至二十二轮采纳`--pool-workers 12`（zh单+5.0%/双+3.4%/代码持平）；**`STRATA_TSUM=1` 在gfx1100加载期SIGABRT崩溃**，勿启用；swiglu（代码−7.9%）、pool16、lfuse未采纳。见[round21-22记录](../benchmarks/2026-11-19-round21-22.md)。

第二十三轮显式绑核／任务粒度筛选没有全面收益，见[记录](../benchmarks/2026-11-19-round23.md)。第二十四轮新增BF16 HC up32，以旧RDNA16浮点输出＋独立原生q8_1量化作为参照。两卡共1600字段、192图像／哨兵、132图重放、256调度返回值和64禁止融合未写入检查零失败；逐进程原始输出为本目录`round24-probe-*.txt`，统计见[JSON](../benchmarks/2026-11-19-round24.json)。stderr诊断可能插入缓冲stdout中间，统计时仅移除已知选择日志再数行，原始文件不改写。

本轮对照和Q32的冻结串行重复、流水线、强制回滚均一致，但第十八至二十轮曾失败，根因尚未定位，不能认定已修复。三题冒烟均2/3。Q32默认关闭，1024×3服务结果没有全面收益，见[记录](../benchmarks/2026-11-19-round24.md)。源码／构建哈希见[清单](round24-build.json)，现役配置、运行中可执行文件、端口与故障日志的恢复核对见[恢复记录](round24-restoration.json)。

第二十五轮另设`STRATA_QFUSE_HC=1`，首次选中的T=1…8均收到量化图像指针，见[选择记录](round25-q32-selection.json)。复用同二进制数值验证，768输出初筛没有全面收益，见[记录](../benchmarks/2026-11-19-round25.md)。未对该入口组合重新做冻结文本／质量检查，未将其设为默认；测后[恢复核对](round25-restoration.json)通过。

第二十六轮新增默认关闭的CPU mapped-row combine。参照实际独立等待／复制／命中加和／合并流程，两GPU各600次逐位和图重放／哨兵检查、240次非法参数拒绝零失败；原始输出为`round26-probe-cpu-combine-gpu*.txt`。第27轮最终探针第三次重放另用全零CPU／GPU／shared结果，重新通过同样检查。具体宽度、命中比例、T和重放序号逐条保留，不以抽样误差容限代替逐位比较。

第26轮1024×3没有全面性能收益，见[数据与决定](../benchmarks/2026-11-19-round26.md)。第27轮最终非HIP调用隔离不改变HIP引擎二进制哈希，GPU核库也未改变；对照／实验分别在冻结专家自适应和后缀草稿后完成中文、代码的串行重复／流水线／强制回滚文本一致检查，见[记录](../benchmarks/2026-11-19-round27.md)。普通自适应测速的重复文本差异另外保留，不将它用作冻结检查。默认不开启新路径，现役恢复核对见`round26-restoration.json`和`round27-restoration.json`；全天结论见[总结](../benchmarks/2026-11-19-summary.md)。
