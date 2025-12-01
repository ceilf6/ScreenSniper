# ScreenSniper - 屏幕截图工具

一个基于 Qt 和 C++ 开发的跨平台截图工具，提供简洁易用的截图功能。

## 功能特性

- ✅ **全屏截图** - 快速截取整个屏幕
- ✅ **区域截图** - 自由选择截图区域
- ✅ **窗口截图** - 自动吸附并截取指定窗口
- ✅ **快速保存** - 自动保存到图片文件夹
- ✅ **剪贴板支持** - 截图自动复制到剪贴板
- ✅ **系统托盘** - 最小化到托盘，快速访问
- ✅ **图像编辑** - 丰富的标注工具：
  - 矩形、椭圆、箭头
  - 自由画笔
  - 文字输入
  - 马赛克 / 高斯模糊
  - 隐水印
- ✅ **OCR 文字识别** - 支持中英文识别（macOS 原生 / Tesseract）
- ✅ **Pin 图** - 将截图钉在桌面上

## 系统要求

- **操作系统**: Windows 10+, macOS 10.15+, Linux
- **Qt版本**: Qt 5.15+ 或 Qt 6.x
- **编译器**: 
  - Windows: MSVC 2019+ 或 MinGW
  - macOS: Xcode 12+
  - Linux: GCC 8+ 或 Clang 10+

## 编译安装

### 1. 安装依赖

**macOS (使用 Homebrew):**
```bash
brew install qt
brew install opencv
brew install tesseract # 可选，用于增强 OCR
```

**Windows:**
1. 从 [Qt 官网](https://www.qt.io/download) 下载并安装 Qt。
2. (可选) 下载并安装 [Tesseract OCR](https://github.com/UB-Mannheim/tesseract/wiki)。
3. (可选) 下载并配置 OpenCV。

### 2. 配置本地环境 (可选)

为了适应不同开发者的环境差异（如 Tesseract/OpenCV 安装路径不同），本项目支持本地配置文件 `local_config.pri`。

1. 复制示例文件：
   ```bash
   cp local_config.pri.example local_config.pri
   ```
2. 编辑 `local_config.pri`，根据您的机器环境取消注释并修改相关路径。
   - **macOS**: 如果使用 Homebrew 安装了 Tesseract/OpenCV，通常只需取消注释对应块。
   - **Windows**: 需要修改为您实际的安装路径。

### 3. 安装翻译文件

项目使用 npm 管理翻译文件。在首次构建前，需要安装翻译文件：

```bash
# 安装依赖（包括翻译文件）
npm install

# 将翻译文件复制到 locales 目录
npm run install-locales
```

**注意**：翻译文件已经打包到 Qt 资源文件中，会被编译到可执行文件。如果修改了翻译文件，需要重新编译才能生效。

### 4. 编译步骤

**命令行编译:**
```bash
# 1. 克隆项目
git clone https://github.com/ceilf6/ScreenSniper.git
cd ScreenSniper

# 2. 安装翻译文件
npm install
npm run install-locales

# 3. 运行构建脚本 (macOS/Linux)
./build.sh

# 或者手动编译
mkdir build && cd build
qmake ../ScreenSniper.pro
make
```

**使用 Qt Creator:**
1. 打开 Qt Creator。
2. 选择 "文件" -> "打开文件或项目"，选择 `ScreenSniper.pro`。
3. **重要**：在编译前先运行 `npm install && npm run install-locales` 安装翻译文件。
4. 配置项目构建套件。
5. 点击 "运行" 按钮。

### 常见问题

**Q: 编译时提示 "无法打开语言文件：locales/zh.json"**

A: 这是因为翻译文件未安装。解决方法：
```bash
npm install
npm run install-locales
```
然后重新编译项目。翻译文件会被打包到 Qt 资源中。

**Q: Windows 上提示 "QSystemTrayIcon::setVisible: No Icon set"**

A: 这是因为系统托盘图标未正确设置。确保项目中有托盘图标资源文件，或在代码中设置默认图标。

## OCR 功能说明

本项目支持两种 OCR 引擎：

1. **macOS 原生 Vision (仅 macOS)**:
   - 无需额外安装库，开箱即用。
   - 支持中英文识别。

2. **Tesseract OCR (跨平台)**:
   - 需要安装 Tesseract 库和语言包 (`chi_sim`, `eng`)。
   - 在 `local_config.pri` 中启用 `DEFINES += USE_TESSERACT` 并配置路径。
   - 识别准确率高，支持更多语言。

## 使用说明

### 基本操作

1. **启动程序**
   - 运行 ScreenSniper，主窗口会显示。
   - 程序会自动在系统托盘显示图标。

2. **截图流程**
   - 点击 "截取区域" 或使用快捷键。
   - 拖动鼠标选择区域（支持自动吸附窗口）。
   - 松开鼠标后显示工具栏，可进行标注、OCR、Pin 图等操作。
   - 双击截图区域完成截图（复制并保存）。

3. **工具栏功能**
   - 🖊️ **画笔/形状**: 绘制矩形、箭头、文字等。
   - 💧 **模糊/马赛克**: 对敏感信息进行遮挡。
   - 👁️ **OCR**: 识别选区内的文字。
   - 📌 **Pin**: 将截图贴在屏幕上。
   - 💾 **保存/复制**: 保存文件或复制到剪贴板。

### 快捷键

| 功能 | 快捷键 |
|-----|--------|
| 全屏截图 | `Ctrl+Shift+F` |
| 区域截图 | `Ctrl+Shift+A` |
| 退出截图 | `ESC` |
| 完成截图 | `Enter` 或 双击 |

## 许可证

MIT License
| 区域截图 | `Ctrl+Shift+A` |
| 窗口截图 | `Ctrl+Shift+W` |
| 取消截图 | `ESC` |
| 确认截图 | `Enter` |

## 项目结构

```
ScreenSniper/
├── main.cpp                  # 程序入口
├── mainwindow.h             # 主窗口头文件
├── mainwindow.cpp           # 主窗口实现
├── mainwindow.ui            # 主窗口UI设计
├── screenshotwidget.h       # 截图窗口头文件
├── screenshotwidget.cpp     # 截图窗口实现
├── resources.qrc            # 资源文件
├── ScreenSniper.pro         # Qt项目文件
└── README.md               # 项目说明
```

## 技术栈

- **语言**: C++17
- **GUI框架**: Qt 5/6
- **构建工具**: qmake
- **版本控制**: Git

## 贡献指南

欢迎贡献代码！请遵循以下步骤：

1. Fork 本仓库
2. 创建特性分支 (`git checkout -b feature/AmazingFeature`)
3. 提交更改 (`git commit -m 'Add some AmazingFeature'`)
4. 推送到分支 (`git push origin feature/AmazingFeature`)
5. 开启 Pull Request

## 许可证

本项目采用 MIT 许可证。详见 [LICENSE](LICENSE) 文件。

## 致谢

- Qt Framework - 强大的跨平台GUI框架
- 所有贡献者

## 联系方式

- **项目地址**: https://github.com/ceilf6/ScreenSniper
- **问题反馈**: https://github.com/ceilf6/ScreenSniper/issues

---

**提示**: 如果在编译过程中遇到问题，请检查 Qt 版本和编译器配置。
