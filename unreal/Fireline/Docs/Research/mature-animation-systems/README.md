# 成熟动作系统对照 — 2026-09-27

用户再次判定当前动作怪异，要求下载并研究成熟方案。本次暂停猜测式动作修改；主智能体独立完成，没有子智能体、购买、角色替换或游戏运行时代码修改。当前体态仍未获得视觉验收。

后续用户反馈：**ALS参考的摆臂幅度偏大，其他地方很好。** 因此保留其已观察到的整体姿态、动作节奏与身体协调，适配时收小持枪摆动。该意见认可参考方向，不代表Fireline现有动作通过验收，也不扩展为所有未观察状态已获认可。本次仅记录反馈，没有修改动画。

这次的重点是把**原片、原骨架、配套曲线、动画图和最终约束顺序**一起检查。下载数量不等于研究完成，读取所有片段也不等于逐帧视觉验收。

## 实际取得和读取的内容

| 来源 | 本次操作 | 能回答的问题／限制 |
|---|---|---|
| [Sixze / ALS Refactored](https://github.com/Sixze/ALS-Refactored) | 新下载完整固定提交 `b754d6f0f2bb03741d301f8fb88077ebfe561e17`，28,964,766字节ZIP，726个原文件；实际插件4.18 / UE5.8 | 原骨架、人物、步态、步枪姿态层、曲线、Control Rig与C++实现齐全；不是Apex源工程 |
| ALS原片 | 读取127个AnimSequence，均匀采样3,000个姿势、25个指定骨骼，提取全部浮点曲线原始键；31种曲线名称 | 短片采样更少、每片最多31个姿势；加法动画按其基姿态展开；不能保证采样间没有缺陷 |
| ALS动画图 | 导出22个动画图资产、1个层接口和CR_Als；解析连线与属性 | 导出数量包含接口，不能称为23套独立运行系统；静态图不等于最终运行姿势 |
| 已安装的Epic Lyra | 复用先前完整导出的Base / ItemAnimLayers / Rifle图；新增读取7段步枪动作、427个姿势和精确曲线键、两个Montage | 换弹、切枪、待机、前走、慢跑及其配套控制；不是只有宣传视频 |
| 既有MoCap Online、Quaternius UAL1/2、KayKit、Montray Rifle/MP7、ALS Community、CMU与已选武器供体 | 复核已有研究覆盖，不重复下载、重复计数 | 完整系统参考优先ALS/Lyra；其他片段用于动作内容与时序，不擅称拥有配套游戏实现 |

逐片目录：[source-catalog.csv](source-catalog.csv)。来源、版本、哈希、读取量：[sources.json](sources.json)。下载包SHA256为`c8affb459caa9037f9a1f017701ce31ffaebccaf54dd56006cf7af235c589e11`。

ALS原件726文件逐个与Git树blob哈希核对，含566个Content文件，零差异。Fireline本机1252个资源与锁定清单核对，零缺失、零变化。第三方原件、图导出和完整姿势数据只保存在本地Saved，不进入公开仓库；原作者许可不因研究而改变。ALS源码许可文件为MIT，Lyra继续遵守Epic适用条款。

## 一、当前动作怪异的可定位原因

基线`8e66f29`，`RangeAnimInstance.cpp:133`实际执行顺序是BodyPose → ContactCarry → Feet。前两层都会改手臂，不能把它们当成一个小幅握点修正。

`RangeBodyPose.cpp:145–163`把第一人称操作的整体旋转缩到0.15、位移缩到0.5，再放到头部相对的持枪锚点，随后重求双臂。这保留了手枪内部关系，却不等于第三人称完整动作重定向。

`RangeBodyPose.cpp:196–206`的肘偏好在未启用SourceBody时主要来自躯干空间的固定前／侧／下偏移，再结合腕向与身体代价选肘。后面的ContactCarry又进行注册、接触重建和肘求解。原动作中肩、肘、腕如何共同移动，并未因此得到完整保留。

本次重新计算已有原生追踪`Saved/MP7CorrectionMovementFinal/poses.csv`，M4前移group 0，0.75–1.4秒共40帧。每层先去掉胸部旋转，再比较相同上臂段的方向；结果为中位数：

| 层间 | 左上臂变化 | 右上臂变化 |
|---|---:|---:|
| 已适配武器／操作层2 → BodyPose层5 | 30.8° | 31.8° |
| BodyPose层5 → 最终层3 | 31.9° | 26.8° |

这证明存在明显的连续重写；**不能把两行相加，也不能把这些数值当作人体非法角度或穿模判据**。层2、Contact层6本身都已适配，均不是未经处理的供体原片。报告：[layer-changes.json](layer-changes.json)。旧MP7连续性修正只解决部分突变，未解决整个姿态结构。

## 二、Lyra真正值得迁移的设计

[Epic官方动画说明](https://dev.epicgames.com/documentation/en-us/unreal-engine/animation-in-lyra-sample-game-in-unreal-engine)配合本地实际图核验：

1. **全身移动是基础，操作分层有明确顺序。** Base图有移动状态机、左右手状态处理、上下身遮罩、加法操作／瞄准、FullBody槽、惯性化、根朝向、骨骼控制和足底处理。不同层不能都独立决定肩肘最终位置。
2. **手部目标与武器参考空间一起移动。** Item图先HandIKRetargeting，随后从`VB IK_Hand_L_weaponSpace`复制左手IK目标，再求双臂。不是任意挪动某一只手补画面。
3. **肘平面有动画来源。** 两个TwoBoneIK的JointTarget分别使用`lowerarm_l/r`骨骼空间，局部偏移为±50。应学习“以已动画化小臂为参考”的关系；不得把±50或源骨轴直接复制到Ryan/DJ。
4. **手何时松开由动作数据决定。** `UpdateSkelControlData`实际得到`HandIK_Left_Alpha = clamp((DisableHandIK ? 0 : 1) - DisableLHandIK, 0, 1)`，右手同理。不能在整段换弹期间强锁左手，也不能动作末尾突然恢复。

本地`MM_Rifle_Reload`长2.2秒，`DisableLHandIK`键为时间约[0.3333, 0.4667, 0.6667, 0.7667]、值[0, 1, 1, 0]。这说明原图有明确松手窗口，**不是本项目的换弹时间表**；Fireline的弹匣插入事件、0.85x速度和选定供体必须各自对应。

可复查图位置（UTF8本地副本）：`ABP_ItemAnimLayersBase.t3d`的UpdateSkelControlData约10340–10485行、FullBody_SkeletalControls约14840–15140行；`ABP_Mannequin_Base.t3d`的最终图约6570–6970行。原图的默认对象未完整提取，不猜测未核实的默认权重。

## 三、ALS完整工程揭示的遗漏

### 姿势库不能当普通动画播放

步枪包含Ready、Relaxed、Aiming状态。`A_Als_Rifle_Poses`虽只有0.4秒，但其用途是姿势库：Ready取7／2／3帧，Relaxed取0／1／6／9／10帧，Aiming取4／5／8帧等。把它从头播放，或只取一帧替代全部状态，都会误用资产。

### 持枪跑与腿部使用同一动作相位

Rifle图的Run_Arms、Sprint_Arms、Sprint_Impulse_Arms播放器使用Movement同步组，角色为AlwaysFollower；它们不是另起一个正弦摆臂。配套分层曲线分别管理左右臂、手、脊柱、局部／网格空间及加法权重。

因此“下载跑步片段，把枪跟着晃”遗漏了相位、原姿态与遮罩。名字相近的Run和Run_Arms也不是全骨架可以无条件互换的资产。

### 先分层，再统一完成约束

主图顺序可追踪为：站／蹲→Grounded→Transitions槽→Locomotion→PostLocomotion槽；Overlay经过其惯性化，与身体在Layering合并，再经Head/View、Control Rig和Ragdoll到输出。

C++ `AlsAnimationInstance.cpp:288–334`按头、左右臂／手、脊柱、骨盆、腿读取独立曲线；约408–475行的瞄准脊柱启停保留变化过程并阻尼恢复。`AlsRigUnit_HandIkRetargeting.cpp`按左右FK手与IK手的位置差求共同平移，再作用于目标集合。我们应借鉴职责与时序，不能宣称这一个节点就能解决所有腕弯与碰撞。

[作者关于重定向的说明](https://github.com/Sixze/ALS-Refactored/discussions/15)也明确涉及重定向后的手IK、虚拟骨与武器关系；只换骨架映射不等于适配结束。ALS ControlRig已导出，但本次未把其每条PoleVector连接全部追完，不能把Lyra的具体肘参考做法自动归到ALS名下。

本次ALS动画目录没有命名为Rifle Reload的完整换弹片段；不能把跑步姿态包宣称为M4操作动画包。

## 四、对Fireline下一次修改的约束

以下是从原系统与当前代码对比得到的工程方案，**尚未接入，也不是已修复声明**。

摆臂幅度的适配要求：普通持枪行走采用小而均匀的摆动；冲刺保留明确的运动感，但收敛参考中夸张的幅度；ADS／开火继续保持稳定。沿原动作的相位与携枪路径收小运动范围，并检查整条肩肘腕链与双手接触；不得用整体减速、逐关节独立压缩旋转或重新添加通用肘偏移替代。具体收小比例尚未确定，先比较原幅度与收敛候选，不凭空指定统一百分比。原版参考资产保持不动。

| 内容 | 必须保留／改变的关系 | 首轮验收 |
|---|---|---|
| 第三人称动作基础 | 使用完整躯干→锁骨→上臂→小臂→手的源动作关系；明确唯一主导层 | 原供体、仅重定向、接触修正、最终蒙皮四阶段对照 |
| 持枪接触 | 保留已选手／枪关系；整套携枪姿态与身体共同配准；IK只补比例和有限接触差 | 正面、侧面、背面都握住，右臂无插身，腕不被迫折起 |
| 肘方向 | 根据可靠源动画肘平面求解，近伸直时保留连续参考；不能复活此前失败的简单source-elbow补丁 | 全周期、起停、切换时没有极向翻转 |
| 换弹 | 操作动作决定左手松开／抓匣／回握窗口；用同一时序驱动接触与弹匣 | 对齐插入刷新弹药；保留开火／切枪打断与尾段就绪 |
| 第一人称 | 沿用已认可手枪动画和独立构图，不把镜头夸张位置当全身目标 | 第一人称握点、机瞄／Coyote、开火反馈不退化 |
| 移动 | 步态、武器层共享相位，原片起停与脚底修正保留 | 普通行走安静，冲刺状态独立；开火限速和抑制步态枪晃保留 |

先只做M4待机→前走→停下的小闭环，证明源动作关系未被后层破坏，再扩展换弹与MP7。若必须大幅改变肩肘才能接触枪，应退回检查枪体尺度、骨轴、配准和动作选择；不得把大幅偏移藏进“平滑”或再加第三个求臂层。数值、功能与视觉分别验收。

## 原版观察入口与复现

桌面「火力对决」内 **参考·ALS原版动作.app**：独立ReferenceProject，运行下载工程的原版Playground、角色与动画图。宿主只设置初始Rifle模式和观察方向，不替换Fireline角色，也不是Fireline新测试版本。原窗口已成功运行；窗口右侧显示原版操作表。WASD移动，Shift冲刺，Ctrl蹲伏，空格跳跃／攀爬，右键瞄准，Q姿态菜单，Tab显示／隐藏操作表。

本次自动界面控制只确认了场景、角色与Tab响应；这不能算所有原版动作和手柄通过实玩。命令行姿势采样与静态图结论单独记录，不冒充原版全状态视觉检查。

本地资料目录：`unreal/Fireline/Saved/MatureMotionResearch`。首次宿主编译使用最多两个并行任务，降低机器负载；演示限45FPS并降低阴影。没有启动Fireline编译或批量游戏回归。

可复现命令（仓库根目录运行）：

```sh
python3 scripts/prepare_mature_motion_reference.py --build --launcher --lyra /Users/hht/UnrealProjects/LyraStarterGame
python3 scripts/research_animation_graphs.py unreal/Fireline/Saved/MatureMotionResearch/als-graphs unreal/Fireline/Saved/MatureMotionResearch/als-graph-nodes.json
python3 scripts/research_pose_layer_changes.py unreal/Fireline/Saved/MP7CorrectionMovementFinal unreal/Fireline/Saved/MatureMotionResearch/current-layer-changes.json
python3 scripts/verify_assets.py
```

UE读取脚本为`scripts/inspect_mature_animation_reference.py`、`scripts/inspect_lyra_contact_design.py`，通过ReferenceProject的UnrealEditor-Cmd运行`-run=pythonscript -script=绝对路径 -unattended -nullrhi -nosound`；Lyra只读取原片时加`-ReferenceSourceOnly`。图脚本可用`-ReferenceGraphsOnly`重导，避免重复采样。

过程限制：第一次ALS加法片采样遇到基础姿态异步编译死锁，隔离宿主增加FinishAllCompilation后完成；一次错猜图路径失败，改为注册表枚举后完成。直接启动已装Lyra因LyraGame模块不能加载失败；隔离宿主完成7片读取，但其Lyra Notify蓝图缺少游戏类并有编译报错，因此**没有验证Notify运行、完整Lyra玩法或默认对象权重**。这些失败没有通过修改原件掩盖。
