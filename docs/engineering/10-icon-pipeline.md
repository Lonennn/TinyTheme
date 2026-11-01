# 图标生成

> 状态：v0.1 已实现 ｜ 更新：2026-11-01

`res/*.svg` → `res/*.ico` → exe 资源段。设计规范见 [托盘图标设计](../product/06-icon-design.md)。脚本的参数与实现见 `tools/`，本文只讲流程与取舍。

## 1. 采用方式

SVG 只作源文件；发布物用预生成的多尺寸 ICO；**SVG 与 ICO 一起入库**，CI 只调 `rc.exe`。

```
res/icon-{light,dark}.svg
        │  tools/render-icons.ps1（Edge / Chrome 无头）
        ▼
res/build/icon-{light,dark}-{16,20,24,32}.png   （不入库）
        │  tools/pack-icon.ps1
        ▼
res/icon-{light,dark}.ico                        （入库）
        │  rc.exe
        ▼
TinyTheme.exe 的资源段
```

关键点：**生成在本地做，产物入库**。CI 与用户都不需要图像工具，构建保持可复现。

## 2. 为什么不在运行时用 SVG

- `Shell_NotifyIcon` 只接受 `HICON`，Win32 没有 SVG → `HICON` 的接口。
- `rc.exe` 只接受 ICO / BMP，`.rc` 里引用 SVG 会直接编译失败。
- Direct2D 能解析 SVG，但要为一个 16 px 图标链接 `d2d1` / `dwrite` 并运行时渲染，与"体积小、启动快"冲突。

## 3. 工具选择

| 步骤 | 工具 | 理由 |
|---|---|---|
| 渲染 PNG | `tools/render-icons.ps1`，调系统自带 Edge / Chrome 无头模式 | Windows 10/11 默认有 Edge，不引入 Inkscape |
| 打包 ICO | `tools/pack-icon.ps1`，只用 `System.Drawing` | 无第三方模块 |
| 编译资源 | `rc.exe` | 唯一进入 CI 与发布物的步骤 |

不用 ImageMagick：它的 SVG 渲染依赖编译时的 delegate，不同机器结果可能不一致，与可复现构建冲突。

生成工具不进入发布物；零依赖约束只针对发布物与构建产物，开发期工具不受限制。

## 4. 生成流程

```powershell
pwsh -File tools/render-icons.ps1                 # 两套主题 × 四档，共 8 张
pwsh -File tools/render-icons.ps1 -Themes dark    # 只渲染一套
pwsh -File tools/pack-icon.ps1 -Name icon-light
pwsh -File tools/pack-icon.ps1 -Name icon-dark
```

约定：

- **每档单独渲染，不做缩放**；脚本会校验输出 PNG 的像素尺寸，不符直接报错。
- 透明背景由 `--default-background-color=00000000` 保证，SVG 里不要画底色矩形。
- 中间 PNG 放 `res/build/`，不入库。
- `pack-icon.ps1` 尺寸缺失时报错，不会静默生成残缺 ICO。

## 5. ICO 的四个决定

字段级细节看 `tools/pack-icon.ps1`，这里只留会影响兼容性的选择：

- **条目按 16 → 32 升序**：只取第一条的旧接口会拿到最小尺寸。
- **每个条目用 BMP（DIB）承载，不内嵌 PNG**：小尺寸下 PNG 条目的兼容性更差，而托盘最大只用到 32 px。
- **DIB 的 `biHeight` 写图像高度的 2 倍**：颜色位图 + AND 掩码。
- **32 位色也要写 AND 掩码**（1 表示透明）：个别环境缺了会出现黑边。

## 6. 接入构建

`res/app.rc.in` 里通过 `IDI_LIGHT` / `IDI_DARK` 引用 ICO，运行时用 `LoadImageW` 加载。

两条不能改的约束：

- **用 `LoadImageW` 并显式传 `cx` / `cy`**（取 `GetSystemMetrics(SM_CXSMICON)`）。`LoadIcon` 不按尺寸选择、只会缩放，16 px 下明显发虚。
- 四档必须齐全：100% / 125% / 150% / 200% 分别取 16 / 20 / 24 / 32，缺档就会拿邻近尺寸缩放。

资源缺失时回退到系统 `IDI_APPLICATION`，不显示空白。

## 7. 校验清单

- [ ] ICO 含 4 个条目，尺寸 16 / 20 / 24 / 32
- [ ] `LoadImageW` 在 16 与 32 两种 `SM_CXSMICON` 下都取到对应档位
- [ ] 透明背景：深色任务栏上不出现白方块
- [ ] 16 px 下目视通过 [托盘图标设计 §8](../product/06-icon-design.md#8-可辨识度验收)
- [ ] 改缩放后图标自动重载（`WM_DPICHANGED`）

## 8. 版本关系

v0.1 交付两套图标资产与 `.rc` 接入，启动与自身切换后按系统主题选图标；系统主题被外部改动后的刷新（F-11）在 v0.2。
