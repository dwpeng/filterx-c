format:
    clang-format -i --sort-includes ./src/*.cc ./include/*.h

build:
    xmake build

release:
    zig build -Dtarget="x86_64-linux-gnu.2.17" -Doptimize=ReleaseFast -j4
