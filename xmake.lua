add_rules("mode.debug", "mode.release")

target("zlib")
    set_kind("static")
    add_includedirs("deps/zlib")
    add_files("deps/zlib/*.c")
    add_cflags("-O2", "-std=c89")


target("filterx")
    set_kind("binary")
    if is_mode("debug") then
        add_cflags("-ggdb", "-fno-omit-frame-pointer", "-fsanitize=address")
        add_syslinks("asan")
    end
    add_includedirs("include", "deps/zlib")
    add_files("src/*.cc")
    add_deps("zlib")
