#CMakeLists.txt
# 指定最低 CMake 版本（Qt6 通常需要 3.16+）
cmake_minimum_required(VERSION 3.16)

# 项目名称与版本
project(MediaPlayer3 VERSION 1.0.0 LANGUAGES CXX)

# 设置 C++ 标准为 C++17（与原 .pro 中的 CONFIG += c++17 对应）
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# 启用 Qt 的自动工具（moc、uic、rcc）
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTORCC ON)

# 查找 Qt 6 所需的组件
# 若需使用 Qt 5，请将 Qt6 替换为 Qt5，并去掉版本要求或指定相应版本
find_package(Qt6 REQUIRED COMPONENTS Core Widgets Multimedia MultimediaWidgets)

# 源文件列表
set(SOURCES
    ${CMAKE_CURRENT_SOURCE_DIR}/src/main.cpp
    ${CMAKE_CURRENT_SOURCE_DIR}/src/videoplayer.cpp
)

# 头文件列表
set(HEADERS
    ${CMAKE_CURRENT_SOURCE_DIR}/src/videoplayer.h
)


# 翻译文件（原 .pro 中 TRANSLATIONS 为空，故此处无实际文件）
# 若存在 .ts 文件，可取消以下注释并添加文件列表
# set(TS_FILES
#     translations/MediaPlayer3_zh_CN.ts
# )
# qt6_add_translation(QM_FILES ${TS_FILES})
# 若需要将翻译文件嵌入资源，可使用 qt6_add_resource

set(FORMS videoplayer.ui)

# 定义可执行文件
add_executable(MediaPlayer3 ${SOURCES} ${HEADERS} ${FORMS}
    resources.qrc)

# 链接 Qt 库
target_link_libraries(MediaPlayer3
    Qt6::Widgets
    Qt6::Multimedia
    Qt6::MultimediaWidgets
)

# 安装规则（对应原 .pro 中的部署配置）
# 默认安装到 CMAKE_INSTALL_PREFIX 下的 bin 目录
install(TARGETS MediaPlayer3
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
    BUNDLE DESTINATION ${CMAKE_INSTALL_BINDIR}
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
)

# 可选：若需按平台定制安装路径，可参考以下条件判断
# if(QNX)
#     install(TARGETS MediaPlayer3 RUNTIME DESTINATION /tmp/bin)
# elseif(UNIX AND NOT ANDROID)
#     install(TARGETS MediaPlayer3 RUNTIME DESTINATION /opt/bin)
# endif()
