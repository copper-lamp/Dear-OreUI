add_rules("mode.debug", "mode.release")

add_repositories("levimc-repo https://github.com/LiteLDev/xmake-repo.git")

-- ============================================================
-- 自动使用 LLVM (clang-cl) 优化工具链
-- 条件：检测到 LLVM 安装目录 且 其 bin 已在 PATH 中（工具链按 PATH 发现 clang-cl）
-- 满足时全局使用 clang-cl[llvm]（clang-cl + lld-link/llvm-rc/llvm-ar），
-- 否则保持默认 MSVC 工具链（过期终端/CI/其他机器不受影响）。
-- ============================================================
local _use_clang_cl = false
if is_plat("windows") then
    local candidates = {}
    local llvm_home = os.getenv("LLVM_HOME") or os.getenv("LLVM_ROOT") or os.getenv("LLVM")
    if llvm_home then
        table.insert(candidates, llvm_home)
    end
    table.insert(candidates, "D:/LLVM")
    table.insert(candidates, "C:/Program Files/LLVM")
    table.insert(candidates, "C:/Program Files (x86)/LLVM")
    local pathenv = (os.getenv("PATH") or ""):lower()
    for _, p in ipairs(candidates) do
        if os.isdir(p) and os.isfile(path.join(p, "bin/clang-cl.exe")) then
            -- 本机 LLVM 已加入系统 PATH，新开终端自动生效
            _use_clang_cl = pathenv:find(path.join(p, "bin"):lower(), 1, true) ~= nil
            break
        end
    end
end

if _use_clang_cl then
    set_toolchains("clang-cl[llvm]")
end

option("target_type")
    set_default("client")
    set_showmenu(true)
    set_values("server", "client")
option_end()

add_requires("levilamina 26.10.*", {configs = {target_type = get_config("target_type")}})

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
    end
    add_packages("levilamina")
    set_kind("shared")
    set_languages("c++20")
    set_symbols("debug")
    add_headerfiles("src/**.h")
    add_files("src/**.cpp")
    add_includedirs("src")

target("DearOreUIUnitTests")
    set_default(false)
    set_kind("binary")
    set_languages("c++20")
    if is_plat("windows") then
        add_defines("NOMINMAX", "UNICODE", "_CRT_SECURE_NO_WARNINGS")
        set_exceptions("none")
        add_cxflags("/EHa", "/utf-8", "/W4", "/w44265", "/w44289", "/w44296", "/w45263", "/w44738", "/w45204")
    end
    add_packages("levilamina")
    add_includedirs("src")
    add_includedirs("tests")
    add_includedirs(".")
    -- Tests and the source modules they exercise (no mod entry, stubs replace hook/poc).
    add_files("tests/main.cpp")
    add_files("tests/Stage1NavigationStateTests.cpp")
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
    add_files("tests/stubs/*.cpp")
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
