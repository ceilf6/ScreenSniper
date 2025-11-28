# ScreenSniper - 屏幕截图工具

一个基于 Qt 和 C++ 开发的跨平台截图工具，提供简洁易用的截图功能。

## 功能特性

- ✅ **全屏截图** - 快速截取整个屏幕
- ✅ **区域截图** - 自由选择截图区域
- 🚧 **窗口截图** - 截取指定窗口（开发中）
- ✅ **快速保存** - 自动保存到图片文件夹
- ✅ **剪贴板支持** - 截图自动复制到剪贴板
- ✅ **系统托盘** - 最小化到托盘，快速访问
- 🚧 **图像编辑** - 添加矩形、箭头、文字标注（开发中）
- 🚧 **快捷键支持** - 全局快捷键（开发中）

## 系统要求

- **操作系统**: Windows 10+, macOS 10.15+, Linux
- **Qt版本**: Qt 5.15+ 或 Qt 6.x
- **编译器**: 
  - Windows: MSVC 2019+ 或 MinGW
  - macOS: Xcode 12+
  - Linux: GCC 8+ 或 Clang 10+

## 编译安装

### 前置条件

确保已安装 Qt 开发环境：

**macOS (使用 Homebrew):**
```bash
brew install qt
```

**Ubuntu/Debian:**
```bash
sudo apt-get install qt5-default qtbase5-dev qttools5-dev-tools
```

**Windows:**
从 [Qt 官网](https://www.qt.io/download) 下载并安装 Qt。

### 编译步骤

1. **克隆项目**
```bash
git clone https://github.com/ceilf6/ScreenSniper.git
cd ScreenSniper
```

2. **使用 qmake 编译**
```bash
qmake ScreenSniper.pro
make
```

或者使用 Qt Creator:
- 打开 Qt Creator
- 选择 "文件" -> "打开文件或项目"
- 选择 `ScreenSniper.pro`
- 点击 "配置项目"
- 点击 "运行" 按钮

3. **运行程序**
```bash
./ScreenSniper  # Linux/macOS
ScreenSniper.exe  # Windows
```

## 使用说明

### 基本操作

1. **启动程序**
   - 运行 ScreenSniper，主窗口会显示
   - 程序会自动在系统托盘显示图标

2. **全屏截图**
   - 点击 "截取全屏" 按钮
   - 或使用快捷键 `Ctrl+Shift+F`
   - 截图会自动保存到剪贴板和图片文件夹

3. **区域截图**
   - 点击 "截取区域" 按钮
   - 或使用快捷键 `Ctrl+Shift+A`
   - 鼠标拖动选择截图区域
   - 松开鼠标后会显示工具栏
   - 点击 "保存" 或 "复制" 完成截图
   - 按 `ESC` 取消截图

4. **系统托盘**
   - 双击托盘图标显示主窗口
   - 右键托盘图标显示快捷菜单

### 快捷键

| 功能 | 快捷键 |
|-----|--------|
| 全屏截图 | `Ctrl+Shift+F` |
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

## 开发计划

- [ ] 实现窗口截图功能
- [ ] 添加图像编辑工具（矩形、箭头、文字、画笔）
- [ ] 实现全局快捷键
- [ ] 添加设置界面（快捷键配置、保存路径等）
- [ ] 支持多显示器
- [ ] 添加延迟截图功能
- [ ] 支持滚动截图
- [ ] 添加 OCR 文字识别
- [ ] 云同步功能

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
