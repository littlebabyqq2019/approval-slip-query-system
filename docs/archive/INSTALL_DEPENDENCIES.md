# CrossNetShare 编译前准备

## ⚠️ 检测到缺少必要的编译环境

您的系统目前缺少以下组件：
- Qt5 框架
- C++ 编译器（MSVC）

## 🔧 安装步骤

### 方案1：完整安装（推荐用于开发）

#### 1. 安装 Visual Studio 2019/2022

1. 访问：https://visualstudio.microsoft.com/downloads/
2. 下载 "Visual Studio 2019 Community" 或 "Visual Studio 2022 Community"
3. 安装时选择工作负载：
   - ✅ "使用C++的桌面开发"
   - ✅ "CMake工具"
4. 安装大小：约 8GB

#### 2. 安装 Qt5

**方法A：在线安装器（推荐）**
1. 访问：https://www.qt.io/download-qt-installer
2. 下载 "Qt Online Installer"
3. 注册或登录Qt账号（免费）
4. 安装时选择：
   - ✅ Qt 5.15.2
   - ✅ MSVC 2019 64-bit
   - ✅ Qt Creator（可选）
5. 安装路径：建议使用默认路径（C:\Qt）
6. 安装大小：约 2-3GB

**方法B：vcpkg安装（更快）**
```powershell
# 安装vcpkg
git clone https://github.com/Microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat

# 安装Qt5
.\vcpkg install qt5-base:x64-windows
.\vcpkg install qt5-widgets:x64-windows
.\vcpkg install qt5-network:x64-windows

# 集成到系统
.\vcpkg integrate install
```

#### 3. 编译项目

```powershell
# 返回项目目录
cd e:\dev\crossnet-share

# 创建构建目录
mkdir build
cd build

# 配置项目（替换为您的Qt路径）
cmake .. -DCMAKE_PREFIX_PATH="C:\Qt\5.15.2\msvc2019_64"

# 编译
cmake --build . --config Release
```

### 方案2：最小化安装（仅用于编译）

#### 1. 安装 Build Tools for Visual Studio

1. 访问：https://visualstudio.microsoft.com/downloads/
2. 找到 "Tools for Visual Studio"
3. 下载 "Build Tools for Visual Studio 2019"
4. 安装时选择：
   - ✅ "C++ 生成工具"
5. 安装大小：约 3GB

#### 2. 使用预编译的Qt二进制包

1. 访问：https://download.qt.io/archive/qt/5.15/5.15.2/
2. 下载：qt-opensource-windows-x86-5.15.2.exe
3. 安装到 C:\Qt\5.15.2

#### 3. 设置环境变量并编译

```powershell
# 添加MSVC到PATH（根据实际安装路径调整）
$env:Path += ";C:\Program Files (x86)\Microsoft Visual Studio\2019\BuildTools\VC\Tools\MSVC\14.29.30133\bin\Hostx64\x64"

# 编译项目
cd e:\dev\crossnet-share
mkdir build
cd build
cmake .. -G "NMake Makefiles" -DCMAKE_PREFIX_PATH="C:\Qt\5.15.2\msvc2019_64"
nmake
```

### 方案3：使用Qt Creator（最简单）

#### 1. 只安装Qt（包含Qt Creator）

1. 下载Qt在线安装器
2. 安装Qt 5.15.2 + MSVC 2019 + Qt Creator
3. Qt Creator会自动检测MSVC编译器

#### 2. 在Qt Creator中打开项目

1. 启动Qt Creator
2. 文件 → 打开文件或项目
3. 选择 `e:\dev\crossnet-share\CMakeLists.txt`
4. 选择Kit：Desktop Qt 5.15.2 MSVC2019 64bit
5. 点击"配置项目"
6. 点击左下角的"锤子"图标编译

## 📦 预编译二进制包（临时方案）

如果您只是想快速测试，不想安装完整的开发环境，可以考虑：

### 选项1：使用Docker容器编译

```powershell
# 拉取Qt开发镜像
docker pull darkmattercoder/qt-build:5.15-msvc2019

# 编译项目
docker run --rm -v e:\dev\crossnet-share:/project darkmattercoder/qt-build:5.15-msvc2019 `
  /bin/bash -c "cd /project && mkdir build && cd build && cmake .. && cmake --build ."
```

### 选项2：使用GitHub Actions云编译

我可以为您创建GitHub Actions配置，在云端自动编译并下载exe文件。

## ⏱️ 预计安装时间

| 方案 | 下载时间 | 安装时间 | 总计 |
|------|---------|---------|------|
| 方案1（完整） | 30-60分钟 | 20-30分钟 | 50-90分钟 |
| 方案2（最小） | 20-30分钟 | 10-20分钟 | 30-50分钟 |
| 方案3（Qt Creator） | 20-30分钟 | 10-15分钟 | 30-45分钟 |

## 🎯 推荐方案

**如果您是开发者**: 选择 **方案1**（完整安装）
- 可以进行后续开发和调试
- 最完整的工具链

**如果只想编译一次**: 选择 **方案3**（Qt Creator）
- 最简单，一站式解决方案
- GUI界面，易于操作

**如果网络慢**: 选择 **方案2**（最小化）
- 下载量最小
- 仅包含必要组件

## 📝 安装后验证

安装完成后，验证环境：

```powershell
# 检查CMake
cmake --version

# 检查Qt（假设安装在C:\Qt\5.15.2）
C:\Qt\5.15.2\msvc2019_64\bin\qmake.exe --version

# 检查MSVC
"C:\Program Files (x86)\Microsoft Visual Studio\2019\Community\VC\Auxiliary\Build\vcvars64.bat"
cl.exe
```

## 🆘 需要帮助？

请告诉我您选择哪个方案，我可以提供详细的步骤指导。

或者，如果您希望我帮您：
1. **创建Docker编译配置**
2. **创建GitHub Actions云编译**
3. **提供详细的安装步骤截图指南**

请告诉我您的选择！

---

**注意**：编译C++项目需要完整的开发环境，第一次安装可能需要较长时间，但这是一次性的工作。
