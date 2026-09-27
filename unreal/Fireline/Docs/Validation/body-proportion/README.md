# Ryan/DJ 比例与连接修订（2026-09-27）

用户否决上一版 WholeCarry：胸部过大、穿模、手臂与身体连接不自然。本次按原版 ALS 人体比例，修改**现有 Ryan/DJ 的独立派生模型**，没有换成 ALS 人物；没有更新默认游戏、第一人称或正式 0.1.10。待机、前走、停下之外的动作尚未迁移到此派生骨架。

## 体验入口

桌面「火力对决／测试版·角色比例修正.app」。双击后直接进入原生 UE 演示。

- 左：ALS 原版；中：上一版 Ryan/DJ；右：本次比例与连接修订。
- `R` 重播6.5秒片段；`空格` 暂停；`Z` 四分之一速度。
- `1/2/3/4` 正面／右侧／背面／左侧；`Tab` 三栏／单独观察。
- `5` 上半身近景；左右方向键逐帧。

原「测试版·M4全身动作对照.app」、手柄基线、正式版及原始资源保留。该演示是本机派生模型/录制动作回放，不是可自由输入移动的新游戏版本。

## 定位到的问题与处理

1. **肩宽与胸甲不匹配参考人体。** 原肩关节跨度49.51cm；按 ALS 肩宽/模型高度比例，保持Ryan原高度后应为39.18cm。肩关节及其完整手臂分支一起向内移动，每侧约5.17cm，骨骼自身轴与上下臂长度保留。胸部主截面宽度乘0.84、厚度乘0.74，邻接腰部/肩甲分区过渡；没有运行时非均匀缩放骨骼。
2. **袖口依靠遮挡而非连接。** 原网格经合并渲染接缝后有身体、左右手三个分量；两只袖口各有6点开口，两只手套内衬各有30点开口。本次沿真实边界顺序增加72个内部连接三角面，不新增圆球、独立补丁或尖缩接头。闭合后一个连通分量、零开放边、零非流形边。肩/肘原有黑色内衬仍是连接结构，不以取消正常关节接缝冒充修复。
3. **先前使用了错误层级的参考矩阵。** 原 SkeletalMesh 自身的 RefSkeleton 与共享 Skeleton 的部分DJ参考变换不同。旧 `get_reference_pose(mesh.skeleton)` 不能代表该网格的实际蒙皮绑定；例如DJ前臂辅助骨的位置、腕骨方向不一致。本次直接从实际 SkeletalMesh 导出组件空间绑定矩阵，派生模型拥有单独持久化 Skeleton，动画计算也使用该模型的实际矩阵。没有覆盖原Skeleton来强行修正历史动作。
4. **端点正确不足以排除穿模。** 原只限制肘/腕中心的胸廓近似无法约束护甲厚度和枪托。派生版本以实际蒙皮胸廓表面构建保守凸包，拟合时加入完整前臂、手套、远端上臂及枪械表面间隙。双手、手指、整枪保持公共刚体接触关系，只调整整个携枪组件和可行肘平面。
5. **重导入平滑过度。** 合并渲染裂缝后重新保留Ryan的硬棱面；DJ手部保持平滑。没有通过加细分统一风格。

DJ手指/手掌表面保持原形。新旧绑定空间中消除每侧统一平移后，原手部顶点最近点最大误差约0.000037cm。新模型蒙皮仍使用原权重；仅新连接面的边缘共享现有顶点/权重。原下半身几何、既有脚步适配和模型高度保留。

## 验证与边界

数据：`surface-audit.json`、`audit.json`、`registration.json`、`construction.json`、`derivative-manifest.json`。

- 391帧，60Hz：固定上下肢长度、双腕相对机匣位置、实际靴底高度检查。
- 391帧实际表面：7,936个手套/前臂/远端上臂三角面及10,702个枪械三角面对胸廓保守凸包做半空间裁剪，交叠三角面为0；最低顶点间隙手臂约0.89cm、枪约0.96cm。
- 肩关节9cm内属于连接区域，未作为分离物体参与胸廓穿插测试。该检查不等于全人物所有部位互相不交叠，也不覆盖换弹、瞄准、冲刺、侧移、MP7、近战等未移植状态。
- 7个关键时刻 × 4个方向 × 全身/近景，共56组原生骨骼输出及截图；已查看正面、侧面和背面关键帧。实际桌面入口启动到ReferenceProject并记录READY；本轮自动控制被应用切换中断，不能把新近景快捷键记为人工操作验收。
- 原1252个Fireline资源哈希未变；原ALS源文件不修改。第一人称和正式版本不改。

本次是比例、连接和M4录制携枪片段的可观察修正版；用户视觉验收尚未发生。不得用数值通过宣称所有动作完美或默认游戏已全面修复。上一版WholeCarry已被用户否决，旧报告中有限观察不能推翻用户指出的缺陷。

## 本机资源与复现

独立宿主：`unreal/Fireline/Saved/MatureMotionResearch/ReferenceProject`。派生UE资源只在该宿主 `/Game/BodyProportionStudy`，绝不写入映射的 `/Game/Fireline`。原导出、Blender修订、FBX、派生UE资源和回放数据另存于 `unreal/Fireline/SourceAssets/BodyProportionStudy/ba7589acc53a`，本机保留，不分发到公开Git。

1. `python3 scripts/prepare_mature_motion_reference.py --build --fireline` 构建独立宿主。
2. UE Python运行 `export_body_proportion_reference.py`，导出原网格和参考网格；FBX材质导出需 `-AllowCommandletRendering -RenderOffScreen`，不可用NullRHI。下一步脚本会从FBX自动创建Blender测量数据与源副本。
3. Blender运行 `build_body_proportion_mesh.py`。UE Python运行 `import_body_proportion_mesh.py`，仅保存独立派生网格和Skeleton并导出实际渲染/绑定数据。
4. 本机 `Saved/AnimationPython/bin/python scripts/build_whole_carry_candidate.py --proportions`。携枪初值固定为第一次表面注册结果，避免依赖任意上一次输出；当前0.55摆动幅度仍是候选参数。
5. `scripts/audit_body_proportion_surface.py`做全片实际三角面检查。启动独立宿主 `-game -WholeCarryReview -BodyProportionReview -WholeCarryCapture` 获取56组原生记录，再运行 `scripts/audit_whole_carry_review.py --proportions`。
6. `python3 scripts/install_whole_carry_review.py --proportions`安装桌面入口。没有该参数时仍安装旧演示，不会覆盖它。

本地日志记录过导出NullRHI断言、导入未持久化新Skeleton的启动失败；已分别以渲染导出和显式保存派生Skeleton修复。最终实际启动必须通过后才交付。
