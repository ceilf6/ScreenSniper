#!/bin/bash

echo "🔨 开始编译 ScreenSniper..."

# 国际化支持
echo "📥 更新 locales 包..."
# 修复 npm 缓存权限问题
if [ -d "$HOME/.npm" ]; then
    sudo chown -R $(id -u):$(id -g) "$HOME/.npm" 2>/dev/null || true
    rm -rf ~/.npm
fi
npm install @screensniper/locales
npm run install-locales
echo ""

# 创建构建目录
if [ ! -d "build" ]; then
    mkdir build
fi

cd build

# 运行 CMake
echo "📝 运行 CMake 配置..."
cmake ..

if [ $? -ne 0 ]; then
    echo "❌ CMake 配置失败，请检查错误信息"
    exit 1
fi

# 编译
echo "🔧 编译项目..."
cmake --build . --config Release

if [ $? -eq 0 ]; then
    echo "✅ 编译成功！"
    echo ""
    echo "运行程序："
    if [ -d "ScreenSniper.app" ]; then
        echo "  ./build/ScreenSniper.app/Contents/MacOS/ScreenSniper"
    else
        echo "  ./build/ScreenSniper"
    fi
else
    echo "❌ 编译失败，请检查错误信息"
    exit 1
fi
