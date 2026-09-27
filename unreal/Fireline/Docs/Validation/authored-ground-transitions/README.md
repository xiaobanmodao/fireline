# 原片起停：独立候选，P2 尚未完成 · 2026-09-26

入口：桌面「火力对决」→ **测试版·原片起停.app**。开始游戏后用 WASD 起步／松键停步，V 可观察全身；M4／MP7 均可测试。此入口只启用 `FirelineAuthoredGroundStudy`，不启用失败的 SideJog／Pivot 实验。正式 0.1.10、原侧步和接触修复入口保留。

## 交付范围

- 从本机原版 Lyra 读取 MF Rifle 四方向 Walk／Jog 的 Start、Stop、Pivot，共 24 段；另保留 MF／MM 左右 Jog 共 4 段参考。原片与 skeleton 共 29 文件，路径／SHA256 在 `source-manifest.json`。不是 Apex 提取资源，也不替换 Ryan、DJ、枪械供应者。
- IK Retargeter 先输出隔离 Ryan proxy，再恢复原生 Root scale、骨架和握持上半身。删除根平移驱动，仅将 root 行程制成 `FL_Travel`；胶囊移动仍只有 CharacterMovement 一个来源。
- 当前启用四个正方向起步，按实际意图速度选 Walk／Jog，实际移动距离推进时间。新方向输入、起跳／蹲滑可退出动画，不锁输入。斜向超过原片方向 22.5° 时不硬套。
- 停步按 UE 5.8 `AnimCharacterMovementLibrary::PredictGroundMovementStopLocation` 的摩擦／减速度关系估计剩余距离。对原片及镜像支撑脚副本，在距离时间附近用上一帧真实脚／膝位置选择更接近的姿势。镜像仅在隔离原生动画副本上操作，原始绑定和权重不改。
- 支撑曲线进入同一个足底接触层；专门动作内逐渐保留原片膝盖平面，旧循环保持原求解，避免直接切换求解方式。新动作完全退层后恢复旧姿态路径。
- 起停播放状态、旧片／新片混合状态随地面 gait 一起跨切枪保存，初始化时同步恢复节点姿势。早期 MP7 实例重建的单帧跳变已修复。

**这是地面起停适配候选，不是完整的全身原片迁移。** 躯干／双手仍由既有 BodyPose／ContactCarry 协调；源动画全身联动、斜向专用停止、匹配的侧跑和急反向仍待完成。不能把该候选当作 Apex 同等质量或所有角度美术验收。

## 实机证据

`AuthoredGroundDeliveryVisual`：24 组、2880 个实际 UE 最终姿势；画面按原时间轴 10 Hz 采样，骨骼数据 60 Hz。见 `delivery-checks.json`、`check_runtime.py`。

| 最终腿骨单帧角变化 | 旧接触候选 | 本轮起停候选 |
| --- | ---: | ---: |
| 起步窗口峰值 | 19.56° | 17.12° |
| 起步窗口 P99 | 13.36° | 14.41° |
| 停步窗口峰值 | 22.07° | 22.07° |
| 停步窗口 P99 | 16.59° | 16.16° |
| 快速反向峰值（左右及前后） | 19.01° | 19.01° |

并非所有指标都有改善。最大停步值仍在未覆盖的斜向；快速反向仍是旧循环过渡，不冒充新 Pivot 已完成。左右高速姿态不对称也保留为已知问题。

本轮固定腿长误差最大约 0.003 cm；采样持枪枪托目标误差最大约 0.057 cm；有限的完全支撑窗口平面脚速为 0。后者不是所有近地脚步均无滑动。33 项 M4 操作／1341 姿势采样通过（`m4-regression.txt`），1023 个受保护旧资源哈希一致。数值检查不能代替视觉验收。

- [左向起停逐帧](Images/poses-06.jpg) / [正常时间播放](Images/playback-06.gif)
- [右向起停逐帧](Images/poses-02.jpg) / [正常时间播放](Images/playback-02.gif)
- [MP7 连续反向](Images/poses-18.jpg) / [播放](Images/playback-18.gif)
- [M4 连续反向](Images/poses-16.jpg) / [播放](Images/playback-16.gif)

## 未采用的实验及依据

1. 直接插入 Pivot：姿势／支撑阶段不匹配；初版反向约 22.74°/帧，比旧候选更差。`FirelineAuthoredPivotStudy` 是失败实验开关，不在交付入口中。
2. MF 侧跑左右原片站姿与片长差异较大，不能把同名片段直接看作匹配的一对。
3. MM 原生侧跑单独站姿更接近，但与 CMU 慢走混合时髋部朝向不同。中间层反向约 37.79°/帧；旧固定膝盖重建曾将其压到最终约 20.42°，这种遮掩不等于源混合正确。保留源膝盖后暴露约 37.19°，所以撤出当前入口。`FirelineAuthoredSideJogStudy` 仅供隔离研究，不能作为完成版开启。
4. 保留源膝盖不能对旧循环一刀切：旧生成侧跑与新原片的关节平面不同。本轮改为仅随专门动作权重过渡保留，退出后恢复旧循环逻辑。全循环原生化仍须先配齐相容的 Walk／Jog 和相位，不得继续靠足底层掩盖。

失败证据分别保存在本机 Saved 的 `AuthoredGroundFirst`、`AuthoredGroundVisual`、`AuthoredGroundNativeJog`、`AuthoredGroundMatched`、`AuthoredGroundContactFinal`、`AuthoredGroundHingeTransport`；摘要 JSON 一并保留。名称中的 Final/Verified 只是当时运行目录名，不能覆盖后来发现的失败结论。

## 复现与下一步

- `Scripts/prepare_authored_ground.py`：采样不保存原片，隔离 IK retarget。
- `Scripts/build_authored_ground.py` + `URangeAssetTools::BuildAuthoredGroundAssets`：转换原生绑定、镜像停止、姿势特征与距离／支撑曲线。
- 体验开关还需 `FirelineDirectionalLocomotion`、`FirelineCMUSideStepStudy`、`FirelineContactCarryStudy`、`FirelineLocomotionContactFixStudy`；launcher 已设置。
- 检查使用 `FirelineDirectionalGameplayAudit`、`FirelineFullBodyTrace`，`FirelineTraceOnly` 可只导出数据，`FirelineReviewName` 指定独立输出目录。
- 下一步先解决一致的左右 Walk／Jog 原片组合、源脚相位与朝向适配，再处理完整反向支撑和斜向停步；不要跳到 P3 并宣称 P2 已完成。
