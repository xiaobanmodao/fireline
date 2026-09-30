# 原作者姿态与静止接触候选（2026-10-01）

本轮继续修复 M4 枪托、头颈、肩臂接触，保留原 Ryan 头盔几何。新增的是独立静止持枪候选，**不是默认游戏更新，也不是眼线/移动/换弹完成**。用户尚未视觉验收。

## 体验

在 Finder 双击桌面「火力对决／测试版·静止持枪接触.app」。完整路径：`/Users/hht/Desktop/火力对决/测试版·静止持枪接触.app`，指向 `~/UnrealBuilds/Fireline/Launchers/FirelineStaticContactStudy.app`。这是引用共享 LiveCarryProject 的可变测试入口，不是冻结历史版本。

鼠标环绕；1–4 正、侧、后、另一侧；Z 近景/全身；O 机瞄/Coyote；7 辅助线。人物固定在同一个静止姿态，移动、俯仰和武器操作没有在此入口启用。观察枪托间隙、两侧腋下与肘部、握持，尤其注意尚未解决的头部侧倾和眼线。

实际后台 Metal 渲染四面已查看；没有前台窗口、没有操作实体键鼠。截图路径：`Saved/MatureMotionResearch/LiveCarryProject/Saved/LiveAim/native-stock-seat-20261001-30/shot-00.png` 至 `shot-03.png`。启动器资源指向相同项目与姿态，未自动把游戏带到用户前台。

## 确认的根因与改动

原始胸甲侧板部分顶点混入 Shoulder/UpperArm 权重。例：胸甲面645下端 Chest .596 / Torso .404，上端 UpperArm_L .427 / Shoulder_L .573，抬臂会牵拉硬板。原作者原始 Hold/Aim 模型也有腋下相交，直接复制源动作不能免检。

按连接关系、材质与共面硬板识别56个面，修正34个作者顶点（导入为230个拆分渲染顶点）的胸甲权重。9936个原生顶点、9360三角形、131骨骼参考矩阵、手部权重全部不变。没有平滑/缩小/压扁头盔。

读取原作者 `Character.blend` 的37骨骼与 HoldRifle-loop、AimRifle、CombatIdle-loop。AimRifle 的锁骨上抬约4cm是作者位置关键帧，适配保留了它；仅迁移旋转会漏掉肩部动作。全身先做源FK，M4与已有DJ双手保持同一个刚性接触组件，不分别移手。

静止 AimRifle 适配用固定臂长、腕角可行弧与实际表面避让：左肘偏离作者平面约6.46°，右肘约25.73°。这不是原片逐骨骼复制。枪械整体沿枪轴后移3.1127cm，由连续三角面扫掠的首次身体接触3.2627cm减去0.15cm余量得出，不使用骨骼附近的任意枪托点。腕角L11.20°/R38.05°只是范围指标，不能代表美术已合格。

## 检查边界

实际导入网格与原生实播姿态对齐，使用修改权重**之前冻结的面分类和作者顶点邻接**，避免改权重后把失败面移出检查集合。该静止姿态的头颈/枪、胸甲/枪、上臂/枪、非邻接臂/躯干、头颈/身体交叉对均为0。真实后垫135面到肩部表面完整最近距离约0.1215cm（顶点/面＋内点边/边），三条射线的后垫内部判定均为0。不是只检查点或凸包。

131骨骼原生组件输出与候选同帧一致；4面 Metal 截图查看完成；已有头颈对照入口短回放回归通过；原1252资产SHA校验无变化。没有检查全动作、动态混合、所有自相交或全部手指/枪接触。

## 未完成与失败路线

- 原作者 AimRifle 头部侧倾明显，当前保留而不是宣称自然；原作者 Camera 是视点参考，不是眼骨。其位置离现有瞄具轴仍偏离，眼线/贴腮未完成。
- 将 ALS 完整头颈方向套到此身体位置，会产生285对头颈/枪面交叉，未接入候选。
- 把枪对齐旧眼代理/作者 Camera 再向前避让，使左臂不可达，未接入候选。
- 作者 HoldRifle 的普通持枪适配仍有20对臂/躯干交叉；30°限幅避让也未通过，不在此入口混入，也未更新游戏。
- 胸甲权重修正单独套到旧ALS瞄准并不解决枪械穿胸，因此未更新旧入口网格。

下一步应解决保留头盔条件下的头颈与瞄具/枪托共同布局，然后建立同样通过表面检查的普通持枪姿态，才扩展连续准备/瞄准、移动和换弹。禁止用本单帧结果证明这些状态已通过。

## 本地重现

源文件和UE资产单独合法存放，未分发到Git。先具备已有 BodyProportionStudy/ALS/原作者模型数据。

1. Blender运行 `scripts/export_native_ryan_carry.py`，解析原始动作；其原文件路径与SHA见 evidence.json。
2. AnimationPython运行 `scripts/study_chest_panel_weights.py`，得到胸甲权重候选；运行 `scripts/study_native_contact_arc.py --seat-stock` 得到静止姿态。
3. Blender运行 `scripts/export_chest_panel_mesh.py`，只导出新候选；ReferenceProject 的UE Python commandlet运行 `scripts/import_chest_panel_mesh.py`。它拒绝覆盖既有候选；检查既有候选用audit而不是重新导入。
4. `python3 scripts/prepare_live_carry_study.py --static-contact --build --install`。
5. `python3 scripts/run_background_review.py --static-contact --label 新的不重复标签`；AnimationPython运行 `scripts/audit_static_contact.py --run 标签-30`。

不修改默认baseline.json、第一人称、选定作者/原资产、正式0.1.10。证据见 evidence.json。
