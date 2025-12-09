# OpenCV 安装指南

本指南将帮助您在 Windows 上安装和配置 OpenCV，以启用人脸识别功能。

## 方法一：使用预编译版本（推荐，最简单）

### 1. 下载 OpenCV

1. 访问 [OpenCV 官网](https://opencv.org/releases/)
2. 下载最新版本的 Windows 预编译包（例如：`opencv-4.x.x-windows.exe`）
3. 运行安装程序，选择一个安装路径（例如：`D:\opencv`）

### 2. 配置项目文件

编辑 `ScreenSniper.pro` 文件：

1. **找到第 73 行**，注释掉或删除 `DEFINES += NO_OPENCV`：
   ```pro
   # DEFINES += NO_OPENCV  # 注释掉这一行
   ```

2. **找到 Windows 配置部分（约第 57-70 行）**，取消注释并修改 OpenCV 路径：
   ```pro
   win32 {
       # OpenCV 配置
       # 将下面的路径改为您实际的 OpenCV 安装路径
       OPENCV_DIR = D:/opencv/build
       
       INCLUDEPATH += $$OPENCV_DIR/include
       INCLUDEPATH += $$OPENCV_DIR/include/opencv2
       DEPENDPATH += $$OPENCV_DIR/include
       
       # 根据您的编译器选择：
       # MSVC 编译器
       LIBS += -L$$OPENCV_DIR/x64/vc16/lib
       CONFIG(debug, debug|release) {
           LIBS += -lopencv_world4xxd  # 将 4xx 替换为实际版本号（如 480）
           DEFINES += OPENCV_DEBUG
       } else {
           LIBS += -lopencv_world4xx    # 将 4xx 替换为实际版本号（如 480）
       }
       
       # 或者 MinGW 编译器
       # LIBS += -L$$OPENCV_DIR/x64/mingw/lib
       # CONFIG(debug, debug|release) {
       #     LIBS += -lopencv_world4xxd
       # } else {
       #     LIBS += -lopencv_world4xx
       # }
   }
   ```

3. **重要**：将 `OPENCV_DIR` 和版本号（`4xx`）替换为您实际的路径和版本号。

### 3. 配置环境变量（可选但推荐）

将 OpenCV 的 DLL 目录添加到系统 PATH：
- 路径示例：`D:\opencv\build\x64\vc16\bin`（MSVC）或 `D:\opencv\build\x64\mingw\bin`（MinGW）

或者将 DLL 文件复制到可执行文件目录。

### 4. 重新编译项目

```bash
# 清理之前的构建
qmake
make clean

# 重新生成 Makefile
qmake

# 编译
make
```

## 方法二：使用 vcpkg（推荐用于 Qt Creator）

### 1. 安装 vcpkg

```bash
# 克隆 vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg

# Windows 上运行
.\bootstrap-vcpkg.bat

# 集成到系统（可选）
.\vcpkg integrate install
```

### 2. 安装 OpenCV

```bash
# 安装 OpenCV（Qt 支持）
.\vcpkg install opencv[contrib,qt]:x64-windows

# 或者仅安装基础版本
.\vcpkg install opencv:x64-windows
```

### 3. 配置项目文件

在 `ScreenSniper.pro` 中添加：

```pro
win32 {
    # 移除 NO_OPENCV 定义
    # DEFINES += NO_OPENCV
    
    # vcpkg 配置（根据您的 vcpkg 安装路径修改）
    VCPKG_DIR = C:/vcpkg/installed/x64-windows
    
    INCLUDEPATH += $$VCPKG_DIR/include
    LIBS += -L$$VCPKG_DIR/lib
    
    CONFIG(debug, debug|release) {
        LIBS += -lopencv_worldd
    } else {
        LIBS += -lopencv_world
    }
}
```

## 方法三：从源码编译（高级用户）

### 1. 安装依赖

- **CMake**: 从 [CMake 官网](https://cmake.org/download/) 下载安装
- **Visual Studio**: 安装 "Desktop development with C++" 工作负载
- **Qt**: 确保已安装 Qt

### 2. 下载 OpenCV 源码

```bash
git clone https://github.com/opencv/opencv.git
cd opencv
git checkout 4.x  # 选择稳定版本
```

### 3. 使用 CMake 配置

1. 打开 CMake GUI
2. 设置源码路径：`<opencv_source_dir>`
3. 设置构建路径：`<opencv_build_dir>`
4. 点击 "Configure"，选择 Visual Studio 生成器
5. 配置选项：
   - `CMAKE_PREFIX_PATH` = 您的 Qt 安装路径
   - `WITH_QT` = ON（如果需要 Qt 支持）
6. 点击 "Generate"

### 4. 编译

在 Visual Studio 中打开生成的解决方案，编译 Debug 和 Release 版本。

### 5. 安装

```bash
cmake --install . --prefix D:/opencv/install
```

## 验证安装

编译完成后，运行程序并点击"自动打码"按钮。如果配置正确，应该能够正常使用人脸识别功能。

## 常见问题

### Q: 编译时提示找不到 OpenCV 头文件

A: 检查 `INCLUDEPATH` 是否正确设置，路径中不要使用反斜杠，使用正斜杠 `/` 或双反斜杠 `\\`。

### Q: 运行时提示找不到 DLL

A: 
1. 将 OpenCV 的 `bin` 目录添加到系统 PATH
2. 或将 DLL 文件复制到可执行文件目录
3. 确保使用正确版本的 DLL（Debug/Release）

### Q: MinGW 和 MSVC 库不兼容

A: 如果使用 MinGW 编译器，必须使用 MinGW 编译的 OpenCV 库。预编译版本通常只提供 MSVC 版本，需要自己编译 MinGW 版本。

### Q: 如何检查 OpenCV 版本

A: 查看 `opencv\build\include\opencv2\opencv_version.hpp` 文件，或查看库文件名中的版本号。

## 模型文件

确保 `models` 目录下有以下文件：
- `opencv_face_detector_uint8.pb`
- `opencv_face_detector.pbtxt.txt` 或 `opencv_face_detector.pbtxt`

这些文件可以从 [OpenCV 官方仓库](https://github.com/opencv/opencv_extra/tree/master/testdata/dnn) 下载。

## 参考链接

- [OpenCV 官方文档](https://docs.opencv.org/)
- [OpenCV Windows 安装教程](https://docs.opencv.org/4.x/d3/d52/tutorial_windows_install.html)
- [vcpkg 文档](https://vcpkg.io/)

