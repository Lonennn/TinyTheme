# 图标生成

> 状态：草案 v0.1 ｜ 更新：2026-11-01

把 `res/*.svg` 变成 `res/*.ico` 并编译进 exe。设计规范见 [托盘图标设计](../product/06-icon-design.md)。

## 1. 采用方式

SVG 只作源文件；发布物使用预生成的多尺寸 ICO；SVG 与 ICO 一起入库，CI 只调用 `rc.exe`。

```
res/icon-{light,dark}.svg
        │  tools/render-icons.ps1（Edge / Chrome 无头模式）
        ▼
res/build/icon-{light,dark}-{16,20,24,32}.png
        │  tools/pack-icon.ps1
        ▼
res/icon-{light,dark}.ico（入库）
        │  rc.exe
        ▼
TinyTheme.exe 的资源段
```

关键点：**生成步骤在本地做，产物入库**。这样 CI 和最终用户都不需要任何图像工具，构建保持可复现。

## 2. 为什么不在运行时用 SVG

- `Shell_NotifyIcon` 只接受 `HICON`；Win32 没有 SVG → `HICON` 的接口。
- `rc.exe` 只接受 ICO / BMP，`.rc` 里引用 SVG 会编译失败。
- Direct2D 能解析 SVG（Windows 10 1703 起提供 `ID2D1SvgDocument`），但要在 exe 里链接 `d2d1` / `dwrite` 并在运行时渲染，为一个 16 px 图标不值得，与"体积小、启动快"冲突。
- 预生成之后，运行时只剩 `LoadImage` + `Shell_NotifyIcon`。

## 3. 工具

| 步骤 | 工具 | 进入 CI | 进入发布物 |
|---|---|---|---|
| 渲染 PNG | `tools/render-icons.ps1`（Edge / Chrome 无头） | 否 | 否 |
| 打包 ICO | `tools/pack-icon.ps1` | 否 | 否 |
| 编译资源 | `rc.exe` | 是 | 是 |

两点说明：

- 工具只在**生成图标**时使用，产物入库。这不与 ADR-0002 的零第三方依赖冲突——该约束针对发布物与构建产物，测试与生成工具不在此列。
- 渲染用系统自带的 Edge / Chrome 无头模式，不装 Inkscape：Windows 10/11 默认就有 Edge，脚本可直接调用，少一个前置条件。
- 不用 ImageMagick 一行命令：它的 SVG 渲染依赖编译时的 delegate（MSVG / RSVG），不同机器结果可能不一致，与可复现构建冲突。

## 4. 渲染

```powershell
pwsh -File tools/render-icons.ps1                # 两套主题 × 四档，共 8 张
pwsh -File tools/render-icons.ps1 -Themes dark   # 只渲染一套
```

脚本的做法：为每档生成一个 1:1 的 HTML 包装页（图片按目标像素尺寸摆放、页面透明），再用浏览器无头模式按同样大小的窗口截图。

注意：

- 每档单独渲染，不做缩放；脚本会校验输出 PNG 的像素尺寸，不符直接报错。
- 透明背景由 `--default-background-color=00000000` 保证；SVG 里不要画底色矩形。
- 中间 PNG 放 `res/build/`，不入库。

## 5. 打包

```powershell
pwsh -File tools/pack-icon.ps1 -Name icon-light
pwsh -File tools/pack-icon.ps1 -Name icon-dark
```

脚本读取 `res/build/<Name>-<size>.png`，输出 `res/<Name>.ico`，尺寸缺失时直接报错，不会静默生成残缺图标。

## 6. ICO 格式要点

| 字段 | 取值 | 说明 |
|---|---|---|
| `ICONDIR.reserved` | 0 | |
| `ICONDIR.type` | 1 | 图标 |
| `ICONDIR.count` | 4 | 四种尺寸 |
| 条目 `width` / `height` | 实际像素 | 只有 256 才写 0；本方案最大 32 |
| 条目 `colorCount` | 0 | 32 位色 |
| 条目 `planes` | 1 | |
| 条目 `bitCount` | 32 | BGRA |
| 条目 `bytesInRes` | 图像数据长度 | |
| 条目 `imageOffset` | 相对文件头 | |

- 条目顺序 16 → 32 升序：`LoadIcon` 只取第一条，小尺寸在前更安全。
- 每个条目用 BMP（DIB）承载，不内嵌 PNG：小尺寸下 PNG 条目的兼容性不如 BMP，托盘最多用到 32 px，没必要省这点体积。
- DIB 的 `biHeight` 是图像高度的 2 倍（颜色位图 + AND 掩码）。
- AND 掩码里 1 表示透明；32 位色仍需写掩码，避免个别环境出现黑边。

## 7. 接入构建

`res/app.rc`：

```
IDI_LIGHT ICON "icon-light.ico"
IDI_DARK  ICON "icon-dark.ico"
```

加载：

```cpp
const int cx = GetSystemMetrics(SM_CXSMICON);
HICON icon = static_cast<HICON>(LoadImageW(
    instance, MAKEINTRESOURCEW(id), IMAGE_ICON, cx, cx, LR_DEFAULTCOLOR));
```

- 用 `LoadImageW` 并显式传 `cx` / `cy`：它会从四档里挑最接近的。`LoadIcon` 不按尺寸选择，只会缩放，16 px 下会明显发虚。
- 高 DPI 下 `SM_CXSMICON` 可能是 24 或 32，这也是四档必须齐全的原因。
- 返回 `nullptr` 时按 [托盘图标设计 §7](../product/06-icon-design.md#7-状态映射) 回退到 `IDI_APPLICATION`。

## 8. 校验清单

- [ ] ICO 含 4 个条目，尺寸为 16 / 20 / 24 / 32
- [ ] `LoadImageW` 在 16 与 32 两种 `SM_CXSMICON` 下都取到对应档位
- [ ] 透明背景：深色任务栏上不出现白方块
- [ ] 16 px 下目视通过 [托盘图标设计 §8](../product/06-icon-design.md#8-可辨识度验收) 的全部检查项
- [ ] exe 体积变化记录到 [构建 §9](06-build.md) 的验证数据

## 9. 版本关系

| 版本 | 内容 |
|---|---|
| v0.1 | 两套图标资产、`.rc` 接入、启动时按系统主题选定；运行中不刷新 |
| v0.2 | F-11：系统主题变化后刷新图标 |

## 10. 脚本入口与依赖

两个脚本都只用 PowerShell 自带的 `System.Drawing`，不依赖第三方模块。

| 脚本 | 作用 | 依赖 |
|---|---|---|
| `tools/render-icons.ps1` | 调 Edge / Chrome 无头模式，把 SVG 渲染成四档 PNG | 系统自带浏览器 |
| `tools/pack-icon.ps1` | 把四档 PNG 打包成 ICO | 无 |

`render-icons.ps1` 参数：

| 参数 | 默认值 | 说明 |
|---|---|---|
| `-ResDir` | `res` | SVG 所在目录 |
| `-BuildDir` | `res/build` | PNG 输出目录 |
| `-Sizes` | `16, 20, 24, 32` | 渲染尺寸 |
| `-Themes` | `light, dark` | 渲染哪些主题 |
| `-Browser` | 自动查找 | 指定 Edge / Chrome 可执行文件 |

`pack-icon.ps1` 参数：

| 参数 | 默认值 | 说明 |
|---|---|---|
| `-Name` | 必填 | 如 `icon-light`，对应 `icon-light.svg` |
| `-BuildDir` | `res/build` | PNG 所在目录 |
| `-OutDir` | `res` | ICO 输出目录 |
| `-Sizes` | `16, 20, 24, 32` | 条目尺寸与顺序 |

已在 PowerShell 7 上跑通：生成的四条目 ICO 可被 `System.Drawing.Icon` 按 16 / 32 正确读取，角点 alpha = 0。
