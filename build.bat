@echo off
echo 🔨 开始编译 ScreenSniper...
echo.

REM 检查 Node.js 是否安装
where node >nul 2>nul
if %errorlevel% neq 0 (
    echo ❌ 未找到 Node.js，无法安装翻译文件
    echo 💡 请先安装 Node.js: https://nodejs.org/
    pause
    exit /b 1
)

REM 国际化支持
echo 📥 更新 locales 包...
REM 修复 npm 缓存权限问题（Windows通过清理缓存解决）
if exist "%USERPROFILE%\.npm" (
    echo 🧹 清理 npm 缓存...
    call npm cache clean --force
)
call npm install @screensniper/locales
call npm run install-locales
echo.

REM 创建构建目录
if not exist build (
    mkdir build
)

cd build

REM 运行 CMake
echo 📝 运行 CMake 配置...
cmake ..
if %errorlevel% neq 0 (
    echo ❌ CMake 配置失败，请检查环境配置
    cd ..
    pause
    exit /b 1
)

REM 编译项目
echo 🔧 编译项目...
cmake --build . --config Release

if %errorlevel% equ 0 (
    echo.
    echo ✅ 编译成功！
    echo.
    echo 运行程序：
    if exist Release\ScreenSniper.exe (
        echo   .\build\Release\ScreenSniper.exe
    ) else if exist Debug\ScreenSniper.exe (
        echo   .\build\Debug\ScreenSniper.exe
    ) else (
        echo   .\build\ScreenSniper.exe
    )
) else (
    echo ❌ 编译失败，请检查错误信息
    cd ..
    pause
    exit /b 1
)

cd ..
pause
