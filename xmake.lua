add_rules("mode.debug", "mode.release")
set_languages("cxx20")

target("sheep")
    set_kind("static")
    add_files("src/*.cpp")
    add_includedirs("include")
    add_syslinks("pthread", "uring")

target("test_request")
    set_kind("binary")
    add_includedirs("include")
    add_files("test/test_request.cpp")
    add_deps("sheep")

target("test_response")
    set_kind("binary")
    add_includedirs("include")
    add_files("test/test_response.cpp")
    add_deps("sheep")

target("test_uri")
    set_kind("binary")
    add_includedirs("include")
    add_files("test/test_uri.cpp")
    add_deps("sheep")


target("test_address")
    set_kind("binary")
    add_includedirs("include")
    add_files("test/test_address.cpp")
    add_deps("sheep")


target("test_connection")
    set_kind("binary")
    add_includedirs("include")
    add_files("test/test_connection.cpp")
    add_deps("sheep")


target("echo_server")
    set_kind("binary")
    add_includedirs("include")
    add_files("examples/echo_server.cpp")
    add_deps("sheep")
--
-- If you want to known more usage about xmake, please see https://xmake.io
--
-- ## FAQ
--
-- You can enter the project directory firstly before building project.
--
--   $ cd projectdir
--
-- 1. How to build project?
--
--   $ xmake
--
-- 2. How to configure project?
--
--   $ xmake f -p [macosx|linux|iphoneos ..] -a [x86_64|i386|arm64 ..] -m [debug|release]
--
-- 3. Where is the build output directory?
--
--   The default output directory is `./build` and you can configure the output directory.
--
--   $ xmake f -o outputdir
--   $ xmake
--
-- 4. How to run and debug target after building project?
--
--   $ xmake run [targetname]
--   $ xmake run -d [targetname]
--
-- 5. How to install target to the system directory or other output directory?
--
--   $ xmake install
--   $ xmake install -o installdir
--
-- 6. Add some frequently-used compilation flags in xmake.lua
--
-- @code
--    -- add debug and release modes
--    add_rules("mode.debug", "mode.release")
--
--    -- add macro defination
--    add_defines("NDEBUG", "_GNU_SOURCE=1")
--
--    -- set warning all as error
--    set_warnings("all", "error")
--
--    -- set language: c99, c++11
--    set_languages("c99", "c++11")
--
--    -- set optimization: none, faster, fastest, smallest
--    set_optimize("fastest")
--
--    -- add include search directories
--    add_includedirs("/usr/include", "/usr/local/include")
--
--    -- add link libraries and search directories
--    add_links("tbox")
--    add_linkdirs("/usr/local/lib", "/usr/lib")
--
--    -- add system link libraries
--    add_syslinks("z", "pthread")
--
--    -- add compilation and link flags
--    add_cxflags("-stdnolib", "-fno-strict-aliasing")
--    add_ldflags("-L/usr/local/lib", "-lpthread", {force = true})
--
-- @endcode
--

