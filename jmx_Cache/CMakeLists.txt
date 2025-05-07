# CMakeList.txt: jmx_Cache 的 CMake 项目，在此处包括源代码并定义
# 项目特定的逻辑。
#

# 将源代码添加到此项目的可执行文件。
add_executable (jmx_Cache  "jmx_Cache.h" "jlrucache.h" "test.cpp" "jARCcache/jarcnode.h" "jARCcache/jarclru.h" "jARCcache/jarccache.h")

if (CMAKE_VERSION VERSION_GREATER 3.12)
  set_property(TARGET jmx_Cache PROPERTY CXX_STANDARD 20)
endif()

# TODO: 如有需要，请添加测试并安装目标。
