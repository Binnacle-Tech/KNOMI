# PlatformIO pre-script: build the code that draws every frame for speed (-O2) instead of size (-Os).
# LVGL's software renderer, TFT_eSPI's pixel pushing and Coaster's face; everything else stays small.
# The app slot has room (tools/check.py reports how full it is).
Import("env")

FAST = ("/lvgl/src/draw/", "/lvgl/src/misc/", "/lvgl/src/core/lv_refr", "/TFT_eSPI/", "/src/knomi_coaster.cpp")


def fast(env, node):
    path = node.srcnode().get_abspath().replace("\\", "/")
    if any(f in path for f in FAST):
        return env.Object(node, CCFLAGS=env["CCFLAGS"] + ["-O2"])
    return node


env.AddBuildMiddleware(fast)
