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

    // rfb_window_demo mixes a HIP source (hip_helper.cpp, needs amdclang++
    // -x hip) with plain C/C++ sources that must NOT get -x hip forced onto
    // them (the 3 generated wayland protocol .c files in particular link
    // with undefined symbols if they do - see commit history). Compile each
    // source to its own object with the right compiler, like CMake's
    // per-target compiler selection does, then link with a plain host
    // linker - this is exactly what compile_object/link_objects are for.
    {
        auto posix_cpp = root / "posix_cpp";
        auto xkb_helper = root / "xkb_helper";
        auto wayland_helper = root / "wayland_helper";
        auto drm_xxxx = root / "drm_xxxx";
        auto hip_helper = root / "hip_helper";
        auto wl_proto = root / "build_librfb_wl_proto";
        auto wayland_protocols = root / "wayland-protocols";

        std::filesystem::create_directories(wl_proto);
        struct protocol { std::string name; std::filesystem::path xml; };
        std::vector<protocol> protocols = {
            {"xdg-shell-client", wayland_protocols / "stable" / "xdg-shell" / "xdg-shell.xml"},
            {"primary-selection-unstable-v1-client",
             wayland_protocols / "unstable" / "primary-selection" / "primary-selection-unstable-v1.xml"},
            {"linux-dmabuf-v1-client", wayland_protocols / "stable" / "linux-dmabuf" / "linux-dmabuf-v1.xml"},
        };
        bool proto_ok = true;
        for (auto& p : protocols) {
            auto header = wl_proto / (p.name + ".h");
            auto source = wl_proto / (p.name + ".c");
            auto cmd_h = std::format("wayland-scanner client-header {} {}", p.xml.string(), header.string());
            auto cmd_c = std::format("wayland-scanner private-code {} {}", p.xml.string(), source.string());
            std::cout << cmd_h << std::endl;
            proto_ok = proto_ok && (system(cmd_h.c_str()) == 0);
            std::cout << cmd_c << std::endl;
            proto_ok = proto_ok && (system(cmd_c.c_str()) == 0);
        }

        std::vector<std::string> includes = {
            "-I" + librfb.string(), "-I" + cpp_helper.string(), "-I" + socket_helper.string(),
            "-I" + remote_media_include.string(), "-I" + posix_cpp.string(),
            "-I" + xkb_helper.string(), "-I" + wayland_helper.string(),
            "-I" + drm_xxxx.string(), "-I" + hip_helper.string(),
            "-I" + wl_proto.string(), "-I/usr/include/libdrm",
        };
        auto cxx = includes;
        cxx.push_back("-std=c++23");
        // Only window.cpp (and hip_helper.cpp) #include hip_helper.hpp, whose
        // __global__/<<<>>> syntax needs -x hip; every other source here is
        // plain host C/C++ and compiles with the ordinary system compiler.
        auto host_cxx = build::find_cxx_compiler();

        enum class lang { cxx, hip, c };
        struct obj_src { std::filesystem::path src; lang language; };
        // Shared across both rfb_window_demo and test_hip - both CMake
        // targets link the identical posix_cpp/xkb_helper/wayland_helper/
        // hip_helper/drm_helper/rfb set, differing only in their own main.
        std::vector<obj_src> common_srcs = {
            {cpp_helper / "cpp_helper.cpp", lang::cxx},
            {socket_helper / "socket_helper.cpp", lang::cxx},
            {librfb / "rfb.cpp", lang::cxx},
            {posix_cpp / "posix.cpp", lang::cxx},
            {xkb_helper / "xkb_helper.cpp", lang::cxx},
            {wayland_helper / "wayland_helper.cpp", lang::cxx},
            {drm_xxxx / "drm_helper.cpp", lang::cxx},
            // These generated protocol files are real C (declare plain, non-
            // mangled wl_interface symbols); compiling them as C++ (even
            // without -x hip) mangles those symbols with internal linkage
            // (nm shows "_ZL21xdg_wm_base_interface" instead of the plain C
            // name window.cpp's generated header expects), so they must go
            // through an actual C compiler, not c++/clang++ in C++ mode.
            {wl_proto / "xdg-shell-client.c", lang::c},
            {wl_proto / "primary-selection-unstable-v1-client.c", lang::c},
            {wl_proto / "linux-dmabuf-v1-client.c", lang::c},
            {hip_helper / "hip_helper.cpp", lang::hip},
        };

        auto compile_one = [&](const obj_src& s) {
            auto obj = wl_proto / (s.src.stem().string() + ".o");
            int res;
            if (s.language == lang::hip) {
                auto flags = includes;
                flags.push_back("-x hip --offload-arch=gfx1201 -std=c++23");
                res = build::compile_object("/opt/rocm/bin/amdclang++", flags, s.src, obj);
            } else if (s.language == lang::c) {
                res = build::compile_object("cc", includes, s.src, obj);
            } else {
                res = build::compile_object(host_cxx, cxx, s.src, obj);
            }
            return std::pair{res == 0, obj};
        };

        std::vector<std::string> common_objs;
        bool compile_ok = proto_ok;
        for (auto& s : common_srcs) {
            auto [ok, obj] = compile_one(s);
            compile_ok = compile_ok && ok;
            common_objs.push_back(obj.string());
        }

        std::vector<std::string> extra_libs = {
            "-lwayland-client", "-lxkbcommon", "-ldrm", "-ldrm_amdgpu",
            "-L/opt/rocm/lib", "-lamdhip64",
            "/usr/lib/libz.so", "/usr/lib/libssl.so", "/usr/lib/libcrypto.so",
            "/usr/lib/libavcodec.so", "/usr/lib/libavdevice.so",
            "/usr/lib/libavutil.so", "/usr/lib/libswscale.so", "-ltbb",
        };

        struct wl_target { std::string name; std::filesystem::path main_src; };
        std::vector<wl_target> wl_targets = {
            {"rfb_window_demo", librfb / "window.cpp"},
            {"test_hip", librfb / "test_hip.cpp"},
        };
        for (auto& t : wl_targets) {
            auto [main_ok, main_obj] = compile_one({t.main_src, lang::hip});
            auto out = t.name + "_bh";
            if (compile_ok && main_ok) {
                std::vector<std::string> link_items = common_objs;
                link_items.push_back(main_obj.string());
                link_items.insert(link_items.end(), extra_libs.begin(), extra_libs.end());
                auto res = build::link_objects(host_cxx, link_items, out);
                if (res == 0) {
                    std::cout << "build_librfb: built ./" << out << std::endl;
                } else {
                    std::cerr << "build_librfb: FAILED " << t.name << " (link)" << std::endl;
                    all_ok = false;
                }
            } else {
                std::cerr << "build_librfb: FAILED " << t.name << " (compile)" << std::endl;
                all_ok = false;
            }
        }
    }

    return all_ok ? 0 : 1;
}
