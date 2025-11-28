#!/bin/bash

echo "🔨 开始编译 ScreenSniper..."

# 创建构建目录
if [ ! -d "build" ]; then
    mkdir build
fi

cd build

# 运行 qmake
echo "📝 运行 qmake..."
qmake ../ScreenSniper.pro

# 编译
echo "🔧 编译项目..."
make

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
