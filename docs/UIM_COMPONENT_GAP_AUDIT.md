# UIM 能力审计与编辑器缺口补齐报告

- 审计对象：`C:\Users\chen\Documents\GitHub\UIM`（C11 UI 最小件库，Windows，CMake + VS 2026）
- 消费方 A：`C:\Users\chen\Documents\GitHub\zcsr_editor`（C++/ImGui 编辑器宿主 + 共享 C11 kernel）
- 消费方 B：`C:\Users\chen\Documents\PVZ`（C11 + 自有 GL 编辑器壳 `pvzeditor`）
- 审计日期：2026-11-24；基线：现有 13 项 CTest，全部通过。
- 判定线：**两个以上工程都会用到、且是通用编辑器能力**才进入 UIM；项目私有数据、宿主外壳组合、渲染绘制一律留在消费方。

## 1. 结论摘要

| 项 | 结果 |
|---|---|
| UIM 基线测试（独立 build） | **13/13 通过**（`docs/audit_logs/baseline_ctest.log`） |
| 本次新增原子 | **7 个** header-only 纯 C 原子，全部接入 CMake INTERFACE 目标、安装规则、契约测试与 C++ 消费方契约 |
| 补齐后 UIM 全量测试 | **13/13 通过**（`docs/audit_logs/audit_ctest_final.log`），基线 13 项未减少 |
| zcsr_editor 构建验证 | 配置通过；链接 UIM 的 `zcsr_editor_kernel` 与无头测试目标构建通过（`docs/audit_logs/editor_*.log`） |
| PVZ 构建验证 | 配置通过；编译 UIM 源码的 `pvz_editor_app` 与宿主 `pvzeditor.exe` 链接通过；`check` 无头探针运行通过（`docs/audit_logs/pvz_*.log`） |
| 未补齐 | 见第 4 节，均为单消费方需求、宿主外壳组合或项目私有数据 |

新增原子（均在 `include/uim_*.h`，无归档、无分配、无全局状态、无 ImGui/SDL/第三方 UI 依赖）：

| 原子 | 头文件 | 一句话职责 |
|---|---|---|
| numeric_field | `uim_numeric_field.h` | 带范围钳制、步进、拖拽增量、严格文本解析与提交/回退基线的整型/浮点数值输入件 |
| dialog | `uim_dialog.h` | 有界模态/非模态对话框栈，接受/取消/关闭结果与焦点归还 |
| menu | `uim_menu.h` | 菜单栏、下拉、右键上下文菜单共用的扁平菜单项模型与开放/悬停/键盘步进/命中 |
| tree | `uim_tree.h` | 与节点数据/文件 IO 解耦的展开集合与单选主选状态 |
| tooltip | `uim_tooltip.h` | 悬停延迟显隐与重新触发的计时状态机，弹窗布局交给 `uim_placement` |
| property_row | `uim_property_row.h` | 属性行 label/editor 成对布局与 enabled/read-only 交互状态 |
| splitter | `uim_splitter.h` | 分割条比例与双面板最小尺寸约束，面板几何交给 `uim_layout_split` |

---

## 2. 缺口矩阵

证据列为消费方源码位置；“UIM 现有覆盖”列指向现有头文件。结论：**已覆盖** / **已补齐** / **不需要（私有或宿主层）**。

### 2.1 按钮、开关、选择类

| 能力 | 消费方证据 | UIM 现有覆盖 | 结论 |
|---|---|---|---|
| 按钮点击 / 重复 | editor_app.cpp `Button`×42、`SmallButton`×16（如 3699 一带菜单项按钮） | `uim_button.h`（hover/press/click edge、键盘激活、长按重复） | 已覆盖 |
| 复选框 / 开关 | editor_app.cpp `Checkbox`×11 | `uim_toggle.h` | 已覆盖 |
| 滑条 | editor_app.cpp `SliderFloat`×1 | `uim_slider.h` | 已覆盖 |
| 下拉 / Combo | editor_app.cpp `Combo`/`BeginCombo`×5；PVZ `pvzeditor.c:1987` `uim_dropdown_init`、`:2081` `uim_dropdown_hit` | `uim_dropdown.h` | 已覆盖 |
| 页签 | shell 页签；PVZ 面板切换 | `uim_tabs.h` | 已覆盖 |
| 单选组 | editor_app.cpp `RadioButton` 3654、3656 | `uim_dropdown.h` 单选索引模型可表达；`uim_selection.h` 可选集合 | 已覆盖（组合复用，不单列原子） |
| 可选列表行 | editor_app.cpp `Selectable`×5 | `uim_button.h` + `uim_selection.h` + `uim_virtual_list.h` | 已覆盖（组合复用） |

### 2.2 文本、数值、属性行

| 能力 | 消费方证据 | UIM 现有覆盖 | 结论 |
|---|---|---|---|
| 文本输入 / 行内改名 | editor_app.cpp 改名弹窗 `1910`（文本状态走 UIM，注释见 252/300/556）；PVZ `pvzeditor.c:570-642`、`2261-2315`、`2902-3046` 多处 `uim_text_input_*` | `uim_text_input.h`（UTF-8 光标/选择/提交/取消） | 已覆盖 |
| **数值字段（键入 + 步进 + 范围钳制 + 拖拽）** | editor_app.cpp `DragFloat`×27（2441、2447-2451、2482、2488、2494、2497、2579、3285、3286、3358、3545、3597-3611、3645-3659…）、`DragInt`×5（2500、2503、2514、3352、3622）、`InputFloat` 3581-3587、`InputScalar` 2529；PVZ `pvzeditor.c:2231` `strtod`、`:2712` `strtol`、`:5586` 自绘步进区 | 仅有 `uim_stepper.h`（步进+钳制）与 `uim_slider.h`（轨道拖拽）；**没有**“文本键入解析 + 步进 + 拖拽增量 + 提交基线”合一的字段状态 | **已补齐 `uim_numeric_field`** |
| **属性行（label + editor 成对布局）** | editor_app.cpp 检视面板每一行 `DragFloat("x", …)`、`ColorEdit4("tint", …)`；PVZ 右侧字段行 | `uim_layout.h` 只做行列求解；没有 label 列宽/间隙/只读约定 | **已补齐 `uim_property_row`** |
| 数值格式化 | 各处 `%.2f`/`%.3f` | `uim_format.h` | 已覆盖 |
| 颜色值 / 解析 / 对比 | editor_app.cpp `ColorEdit4` 2507、3239 的值侧 | `uim_color.h`（RGBA、`#RRGGBB[AA]` 解析/格式化、混合、亮度/对比） | 部分覆盖；**颜色选择器面板见第 4 节，本次不补** |

### 2.3 弹窗、菜单、右键、树、提示、分割

| 能力 | 消费方证据 | UIM 现有覆盖 | 结论 |
|---|---|---|---|
| **模态对话框 / 确认框（层级 + 焦点归还）** | editor_app.cpp `BeginPopupModal` 1910（改名）、2330（项目改名）、2359（删除确认）；`OpenPopup` 1908、2219、2233、2238；PVZ `pvzeditor.c:158` `close_confirm_pending`、`:5178/:5483/:5762/:5777` 关闭确认流 | 无弹窗栈、无模态阻塞标记、无结果生命周期与焦点归还 | **已补齐 `uim_dialog`** |
| **菜单栏 / 命令下拉** | editor_app.cpp `BeginMainMenuBar` 3697、`BeginMenu` 3698/3731/3763/3792/3811、`MenuItem`×56（3699-3808 等）；PVZ 经共享 shell 走菜单命令路由 | UIM 无通用菜单项模型；shell 内菜单耦合在编辑器共享源码 | **已补齐 `uim_menu`**（通用开放/悬停/选择模型；命令注册表仍属 shell） |
| **右键上下文菜单** | editor_app.cpp `BeginPopupContextItem` 2418、3117；PVZ `handle_right_click` 5732（目前直接执行动作，无菜单模型） | 无 | **已补齐 `uim_menu`**（root 头 id=0 即在光标处弹出的上下文菜单） |
| **树视图（展开 / 选中）** | editor_app.cpp `TreeNodeEx` 3427、`CollapsingHeader` 2439；`ui_hierarchy.c` 自维护 `expanded[]/selected`；PVZ 复用 `ui_project_browser.c` 文件树 | 无通用展开集合/单选状态（文件树状态在消费方领域层） | **已补齐 `uim_tree`** |
| **Tooltip 延迟提示** | editor_app.cpp `SetTooltip` 2208、2311 | `uim_placement.h` 只做弹窗定位；无悬停计时/显隐/重触发状态 | **已补齐 `uim_tooltip`** |
| **Splitter / Sash 拖拽** | shell `SPLITTER` 捕获；面板分割布局在 `ui_editor_workspace.c`；PVZ 左/中/右三栏 | `uim_layout.h` `layout_split` 只算静态几何；`uim_drag.h` 只管手势阈值；无比例 + 最小面板约束状态 | **已补齐 `uim_splitter`** |
| 滚动区域 / 滚动条 | editor_app.cpp `BeginChild`×6；PVZ `draw_scrollbar` 3458 及 3595/3832/4260/4567/4868/4918 自绘调用 | `uim_scroll.h`（钳制偏移、滚轮/翻页、thumb 几何、reveal）+ `uim_virtual_list.h` | 已覆盖（PVZ 绘制迁移为后续候选，非新缺口） |
| 焦点环 / 键盘导航 | 编辑器与 PVZ 的 Tab/方向键路由 | `uim_focus.h` | 已覆盖 |
| 浮层定位（翻转/钳制/箭头） | 弹窗、菜单、tooltip 定位 | `uim_placement.h` | 已覆盖 |

### 2.4 明确不进入 UIM 的能力（理由见第 4 节）

| 能力 | 证据 | 判定 |
|---|---|---|
| 颜色选择器面板（HSV 环/色域/alpha 弹层） | editor_app.cpp `ColorEdit4` 2507、3239；PVZ 无颜色编辑 | 单消费方 + 渲染密集，暂缓 |
| 停靠工作区 / 浮动面板 / DockSpace | editor_app.cpp `DockBuilder*`；`ui_editor_workspace.c` | 编辑器外壳组合，非可复用原子 |
| 拖拽载荷类型（节点 id / 资产引用） | editor_app.cpp `BeginDragDropSource/Target` | `uim_drag.h` 管手势；载荷是领域数据 |
| 状态栏文本槽位 | shell `STATUS_BAR` 元素；PVZ `state.status` | 宿主文本条，无可复用状态 |
| 多列表格模型 | editor_app.cpp 检视网格 | 几何由 `uim_layout.h` grid + virtual list 覆盖；表格模型暂缓 |
| 菜单命令注册表 / 快捷键表 | editor_app.cpp 菜单命令分发 | shell 领域；`uim_menu` 只提供通用模型 |
| 文件对话框 / 偏好设置面板 | editor_app.cpp Open/Save | 文件服务与 shell 领域 |

---

## 3. 已补齐组件明细

七个组件均为 **header-only static inline 原子**：结构体由调用方分配，库内不分配、不持全局状态、不调 OS/渲染 API；跨原子依赖仅通过 `#include` 表达（`property_row`/`splitter` → `layout`，其余 → `types`）。

### 3.1 `uim_numeric_field` — 数值字段

- 头文件：`include/uim_numeric_field.h`；CMake 目标：`zcsr_uim_atom_numeric_field`
- 状态机 / 生命周期：`init` → （`step` / `drag_by` / `parse` 编辑，`dirty` 跟踪与基线差异）→ `commit`（固化基线）或 `cancel`（回退基线）；`read_only` 下一切写入返回 `uim_ERR_STATE`。
- 关键 API：

```c
uim_Result uim_numeric_field_init(uim_NumericField *f, uim_NumericKind kind,
                                  double min_value, double max_value,
                                  double value, double step);
uim_Result uim_numeric_field_set_value(uim_NumericField *f, double value);
uim_Result uim_numeric_field_step(uim_NumericField *f, int delta);      /* +1/-1 步长 */
uim_Result uim_numeric_field_drag_by(uim_NumericField *f, double units); /* 拖拽已换算成值单位 */
uim_Result uim_numeric_field_parse(uim_NumericField *f, const char *text, size_t bytes);
uim_Result uim_numeric_field_commit(uim_NumericField *f);
uim_Result uim_numeric_field_cancel(uim_NumericField *f);
uim_Result uim_numeric_field_get_double(const uim_NumericField *f, double *out);
uim_Result uim_numeric_field_get_int64(const uim_NumericField *f, int64_t *out);
```

- 错误码：`uim_ERR_ARGUMENT`（空指针/非有限/区间非法/kind 非法/step<0）、`uim_ERR_RANGE`（值被钳到边界；`parse` 越界不改原值）、`uim_ERR_TYPE`（严格全串解析失败：空白、NaN/Inf、尾垃圾、整型字段含小数）、`uim_ERR_STATE`（只读写入）。
- 测试清单（`tests/test_atoms.c` → `test_numeric_field`）：初始化与初始值钳制；步进到边界；只读拒绝；浮点解析成功/非法串/空白/残缺指数/越界；整型拒绝小数；提交/取消 dirty 与回退；拖拽增量钳制；空指针。压力不变量（`test_atoms_stress.c` → `stress_numeric`）：随机区间 + 随机步/拖拽/赋值后值始终有限且落在 `[min,max]`。
- 消费方接入示例（文档示例，不改消费方源码）：

```c
uim_NumericField radius;
uim_numeric_field_init(&radius, uim_NUMERIC_FLOAT, 0.1, 200.0, current, 0.1);
/*  spin 按钮按下： */ uim_numeric_field_step(&radius, +1);
/*  轨道拖拽：       */ uim_numeric_field_drag_by(&radius, dx * speed);
/*  文本框提交时：   */ uim_numeric_field_parse(&radius, text, bytes);
/*  Enter 固化：     */ uim_numeric_field_commit(&radius);
/*  Esc 还原：       */ uim_numeric_field_cancel(&radius);
```

> 向量 2/3/4（`DragFloat2/3/4`）由多个 `uim_numeric_field` 组合，不另设原子。

### 3.2 `uim_dialog` — 对话框栈

- 头文件：`include/uim_dialog.h`；CMake 目标：`zcsr_uim_atom_dialog`
- 状态机 / 生命周期：`init` → `open` 压栈（记录模态标记、打开前焦点属主）→ 宿主每帧读 `top`/`modal_blocked` 决定是否屏蔽背景 → 按钮/Esc 调 `accept_top`/`cancel_top`/`resolve` 记录结果并标记关闭 → `dismiss_closed` 从栈顶弹出已关闭帧 → 栈空后 `focus_to_restore` 归还焦点。容量 `UIM_DIALOG_CAPACITY = 8`。
- 关键 API：

```c
uim_Result uim_dialog_open(uim_DialogStack *s, uint32_t id, bool modal,
                           uint32_t focus_owner, size_t *out_index);
uim_Result uim_dialog_top(const uim_DialogStack *s, const uim_DialogFrame **out);
bool        uim_dialog_modal_blocked(const uim_DialogStack *s);
uim_Result uim_dialog_resolve(uim_DialogStack *s, uint32_t id, uim_DialogResult r);
uim_Result uim_dialog_accept_top(uim_DialogStack *s);
uim_Result uim_dialog_cancel_top(uim_DialogStack *s);
uim_Result uim_dialog_dismiss_closed(uim_DialogStack *s);
uim_Result uim_dialog_focus_to_restore(const uim_DialogStack *s, uint32_t *out_id);
```

- 错误码：`uim_ERR_ARGUMENT`（空指针/id=0/结果为 NONE）、`uim_ERR_EXISTS`（重复打开同 id，栈不变）、`uim_ERR_BUFFER`（超容量）、`uim_ERR_NOT_FOUND`（关闭/查找不存在）、`uim_ERR_STATE`（栈仍非空时取归还焦点）。
- 测试清单（`test_dialog`）：打开模态、重复打开拒绝、叠加非模态、顶部解析、结果读取、弹出回收、焦点归还 id、空栈取消顶部报错、容量上限、空指针。
- 消费方接入示例：

```c
static uim_DialogStack g_dialogs;
uim_dialog_init(&g_dialogs);
/* 删除确认： */
uim_dialog_open(&g_dialogs, 7u, /*modal=*/true, current_focus, NULL);
/* 每帧：背景交互在 uim_dialog_modal_blocked(&g_dialogs) 为 true 时屏蔽。 */
/* 确认按钮： */ uim_dialog_accept_top(&g_dialogs);
/* Esc：     */ uim_dialog_cancel_top(&g_dialogs);
/* 分发结果后： */ uim_dialog_dismiss_closed(&g_dialogs);
uint32_t restore;
if (uim_dialog_focus_to_restore(&g_dialogs, &restore) == uim_OK) focus_set(restore);
```

### 3.3 `uim_menu` — 菜单 / 上下文菜单模型

- 头文件：`include/uim_menu.h`；CMake 目标：`zcsr_uim_atom_menu`
- 状态机 / 生命周期：`set_items` 一次性装载扁平描述（id 唯一、label 借用）→ `open(header_id)` 弹出某头部；`header_id = 0` 即根上下文菜单 → 指针 `hit` 同步激活项；键盘 `step` 在同头子项间循环步进，跳过分隔符与禁用项 → `activate` 取走可执行叶子命令 id 并关闭。容量 `UIM_MENU_CAPACITY = 64`。
- 关键 API：

```c
uim_Result uim_menu_set_items(uim_Menu *m, const uim_MenuItem *items, size_t count);
uim_Result uim_menu_open(uim_Menu *m, uint32_t header_id);   /* 0 = 根/上下文菜单 */
void       uim_menu_close(uim_Menu *m);
uim_Result uim_menu_step(uim_Menu *m, int direction);       /* +1 下 / -1 上 */
uim_Result uim_menu_activate(uim_Menu *m, uint32_t *out_id);
uim_Result uim_menu_hit(uim_Menu *m, const uim_Rect *rects, size_t n,
                        float x, float y, size_t *out_index);
```

- 标志位：`SEPARATOR / CHECKABLE / ENABLED / CHECKED / SUBMENU`。
- 错误码：`uim_ERR_ARGUMENT`、`uim_ERR_BUFFER`（超容量）、`uim_ERR_EXISTS`（id 重复，模型不变）、`uim_ERR_NOT_FOUND`（打开不存在的子头/项）、`uim_ERR_STATE`（未打开时步进/激活；激活到分隔符/禁用/子头）、`uim_ERR_RANGE`（越界取项）。
- 测试清单（`test_menu`）：装载与重复 id 拒绝；打开子头并定位首项；步进跳过分隔符/禁用项并回绕；命中测试与越界；根上下文菜单激活；命令 id 返回并关闭。
- 消费方接入示例：

```c
static const uim_MenuItem items[] = {
  {1u, 0u, 10u, "File",  ENABLED | SUBMENU},
  {2u, 1u, 0u,  "New",   ENABLED},
  {3u, 1u, 0u,  "Open",  ENABLED},
  {4u, 1u, 0u,  NULL,    SEPARATOR},
  {5u, 1u, 0u,  "Quit",  0u /* disabled */},
};
uim_Menu menu;
uim_menu_init(&menu);
uim_menu_set_items(&menu, items, 5);
uim_menu_open(&menu, 1u);              /* 菜单栏下拉；上下文菜单传 0u */
uim_menu_hit(&menu, rects, 5, x, y, &idx);
uim_menu_step(&menu, 1);
uint32_t cmd;
if (uim_menu_activate(&menu, &cmd) == uim_OK) dispatch(cmd);
```

### 3.4 `uim_tree` — 树展开 / 选中状态

- 头文件：`include/uim_tree.h`；CMake 目标：`zcsr_uim_atom_tree`
- 状态机 / 生命周期：`init` → 宿主按深度优先枚举可见行（折叠子树跳过）→ 箭头按压 `toggle` 维护展开集合；行点击 `select` 维护主选；`hit` 用宿主提供的行 id + 矩形做命中。与节点数据、父子遍历、文件 IO 完全解耦。展开容量 `UIM_TREE_CAPACITY = 64`。
- 关键 API：

```c
uim_Result uim_tree_expand(uim_Tree *t, uint32_t id);
uim_Result uim_tree_collapse(uim_Tree *t, uint32_t id);
uim_Result uim_tree_toggle(uim_Tree *t, uint32_t id, bool *out_expanded);
void       uim_tree_collapse_all(uim_Tree *t);
uim_Result uim_tree_select(uim_Tree *t, uint32_t id);   /* id=0 清除选择 */
uim_Result uim_tree_get_selected(const uim_Tree *t, uint32_t *out_id);
uim_Result uim_tree_hit(const uim_Tree *t, const uint32_t *ids,
                        const uim_Rect *rects, size_t count,
                        float x, float y, uint32_t *out_id);
```

- 错误码：`uim_ERR_ARGUMENT`、`uim_ERR_EXISTS`（重复展开）、`uim_ERR_BUFFER`（展开集合满）、`uim_ERR_NOT_FOUND`（折叠/取选择不存在）。
- 测试清单（`test_tree`）：展开/重复展开拒绝/折叠/toggle 往返；主选设置与清除；行命中；折叠不存在项报错；空指针。
- 消费方接入示例：

```c
uim_Tree tree;
uim_tree_init(&tree);
/* 箭头按压： */ bool open; uim_tree_toggle(&tree, node_id, &open);
/* 行点击：   */ uim_tree_select(&tree, node_id);
/* 命中：     */ uint32_t hit;
uim_tree_hit(&tree, row_ids, row_rects, row_count, x, y, &hit);
```

### 3.5 `uim_tooltip` — 悬停提示计时

- 头文件：`include/uim_tooltip.h`；CMake 目标：`zcsr_uim_atom_tooltip`
- 状态机 / 生命周期：`init(delay)` → 每帧 `set_hover(id, hovered)`：换触发件或重新进入会重置停留计时 → `tick(dt)` 累计，达到延迟后 `visible`；离开立即隐藏；点击/滚动/弹层打开调 `dismiss`。弹窗矩形与翻转钳制交给 `uim_placement`。
- 关键 API：

```c
uim_Result uim_tooltip_init(uim_Tooltip *tip, float delay_seconds);
uim_Result uim_tooltip_set_hover(uim_Tooltip *tip, uint32_t trigger_id, bool hovered);
uim_Result uim_tooltip_tick(uim_Tooltip *tip, float dt);
void       uim_tooltip_dismiss(uim_Tooltip *tip);
bool       uim_tooltip_is_visible(const uim_Tooltip *tip);
```

- 错误码：`uim_ERR_ARGUMENT`（空指针/负延迟/负或非有限 dt）。
- 测试清单（`test_tooltip`）：非法延迟拒绝；未达延迟不可见；达到延迟可见；切换触发件重置计时；离开后 tick 不再显示；dismiss 立即隐藏。
- 消费方接入示例：

```c
uim_Tooltip tip;
uim_tooltip_init(&tip, 0.25f);
/* 每帧： */
uim_tooltip_set_hover(&tip, item_id, hovered);
uim_tooltip_tick(&tip, dt);
if (uim_tooltip_is_visible(&tip)) {
    /* 用 uim_place_popup 把提示框摆在触发件下方/上方 */
}
```

### 3.6 `uim_property_row` — 属性行

- 头文件：`include/uim_property_row.h`；CMake 目标：`zcsr_uim_atom_property_row`（链接 `layout`）
- 状态机 / 生命周期：`init(label_width)` → `layout` 把一行切成 label 矩形（固定列宽、纵向居中）与 editor 矩形（列宽 + 间隙后的剩余空间）；`enabled`/`read_only` 决定 `interactive`，宿主据此灰显或拒收输入。
- 关键 API：

```c
uim_Result uim_property_row_init(uim_PropertyRow *row, float label_width);
uim_Result uim_property_row_set_enabled(uim_PropertyRow *row, bool enabled);
uim_Result uim_property_row_set_read_only(uim_PropertyRow *row, bool read_only);
bool        uim_property_row_interactive(const uim_PropertyRow *row);
uim_Result uim_property_row_layout(const uim_PropertyRow *row, const uim_Rect *container,
                                   float label_gap, uim_Rect *out_label,
                                   uim_Rect *out_editor);
```

- 错误码：`uim_ERR_ARGUMENT`（空指针/负列宽或间隙/非有限容器）；容器过窄时 editor 矩形宽度钳为 0，不报错。
- 测试清单（`test_property_row`）：label/editor 矩形分割与间隙；只读/禁用下 `interactive` 为假；窄容器 editor 宽度为 0；空指针。
- 消费方接入示例：

```c
uim_PropertyRow row;
uim_property_row_init(&row, 120.0f);
uim_Rect label, editor;
uim_property_row_layout(&row, &container, 8.0f, &label, &editor);
/* 在 label 矩形画标题，在 editor 矩形放置按钮/下拉/数值字段 */
if (!uim_property_row_interactive(&row)) { /* 灰显，拒收指针/键盘 */ }
```

### 3.7 `uim_splitter` — 分割条

- 头文件：`include/uim_splitter.h`；CMake 目标：`zcsr_uim_atom_splitter`（链接 `layout`）
- 状态机 / 生命周期：`init(axis, ratio, min_first, min_second)` → 按压 `begin_drag` → 移动 `drag_to` 按指针位置换算比例并按双面板最小尺寸钳制 → 释放 `end_drag`；`pane_rects` 委托 `uim_layout_split` 出两面板矩形。手势阈值/轴向锁定仍由 `uim_drag` 负责。
- 关键 API：

```c
uim_Result uim_splitter_init(uim_Splitter *s, uim_LayoutAxis axis, float ratio,
                            float min_first, float min_second);
uim_Result uim_splitter_begin_drag(uim_Splitter *s);
void       uim_splitter_end_drag(uim_Splitter *s);
uim_Result uim_splitter_ratio_at(const uim_Splitter *s, const uim_Rect *container,
                                 float gap, float position, float *out_ratio);
uim_Result uim_splitter_drag_to(uim_Splitter *s, const uim_Rect *container,
                                float gap, float position);
uim_Result uim_splitter_pane_rects(const uim_Splitter *s, const uim_Rect *container,
                                   float gap, uim_Rect *first, uim_Rect *second);
```

- 错误码：`uim_ERR_ARGUMENT`（空指针/非法轴向/负最小尺寸/非有限几何）、`uim_ERR_RANGE`（指针位置被最小面板边界钳制）、`uim_ERR_STATE`（未进入拖拽就 `drag_to`）。
- 测试清单（`test_splitter`）：比例边界钳制；拖拽更新比例与面板矩形；非拖拽态 `drag_to` 报 `STATE`；非法最小尺寸拒绝。压力不变量（`stress_splitter`）：随机容器/最小尺寸下比例始终落在 `[lo, hi]`，两面板矩形非负。
- 消费方接入示例：

```c
uim_Splitter sp;
uim_splitter_init(&sp, uim_LAYOUT_ROW, 0.3f, 80.0f, 80.0f);
uim_splitter_begin_drag(&sp);
uim_splitter_drag_to(&sp, &panel, 4.0f, pointer_x);
uim_splitter_end_drag(&sp);
uim_Rect first, second;
uim_splitter_pane_rects(&sp, &panel, 4.0f, &first, &second);
```

---

## 4. 未补齐项及理由（不该进 UIM 的私有 / 宿主功能）

| 项 | 现状证据 | 不进 UIM 的理由 |
|---|---|---|
| 颜色选择器面板（HSV 色域/色相环/alpha 弹层） | editor_app.cpp `ColorEdit4` 2507、3239；PVZ 无颜色编辑 | 仅一个消费方使用；拾取面板渲染密集、交互范式多样。`uim_color.h` 已覆盖值/解析/格式化/对比；待第二个工程出现颜色编辑需求再提最小拾取状态原子 |
| 停靠工作区、浮动面板、DockSpace 布局 | editor_app.cpp `DockBuilder*`；`ui_editor_workspace.c` | 产品级外壳组合（面板树、落点分区、持久化），不是可复用单件；留在编辑器 shell |
| 拖拽载荷类型（节点句柄、资产引用） | editor_app.cpp `BeginDragDropSource/Target` | `uim_drag.h` 只负责手势阈值/轴向；载荷内容是编辑器/游戏领域数据 |
| 状态栏文本槽位 | shell `STATUS_BAR`；PVZ `state.status` | 纯宿主文本条，除字符串外无可复用状态；不构成状态机原子 |
| 多列表格模型（列宽/排序/选中单元格） | 检视属性网格 | 几何已由 `uim_layout.h` grid + `uim_virtual_list.h` 覆盖；表格行列模型属应用层 |
| 菜单命令注册表、快捷键绑定表 | editor_app.cpp 菜单命令分发 | `uim_menu` 只提供通用开放/悬停/选择模型；命令 id 到动作的映射是 shell 领域 |
| 原生文件对话框、偏好设置面板 | editor_app.cpp Open/Save/Preferences | 文件选择走 `uim_file_service`；设置面板是页面组合 |
| 行内改名输入框 | editor_app.cpp 改名流程；PVZ 字段改名 | 已由 `uim_text_input.h`（就地聚焦）+ `uim_dialog.h`（如需弹窗）覆盖，无需新件 |
| PVZ 自绘滚动条绘制 | `pvzeditor.c:3458` 及各调用点 | 几何/thumb 状态已在 `uim_scroll.h`；像素绘制是宿主迁移候选，不是 UIM 缺口 |
| 向量 2/3/4 编辑器 | `DragFloat2/3/4` | 多个 `uim_numeric_field` 横向组合即可，不单列原子 |
| 单选组 | `RadioButton` 3654/3656 | `uim_dropdown.h` 单选索引 / `uim_selection.h` 可选集合覆盖 |

## 5. 工程接入与安装规则

- CMake：七个原子加入 `UIM_ATOMS` 列表，各自生成 `zcsr_uim_atom_<name>` INTERFACE 目标；`property_row`、`splitter` PUBLIC 链接 `zcsr_uim_atom_layout`；全部经 `zcsr_uim_atoms` 聚合，并进入 `UIM_INSTALL_TARGETS` 导出集。
- 安装：`install(DIRECTORY include/ ...)` 自动安装新头文件；无新增归档目标，不改变既有安装布局。
- 注册表边界：`uim_ComponentRegistry` 描述的是**文档实例组件类型**及其检视控件提示（`uim_INSPECTOR_NUMBER / SLIDER / DROPDOWN / COLOR …`）；本次新增的是编辑器侧通用状态原子，不注册为文档组件类型，也不改动既有描述符表。
- 测试接入：`tests/test_atoms.c` 每个原子一节契约；`tests/test_atoms_stress.c` 补数值钳制与分割比例不变量；`tests/test_cpp_consumer.cpp`（C++20 与 C++14 两版）实例化新头文件，守住 C ABI / `extern "C"` 边界。
- 增量性：未改动既有 33 个头的 API 与语义；既有编译型目标（`zcsr_uim_component/document/project`）源码未改。

## 6. 构建与验证记录

环境：Windows 10 (19045)，MSVC 19.51，CMake 4.3.1（VS 18 2026 生成器，x64，Release）。

| 阶段 | 日志 | 结果 |
|---|---|---|
| UIM 基线配置 | `docs/audit_logs/baseline_configure.log` | 成功 |
| UIM 基线构建 | `docs/audit_logs/baseline_build.log` | 成功 |
| UIM 基线测试 | `docs/audit_logs/baseline_ctest.log` | **13/13 通过** |
| UIM 审计配置 | `docs/audit_logs/audit_configure.log` | 成功 |
| UIM 审计构建 | `docs/audit_logs/audit_build.log`、`audit_build2.log` | 成功，无新告警/错误 |
| UIM 审计全量测试 | `docs/audit_logs/audit_ctest_final.log` | **13/13 通过**（`uim_atoms_contract`、`uim_atoms_stress_contract`、两个 C++ 契约均通过） |
| zcsr_editor 配置 | `docs/audit_logs/editor_configure.log` | 成功 |
| zcsr_editor 链接 UIM 目标构建 | `docs/audit_logs/editor_build_kernel.log`、`editor_build_tests.log` | `zcsr_editor_kernel`、`page_runtime_test`、`runtime_state_test`、`editor_project_test`、`editor_model_test` 链接通过 |
| zcsr_editor 无头测试 | `docs/audit_logs/editor_ctest_headless.log` | 8/8 通过（editor_model、page_runtime、runtime_state、editor_project 系列） |
| PVZ 配置 | `docs/audit_logs/pvz_configure.log` | 成功（显式 `-DZCSR_UIM_DIR`，见下注） |
| PVZ 链接 UIM 目标构建 | `docs/audit_logs/pvz_build_editor_app.log`、`pvz_build_host.log` | `pvz_editor_app`（编译 `uim_text_input.c`/`uim_file_service.c`）与 `pvzeditor.exe` 链接通过 |
| PVZ 无头探针 | `pvzeditor check` | `check OK plants=10 zombies=5 … levels=10` |

> 注：PVZ 的 CMake 自动探测逻辑在本机把 `ZCSR_UIM_DIR` 解析为空（候选名替换与实际目录名不一致），配置时通过命令行 `-DZCSR_UIM_DIR=C:/Users/chen/Documents/GitHub/UIM -DZCSR_EDITOR_DIR=...` 显式传入；未修改 PVZ 源码。该探测问题属消费方配置项，不在本次 UIM 补齐范围内。

构建目录均为审计专用：`UIM/build_audit_uim`、`zcsr_editor/build_audit_uim_editor`、`PVZ/build_audit_uim_pvz`；未写入 `build_main` / `build_arch` / `build_asan` 等其它代理使用的目录。
