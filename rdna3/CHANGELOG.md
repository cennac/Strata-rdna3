# 更新记录

## Pool compute queue / async C (2026-11-21)

- Implemented the pool-serve redesign as `STRATA_LAB_RDNA_POOL_QUEUE=1` (default off): a lock-free 4-slot SPSC ring feeds one runner thread that is the pool's only caller and raises C per ring. Frozen texts byte-identical; throughput −9.0%/−4.8%/−10.4%/−5.7% (medians) — **not adopted**. The inline synchronous design is confirmed the best serve model tested on this rig; the remaining gap is structural window scheduling. Also documented an unexplained `std::system_error(EPERM)` from `pthread_mutex_unlock` in a HIP-build worker thread (the reason the ring is lock-free). See the [record](benchmarks/2026-11-21-pool-queue.md).

## Legacy fused-A8 ablation attempt (2026-11-20)

- Ran the production 0.1.43 binary with `STRATA_GR_FUSED` 1 vs 0 under the frozen protocol: inconclusive, because 0.1.43 does not reproduce texts across identical requests on this rig while the development engine does (a role reversal of the R14–R20 instability). The HC fused-A8 port is deprioritized against the pool serve model's larger 2.4 ms/window wait chain. See the [attempt record](benchmarks/2026-11-20-legacy-fused-ablation.md).

## Light profile of the adopted baseline (2026-11-20)

- Single frozen load with `STRATA_VERIFY_PROFILE=1` + `STRATA_VERIFY_PROFILE_LIGHT=1` on the adopted template binary: bucket-level determinism to 0.01 ms; waitB/PCIe ≈ 0.05 ms confirms the fixed-zero graph; the surviving host wait chain is ~2.4 ms/window (stage-0 waitA ~1.0 + stage-1 waitCPU ~1.7); HC reads ~2.9 ms/window combined. Ranks the pool serve model as the next target, then the HC fused A8 port. See the [diagnostic record](benchmarks/2026-11-20-light-profile.md).

## MTP draft stream priority screen (2026-11-20)

- Verified gfx1100's stream priority range (1..-1) with a live probe, then screened `STRATA_MTP_HIP_PRIO=1` on the dual-card template: Chinese dual +3.83% in every pair, Chinese single −2.63% median, code lanes flat; frozen texts byte-identical. Not adopted. Added a diagnostic log line to `mtp.cpp` that reports the applied priority and range. See the [screen record](benchmarks/2026-11-20-mtp-prio.md).

## Routing look-ahead off screen (2026-11-20)

- Tested `STRATA_LOOKAHEAD=0` on the dual-gfx1100 template: Chinese dual improved in all three pairs (+8.03% median) but code single regressed in all three (−2.17% median); frozen serial/pipelined/forced-rollback texts stayed byte-identical across arms. Not adopted; the look-ahead stays on. A first campaign attempt aborted on its own config assertion before scoring and production was restored automatically. See the [screen record](benchmarks/2026-11-20-lookahead-off.md).

## Fixed-zero expert PCIe verifier graph (2026-11-20)

- Added a default-off HIP/gfx1100 experiment that omits the expert PCIe branch from verifier graphs only on a fixed-zero two-card split. Cross-card activation hand-off and ordered DMA publication remain unchanged.
- Confirmed 156/132 fewer nodes on the two stages, exact token IDs in the frozen checks (including forced rollback and batch), and a working nonzero-share fallback. The ordered-flag test passed on both cards.
- Three fresh-load pairs at 1,024 output tokens improved median Chinese single/dual and code single/dual throughput by 3.7%/7.1%/4.6%/1.2%. Enabled in the fixed-zero dual-card development template (`shstream-aux-pool12-zero-pcie`), not in the engine defaults. Production 0.1.43 was restored without changing its configuration. See the [validation and measurements](benchmarks/2026-11-20-zero-pcie-graph.md).

## dev/0.2.16 异步C设计与驻留/页缓存假设关闭（2026-11-20）

- 架构分析(无新测速):ExpertPool是串行批机器——serve线程自身参与相计算、批状态复用,双批次并发需要双池;异步C因此退化为"每stage独立serve线程+独立池"的结构项目。驻留率两版持平(21785对21636槽),dev的miss读全部页缓存命中(0存储读)——驻留与页锁补充假设均不成立。
- 结论:dev流水线使单窗口CPU专家需求相对现役串行翻倍,等待是这需求的调度伪影;重排(r37)与量化(r41)已被数据否决,剩余唯一路径为多线程serving(每stage独立serve线程+池),规格见`benchmarks/2026-11-20-round42.json`。
- 本轮无源码改动;现役未动。

## dev/0.2.16 池trace工具与服务量化（2026-11-20）

- 新增:serve路径的`STRATA_VERIFY_TRACE`请求尾转储(此前仅超时转储)——池逐ring握手面包屑成为常规诊断工具。trace证实:四verifier竞争(pw2每级两个),服务节奏约0.5ms/ring,存在单verifier贪心连做20-24个ring(约5ms)的突发,期间另一级干等——即stage-1 waitCPU的饿死机理。
- 新增默认关闭的`STRATA_LAB_RDNA_SERVE_QUANTUM=N`(每次service()至多服务N个ring):q4单路+5.0%但双路-3.4%/代码单-2.0%,q8全负,**不采纳**——平滑突发以双路批处理换单路时延,不划算。
- 至此三个有界调度修复(重排r37/直抬B r37/量化r41)全部以数据否决;等待链(+0.82ms)的闭合需要跨窗口工作预跑的serve模型重设计,为专门工程。记录见`benchmarks/2026-11-20-round40-41.json`;测后现役恢复。

## dev/0.2.16 串行分解与分割平衡收官（2026-11-20）

- 第38轮(无新测速,取自r35日志):dev串行窗口31-33ms的首次完整分解——池等待13.5ms、**图执行本身约14.6ms(无计数器覆盖,串行暴露/流水线隐藏)**、commit/emit 0.79+draft 1.3+adapt 0.3-1.5(现役全为0且其等待全部内嵌于GPU-reach wait)。流水线模式的verify内仍有约3.1ms未归因项。结论:等待链修复必须让池在ring之前预计算(跨窗口前瞻),重排类修复已被第37轮证明不足。
- 第39轮:layer-split平衡筛选——24/24双路变差(145.2/199.3),28/20引擎SIGABRT崩溃,**26/22保持最优**,该方向关闭。
- 记录见`benchmarks/2026-11-20-round38.json`、`round39.{md,json}`;测后现役恢复。

## dev/0.2.16 重叠调度有界修复未过确认（2026-11-20）

- 机制定位:waitB来自`raise_flag_b_ordered`的粘滞`copy_used_`(空计划也要等拷贝流排空),stage-1 waitCPU来自泵序D→A→B(新窗口)→s1(老窗口)的饿死;现役双卡实为串行解码(min_st=3)。`--pipeline-windows 0`在dev单路崩至60/75,不可用配置复现。
- 新增两个默认关闭开关(构建`f5d5838`):`STRATA_LAB_RDNA_B_DIRECT`(以已执行计数判定可直抬,#5096危害仍覆盖)与`STRATA_LAB_RDNA_PUMP_AGE`(老窗口stage-1优先)。R36初筛pumpage四路+2.8~8.8%,R37 1024×3确认全部回落(-2.5/+0.6/-0.8/-0.3%),**均不采纳**;单次768筛选±4%臂位漂移再次得到验证。
- 闭合等待链(+0.82ms)需跨窗口池预跑(look-ahead),属流水线结构改动,列为后续专项。记录见`benchmarks/2026-11-20-round35-37.md`;三轮测后现役恢复。

## dev/0.2.16 HC量化重筛收官（2026-11-20）

- 第34轮在已采纳组合上重筛HC Q8/Q8+A8(现役生产即Q8+A8,dev移植版为BF16):hcq8混合(-2.2/+2.5/-1.8/0),hcq8a8全负,均不采纳;核对源码确认`STRATA_GR_FUSED`屏障仅作用于本身已更慢的A8读,接通亦无法翻盘。HC差距(+0.38ms/窗口)归入内核launch/占用级专项。
- 至此34轮内配置/参数/单路径候选全部筛完并留证;剩余为两项代码级专项:generate.cpp窗口重叠调度(+0.82ms等待链)与HC内核launch差异(+0.38ms)。记录见`benchmarks/2026-11-20-round34.md`;测后现役恢复。

## dev/0.2.16 服务态轻量剖析与最终归因（2026-11-19）

- 新增`STRATA_VERIFY_PROFILE_LIGHT=1`(构建`75a1968c`):保留阶段时间戳但八处融合门控改为`prof_blocks_fusion()`,生产执行不变;补上插桩会关融合导致服务态无法归因的缺口,保留为后续标准工具。
- 服务态阶段表对现役插桩表(中文主阶段):等待链+0.82ms、HC读取+0.38ms为仅有的两块差距;计算核−0.16ms、copy+combine−0.19ms开发版反超(convab融合与SH_STREAM的服务态效果首次可见:ab 0.20→0.04、shared+quant 0.65→0.04)。
- 至此差距归因闭环:后续专项为generate.cpp窗口重叠调度(打+0.82ms的CPU专家等待)与HC内核launch/占用差异(+0.38ms)。记录见`benchmarks/2026-11-19-round33.md`;测后现役恢复。

## dev/0.2.16 窗口效率旋钮终筛（2026-11-19）

- 第32轮筛`--pipeline-windows 3`、`--spec-min-p 0.45`、`--mtp-max-t 4`、`--spec 5`:全部为负或混合,现有参数(pw2/spec4/min-p0.5/max-t3)确认为该构建最优。结合第29轮回滚率与每窗口token数两版持平的观察,**开关与参数层的空间至此清零**;剩余差距(约−15%/−12%/−18%/−10%)全部在每窗口重叠调度成本,后续唯一方向为generate.cpp窗口重叠设计本体的专项移植/重设计。
- 记录见`benchmarks/2026-11-19-round32.md`;测后现役恢复。

## dev/0.2.16 服务态等待归因与路由发布融合移植（2026-11-19）

- 第29轮仅开`STRATA_DECODE_TIMING`(融合路径不变)对照:开发版窗口16.18ms vs 现役13.41ms,**GPU-reach wait 11.80 vs 10.09(+1.71ms)为最大差距块**,host plan +0.17ms次之,开发版CPU切片反而略低——等待链差距在重叠设计而非CPU计算本身。
- 移植现役路由发布融合:新增默认关闭的`STRATA_LAB_RDNA_ROUTE_PUBLISH=1`(构建`4bd26459`),多token top-10与resident doorbell一次发射;仅普通路径接管,resident-plan/doorbell-store/always-publish/route-prior等保持原路径,shared-expert晚分叉流序不变。
- 1024×3对照基本持平(中文双+1.1%其余噪声内),一次内容级提前结束(844/1024)记录,**不采纳**;冻结跨臂串行文本逐位一致,融合与分离路径bit级等价,正确性留档。找回单个旧路径仍不等于找回现役重叠设计;后续剩余方向为窗口重叠调度整体与HC内核残余。
- 记录见`benchmarks/2026-11-19-round29-31.md`;测后现役恢复。

## dev/0.2.16 草稿层HC融合确认收口（2026-11-19）

- 第28轮以第27轮最终构建(`5614920c`)1024×3确认第25轮筛选候选:`STRATA_QFUSE_HC=1`各路噪声内(对照114.08/155.18/148.84/212.62,-1.6%/+0.2%/+0.2%/-0.2%);Q32+QFUSE_HC为-1.3%/-0.5%/+1.9%/+2.0%,代码两路受益但中文单路退化,均不采纳,维持默认关闭。开关级候选至此全部筛完。
- 增加英文`rdna3/README.md`总览(与中文日总结互补);工作树来源表述统一为`cennac-rdna3-dev`分支。无源码改动,所有流达到1024预算,测后现役恢复。见`benchmarks/2026-11-19-round28.md`。

## dev/0.2.16 CPU结果合并迁移与全天总结（2026-11-19）

- 确认开发版继承的 `STRATA_COMBINE_CPU=1` 没有实现；新增独立默认关闭的 `STRATA_LAB_RDNA_COMBINE_CPU=1`，直接合并CPU映射结果与GPU命中结果，省去复制／命中加和，保留原等待与shared gate。限制适用路径并保留回退，非HIP后端不引用新API。
- 两卡各600组逐位／图重放／哨兵检查和240次非法参数拒绝零失败；最终探针补全零CPU／GPU／shared边界并重测通过。冻结专家自适应和后缀草稿后，对照／实验两组中文与代码的串行重复、流水线、强制回滚文本均一致。有限三题冒烟均2/3，不宣称完整质量通过。
- 第26轮1024×3：对照中文单／双117.70／163.49、代码单／双159.60／212.89；CPU合并115.40／162.78、159.20／214.11 tok/s，约−2.0%／−0.4%／−0.3%／+0.6%，没有全面收益，保持默认关闭。最终平台隔离改动没有改变HIP引擎二进制哈希，第27轮另做确认与冻结验证。
- 汇总已采纳的惩罚流水线、BF16 HC、shared stream、aux CPU放置与pool12，以及未采纳实验、质量边界和剩余等待链差距，见 `benchmarks/2026-11-19-summary.md`。开发版仍未追平现役，不合入main；两轮测后恢复现役，配置／运行中二进制核对通过，所监测AER增量及GPU故障为0。

## dev/0.2.16 BF16 HC 32列核与调度边界验证（2026-11-19）

- 新增默认关闭的 `STRATA_LAB_RDNA_GR_Q32=1`：BF16 HC up每组32列，可在没有全局计数器时融合q8_1图像；保留逐行求和顺序，没有跨工作组自旋。Q8/A8路径不变。
- 保留调用方的融合策略，禁止把未写入图像误报为已生成。探针经实际调度入口检查T=1…8、apply/inject、零值、图重放及关闭融合后的回退；两卡共1600字段、192图像／哨兵、132图重放、256调度返回值及64未写入检查零失败。
- 第24轮1024×3：对照中文单／双115.80／162.84、代码单／双156.50／216.59；Q32为116.60／159.85、157.80／213.72 tok/s，双路略慢，不设为默认。两组冻结串行重复／流水线／强制回滚本轮均一致，三题冒烟仍2/3；此前重复输出不稳定的诱因仍未定位。
- 第23轮显式绑核及tasks13均无全面收益，未采纳；末尾对照显示约3%会话漂移，保留否定结果。全部详细速度、首字、资源、失败与恢复记录见 `benchmarks/2026-11-19-round23.md`、`round24.md`。
- 第25轮单独打开草稿层 `STRATA_QFUSE_HC=1`，确认Q32的无计数器图像入口实际生效；768输出初筛119.40／159.72、157.60／214.25 tok/s，代码双路持平，没有全面收益，保持默认关闭。三轮均恢复现役，原配置／运行中二进制哈希一致，所监测AER计数无新增。

## dev/0.2.16 CPU池扩容采纳与TSUM崩溃记录（2026-11-19）

- `--pool-workers` 8→12 于1024×3确认:中文单115.63(+5.0%)、双162.13(+3.4%)、代码持平;pool16不如12,swiglu代码-7.9%否决,lfuse混合未采纳。模板更新为 `shstream-aux-pool12`。
- **`STRATA_TSUM=1` 在 gfx1100 引擎加载期 SIGABRT 崩溃**(exit -6),含TSUM的组合未测;保留为否定证据,勿在gfx1100启用。
- 本日三次采纳(SH_STREAM、AUX、pool12)累计交错对照:zh单+6.9%、zh双+5.5%、code单+4.4%、code双+1.1%;对现役剩余差距约−17%/−13%/−22%/−10%。
- 记录见 `benchmarks/2026-11-19-round21-22.md` 及JSON/实验计划;无源码改动,测后现役恢复。

## dev/0.2.16 SH_STREAM＋AUX_CPUS 采纳与一致性检查失效发现（2026-11-19）

- 按第十三轮归因筛选调度/放置类开关并确认：`STRATA_SH_STREAM=1`＋`STRATA_AUX_CPUS=auto` 于1024×3对照中文单112.36（+3.9%）、双164.27（+6.9%）、代码单152.57（+4.5%）、双212.18（+0.7%）。两开关不改浮点算术，模板已更新（`shstream-aux-stage1`），对现役差距收窄至约−16.5%/−7.4%/−16.4%/−7.5%。
- copyprio（本轮−0.5/−0.9%代码路）与 coldov4 未采纳；shstream 单独亦未采纳（zh双仅+1.1%）。
- r16-shstream 一次内容级提前结束（977/1024，`stop`）按设计记录并中止该战役；连同第九轮事件共两次，均为SH臂，继续监控。r17 确认十二流全部有效。
- **方法论发现：当日下午起冻结串行重复检查在本机对纯对照配置也失败**（热/冷加载共4组对照、6/6中文），与开关无关；此前各轮以该检查作否决依据的结论作废（exact1 的否决仍以其更慢独立成立），诱因未定位，列为开放问题，恢复可复现前该检查不用于开关甄别。
- 无源码改动；记录见 `benchmarks/2026-11-19-round15-17.md`、`round18-20.md` 及同名JSON/实验计划；测后现役恢复。

## dev/0.2.16 冻结归因与CPU分发对照（2026-11-19）

- 核对本地工作树两候选：`STRATA_IQ3S_EXACT1`已被上游收编为`STRATA_CPU_IQ_ALL_EXACT1`（五格式ggml逐位路径，默认关），其速度收益经`STRATA_IQ_MT_MIN=1`已在当前组合兑现；`STRATA_PIPED_ADAPT_EVERY`的上游等价物在layer-split上被排除，不移植。对照审计见`validation/2026-11-19-switch-audit.md`。
- 第十三轮冻结插桩逐阶段对照（512输出单路）：现役重建10.27/10.87、开发版12.20/12.81 ms/窗口；开发版慢约1.9ms，约45%为等待链（waitB+PCIe grp+waitCPU 0.21→1.04），18%为HC读取残余，copy+combine开发版更好。剩余差距指向未移植的重叠调度与HC内核，不是CPU分发内核。
- 第十四轮768输出对照：MT_MIN=1对照组中文单/双108.8/155.3、代码143.4/201.8 tok/s；ALL_EXACT1臂−3.4%/−3.9%/−4.1%/持平且中文冻结串行重复文本不一致（nt=1与成组舍入不同，违反窗口不变性），不采纳；无MT_MIN的ggml臂更慢，确认MT_MIN=1最优。
- `iq_avx2_parity`在本机CPU全量零失败（含exact行）；exact1开关生效日志、引擎阶段表、逐次数据保留在`validation/round14-*`与`benchmarks/2026-11-19-round13/14.json`。无源码改动，模板与main不变，测后恢复现役。

## dev/0.2.16 分格式迁移与MTP窗口核对（2026-11-19）

- 增加默认关闭的K-quant实验类型选择，支持只启用模型内部的IQ4_XS／Q6_K投影；空值、非法列表或未知类型完全回退，不更换IQ3_XXS权重。
- 十个独立进程共4000组数值检查及100组图重放通过；未选格式与上游逐位一致，补上实际选中格式日志。微基准计时改为显式开启。
- 1024输出、每项三次：中文单／双路115.4／155.9→111.7／160.3，代码154.8／211.5→156.3／216.8 tok/s。中文单路退化，没有成为新的全面更优组合，默认模板与main均不变。
- 将普通解码窗口计数与每组六条单路请求逐条对上：中文窗口耗时17.23→17.14ms，但窗口数515→532，更多窗口抵消局部计算收益；不能将短测+3.5%宣称为稳定提升。
- 冻结条件下中文／代码串行重复、流水线及强制回滚文本一致；三题冒烟仍均2/3，订单算术失败。测试后核验现役二进制、原配置和端口恢复，未新增AER或GPU故障。


## dev/0.2.16 阶段定位与K-quant迁移（2026-11-19）

- 对现役和开发版做GPU阶段计时；区分诊断模式与正常服务，不将插桩成绩当作生产速度。
- 完成共享专家流、复制优先级、专家SwiGLU融合筛选，保留一次提前EOS；没有改变默认模板。
- 核对实际权重，补上IQ4_XS、Q6_K、IQ3_S及适用Q4_K/Q5_K的main稠密投影核，新增默认关闭的`STRATA_LAB_RDNA_KQUANTS=1`，仅gfx1100进入。
- 两卡各400组数值、边界、列一致性、成对发射与图重放通过，最大相对L2误差`1.69e-7`；冻结条件下中文、代码的串行重复／流水线／强制回滚文本一致。
- 同构建三次对照：中文单／双路113.9／155.1→111.3／162.9，代码152.0／210.3→156.2／220.6 tok/s。双路约+5%，中文单路有退化，保留独立开关，未合入main。
- 三题格式／算术冒烟两组均2/3，不能宣称完整质量通过。详细速度、首字、MTP接受率、资源与失败记录见第八至十轮。

## dev/0.2.16 后续实验（2026-11-19，未改变默认组合）

- 接通复用上游Q8_0权重的HC Q8/A8读取，修正探针对未定义`lo`工作区的比较，保留初次失败证据。没有额外分配旧R8权重；没有启用跨工作组自旋屏障。
- 迁移保持上游求和顺序的small-format wave32布局，并为实际Q5_K输出头增加独立实验开关；两卡各400组逐位检查及图重放通过。
- 记录融合、行分组、Q8/A8、精确MMVQ与输出头布局的筛选及三次复测。Q6开关因实际输出头为Q5_K而未生效，明确保留为无效开关重复对照。
- Q5_K头局部4列耗时下降27.5%，整机仅约±1%，没有确认全面收益，不替换默认模板。该轮中文111.9／156.3、代码152.9／212.6 tok/s（单路decode／双路整批）。
- 冻结条件下中文与代码的串行重复、流水线和强制回滚文本一致；有限格式冒烟仍有算术失败，不宣称完整质量通过。全部实验完成后恢复现役，未合入main。
- 复现开关、数值探针、源码哈希和各轮速度／首字／资源记录见`experiments/`、`validation/`与`benchmarks/2026-11-19-round*.md`。

## v0.1.43-rdna3.1（legacy-20261110）

- 以实际运行的 engine 0.1.43 优化版作为主分支基线。
- 汇总 RDNA3 计算核、单请求流水线、双请求调度、MTP、专家驻留刷新、会话复制等现役改动。
- 收拢实际构建采用的源码覆盖层和依赖，避免依赖机器上散落的旧静态库。
- 保留上游及参考实现的许可证和来源说明。

## v0.2.16-rdna3.1（开发中，engine 0.1.62）

- 在正式版源码上增加可选的惩罚历史流水线支持。
- 筛选草稿设备、草稿长度、硬件队列数量、HC v3 和 HC 量化。
- 实验迁移 BF16 RDNA3 HC read；尚未达到主分支性能，不合入 main。
- 详细结果、提前 EOS 和一致性检查限制见各轮测试记录。

本开发分支已纳入惩罚历史流水线及BF16 HC read源码、数值探针和验证输出。最佳组合中文单路113.2、双路合计156.9，代码单路150.9、双路合计208.3 tok/s；未达到main，不合并。
