add_rules("mode.debug", "mode.release")

add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

option("target_type")
    set_default("client")
    set_showmenu(true)
    set_values("server", "client")
option_end()

add_requires("levilamina 26.10.*", {configs = {target_type = get_config("target_type")}})

-- Keep libcurl and its zlib dependency on the same static linkage path on Windows.
add_requires("zlib 1.3.1", {configs = {shared = false}})
add_requires("libcurl 8.21.0", {configs = {shared = false}})

add_requires("levibuildscript")

if not has_config("vs_runtime") then
    set_runtimes("MD")
end

target("DearOreUI")
    add_rules("@levibuildscript/linkrule")
    add_rules("@levibuildscript/modpacker")
    if is_plat("windows") then
        add_defines("NOMINMAX", "UNICODE", "_CRT_SECURE_NO_WARNINGS")
        set_exceptions("none")
        add_cxflags( "/EHa", "/utf-8", "/W4", "/w44265", "/w44289", "/w44296", "/w45263", "/w44738", "/w45204")
        -- Native Facet/ExecuteScript transport requires no loopback socket.
    end
    add_packages("levilamina", "libcurl", "zlib")
    set_kind("shared")
    set_languages("c++20")
    set_symbols("debug")
    add_headerfiles("src/**.h", {install = false})
    add_files("src/**.cpp")
    add_includedirs("src")

    -- Production external-integration headers: install the Public API include
    -- set while preserving the api/ layout (headers reference "api/...").
    after_install(function(target)
        local inc = path.join(target:installdir(), "include", "dearoreui")
        os.mkdir(path.join(inc, "api", "manifest"))
        os.mkdir(path.join(inc, "api", "types"))
        os.mkdir(path.join(inc, "bridge"))
        os.cp("src/api/I*.h", path.join(inc, "api", "/"))
        os.cp("src/api/types/*.h", path.join(inc, "api", "types", "/"))
        for _, f in ipairs({"ModManifest.h", "ResourceManifest.h", "ScriptManifest.h", "StyleSheetManifest.h", "UiManifest.h", "Dependency.h", "Permission.h"}) do
            os.cp(path.join("src/api/manifest", f), path.join(inc, "api", "manifest", "/"))
        end
        os.cp("src/bridge/DearOreUIBridge.h", path.join(inc, "bridge", "/"))
    end)

target("DearOreUIUnitTests")
    set_default(false)
    set_kind("binary")
    set_languages("c++20")
    if is_plat("windows") then
        add_defines("NOMINMAX", "UNICODE", "_CRT_SECURE_NO_WARNINGS")
        set_exceptions("none")
        add_cxflags("/EHa", "/utf-8", "/W4", "/w44265", "/w44289", "/w44296", "/w45263", "/w44738", "/w45204")
        -- Native Facet/ExecuteScript transport requires no loopback socket.
    end
    add_packages("levilamina", "libcurl", "zlib")
    add_includedirs("src")
    add_includedirs("tests")
    add_includedirs(".")
    -- Tests and the source modules they exercise (no mod entry, stubs replace hook/poc).
    add_files("tests/main.cpp")
    add_files("tests/api/types/*.cpp")
    add_files("tests/api/manifest/*.cpp")
    add_files("tests/api/*.cpp")
    add_files("tests/diagnostic/*.cpp")
    add_files("tests/runtime/*.cpp")
    add_files("tests/registry/*.cpp")
    add_files("tests/page/*.cpp")
    add_files("tests/hook/*.cpp")
    add_files("tests/source/*.cpp")
    add_files("tests/resource/*.cpp")
    add_files("tests/inject/*.cpp")
    add_files("tests/ipc/*.cpp")
    add_files("tests/facet/*.cpp")
    add_files("tests/transform/*.cpp")
    add_files("tests/ui/*.cpp")
    add_files("tests/render/*.cpp")
    add_files("tests/component/*.cpp")
    add_files("tests/preview/*.cpp")
    add_files("tests/stubs/*.cpp")
    add_files("tests/bridge/*.cpp")
    add_files("src/bridge/*.cpp")
    add_files("src/hook/Stage5CoherentProbe.cpp")
    add_files("src/api/types/*.cpp")
    add_files("src/api/manifest/*.cpp")
    add_files("src/api/*.cpp")
    add_files("src/diagnostic/*.cpp")
    add_files("src/capability/*.cpp")
    add_files("src/registry/*.cpp")
    add_files("src/page/*.cpp")
    add_files("src/runtime/*.cpp")
    add_files("src/source/*.cpp")
    add_files("src/resource/*.cpp")
    add_files("src/inject/*.cpp")
    add_files("src/ipc/*.cpp")
    add_files("src/facet/*.cpp")
    add_files("src/transform/*.cpp")
    add_files("src/ui/*.cpp")
    add_files("src/render/*.cpp")
    add_files("src/preview/*.cpp")
    add_files("src/component/*.cpp")

target("DearOreUIExternalModApiExample")
    set_default(false)
    set_kind("static")
    set_languages("c++20")
    if is_plat("windows") then
        add_defines("NOMINMAX", "UNICODE", "_CRT_SECURE_NO_WARNINGS")
        add_cxflags("/utf-8", "/W4", "/EHa")
    end
    add_includedirs("src")
    add_files("examples/external_mod/ExternalModExample.cpp")
