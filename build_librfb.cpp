// Builds librfb's Linux, non-GPU executables using build_helper
// (https://github.com/water-chika/build_helper, submodule at ./build_helper)
// instead of CMake.
//
// In scope: rfb_server, rfb_demo, rfb_recorder, rfb_input, rfb_capture,
// rfb_clipboard, rfb_control_tests, rfb_trace_tests, rfb_fragmentation_tests,
// rfb_server_core_tests - everything that only needs cpp_helper/
// socket_helper/remote-media/zlib/openssl/ffmpeg.
// Out of scope (not attempted here): rfb_window_demo + test_hip
// (Wayland+DRM+HIP), rfb_window_win32 (Windows/MSVC-only), and the Python
// ctest harness - porting those to build_helper would mean also
// reimplementing wayland-scanner protocol codegen and a HIP compile path,
// which is a separate, much larger task.
#include <filesystem>
#include <iostream>
#include <vector>
#include "build.hpp"

struct target {
    std::string name;
    std::vector<std::string> extra_sources;
    std::vector<std::string> extra_libs;
};

int main() {
    std::filesystem::path root = ".";
    auto librfb = root / "librfb";
    auto cpp_helper = root / "cpp_helper";
    auto socket_helper = root / "socket_helper";
    auto remote_media_include = root / "remote-media" / "include";

    std::vector<std::string> include_dirs = {
        librfb.string(), cpp_helper.string(), socket_helper.string(),
        remote_media_include.string(),
    };
    std::vector<std::string> common_sources = {
        (cpp_helper / "cpp_helper.cpp").string(),
        (socket_helper / "socket_helper.cpp").string(),
    };
    std::vector<std::string> common_libs = {
        "/usr/lib/libz.so", "/usr/lib/libssl.so", "/usr/lib/libcrypto.so",
    };
    // rfb.cpp (the client-side library) and the ffmpeg libs it needs; only
    // rfb_server skips this and uses rfb_server_core/remote_media instead.
    std::vector<std::string> rfb_lib_sources = {(librfb / "rfb.cpp").string()};
    std::vector<std::string> rfb_lib_libs = {
        "/usr/lib/libavcodec.so", "/usr/lib/libavdevice.so",
        "/usr/lib/libavutil.so", "/usr/lib/libswscale.so",
    };

    std::vector<std::string> server_core_sources = {
        (root / "remote-media" / "src" / "frame.cpp").string(),
        (librfb / "server_core.cpp").string(),
    };

    std::vector<target> targets = {
        {"rfb_server",
         {(root / "remote-media" / "src" / "frame.cpp").string(),
          (librfb / "server_core.cpp").string(), (librfb / "server.cpp").string()},
         {}},
        {"rfb_demo", {(librfb / "main.cpp").string()}, {}},
        {"rfb_recorder", {(librfb / "recorder.cpp").string()}, {"-lavformat"}},
        {"rfb_input", {(librfb / "input.cpp").string()}, {}},
        {"rfb_capture", {(librfb / "capture.cpp").string()}, {}},
        {"rfb_clipboard", {(librfb / "clipboard.cpp").string()}, {}},
        {"rfb_control_tests", {(librfb / "test_control.cpp").string()}, {}},
        {"rfb_trace_tests", {(librfb / "test_trace.cpp").string()}, {}},
        {"rfb_fragmentation_tests", {(librfb / "test_rfb_fragmentation.cpp").string()}, {}},
        {"rfb_server_core_tests", {(librfb / "test_server_core.cpp").string()}, {}},
    };

    bool all_ok = true;
    for (auto& t : targets) {
        bool is_server_core_test = (t.name == "rfb_server_core_tests");
        bool uses_rfb_lib = (t.name != "rfb_server" && !is_server_core_test);

        std::vector<std::string> sources = include_dirs;
        sources.insert(sources.end(), common_sources.begin(), common_sources.end());
        if (is_server_core_test) {
            sources.insert(sources.end(), server_core_sources.begin(), server_core_sources.end());
        }
        if (uses_rfb_lib) {
            sources.insert(sources.end(), rfb_lib_sources.begin(), rfb_lib_sources.end());
        }
        sources.insert(sources.end(), t.extra_sources.begin(), t.extra_sources.end());
        sources.insert(sources.end(), common_libs.begin(), common_libs.end());
        if (uses_rfb_lib) {
            sources.insert(sources.end(), rfb_lib_libs.begin(), rfb_lib_libs.end());
        }
        sources.insert(sources.end(), t.extra_libs.begin(), t.extra_libs.end());

        auto out = t.name + "_bh";
        auto res = build::c_plus_plus_compile(build::find_cxx_compiler(), sources, out);
        if (res != 0) {
            std::cerr << "build_librfb: FAILED " << t.name << std::endl;
            all_ok = false;
        } else {
            std::cout << "build_librfb: built ./" << out << std::endl;
        }
    }
    return all_ok ? 0 : 1;
}
