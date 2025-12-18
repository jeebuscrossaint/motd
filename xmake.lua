set_project("motd")
set_version("0.1.0")
set_languages("c++17")

add_rules("mode.debug", "mode.release")

if is_mode("release") then
    set_optimize("faster")
    set_strip("all")
end

target("motd")
    set_kind("binary")
    add_files("src/*.cpp")
    add_includedirs("src")

    if is_plat("linux") then
        add_syslinks("pthread")
        -- Try to find libpci, but make it optional
        if os.exists("/usr/include/pci/pci.h") or os.exists("/usr/local/include/pci/pci.h") then
            add_syslinks("pci")
            add_defines("HAS_LIBPCI")
        end
    elseif is_plat("bsd") then
        add_syslinks("kvm")
        -- OpenBSD has libpci
        if os.exists("/usr/local/include/pci/pci.h") then
            add_syslinks("pci")
            add_defines("HAS_LIBPCI")
        end
    end

    set_warnings("all")
