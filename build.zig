const std = @import("std");

pub fn build(b: *std.Build) void {
    const target = b.standardTargetOptions(.{});
    const mode = b.standardOptimizeOption(.{});

    const zlib = b.addStaticLibrary(.{
        .name = "z",
        .target = target,
        .optimize = mode,
    });

    zlib.linkLibC();
    zlib.addIncludePath(b.path("./deps/zlib"));
    zlib.addCSourceFiles(.{
        .root = b.path("./deps/zlib"),
        .files = &[_][]const u8{
            "adler32.c",
            "compress.c",
            "crc32.c",
            "deflate.c",
            "gzclose.c",
            "gzlib.c",
            "gzread.c",
            "gzwrite.c",
            "inflate.c",
            "infback.c",
            "inftrees.c",
            "inffast.c",
            "trees.c",
            "uncompr.c",
            "zutil.c",
        },
        .flags = &[_][]const u8{
            "-std=c89",
            "-O2",
        },
    });

    const filterx = b.addExecutable(.{
        .name = "filterx",
        .target = target,
        .optimize = mode,
    });
    filterx.addIncludePath(b.path("./include"));
    filterx.addIncludePath(b.path("./deps/zlib"));
    filterx.linkLibCpp();
    filterx.linkLibrary(zlib);
    filterx.addCSourceFiles(.{
        .root = b.path("./src"),
        .files = &[_][]const u8{
            "main.cc",
            "data_provider.cc",
            "process.cc",
            "param.cc",
        },
        .flags = &[_][]const u8{
            "-std=c++17",
            "-O3",
        },
    });

    b.installArtifact(filterx);
}
