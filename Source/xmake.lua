set_xmakever("3.0.0")

set_project("SimpleAimAssist")
set_version("1.2.5")
set_arch("x64")
set_languages("c++23")
set_warnings("allextra")
set_encodings("utf-8")

add_rules("mode.debug", "mode.releasedbg")

local commonlibf4Root = os.getenv("COMMONLIBF4_ROOT")
if commonlibf4Root == nil or commonlibf4Root == "" then
    raise("Set COMMONLIBF4_ROOT to the CommonLibF4 checkout before building.")
end

includes(commonlibf4Root)

target("SimpleAimAssist", function()
    add_rules("commonlibf4.plugin", {
        name = "SimpleAimAssist",
        author = "Sylva",
        plugin_template = "commonlibf4-plugin.cpp.in"
    })

    add_files("src/**.cpp")
    add_includedirs("src")
end)
