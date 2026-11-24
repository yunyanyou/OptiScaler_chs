import re

p = r'F:/Projects/Optiscaler/chs-build/OptiScaler_chs/OptiScaler/menu/menu_common.cpp'
with open(p, 'r', encoding='utf-8') as fl:
    mc = fl.read()

pattern = r'static const Tab tabs\[\] = \{.*?\n\s*\};'
new_block = """static const Tab tabs[] = {
        { "main", "Custom", RenderCustomTab, nullptr, [](RenderMenuContext& c) { (void)c; return "自定义"; } },
        { "main", "Upscaling", RenderUpscalerStateMessage, nullptr, [](RenderMenuContext& c) { (void)c; return "超分"; } },
        { "main", LOC(UI_Frame_Gen), nullptr, nullptr, [](RenderMenuContext& c) { (void)c; return "帧生成"; } },
        { "main", "Latency", nullptr, nullptr, [](RenderMenuContext& c) { (void)c; return "延迟"; } },
        { "system", "Textures", nullptr, [](RenderMenuContext& c) { return c.state.swapchainApi != Vulkan; },
          [](RenderMenuContext& c) { (void)c; return "纹理"; } },
        { "system", "Compatibility", nullptr, nullptr, [](RenderMenuContext& c) { (void)c; return "兼容性"; } },
        { "advanced", LOC(UI_Menu), nullptr, nullptr, [](RenderMenuContext& c) { (void)c; return "菜单"; } },
        { "advanced", "Quirks", RenderQuirksSettings, [](RenderMenuContext& c) { return QuirksTabVisible(c.state); },
          [](RenderMenuContext& c) { return QuirksTabLabel(c.state); } },
        { "advanced", "Status", nullptr, nullptr, [](RenderMenuContext& c) { (void)c; return "状态"; } },
    };"""

mc, count = re.subn(pattern, new_block, mc, flags=re.DOTALL)
print('Replaced tabs count:', count)
with open(p, 'w', encoding='utf-8') as fl:
    fl.write(mc)
