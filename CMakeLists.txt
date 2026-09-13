cmake_minimum_required(VERSION 3.15)
project(CrossNetShare VERSION 2.2.5 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_AUTOMOC ON)

# 生成版本号头文件
configure_file(
    "${CMAKE_SOURCE_DIR}/common/version.h.in"
    "${CMAKE_BINARY_DIR}/common/version.h"
)
include_directories("${CMAKE_BINARY_DIR}")

# 设置输出目录
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/bin)
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY ${CMAKE_BINARY_DIR}/lib)

# 查找依赖库
find_package(Qt5 COMPONENTS Core Widgets Network Concurrent REQUIRED)

# 第三方库 - nlohmann/json (header-only)
include(FetchContent)
FetchContent_Declare(
    json
    URL https://github.com/nlohmann/json/releases/download/v3.11.2/json.tar.xz
)
FetchContent_MakeAvailable(json)

# 公共库
add_library(common STATIC
    common/protocol.cpp
    common/protocol.h
    common/file_utils.cpp
    common/file_utils.h
    common/message.h
    common/autostart.cpp
    common/autostart.h
)

target_link_libraries(common
    PUBLIC nlohmann_json::nlohmann_json
    PUBLIC Qt5::Core
)

target_include_directories(common PUBLIC ${CMAKE_CURRENT_SOURCE_DIR})

# 服务器
if(WIN32)
    add_executable(CrossNetShareServer WIN32
        server/main.cpp
        server/server.cpp
        server/server.h
        server/file_indexer.cpp
        server/file_indexer.h
        server/client_handler.cpp
        server/client_handler.h
        server/file_watcher.cpp
        server/file_watcher.h
        server/web_server.cpp
        server/web_server.h
        server/auth_manager.cpp
        server/auth_manager.h
        server/user_manager.cpp
        server/user_manager.h
        server/audit_logger.cpp
        server/audit_logger.h
        server/document_converter.cpp
        server/document_converter.h
        server/watermark_service.cpp
        server/watermark_service.h
        server/db_manager.cpp
        server/db_manager.h
        server/ui/main_window.cpp
        server/ui/main_window.h
        server/ui/settings_dialog.cpp
        server/ui/settings_dialog.h
        server/ui/user_management_dialog.cpp
        server/ui/user_management_dialog.h
        server/ui/audit_log_dialog.cpp
        server/ui/audit_log_dialog.h
        server/ui/watermark_settings_dialog.cpp
        server/ui/watermark_settings_dialog.h
        server/server.rc
    )
else()
    add_executable(CrossNetShareServer
        server/main.cpp
        server/server.cpp
        server/server.h
        server/file_indexer.cpp
        server/file_indexer.h
        server/client_handler.cpp
        server/client_handler.h
        server/file_watcher.cpp
        server/file_watcher.h
        server/web_server.cpp
        server/web_server.h
        server/auth_manager.cpp
        server/auth_manager.h
        server/user_manager.cpp
        server/user_manager.h
        server/audit_logger.cpp
        server/audit_logger.h
        server/document_converter.cpp
        server/document_converter.h
        server/watermark_service.cpp
        server/watermark_service.h
        server/db_manager.cpp
        server/db_manager.h
        server/ui/main_window.cpp
        server/ui/main_window.h
        server/ui/settings_dialog.cpp
        server/ui/settings_dialog.h
    )
endif()

target_link_libraries(CrossNetShareServer
    PRIVATE common
    PRIVATE Qt5::Core
    PRIVATE Qt5::Widgets
    PRIVATE Qt5::Network
)

# 客户端
if(WIN32)
    add_executable(CrossNetShareClient WIN32
        client/main.cpp
        client/client.cpp
        client/client.h
        client/file_manager.cpp
        client/file_manager.h
        client/file_indexer.cpp
        client/file_indexer.h
        client/ui/main_window.cpp
        client/ui/main_window.h
        client/ui/index_settings_dialog.cpp
        client/ui/index_settings_dialog.h
        client/client.rc
    )
else()
    add_executable(CrossNetShareClient
        client/main.cpp
        client/client.cpp
        client/client.h
        client/file_manager.cpp
        client/file_manager.h
        client/file_indexer.cpp
        client/file_indexer.h
        client/ui/main_window.cpp
        client/ui/main_window.h
        client/ui/index_settings_dialog.cpp
        client/ui/index_settings_dialog.h
    )
endif()

target_link_libraries(CrossNetShareClient
    PRIVATE common
    PRIVATE Qt5::Core
    PRIVATE Qt5::Widgets
    PRIVATE Qt5::Network
    PRIVATE Qt5::Concurrent
)

# Windows特定设置
if(WIN32)
    # 使用独立的 SQLite C API（不依赖 Qt SQL）
    # SQLite 文件将通过 GitHub Actions 在构建时下载
    set(SQLITE3_INCLUDE_DIR "${CMAKE_SOURCE_DIR}/third_party/sqlite")
    set(SQLITE3_LIBRARY "${CMAKE_SOURCE_DIR}/third_party/sqlite/sqlite3.lib")
    
    if(EXISTS "${SQLITE3_INCLUDE_DIR}/sqlite3.h")
        message(STATUS "Found SQLite headers: ${SQLITE3_INCLUDE_DIR}")
        target_include_directories(CrossNetShareClient PRIVATE ${SQLITE3_INCLUDE_DIR})
        
        if(EXISTS "${SQLITE3_LIBRARY}")
            message(STATUS "Found SQLite library: ${SQLITE3_LIBRARY}")
            target_link_libraries(CrossNetShareClient PRIVATE ${SQLITE3_LIBRARY})
        else()
            message(WARNING "SQLite library not found at: ${SQLITE3_LIBRARY}")
            message(WARNING "Build will continue but runtime may fail")
        endif()
    else()
        message(WARNING "SQLite headers not found at: ${SQLITE3_INCLUDE_DIR}")
        message(WARNING "Please download SQLite from https://www.sqlite.org/download.html")
        message(WARNING "Build will continue but may fail")
    endif()
    
    target_link_libraries(CrossNetShareServer PRIVATE ws2_32)
    target_link_libraries(CrossNetShareClient PRIVATE ws2_32)

    # GUI应用（不显示控制台）
    set_target_properties(CrossNetShareServer PROPERTIES WIN32_EXECUTABLE TRUE)
    set_target_properties(CrossNetShareClient PROPERTIES WIN32_EXECUTABLE TRUE)

    # 部署Qt插件（图像格式插件）
    # 获取Qt安装路径
    get_target_property(QT_QMAKE_EXECUTABLE Qt5::qmake IMPORTED_LOCATION)
    get_filename_component(QT_BIN_DIR "${QT_QMAKE_EXECUTABLE}" DIRECTORY)
    get_filename_component(QT_ROOT_DIR "${QT_BIN_DIR}" DIRECTORY)
    set(QT_PLUGINS_DIR "${QT_ROOT_DIR}/plugins")

    # 在构建后复制图像格式插件到输出目录
    add_custom_command(TARGET CrossNetShareServer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E make_directory
            "$<TARGET_FILE_DIR:CrossNetShareServer>/plugins/imageformats"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_PLUGINS_DIR}/imageformats/qjpeg.dll"
            "$<TARGET_FILE_DIR:CrossNetShareServer>/plugins/imageformats/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_PLUGINS_DIR}/imageformats/qgif.dll"
            "$<TARGET_FILE_DIR:CrossNetShareServer>/plugins/imageformats/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_PLUGINS_DIR}/imageformats/qico.dll"
            "$<TARGET_FILE_DIR:CrossNetShareServer>/plugins/imageformats/"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${QT_PLUGINS_DIR}/imageformats/qsvg.dll"
            "$<TARGET_FILE_DIR:CrossNetShareServer>/plugins/imageformats/"
        COMMENT "Copying Qt image format plugins..."
    )
    
    # 复制Aspose.Words Python转换脚本
    add_custom_command(TARGET CrossNetShareServer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/server/word_to_pdf.py"
            "$<TARGET_FILE_DIR:CrossNetShareServer>/word_to_pdf.py"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/server/db_query.py"
            "$<TARGET_FILE_DIR:CrossNetShareServer>/db_query.py"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/server/fill_template.py"
            "$<TARGET_FILE_DIR:CrossNetShareServer>/fill_template.py"
        COMMENT "Copying Python scripts (word_to_pdf, db_query, fill_template)..."
    )
    
    # 复制批办单模板和 H2 数据库 jar
    add_custom_command(TARGET CrossNetShareServer POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/模板.docx"
            "$<TARGET_FILE_DIR:CrossNetShareServer>/模板.docx"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/h2.jar"
            "$<TARGET_FILE_DIR:CrossNetShareServer>/h2.jar"
        COMMENT "Copying approval template and H2 jar..."
    )
    
    # 复制客户端文本提取脚本
    add_custom_command(TARGET CrossNetShareClient POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/client/extract_pdf_text.py"
            "$<TARGET_FILE_DIR:CrossNetShareClient>/extract_pdf_text.py"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "${CMAKE_SOURCE_DIR}/client/extract_word_text.py"
            "$<TARGET_FILE_DIR:CrossNetShareClient>/extract_word_text.py"
        COMMENT "Copying text extraction scripts..."
    )
    
    # 复制 SQLite DLL（用于内容搜索）
    if(EXISTS "${CMAKE_SOURCE_DIR}/third_party/sqlite/sqlite3.dll")
        add_custom_command(TARGET CrossNetShareClient POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/third_party/sqlite/sqlite3.dll"
                "$<TARGET_FILE_DIR:CrossNetShareClient>/sqlite3.dll"
            COMMENT "Copying SQLite DLL..."
        )
    else()
        message(WARNING "sqlite3.dll not found, client will fail to run content search")
    endif()
    
    # 移除旧的 Qt SQL 驱动复制（不再需要）
    # 现在使用独立的 SQLite 而不是 Qt SQL
    
    # 如果许可证文件存在，复制它
    if(EXISTS "${CMAKE_SOURCE_DIR}/Aspose.Words/python专用whl包/1.lic")
        add_custom_command(TARGET CrossNetShareServer POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E copy_if_different
                "${CMAKE_SOURCE_DIR}/Aspose.Words/python专用whl包/1.lic"
                "$<TARGET_FILE_DIR:CrossNetShareServer>/1.lic"
            COMMENT "Copying Aspose.Words license..."
        )
    endif()
endif()
