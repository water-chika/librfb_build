// Builds rfb_server using build_helper (https://github.com/water-chika/build_helper)
// instead of CMake, as a real exercise of that library (previously only its
// own toy test.cpp had ever been compiled).
//
// build_helper is a git submodule of this repo (./build_helper).
#include <filesystem>
#include <iostream>
#include "build.hpp"

int main() {
    std::filesystem::path root = ".";
    auto librfb = root / "librfb";
    auto cpp_helper = root / "cpp_helper";
    auto socket_helper = root / "socket_helper";
    auto remote_media_include = root / "remote-media" / "include";

    build::builder builder;
    builder.add_executable(
        "rfb_server", "rfb_server_bh",
        librfb.string(),
        cpp_helper.string(),
        socket_helper.string(),
        remote_media_include.string(),
        (cpp_helper / "cpp_helper.cpp").string(),
        (socket_helper / "socket_helper.cpp").string(),
        (root / "remote-media" / "src" / "frame.cpp").string(),
        (librfb / "server_core.cpp").string(),
        (librfb / "server.cpp").string(),
        std::string{"/usr/lib/libz.so"},
        std::string{"/usr/lib/libssl.so"},
        std::string{"/usr/lib/libcrypto.so"}
    );

    auto res = builder.build();
    if (res != build::update_res::success) {
        std::cerr << "build_librfb: build failed" << std::endl;
        return 1;
    }
    std::cout << "build_librfb: built ./rfb_server_bh" << std::endl;
    return 0;
}
