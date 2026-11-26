#include "pch.h"
#include <localization/localization.h>
#include "menu_common.h"
#include "PresentRateMeter.h"
#include <inputs/FG/XeFG_Inputs_Dx12.h>

#include "input/input_system.h"

#include "font/Hack_Compressed.h"

#include <proxies/XeSS_Proxy.h>
#include <proxies/XeFG_Proxy.h>
#include <proxies/FfxApi_Proxy.h>
#include <upscalers/ffx/FFXFeature.h>
#include <upscalers/dlssd/DLSSDFeature.h>
#include <fsr4/FSR4Models.h>
#include <fsr4/FSR4Presets.h>
#include <fsr4/FSR4DriverProviderVk.h>
#include <proxies/Streamline_Proxy.h>

#include <framegen/nvngx/Nvngx_FG.h>
#include <upscalers/nvngx/Nvngx_RR.h>
#include <framegen/reprojection/Reprojection_Dx12.h>
#include <framegen/flipmeter/FlipMeterFG_Dx12.h>
#include <framegen/flipmeter/FlipMeterSwapChain.h>

#include <nvapi/fakenvapi.h>
#include <hooks/Reflex_Hooks.h>
#include <hooks/Vulkan_Hooks.h>
#include <hooks/LibraryLoad_Hooks.h>

#include <BuildInfo.h>
#include <version_check.h>

#include <imgui/imgui_internal.h>
#include <imgui/ImGuiNotify.hpp>
#include <imgui/imgui_impl_win32.h>
#include <imgui/imgui_impl_uwp.h>

#include <mutex>
#include <cstdarg>
#include <sstream>

#include <array>
#include <numeric>
#include <chrono>
#include <memory>
#include <type_traits>
#include <misc/IdentifyGpu.h>
#include <hooks/Amdxc64_Hooks.h>
#include <low_latency/input/input_common.h>
#include <plugins/PluginApi.h>
#include <hooks/Vulkan_Hooks.h>
#include <low_latency/input/input_xell.h>

enum class UiTargetMode
{
    SDR,
    LinearHDR,
    ScRGB,
    PQ,
    HLG
};

#define MARK_ALL_BACKENDS_CHANGED()                                                                                    \
    for (auto& singleChangeBackend : State::Instance().changeBackend)                                                  \
        singleChangeBackend.second = true;

static float fontSize = 14.0f; // just changing this doesn't make other elements scale ideally
static ImVec2 overlaySize(0.0f, 0.0f);
static ImVec2 overlayPosition(-1000.0f, -1000.0f);
static bool _hdrTonemapApplied = false;
static ImVec4 SdrColors[ImGuiCol_COUNT];

static bool inputMenu = false;
static bool inputFG = false;
static bool fgHotkeyLeftDefault = false; // FG hotkey temporarily overrode Default (follow game)
static int fgHotkeySavedOverride = -1;   // Override DLSSG Ratio with value before Off, -1 = Default
static bool inputFps = false;
static bool inputFpsCycle = false;
static uint64_t lastInputTick = 0;
constexpr uint64_t debounceThreshold = 1000;

static bool hasGamepad = false;
static bool ffxInitTried = false;
static bool ffxVkInitTried = false;
static bool xefgInitTried = false;
static std::string windowTitle;
static std::string selectedUpscalerName = "";
static Upscaler currentBackend = Upscaler::Reset;
static std::string currentBackendName = "";
static int refreshRate = 0;
static ImVec2 lastPosition(-1000.0f, -1000.0f);

static ImVec2 splashPosition(-1000.0f, -1000.0f);
static ImVec2 splashSize(0.0f, 0.0f);
static double splashStart = 0.0;
static double splashLimit = 0.0;
static std::vector<LK> splashKeys = {
        LK::Splash_253,
        LK::Splash_254,
        LK::Splash_255,
        LK::Splash_256,
        LK::Splash_257,
        LK::Splash_258,
        LK::Splash_259,
        LK::Splash_260,
        LK::Splash_261,
        LK::Splash_262,
        LK::Splash_263,
        LK::Splash_264,
        LK::Splash_253,
        LK::Splash_253,
        LK::Splash_253,
        LK::Splash_268,
        LK::Splash_269,
        LK::Splash_270,
        LK::Splash_271,
        LK::Splash_253,
        LK::Splash_273,
        LK::Splash_274,
        LK::Splash_275,
        LK::Splash_276,
        LK::Splash_277,
        LK::Splash_278,
        LK::Splash_279,
        LK::Splash_280,
        LK::Splash_281,
        LK::Splash_282,
        LK::Splash_253,
        LK::Splash_283,
        LK::Splash_284,
        LK::Splash_285,
        LK::Splash_286,
        LK::Splash_287,
        LK::Splash_288,
        LK::Splash_289,
        LK::Splash_Extra_2,
        LK::Splash_290,
        LK::Splash_291,
        LK::Splash_292,
        LK::Splash_293,
        LK::Splash_294,
        LK::Splash_295,
        LK::Splash_296,
        LK::Splash_Extra_3,
        LK::Splash_297,
        LK::Splash_298,
        LK::Splash_299,
        LK::Splash_300,
        LK::Splash_301,
        LK::Splash_302,
        LK::Splash_303,
        LK::Splash_304,
        LK::Splash_305,
        LK::Splash_306,
        LK::Splash_307,
        LK::Splash_308,
        LK::Splash_309,
        LK::Splash_310,
        LK::Splash_311,
        LK::Splash_312,
        LK::Splash_313,
        LK::Splash_314,
        LK::Splash_315,
        LK::Splash_316,
        LK::Splash_317,
        LK::Splash_318,
        LK::Splash_319,
        LK::Splash_320,
        LK::Splash_321,
        LK::Splash_322,
        LK::Splash_323,
        LK::Splash_324,
        LK::Splash_325,
        LK::Splash_326,
        LK::Splash_327,
        LK::Splash_328,
        LK::Splash_329,
        LK::Splash_330,
        LK::Splash_253,
        LK::Splash_253,
        LK::Splash_253,
        LK::Splash_253,
        LK::Splash_253,
        LK::Splash_253,
        LK::Splash_253,
        LK::Splash_253,
        LK::Splash_253,
        LK::Splash_253,
        LK::Splash_253,
        LK::Splash_253,
        LK::Splash_253,
        LK::Splash_331
    };

static std::string updateNoticeTag;
static std::string updateNoticeUrl;
static float lastMenuScale = 0.0f;
static CustomOptional<uint32_t> comboPreset { 0 };
static int lastKey = 0;
static bool capturingKey = false;
static int selectedMenuTab = 0;
static const char* requestedMenuTab = nullptr; // Label of a tab to switch to on the next frame

template <typename T, size_t N> struct RingBuffer
{
    std::array<T, N> data {};
    size_t head { 0 };
    size_t count { N };
    double sum { 0.0 };

    RingBuffer() { data.fill(static_cast<T>(0)); }

    void Push(T v)
    {
        if (count == N)
        {
            sum -= data[head];
        }
        else
        {
            ++count;
        }
        data[head] = v;
        sum += v;
        head = (head + 1) % N;
    }

    size_t Size() const { return N; }

    T At(size_t i) const
    {
        size_t start = head;
        return data[(start + i) % N];
    }

    float Average() const { return static_cast<float>(sum / static_cast<double>(N)); }
};

const int plotWidth = 360;
static RingBuffer<float, plotWidth> gFrameTimes;
static RingBuffer<float, plotWidth> gUpscalerTimes;

// FPS overlay fonts offered in the menu, from the Windows fonts folder
static const std::pair<const char*, const char*> overlayFonts[] = {
    { "Segoe UI", "segoeui.ttf" },
    { "Segoe UI Semilight", "segoeuisl.ttf" },
    { "Segoe UI Semibold", "seguisb.ttf" },
    { "Segoe UI Bold", "segoeuib.ttf" },
    { "Segoe UI Italic", "segoeuii.ttf" },
    { "Segoe UI Black", "seguibl.ttf" },
    { "Segoe UI Variable", "SegUIVar.ttf" },
    { "Bahnschrift", "bahnschrift.ttf" },
    { "Consolas", "consola.ttf" },
    { "Consolas Bold", "consolab.ttf" },
    { "Consolas Italic", "consolai.ttf" },
    { "Cascadia Mono", "CascadiaMono.ttf" },
    { "Cascadia Code", "CascadiaCode.ttf" },
    { "Unispace Bold", "unispace bd.ttf" },
    { "Lucida Console", "lucon.ttf" },
    { "Lucida Sans Unicode", "l_10646.ttf" },
    { "Courier New", "cour.ttf" },
    { "Courier New Bold", "courbd.ttf" },
    { "Arial", "arial.ttf" },
    { "Arial Bold", "arialbd.ttf" },
    { "Arial Black", "ariblk.ttf" },
    { "Verdana", "verdana.ttf" },
    { "Verdana Bold", "verdanab.ttf" },
    { "Tahoma", "tahoma.ttf" },
    { "Tahoma Bold", "tahomabd.ttf" },
    { "Calibri", "calibri.ttf" },
    { "Calibri Bold", "calibrib.ttf" },
    { "Trebuchet MS", "trebuc.ttf" },
    { "Trebuchet MS Bold", "trebucbd.ttf" },
    { "Century Gothic", "GOTHIC.TTF" },
    { "Century Gothic Bold", "GOTHICB.TTF" },
    { "Franklin Gothic Medium", "framd.ttf" },
    { "Franklin Gothic Demi", "FRADM.TTF" },
    { "Corbel", "corbel.ttf" },
    { "Corbel Bold", "corbelb.ttf" },
    { "Candara", "Candara.ttf" },
    { "Candara Bold", "Candarab.ttf" },
    { "Constantia", "constan.ttf" },
    { "Constantia Bold", "constanb.ttf" },
    { "Palatino Linotype", "pala.ttf" },
    { "Palatino Linotype Bold", "palab.ttf" },
    { "Georgia", "georgia.ttf" },
    { "Georgia Bold", "georgiab.ttf" },
    { "Garamond", "GARA.TTF" },
    { "Garamond Bold", "GARABD.TTF" },
    { "Sitka Text", "Sitka.ttc" },
    { "Sylfaen", "sylfaen.ttf" },
    { "Times New Roman", "times.ttf" },
    { "Times New Roman Bold", "timesbd.ttf" },
    { "Comic Sans MS", "comic.ttf" },
    { "Comic Sans MS Bold", "comicbd.ttf" },
    { "Impact", "impact.ttf" },
};

// FPS overlay rows, FpsOverlayRows lists their keys. A graph row follows its text row.
enum OverlayRow : int
{
    OverlayRow_Fps,
    OverlayRow_Upscale,
    OverlayRow_FrameTime,
    OverlayRow_FrameTimeGraph,
    OverlayRow_UpscalerTime,
    OverlayRow_UpscalerTimeGraph,
    OverlayRow_LowLatency,
    OverlayRow_COUNT,
};

template <> struct EnumConfig<OverlayRow>
{
    // Unknown keys are skipped
    static constexpr auto default_value = OverlayRow_COUNT;

    static constexpr std::pair<OverlayRow, std::string_view> mapping[] = {
        { OverlayRow_Fps, "fps" },
        { OverlayRow_Upscale, "upscale" },
        { OverlayRow_FrameTime, "frametime" },
        { OverlayRow_FrameTimeGraph, "frametimegraph" },
        { OverlayRow_UpscalerTime, "upscaler" },
        { OverlayRow_UpscalerTimeGraph, "upscalergraph" },
        { OverlayRow_LowLatency, "lowlatency" },
    };
};

static const char* overlayRowNames[] = {
    "FPS",           "Upscale Ratio",       "Frame Time",         "Frame Time Graph",
    "Upscaler Time", "Upscaler Time Graph", "Low Latency Timings"
};
static const char* overlayRowTips[] = {
    "The FPS, the frame generation and the upscaler, FPS Row below sets what's shown",
    "Render -> output resolution and upscale ratio, while upscaling",
    "The last frame time and the average",
    "The frame times of the last frames",
    "The upscaler's last time and the average",
    "The upscaler's times of the last frames",
    "Timings of the frame from the low latency markers, while there are any",
};

struct OverlayRowEntry
{
    int row;
    bool shown;
};

// An overlay type's rows from top to bottom (left to right when horizontal), the hidden ones too so Custom keeps
// their place
static std::vector<OverlayRowEntry> OverlayRows(FpsOverlay type)
{
    auto config = Config::Instance();

    if (type != FpsOverlay_Custom)
    {
        // The lowest type that shows the row, each type shows more than the one before
        static constexpr FpsOverlay shownFrom[] = {
            FpsOverlay_JustFPS, FpsOverlay_Simple,    FpsOverlay_Detailed,         FpsOverlay_DetailedGraph,
            FpsOverlay_Full,    FpsOverlay_FullGraph, FpsOverlay_LowLatencyTimings
        };
        static_assert(std::size(shownFrom) == OverlayRow_COUNT);

        std::vector<OverlayRowEntry> rows;

        for (int row = 0; row < OverlayRow_COUNT; row++)
        {
            bool shown = type >= shownFrom[row];

            if (row == OverlayRow_Upscale)
                shown = shown && config->FpsOverlayShowRatio.value_or_default();

            rows.push_back({ row, shown });
        }

        return rows;
    }

    if (!config->FpsOverlayRows.has_value())
        return OverlayRows(FpsOverlay_FullGraph);

    std::vector<OverlayRowEntry> rows;
    std::stringstream keys(config->FpsOverlayRows.value());
    std::string key;

    auto listed = [&](int row)
    { return std::any_of(rows.begin(), rows.end(), [row](const OverlayRowEntry& entry) { return entry.row == row; }); };

    while (std::getline(keys, key, ','))
    {
        std::string_view name = key;
        while (!name.empty() && std::isspace((unsigned char) name.front()))
            name.remove_prefix(1);
        while (!name.empty() && std::isspace((unsigned char) name.back()))
            name.remove_suffix(1);

        const bool shown = !name.starts_with('-');
        if (!shown)
            name.remove_prefix(1);

        if (const auto row = CodeToEnum<OverlayRow>(name); row != OverlayRow_COUNT && !listed(row))
            rows.push_back({ row, shown });
    }

    for (int row = 0; row < OverlayRow_COUNT; row++)
    {
        if (!listed(row))
            rows.push_back({ row, false });
    }

    return rows;
}

static void SetOverlayRows(const std::vector<OverlayRowEntry>& rows)
{
    std::string keys;

    for (const auto& entry : rows)
        keys += (keys.empty() ? "" : ",") + std::string(entry.shown ? "" : "-") + EnumToCode((OverlayRow) entry.row);

    Config::Instance()->FpsOverlayRows = keys;
}

static FpsOverlayFpsRow OverlayFpsRow(FpsOverlay type)
{
    switch (type)
    {
    case FpsOverlay_JustFPS:
        return FpsOverlayFpsRow_JustFps;

    case FpsOverlay_Simple:
        return FpsOverlayFpsRow_FrameTime;

    case FpsOverlay_Custom:
        return Config::Instance()->FpsOverlayFpsRowType.value_or_default();

    default:
        return FpsOverlayFpsRow_Average;
    }
}

// The digits each number of an FPS overlay value needs to keep the most it had this session, so values start as far
// left as they can and don't move back and forth as they change
static std::vector<int> OverlayValuePadding(const char* id, std::string_view text)
{
    static std::unordered_map<std::string, std::vector<int>> mostDigits;
    auto& most = mostDigits[id];

    std::vector<int> padding;
    size_t i = 0;

    while (i < text.size())
    {
        if (text[i] < '0' || text[i] > '9')
        {
            i++;
            continue;
        }

        const size_t start = i;
        while (i < text.size() && text[i] >= '0' && text[i] <= '9')
            i++;

        const int digits = static_cast<int>(i - start);
        const size_t number = padding.size();

        if (number == most.size())
            most.push_back(digits);
        else
            most[number] = std::max(most[number], digits);

        padding.push_back(most[number] - digits);
    }

    return padding;
}

// FPS overlay font, loaded between frames when FpsOverlayFont or its rasterizer settings change
static ImFont* gOverlayFont = nullptr;
static std::string gOverlayFontName;
static float gOverlayLetterSpacing = 0.0f;

static std::filesystem::path OverlayFontPath(const std::string& name)
{
    std::filesystem::path path(name);

    if (path.has_parent_path())
        return path;

    static const std::filesystem::path fontsDir = []()
    {
        wchar_t windowsDir[MAX_PATH] {};
        GetWindowsDirectoryW(windowsDir, MAX_PATH);
        return std::filesystem::path(windowsDir) / L"Fonts";
    }();

    return fontsDir / path;
}

static void UpdateOverlayFont()
{
    auto config = Config::Instance();
    auto wanted = config->FpsOverlayFont.value_or("");
    const float letterSpacing = config->FpsOverlayLetterSpacing.value_or_default();

    if (wanted == gOverlayFontName && std::abs(letterSpacing - gOverlayLetterSpacing) < 0.001f)
    {
        return;
    }

    gOverlayFontName = wanted;
    gOverlayLetterSpacing = letterSpacing;

    auto& io = ImGui::GetIO();

    if (gOverlayFont != nullptr)
    {
        io.Fonts->RemoveFont(gOverlayFont);
        gOverlayFont = nullptr;
    }

    if (wanted.empty() && std::abs(letterSpacing) < 0.01f)
        return;

    // The menu keeps its font, the first one added would become the default
    if (io.FontDefault == nullptr)
        io.FontDefault = io.Fonts->Fonts.empty() ? io.Fonts->AddFontDefault() : io.Fonts->Fonts[0];

    ImFontConfig fontConfig;
    fontConfig.GlyphExtraAdvanceX = letterSpacing;

    if (!wanted.empty())
    {
        auto path = OverlayFontPath(wanted);

        if (!std::filesystem::exists(path))
        {
            LOG_WARN("FPS overlay font not found: {}", wstring_to_string(path.wstring()));
            return;
        }

        gOverlayFont = io.Fonts->AddFontFromFileTTF(wstring_to_string(path.wstring()).c_str(), fontSize, &fontConfig);
    }
    else if (config->UseHQFont.value_or_default())
    {
        if (config->TTFFontPath.has_value())
        {
            gOverlayFont = io.Fonts->AddFontFromFileTTF(wstring_to_string(config->TTFFontPath.value()).c_str(),
                                                        fontSize, &fontConfig, io.Fonts->GetGlyphRangesDefault());
        }
        else
        {
            gOverlayFont = io.Fonts->AddFontFromMemoryCompressedBase85TTF(hack_compressed_compressed_data_base85,
                                                                          fontSize, &fontConfig);
        }
    }
}

// FPS overlay colors are stored as 0xRRGGBB
static ImVec4 RgbToColor(uint32_t rgb)
{
    return ImVec4(((rgb >> 16) & 0xFF) / 255.0f, ((rgb >> 8) & 0xFF) / 255.0f, (rgb & 0xFF) / 255.0f, 1.0f);
}

static uint32_t ColorToRgb(const float* color)
{
    auto channel = [](float c) { return (uint32_t) std::lround(std::clamp(c, 0.0f, 1.0f) * 255.0f); };
    return (channel(color[0]) << 16) | (channel(color[1]) << 8) | channel(color[2]);
}

struct FsExistsCache
{
    std::wstring lastPath;
    bool cached { false };
    std::chrono::steady_clock::time_point nextRefresh { std::chrono::steady_clock::time_point::min() };
    std::chrono::milliseconds interval { 2000 };

    bool Get(const std::filesystem::path& path)
    {
        auto now = std::chrono::steady_clock::now();
        if (path != lastPath || now >= nextRefresh)
        {
            lastPath = path;
            cached = std::filesystem::exists(path);
            nextRefresh = now + interval;
        }
        return cached;
    }
};

static FsExistsCache nukemsExists;
static FsExistsCache enablerExists;

struct FlagDefinition
{
    std::string name;
    uint32_t mask;
    std::string description;
};

inline std::string StrFmt(const char* fmt, ...)
{
    va_list args;
    va_start(args, fmt);
    int len = std::vsnprintf(nullptr, 0, fmt, args);
    va_end(args);
    std::string out(len, '\0');
    va_start(args, fmt);
    std::vsnprintf(out.data(), len + 1, fmt, args);
    va_end(args);
    return out;
}

static std::string NameWithVersion(IFeature* feature)
{
    const auto version = feature->Version();
    return StrFmt("%s %u.%u.%u", feature->ShortName().c_str(), version.major, version.minor, version.patch);
}

// CARDS -----------------------------
static void DrawCardTitle(const char* label)
{
    ImGui::TextUnformatted(label, ImGui::FindRenderedTextEnd(label));

    // Spacing below is not an item the tooltips should attach to
    const ImRect titleRect(ImGui::GetItemRectMin(), ImGui::GetItemRectMax());
    const ImGuiID titleId = ImGui::GetItemID();

    ImGui::Spacing();

    // HoveredRect has to reflect the mouse, IsItemHovered trusts it
    const ImGuiItemStatusFlags hovered =
        ImGui::IsMouseHoveringRect(titleRect.Min, titleRect.Max) ? ImGuiItemStatusFlags_HoveredRect : 0;
    ImGui::SetLastItemData(titleId, 0, hovered, titleRect);
}

// Used instead of ImGui::SeparatorText. The first one in a card becomes the title of the card
static void SectionTitle(const char* label)
{
    if (!ScopedCard::TryTitle(label))
        ImGui::SeparatorText(label);
}

ScopedCard::ScopedCard(const char* title)
{
    // Cards don't nest, an inner one only gives a title to the outer one
    if (_current != nullptr)
    {
        if (title != nullptr)
            SectionTitle(title);

        return;
    }

    const ImGuiStyle& style = ImGui::GetStyle();

    _window = ImGui::GetCurrentWindow();
    _pad = ImVec2(style.WindowPadding.x, style.WindowPadding.y * 0.75f);
    _min = _window->DC.CursorPos;
    _width = ImGui::GetContentRegionAvail().x;

    // Content goes to the channel 1, the background is added to the channel 0 once the height is known
    _splitter.Split(_window->DrawList, 2);
    _splitter.SetCurrentChannel(_window->DrawList, 1);

    _window->DC.CursorPos.y += _pad.y;
    ImGui::Indent(_pad.x);

    // GetContentRegionAvail (used by child windows) reads ContentRegionRect, so both need the padding
    _oldWorkRectMaxX = _window->WorkRect.Max.x;
    _oldContentRectMaxX = _window->ContentRegionRect.Max.x;
    _window->WorkRect.Max.x -= _pad.x;
    _window->ContentRegionRect.Max.x -= _pad.x;

    _contentStartY = _window->DC.CursorPos.y;
    _active = true;
    _current = this;

    if (title != nullptr)
        DrawTitle(title);
}

void ScopedCard::DrawTitle(const char* label)
{
    // Title sits on the top border, like a group box
    _window->DC.CursorPos.y = _min.y;
    DrawCardTitle(label);

    _titleMin = ImGui::GetItemRectMin();
    _titleMax = ImGui::GetItemRectMax();
    _hasTitle = true;
}

ScopedCard::~ScopedCard()
{
    if (!_active)
        return;

    _current = nullptr;

    const ImGuiStyle& style = ImGui::GetStyle();
    ImDrawList* drawList = _window->DrawList;

    ImGui::Unindent(_pad.x);
    _window->WorkRect.Max.x = _oldWorkRectMaxX;
    _window->ContentRegionRect.Max.x = _oldContentRectMaxX;

    // Nothing was drawn, skip the rectangle
    if (_window->DC.CursorPos.y <= _contentStartY)
    {
        _splitter.Merge(drawList);
        _window->DC.CursorPos.y = _min.y;
        return;
    }

    const float bottom = _window->DC.CursorPos.y - style.ItemSpacing.y + _pad.y;
    const ImVec2 max(_min.x + _width, bottom);

    _splitter.SetCurrentChannel(drawList, 0);

    const ImU32 borderColor = ImGui::GetColorU32(ImGuiCol_Separator);

    if (_hasTitle)
    {
        // Same half pixel offsets as AddRect. Top line goes through the middle of the title, with a gap for it
        const float r = style.ChildRounding;
        const float gap = style.ItemInnerSpacing.x;
        const ImVec2 a(_min.x + 0.5f, std::floor((_titleMin.y + _titleMax.y) * 0.5f) + 0.5f);
        const ImVec2 b(max.x - 0.49f, max.y - 0.49f);

        drawList->PathLineTo(ImVec2(_titleMax.x + gap, a.y));
        drawList->PathArcToFast(ImVec2(b.x - r, a.y + r), r, 9, 12);
        drawList->PathArcToFast(ImVec2(b.x - r, b.y - r), r, 0, 3);
        drawList->PathArcToFast(ImVec2(a.x + r, b.y - r), r, 3, 6);
        drawList->PathArcToFast(ImVec2(a.x + r, a.y + r), r, 6, 9);
        drawList->PathLineTo(ImVec2(_titleMin.x - gap, a.y));
        drawList->PathStroke(borderColor, ImDrawFlags_None, 1.0f);
    }
    else
    {
        drawList->AddRect(_min, max, borderColor, style.ChildRounding);
    }

    _splitter.Merge(drawList);

    // Registers the size of the card and leaves a gap before the next one
    _window->DC.CursorPos.y = bottom;
    ImGui::Dummy(ImVec2(0.0f, 0.0f));
}

bool ScopedCard::TryTitle(const char* label)
{
    if (_current == nullptr || _current->_hasTitle)
        return false;

    // Only when it's the very first thing in the card
    if (ImGui::GetCurrentWindow() != _current->_window || _current->_window->DC.CursorPos.y != _current->_contentStartY)
        return false;

    _current->DrawTitle(label);

    return true;
}

bool ScopedCard::Shows(const char* id)
{
    // Inside of a card everything belongs to that card
    return _filter == nullptr || _current != nullptr || strcmp(_filter, id) == 0;
}

void ScopedCard::SetFilter(const char* id) { _filter = id; }

void MenuCommon::RenderCard(RenderMenuContext& ctx, void (*render)(RenderMenuContext&))
{
    ScopedCard card;
    render(ctx);
}

void MenuCommon::UpdateManualInput(HWND targetHwnd)
{
    OptiInput::BeginFrame(targetHwnd);

    const auto config = Config::Instance();

    auto CheckShortcut = [&](int vk, bool& inputFlag, const char* logMessage)
    {
        if (inputFlag)
            return;

        if (vk <= 0 || vk >= 256)
            return;

        if (OptiInput::IsKeyReleased(vk))
        {
            lastKey = vk;
            // receivingWmInputs = false;
            inputFlag = true;
            LOG_DEBUG("{}", logMessage);
        }
    };

    const auto currentTick = GetTickCount64();
    const bool canAcceptInputs = lastInputTick + debounceThreshold < currentTick;

    if (!capturingKey && canAcceptInputs)
    {
        CheckShortcut(config->ShortcutKey.value_or_default(), inputMenu, LOC(UI_Menu_key_pressed_will_be_switching_menu));
        CheckShortcut(config->FpsShortcutKey.value_or_default(), inputFps, LOC(UI_Menu_key_pressed_will_be_switching_FPS));
        CheckShortcut(config->FGShortcutKey.value_or_default(), inputFG, LOC(UI_Menu_key_pressed_will_be_switching_FG_mode));
        CheckShortcut(config->FpsCycleShortcutKey.value_or_default(), inputFpsCycle,
                      LOC(UI_Menu_key_pressed_will_be_switching_FPS_mode));
    }
    else if (capturingKey)
    {
        lastInputTick = currentTick;
    }

    lastKey = OptiInput::GetLastPressedKey();
}

void MenuCommon::ShowTooltip(const char* tip)
{
    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
    {
        ImGui::BeginTooltip();
        ImGui::Text(tip);
        ImGui::EndTooltip();
    }
}

template <typename TOption>
bool MenuCommon::ConfigCheckbox(const char* label, TOption& option, const char* tip, bool needsRestart)
{
    bool value = option.value_or_default();
    const bool changed = ImGui::Checkbox(label, &value);

    if (changed)
        option = value;

    if (tip != nullptr)
    {
        if (needsRestart)
            ShowTooltip((std::string(tip) + "\n\nAfter changing this option, please Save Settings.\n"
                                            "It will be applied on next launch.")
                            .c_str());
        else
            ShowTooltip(tip);
    }

    return changed;
}

void MenuCommon::ShowResetButton(CustomOptional<bool, NoDefault>* initFlag, std::string buttonName)
{
    ImGui::SameLine();

    ImGui::BeginDisabled(!initFlag->has_value());

    if (ImGui::Button(buttonName.c_str()))
    {
        initFlag->reset();
        ReInitUpscaler();
    }

    ImGui::EndDisabled();
}

inline void MenuCommon::ReInitUpscaler()
{
    if (!State::Instance().currentFeature)
        return;

    // currentBackend is only updated while the menu is drawn, ray reconstruction keeps its DLSSD
    auto feature = State::Instance().currentFeature;
    State::Instance().newBackend =
        State::Instance().rrHandles.contains(feature->Handle()->Id) ? Upscaler::Reset : feature->GetUpscalerType();

    MARK_ALL_BACKENDS_CHANGED();
}

void MenuCommon::SeparatorWithHelpMarker(const char* label, const char* tip)
{
    SectionTitle(label);
    ShowTooltip(tip);
}

bool MenuCommon::SliderUInt(const char* label, uint32_t* v, uint32_t v_min, uint32_t v_max, const char* format,
                            ImGuiSliderFlags flags)
{
    return ImGui::SliderScalar(label, ImGuiDataType_U32, v, &v_min, &v_max, format, flags);
}

class Keybind
{
    std::string name;
    int id;
    bool waitingForKey = false;

  public:
    Keybind(std::string name, int id) : name(name), id(id) {}

    static std::string KeyNameFromVirtualKeyCode(USHORT virtualKey)
    {
        if (virtualKey == (USHORT) UnboundKey)
            return LOC(UI_Unbound);

        UINT scanCode = MapVirtualKeyW(virtualKey, MAPVK_VK_TO_VSC);

        // Keys like Home would display as Num 0 without this fix
        switch (virtualKey)
        {
        case VK_INSERT:
        case VK_DELETE:
        case VK_HOME:
        case VK_END:
        case VK_PRIOR:
        case VK_NEXT:
        case VK_LEFT:
        case VK_RIGHT:
        case VK_UP:
        case VK_DOWN:
        case VK_NUMLOCK:
        case VK_DIVIDE:
        case VK_RCONTROL:
        case VK_RMENU:
            scanCode |= 0xE000;
            break;
        }

        LONG lParam = (scanCode & 0xFF) << 16;
        if (scanCode & 0xE000)
            lParam |= 1 << 24;

        wchar_t buf[64] = {};
        if (GetKeyNameTextW(lParam, buf, static_cast<int>(std::size(buf))) != 0)
            return wstring_to_string(buf);

        return LOC(UI_Unknown);
    }

    void Render(CustomOptional<int>& configKey)
    {
        ImGui::PushID(id);
        if (ImGui::Button(name.c_str()))
        {
            waitingForKey = true;
            capturingKey = true;
            lastKey = 0;
        }
        ImGui::PopID();

        if (waitingForKey)
        {
            ImGui::SameLine();
            ImGui::Text(LOC(TXT_Press_Any_Key_0));

            if (lastKey == 0 || lastKey == VK_LBUTTON || lastKey == VK_RBUTTON || lastKey == VK_MBUTTON)
                return;

            if (lastKey == VK_ESCAPE)
            {
                waitingForKey = false;
                capturingKey = false;
                return;
            }

            if (lastKey == VK_BACK)
                lastKey = UnboundKey;

            configKey = lastKey;
            waitingForKey = false;
            capturingKey = false;
            return;
        }

        ImGui::SameLine();
        ImGui::Text(KeyNameFromVirtualKeyCode(configKey.value_or_default()).c_str());

        ImGui::SameLine();
        ImGui::PushID(id);
        if (ImGui::Button("R"))
        {
            configKey.reset();
        }
        ImGui::PopID();
    }
};

Upscaler MenuCommon::GetBackendCode(const API api)
{
    if (auto feature = State::Instance().currentFeature)
        return feature->GetUpscalerType();

    Upscaler upscaler;

    if (api == DX11)
        upscaler = Config::Instance()->Dx11Upscaler.value_or_default();
    else if (api == DX12)
        upscaler = Config::Instance()->Dx12Upscaler.value_or_default();
    else
        upscaler = Config::Instance()->VulkanUpscaler.value_or_default();

    return upscaler;
}

void MenuCommon::GetCurrentBackendInfo(const API api, Upscaler& upscaler, std::string* name)
{
    upscaler = GetBackendCode(api);
    *name = UpscalerDisplayName(upscaler, api);
}

// Backend that recreates the current feature, ray reconstruction keeps its DLSSD and the upscalers their own
static Upscaler SameBackend(Upscaler currentBackend)
{
    auto feature = State::Instance().currentFeature;

    if (feature != nullptr && State::Instance().rrHandles.contains(feature->Handle()->Id))
        return Upscaler::Reset;

    return currentBackend;
}

// Loaded custom FFX upscaler, nullptr for the regular ones. Only native Vulkan FFX runs the Vulkan dll.
static CustomFfxModule* SelectedCustomFfx(const std::string& name, Upscaler upscaler, API api)
{
    if (name.empty())
        return nullptr;

    return api == API::Vulkan && upscaler == Upscaler::FFX ? FfxApiProxy::FindCustomUpscalerVk(name)
                                                           : FfxApiProxy::FindCustomUpscalerDx12(name);
}

// List of upscalers reported by FFX for a given backend, nullptr if the backend doesn't use FFX
static State::FfxVersionList* GetFfxVersionList(Upscaler upscaler, API api)
{
    if (upscaler == Upscaler::FFX_on12 || (upscaler == Upscaler::FFX && api == API::DX12))
        return &State::Instance().ffxUpscalerVersionsDx12;

    if (upscaler == Upscaler::FFX && api == API::Vulkan)
        return &State::Instance().ffxUpscalerVersionsVk;

    return nullptr;
}

static bool IsOn12(Upscaler upscaler)
{
    switch (upscaler)
    {
    case Upscaler::XeSS_on12:
    case Upscaler::FSR21_on12:
    case Upscaler::FSR22_on12:
    case Upscaler::FFX_on12:
    case Upscaler::DLSS_on12:
        return true;
    default:
        return false;
    }
}

// Name without the w/Dx12 suffix, that one is shown as a tag in the list instead
static std::string ShortUpscalerName(Upscaler upscaler, API api, int ffxIndex)
{
    if (auto versions = GetFfxVersionList(upscaler, api);
        versions != nullptr && ffxIndex >= 0 && ffxIndex < versions->names.size())
        return StrFmt(LOC(UI_FSR_s), versions->names[ffxIndex]);

    switch (upscaler)
    {
    case Upscaler::XeSS_on12:
        return UpscalerDisplayName(Upscaler::XeSS);
    case Upscaler::FSR21_on12:
        return UpscalerDisplayName(Upscaler::FSR21);
    case Upscaler::FSR22_on12:
        return UpscalerDisplayName(Upscaler::FSR22);
    case Upscaler::FFX_on12:
        return UpscalerDisplayName(Upscaler::FFX, API::DX12);
    case Upscaler::DLSS_on12:
        return UpscalerDisplayName(Upscaler::DLSS);
    default:
        return UpscalerDisplayName(upscaler, api);
    }
}

static feature_version UpscalerSortVersion(Upscaler upscaler)
{
    switch (upscaler)
    {
    case Upscaler::FSR21:
    case Upscaler::FSR21_on12:
        return { 2, 1, 2 };
    case Upscaler::FSR22:
    case Upscaler::FSR22_on12:
        return { 2, 2, 1 };
    case Upscaler::FSR31:
        return { 3, 1, 2 };
    case Upscaler::FFX:
    case Upscaler::FFX_on12:
        return { 3, 1, 0 };
    default:
        return {};
    }
}

// Indices into the FFX version list, newest first
static std::vector<int> FfxVersionsNewestFirst(const State::FfxVersionList& versions)
{
    std::vector<int> order(versions.names.size());
    std::iota(order.begin(), order.end(), 0);

    std::stable_sort(order.begin(), order.end(), [&versions](int a, int b)
                     { return feature_version(versions.names[b]) < feature_version(versions.names[a]); });

    return order;
}

void MenuCommon::RenderUpscalerCombo(const API api, Upscaler currentUpscaler, const std::vector<Upscaler>& options)
{
    auto primaryGpu = IdentifyGpu::getPrimaryGpu();
    auto config = Config::Instance();
    const int ffxIndex = config->FfxUpscalerIndex.value_or_default();

    Upscaler targetBackend = State::Instance().newBackend;
    if (targetBackend == Upscaler::Reset)
        targetBackend = currentUpscaler;

    // FSR names already include the version, others only know theirs once running
    auto upscalerName = [&](Upscaler upscaler, int index)
    {
        auto feature = State::Instance().currentFeature;

        if (upscaler == currentUpscaler && !IsFsr(upscaler) && feature != nullptr)
            return NameWithVersion(feature);

        return ShortUpscalerName(upscaler, api, index);
    };

    const auto currentCustom = CustomProviders::Selected(api, currentUpscaler);

    // Interop is only shown here, the list uses a tag for it
    auto selectedName = upscalerName(targetBackend, ffxIndex);
    if (auto custom = CustomProviders::Selected(api, targetBackend); !custom.empty())
        selectedName = custom;
    else if (IsOn12(targetBackend))
        selectedName += LOC(UI_Dx12);

    if (!ImGui::BeginCombo(LOC(UI_UpscalerCombo), selectedName.c_str(), ImGuiComboFlags_HeightLargest))
        return;

    struct Entry
    {
        Upscaler upscaler;
        int ffxIndex; // Newest upscaler reported by FFX, -1 when not using FFX
        feature_version version;
        std::string custom; // Custom FFX or DLSS upscaler name
    };

    std::vector<Entry> entries;

    for (auto opt : options)
    {
        // Check if GPU is capable of a given backend
        if ((opt == Upscaler::DLSS || opt == Upscaler::DLSS_on12) && !primaryGpu.dlssCapable)
            continue;

        // Not all Intel GPUs support native DX11 XeSS but don't think we have a good way to check exactly
        if (opt == Upscaler::XeSS && api == API::DX11 && primaryGpu.vendorId != VendorId::Intel)
            continue;

        // Only the newest FFX upscaler is listed here, the rest are in the FFX Upscaler combo
        if (auto versions = GetFfxVersionList(opt, api); versions != nullptr)
        {
            if (versions == &State::Instance().ffxUpscalerVersionsVk)
                FFXFeature::EnsureVersionsVulkan();
            else
                FFXFeature::EnsureVersionsDx12();

            // Fall back to a generic entry if FFX couldn't be queried
            if (!versions->names.empty())
            {
                // Labelled with the remembered upscaler, sorted by the newest one so picking another FFX version
                // doesn't move the entry
                auto newest = FfxVersionsNewestFirst(*versions)[0];
                auto index = ffxIndex >= 0 && ffxIndex < versions->names.size() ? ffxIndex : newest;
                entries.push_back({ opt, index, feature_version(versions->names[newest]) });
                continue;
            }
        }

        entries.push_back({ opt, -1, UpscalerSortVersion(opt) });
    }

    // Custom ones are sorted after the regular ones, their names don't need versions
    // DLSS ones don't need a DLSS capable GPU
    std::vector<std::string> customNames;

    for (auto kind : { CustomProviders::Kind::Ffx, CustomProviders::Kind::FfxVk, CustomProviders::Kind::Dlss })
    {
        for (auto& name : CustomProviders::List(kind))
        {
            if (!CustomProviders::Contains(customNames, name))
                customNames.push_back(name);
        }
    }

    for (auto& name : customNames)
    {
        for (auto upscaler : CustomProviders::Upscalers(name, api))
        {
            const bool ffx = upscaler == Upscaler::FFX || upscaler == Upscaler::FFX_on12;
            entries.push_back({ upscaler, ffx ? config->FfxUpscalerIndexFor(name).value_or_default() : -1, {}, name });
        }
    }

    // Upscaler of the GPU vendor goes first, FSRs are sorted by version, newest first
    // Stable sort keeps native ahead of w/Dx12
    auto group = [&primaryGpu](Upscaler upscaler)
    {
        int family = IsFsr(upscaler) ? 1 : (upscaler == Upscaler::DLSS || upscaler == Upscaler::DLSS_on12) ? 2 : 0;

        const int vendorFamily = primaryGpu.vendorId == VendorId::AMD      ? 1
                                 : primaryGpu.vendorId == VendorId::Nvidia ? 2
                                                                           : 0;

        return family == vendorFamily ? -1 : family;
    };

    std::stable_sort(entries.begin(), entries.end(),
                     [&group](const Entry& a, const Entry& b)
                     {
                         if (group(a.upscaler) != group(b.upscaler))
                             return group(a.upscaler) < group(b.upscaler);

                         return b.version < a.version;
                     });

    for (size_t i = 0; i < entries.size(); i++)
    {
        const auto& entry = entries[i];

        if (i > 0 && group(entry.upscaler) != group(entries[i - 1].upscaler))
            ImGui::Separator();

        auto name = entry.custom.empty() ? upscalerName(entry.upscaler, entry.ffxIndex) : entry.custom;
        auto label = StrFmt(LOC(UI_s_d_s), name.c_str(), (int) entry.upscaler, entry.custom.c_str());
        // Folder names in Dx12Upscaler aren't case sensitive
        const bool selected =
            currentUpscaler == entry.upscaler && _stricmp(currentCustom.c_str(), entry.custom.c_str()) == 0;

        if (ImGui::Selectable(label.c_str(), selected))
        {
            State::Instance().newBackend = entry.upscaler;

            // Regular and custom entries share an upscaler type, which the backend change check compares,
            // so a switch between them is marked here. A regular entry always clears the custom name, which the
            // ini would otherwise save in place of it.
            if (auto& customSetting = CustomProviders::NameSetting(api);
                !selected && entry.custom != customSetting.value_or(""))
            {
                const bool typeUnchanged =
                    !entry.custom.empty() || CustomProviders::Runs(customSetting.value_or(""), api, entry.upscaler);

                if (entry.custom.empty())
                    customSetting.reset();
                else
                    customSetting = entry.custom;

                if (typeUnchanged)
                    MARK_ALL_BACKENDS_CHANGED();
            }
        }

        // Dimmed tags aligned to the right
        const char* tag = nullptr;

        if (entry.ffxIndex >= 0)
            tag = IsOn12(entry.upscaler) ? LOC(UI_FFX_Dx12) : "FFX";
        else if (!entry.custom.empty())
            tag = IsOn12(entry.upscaler) ? LOC(UI_DLSS_Dx12) : "DLSS";
        else if (IsOn12(entry.upscaler))
            tag = "Dx12";

        if (tag != nullptr)
        {
            auto& style = ImGui::GetStyle();

            ImGui::SameLine();
            ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowWidth() - style.WindowPadding.x -
                                                                      ImGui::CalcTextSize(tag).x));
            ImGui::TextDisabled(tag);
        }
    }

    ImGui::EndCombo();
}

void MenuCommon::AddDx11Backends(Upscaler upscaler)
{
    RenderUpscalerCombo(API::DX11, upscaler,
                        { Upscaler::XeSS, Upscaler::FSR22, Upscaler::FSR31, Upscaler::XeSS_on12, Upscaler::FSR21_on12,
                          Upscaler::FSR22_on12, Upscaler::FFX_on12, Upscaler::DLSS, Upscaler::DLSS_on12 });
}

void MenuCommon::AddDx12Backends(Upscaler upscaler)
{
    RenderUpscalerCombo(API::DX12, upscaler,
                        { Upscaler::XeSS, Upscaler::FSR21, Upscaler::FSR22, Upscaler::FFX, Upscaler::DLSS });
}

void MenuCommon::AddVulkanBackends(Upscaler upscaler)
{
    RenderUpscalerCombo(API::Vulkan, upscaler,
                        { Upscaler::XeSS, Upscaler::FSR21, Upscaler::FSR22, Upscaler::FFX, Upscaler::FSR21_on12,
                          Upscaler::FFX_on12, Upscaler::DLSS, Upscaler::DLSS_on12 });
}

// Upscaler for the game's ray reconstruction: DLSSDs first, NVIDIA's then custom ones, regular upscalers after.
// Feature is the running one, nullptr before the game starts ray reconstruction
static void RenderRRUpscalerCombo(API api, IFeature* feature, const char* label = LOC(UI_RRUpscalerCombo))
{
    auto& state = State::Instance();
    auto config = Config::Instance();

    // Empty for NVIDIA's, custom ones until they turn out not to run on the api
    std::vector<std::string> entries;

    if (Nvngx_RR::NativeAvailable())
        entries.push_back({});

    for (auto& name : CustomProviders::List(CustomProviders::Kind::Dlssd))
    {
        if (Nvngx_RR::Listable(name, api))
            entries.push_back(name);
    }

    auto entryName = [](const std::string& entry) { return entry.empty() ? std::string("DLSSD") : entry; };

    // Regular upscalers take ray reconstruction without denoising it, when allowed
    const bool allowRegulars = config->RRAllowRegularUpscalers.value_or_default();
    std::vector<Upscaler> regulars;

    if (allowRegulars)
    {
        regulars = { Upscaler::XeSS, Upscaler::FSR21, Upscaler::FSR22, Upscaler::FFX };

        if (IdentifyGpu::getPrimaryGpu().dlssCapable)
            regulars.push_back(Upscaler::DLSS);
    }

    const auto picked = Nvngx_RR::Regular();

    // Running: the FSR fallback isn't one of them. Not running yet: the configured one, a DLSSD unless a regular
    // upscaler is picked, Off when no DLSSD can run
    auto* dlssd = dynamic_cast<DLSSDFeature*>(feature);
    const bool runsDlssd = feature != nullptr ? dlssd != nullptr : !picked.has_value() && !entries.empty();
    const auto current = dlssd != nullptr ? dlssd->CustomName() : config->CustomRRUpscaler.value_or("");

    std::string preview;

    if (runsDlssd)
        preview = entryName(current);
    else if (feature != nullptr)
        preview = NameWithVersion(feature);
    else if (picked.has_value())
        preview = UpscalerDisplayName(picked.value(), api);
    else
        preview = LOC(UI_Off);

    if (!ImGui::BeginCombo(label, preview.c_str(), ImGuiComboFlags_HeightLargest))
        return;

    auto changeAll = [&state]()
    {
        Nvngx_RR::ResetFailures();

        for (auto handle : state.rrHandles)
            state.changeBackend[handle] = true;
    };

    for (const auto& entry : entries)
    {
        // Folder names in RRUpscaler aren't case sensitive
        const bool selected = runsDlssd && _stricmp(current.c_str(), entry.c_str()) == 0;
        auto label = StrFmt(LOC(UI_s_rr_s), entryName(entry).c_str(), entry.c_str());

        if (ImGui::Selectable(label.c_str(), selected) && !selected)
        {
            // NVIDIA's is auto, it's picked when the gpu can run it
            if (entry.empty())
                config->CustomRRUpscaler.reset();
            else
                config->CustomRRUpscaler = entry;

            config->RRRegularUpscaler.reset();
            changeAll();
        }
    }

    // Without a DLSSD ray reconstruction is only offered to the game for a regular upscaler
    if (allowRegulars && entries.empty())
    {
        if (ImGui::Selectable(LOC(UI_Off_rrOff), !picked.has_value()))
            config->RRRegularUpscaler.reset();
    }

    if (allowRegulars)
        ImGui::Separator();

    for (auto upscaler : regulars)
    {
        const bool selected = !runsDlssd && picked == upscaler;
        auto label = StrFmt(LOC(UI_s_rrRegular_d), UpscalerDisplayName(upscaler, api).c_str(), (int) upscaler);

        if (ImGui::Selectable(label.c_str(), selected) && !selected)
        {
            config->RRRegularUpscaler = upscaler;
            changeAll();
        }

        ImGui::SameLine();
        ImGui::SetCursorPosX(std::max(ImGui::GetCursorPosX(), ImGui::GetWindowWidth() -
                                                                  ImGui::GetStyle().WindowPadding.x -
                                                                  ImGui::CalcTextSize(LOC(UI_No_denoise)).x));
        ImGui::TextDisabled(LOC(UI_No_denoise));
    }

    ImGui::EndCombo();
}

static ImVec4 StatusWarn();

// Ray reconstruction's upscaler before the game starts it. Without a DLSSD the game checks at start whether it's
// offered, so changing that needs a restart
static void RenderRROfferCombo(API api)
{
    const auto picked = Nvngx_RR::Regular();
    static const bool pickedAtStart = picked.has_value();
    const bool dlssdAvailable = Nvngx_RR::DlssdAvailable();

    RenderRRUpscalerCombo(api, nullptr, LOC(UI_Ray_Reconstruction_RROfferCombo));

    if (ImGui::IsItemHovered())
    {
        if (dlssdAvailable)
            ImGui::SetTooltip(LOC(UI_Upscaler_used_when_Ray_Reconstruction_is_turned_on_in_the_ga));
        else
            ImGui::SetTooltip("No DLSSD can run on this GPU, so the game doesn't offer Ray Reconstruction.\n"
                              "Picking an upscaler here lets you turn it on in the game.");
    }

    if (picked.has_value())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, StatusWarn());
        ImGui::TextWrapped(LOC(UI_This_upscaler_can_t_denoise_nWith_DLSSD_on_in_the_game_image));
        ImGui::PopStyleColor();
    }

    if (!dlssdAvailable && picked.has_value() != pickedAtStart)
    {
        ImGui::PushStyleColor(ImGuiCol_Text, StatusWarn());
        ImGui::TextWrapped(LOC(UI_Save_Settings_and_restart_the_game_to_apply));
        ImGui::PopStyleColor();
    }
}

template <HasDefaultValue B> void MenuCommon::AddResourceBarrier(std::string name, CustomOptional<int32_t, B>* value)
{
    const char* states[] = { "AUTO",
                             "COMMON",
                             "VERTEX_AND_CONSTANT_BUFFER",
                             "INDEX_BUFFER",
                             "RENDER_TARGET",
                             "UNORDERED_ACCESS",
                             "DEPTH_WRITE",
                             "DEPTH_READ",
                             "NON_PIXEL_SHADER_RESOURCE",
                             "PIXEL_SHADER_RESOURCE",
                             "STREAM_OUT",
                             "INDIRECT_ARGUMENT",
                             "COPY_DEST",
                             "COPY_SOURCE",
                             "RESOLVE_DEST",
                             "RESOLVE_SOURCE",
                             "RAYTRACING_ACCELERATION_STRUCTURE",
                             "SHADING_RATE_SOURCE",
                             "GENERIC_READ",
                             "ALL_SHADER_RESOURCE",
                             "PRESENT",
                             "PREDICATION",
                             "VIDEO_DECODE_READ",
                             "VIDEO_DECODE_WRITE",
                             "VIDEO_PROCESS_READ",
                             "VIDEO_PROCESS_WRITE",
                             "VIDEO_ENCODE_READ",
                             "VIDEO_ENCODE_WRITE" };
    const int values[] = { -1,  0,   1,     2,      4,      8,      16,      32,       64,   128,
                           256, 512, 1024,  2048,   4096,   8192,   4194304, 16777216, 2755, 192,
                           0,   310, 65536, 131072, 262144, 524288, 2097152, 8388608 };

    int selected = value->value_or(-1);

    const char* selectedName = "";

    for (int n = 0; n < 28; n++)
    {
        if (values[n] == selected)
        {
            selectedName = states[n];
            break;
        }
    }

    if (ImGui::BeginCombo(name.c_str(), selectedName))
    {
        if (ImGui::Selectable(states[0], !value->has_value()))
            value->reset();

        for (int n = 1; n < 28; n++)
        {
            if (ImGui::Selectable(states[n], selected == values[n]))
                *value = values[n];
        }

        ImGui::EndCombo();
    }
}

static uint32_t GetPresetIndex(IFeature* feature, bool dlssd = false)
{
    auto ratio = (float) feature->TargetWidth() / (float) feature->RenderWidth();

    if (!dlssd)
    {
        if (State::Instance().dlssPresetsOverridenByOpti)
        {
            LOG_DEBUG("DLSS Presets overridden by Opti, using Opti preset indices with ratio: {}", ratio);

            if (ratio <= (Config::Instance()->QualityRatio_UltraPerformance.value_or_default() + 0.01f))
            {
                return Config::Instance()->RenderPresetForAll.value_or(
                    Config::Instance()->RenderPresetUltraPerformance.value_or_default());
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Performance.value_or_default() + 0.01f))
            {
                return Config::Instance()->RenderPresetForAll.value_or(
                    Config::Instance()->RenderPresetPerformance.value_or_default());
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Balanced.value_or_default() + 0.01f))
            {
                return Config::Instance()->RenderPresetForAll.value_or(
                    Config::Instance()->RenderPresetBalanced.value_or_default());
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Quality.value_or_default() + 0.01f))
            {
                return Config::Instance()->RenderPresetForAll.value_or(
                    Config::Instance()->RenderPresetQuality.value_or_default());
            }
            else if (ratio <= (Config::Instance()->QualityRatio_UltraQuality.value_or_default() + 0.01f))
            {
                return Config::Instance()->RenderPresetForAll.value_or(
                    Config::Instance()->RenderPresetUltraQuality.value_or_default());
            }
            else
            {
                return Config::Instance()->RenderPresetForAll.value_or(
                    Config::Instance()->RenderPresetDLAA.value_or_default());
            }
        }
        else if (State::Instance().dlssPresetsOverriddenExternally)
        {
            LOG_DEBUG("DLSS Presets overridden externally, using external preset index: {}",
                      State::Instance().dlssRenderPresetExternal);

            return State::Instance().dlssRenderPresetExternal;
        }
        else
        {
            if (ratio <= (Config::Instance()->QualityRatio_UltraPerformance.value_or_default() + 0.01f))
            {
                return State::Instance().dlssRenderPresetUltraPerformance;
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Performance.value_or_default() + 0.01f))
            {
                return State::Instance().dlssRenderPresetPerformance;
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Balanced.value_or_default() + 0.01f))
            {
                return State::Instance().dlssRenderPresetBalanced;
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Quality.value_or_default() + 0.01f))
            {
                return State::Instance().dlssRenderPresetQuality;
            }
            else if (ratio <= (Config::Instance()->QualityRatio_UltraQuality.value_or_default() + 0.01f))
            {
                return State::Instance().dlssRenderPresetUltraQuality;
            }
            else
            {
                return State::Instance().dlssRenderPresetDLAA;
            }
        }
    }
    else
    {
        if (State::Instance().dlssdPresetsOverridenByOpti)
        {
            if (ratio <= (Config::Instance()->QualityRatio_UltraPerformance.value_or_default() + 0.01f))
            {
                return Config::Instance()->DLSSDRenderPresetForAll.value_or(
                    Config::Instance()->DLSSDRenderPresetUltraPerformance.value_or_default());
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Performance.value_or_default() + 0.01f))
            {
                return Config::Instance()->DLSSDRenderPresetForAll.value_or(
                    Config::Instance()->DLSSDRenderPresetPerformance.value_or_default());
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Balanced.value_or_default() + 0.01f))
            {
                return Config::Instance()->DLSSDRenderPresetForAll.value_or(
                    Config::Instance()->DLSSDRenderPresetBalanced.value_or_default());
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Quality.value_or_default() + 0.01f))
            {
                return Config::Instance()->DLSSDRenderPresetForAll.value_or(
                    Config::Instance()->DLSSDRenderPresetQuality.value_or_default());
            }
            else if (ratio <= (Config::Instance()->QualityRatio_UltraQuality.value_or_default() + 0.01f))
            {
                return Config::Instance()->DLSSDRenderPresetForAll.value_or(
                    Config::Instance()->DLSSDRenderPresetUltraQuality.value_or_default());
            }
            else
            {
                return Config::Instance()->DLSSDRenderPresetForAll.value_or(
                    Config::Instance()->DLSSDRenderPresetDLAA.value_or_default());
            }
        }
        else if (State::Instance().dlssdPresetsOverriddenExternally)
        {
            return State::Instance().dlssdRenderPresetExternal;
        }
        else
        {
            if (ratio <= (Config::Instance()->QualityRatio_UltraPerformance.value_or_default() + 0.01f))
            {
                return State::Instance().dlssdRenderPresetUltraPerformance;
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Performance.value_or_default() + 0.01f))
            {
                return State::Instance().dlssdRenderPresetPerformance;
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Balanced.value_or_default() + 0.01f))
            {
                return State::Instance().dlssdRenderPresetBalanced;
            }
            else if (ratio <= (Config::Instance()->QualityRatio_Quality.value_or_default() + 0.01f))
            {
                return State::Instance().dlssdRenderPresetQuality;
            }
            else if (ratio <= (Config::Instance()->QualityRatio_UltraQuality.value_or_default() + 0.01f))
            {
                return State::Instance().dlssdRenderPresetUltraQuality;
            }
            else
            {
                return State::Instance().dlssdRenderPresetDLAA;
            }
        }
    }

    return 0;
}

// TODO: disable presets based on the detected DLSS version
template <HasDefaultValue B> void MenuCommon::AddDLSSRenderPreset(std::string name, CustomOptional<uint32_t, B>* value)
{
    // clang-format off
    static const std::vector<MenuOption<uint32_t>> presets = {
        { NVSDK_NGX_DLSS_Hint_Render_Preset_Default, "DEFAULT", 
            LOC(UI_Whatever_the_game_uses) },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_A, LOC(UI_PRESET_A),
            LOC(UI_Intended_for_Performance_Balanced_Quality_modes_nAn_older_va) },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_B, LOC(UI_PRESET_B),
            LOC(UI_Intended_for_Ultra_Performance_mode_nSimilar_to_Preset_A_nRe) },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_C, LOC(UI_PRESET_C),
            LOC(UI_Intended_for_Performance_Balanced_Quality_modes_nGenerally_f) },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_D, LOC(UI_PRESET_D),
            LOC(UI_Default_preset_for_Performance_Balanced_Quality_modes_ngener) },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_E, LOC(UI_PRESET_E),
            LOC(UI_DLSS_3_7_a_better_D_preset_nRemoved_on_recent_versions) },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_F, LOC(UI_PRESET_F),
            LOC(UI_Default_preset_for_Ultra_Performance_and_DLAA_modes_nRemoved) },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_G, LOC(UI_PRESET_G),
            LOC(UI_Unused) },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_H_Reserved, LOC(UI_PRESET_H),
            LOC(UI_Unused) },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_I_Reserved, LOC(UI_PRESET_I),
            LOC(UI_Unused) },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_J, LOC(UI_PRESET_J),
            LOC(UI_Similar_to_preset_K_Preset_J_might_exhibit_slightly_nless_gh) },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_K, LOC(UI_PRESET_K),
            LOC(UI_Default_preset_for_DLAA_Balanced_Quality_modes_n1st_Gen_Tran) },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_L, LOC(UI_PRESET_L),
            LOC(UI_Default_for_Ultra_Perf_mode_n2nd_Gen_Transformers) },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_M, LOC(UI_PRESET_M),
            LOC(UI_Default_for_Perf_mode_n2nd_Gen_Transformer) },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_N, LOC(UI_PRESET_N),
            LOC(UI_Unused) },
        { NVSDK_NGX_DLSS_Hint_Render_Preset_O, LOC(UI_PRESET_O),
            LOC(UI_Unused) },
        { NV_PRESET_LATEST, LOC(UI_Latest),
            LOC(UI_Latest_supported_by_the_dll) }
    };
    // clang-format on

    PopulateCombo(name, *value, presets);
}

template <HasDefaultValue B> void MenuCommon::AddDLSSDRenderPreset(std::string name, CustomOptional<uint32_t, B>* value)
{
    // We don't have DLSSD definitions so using raw values
    static const std::vector<MenuOption<uint32_t>> presets = {
        { 0, "DEFAULT", LOC(UI_Whatever_the_game_uses) },
        { 1, LOC(UI_PRESET_A), LOC(UI_Preset_A_nRemoved_on_recent_versions) },
        { 2, LOC(UI_PRESET_B), LOC(UI_Preset_B_nRemoved_on_recent_versions) },
        { 3, LOC(UI_PRESET_C), LOC(UI_Preset_C_nRemoved_on_recent_versions) },
        { 4, LOC(UI_PRESET_D), LOC(UI_Default_model_Transformer) },
        { 5, LOC(UI_PRESET_E), LOC(UI_Latest_Transformer_model_nMust_use_if_DoF_guide_is_needed) },
        { 6, LOC(UI_PRESET_F), LOC(UI_Latest_Transformer_model_nMust_use_if_DoF_guide_is_needed) },
        { NV_PRESET_LATEST, LOC(UI_Latest), LOC(UI_Latest_supported_by_the_dll) }
    };

    PopulateCombo(name, *value, presets);
}

template <typename TStorage, typename T>
void MenuCommon::PopulateCombo(const std::string& name, TStorage& currentValue,
                               const std::vector<MenuOption<T>>& options)
{
    if (options.empty())
        return;

    // Assumes that different types mean that TStorage is std::optional
    T currentVal;
    if constexpr (std::is_same_v<TStorage, T>)
        currentVal = currentValue;
    else
        currentVal = currentValue.value_or(options[0].value);

    // Find the label for the currently selected item
    std::string preview = LOC(UI_Unknown);
    for (const auto& opt : options)
    {
        if (opt.value == currentVal)
        {
            preview = opt.label;
            break;
        }
    }

    if (ImGui::BeginCombo(name.c_str(), preview.c_str()))
    {
        for (const auto& opt : options)
        {
            if (opt.hidden)
                continue;

            if (opt.disabled)
                ImGui::BeginDisabled();

            bool isSelected = (currentVal == opt.value);
            if (ImGui::Selectable(opt.label.c_str(), isSelected))
                currentValue = opt.value;

            // Show tooltip for the individual item if it exists
            if (!opt.tooltip.empty() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                ImGui::SetTooltip("%s", opt.tooltip.c_str());

            if (opt.disabled)
                ImGui::EndDisabled();
        }
        ImGui::EndCombo();
    }
}

static UiTargetMode getUiTargetMode()
{
    const auto& state = State::Instance();

    const bool fallback = !Config::Instance()->OverlayMenu.value_or_default();

    if (fallback)
    {
        // We have no reliable swapchain/output color-space information here.
        // Only classify the upscaled working image.
        if (state.currentFeature && state.currentFeature->IsHdr())
            return UiTargetMode::LinearHDR;

        return UiTargetMode::SDR;
    }

    // sRGB target encodes on write, colors need to be linear
    if (state.menuTargetSrgb)
        return UiTargetMode::LinearHDR;

    const auto& output = state.outputColorSpace;

    // If SetColorSpace1 has not provided a known/valid color space,
    // fall back conservatively.
    if (!output.valid)
        return UiTargetMode::SDR;

    switch (output.transfer)
    {
    case ColorTransfer::Linear:
        // scRGB: linear Rec.709 RGB.
        //
        // hdrOutputActive is intentionally NOT required here.
        // A scRGB swapchain is still linear even when the physical output
        // is currently SDR. hdrOutputActive only affects the desired
        // reference-white scaling in toneMapColor().
        if (output.model == ColorModel::RGB && output.primaries == ColorPrimaries::Rec709)
        {
            return UiTargetMode::ScRGB;
        }

        break;

    case ColorTransfer::PQ:
        // Direct PQ UI rendering currently assumes RGB PQ / Rec.2020.
        //
        // Do not treat YCbCr PQ as an RGB render target.
        if (output.model == ColorModel::RGB && output.primaries == ColorPrimaries::Rec2020)
        {
            return UiTargetMode::PQ;
        }

        break;

    case ColorTransfer::HLG:
        // Current HLG UI path assumes an RGB render target.
        //
        // DXGI HLG modes are commonly YCbCr, so reject unsupported
        // combinations rather than applying an RGB HLG transform blindly.
        if (output.model == ColorModel::RGB && output.primaries == ColorPrimaries::Rec2020)
        {
            return UiTargetMode::HLG;
        }

        break;

    case ColorTransfer::SRGB:
        return UiTargetMode::SDR;

    case ColorTransfer::Unknown:
    default:
        break;
    }

    // Unsupported model / primaries / transfer combination.
    return UiTargetMode::SDR;
}

static float srgbToLinear(float x)
{
    x = std::clamp(x, 0.0f, 1.0f);

    if (x <= 0.04045f)
        return x / 12.92f;

    return std::pow((x + 0.055f) / 1.055f, 2.4f);
}

static float linearToPQ(float nits)
{
    // SMPTE ST.2084
    constexpr float m1 = 2610.0f / 16384.0f;
    constexpr float m2 = 2523.0f / 32.0f;
    constexpr float c1 = 3424.0f / 4096.0f;
    constexpr float c2 = 2413.0f / 128.0f;
    constexpr float c3 = 2392.0f / 128.0f;

    const float y = std::clamp(nits / 10000.0f, 0.0f, 1.0f);
    const float ym1 = std::pow(y, m1);

    return std::pow((c1 + c2 * ym1) / (1.0f + c3 * ym1), m2);
}

static float linearToHLG(float x)
{
    // BT.2100 HLG OETF
    constexpr float a = 0.17883277f;
    constexpr float b = 0.28466892f;
    constexpr float c = 0.55991073f;

    x = std::max(x, 0.0f);

    if (x <= (1.0f / 12.0f))
        return std::sqrt(3.0f * x);

    return a * std::log(12.0f * x - b) + c;
}

static ImVec4 linear709To2020(float r, float g, float b)
{
    return ImVec4(0.6274040f * r + 0.3292820f * g + 0.0433136f * b, 0.0690970f * r + 0.9195400f * g + 0.0113612f * b,
                  0.0163916f * r + 0.0880132f * g + 0.8955950f * b, 0.0f);
}

static ImVec4 legacyHdrToneMap(const ImVec4& color)
{
    constexpr float exposure = 1.0f;
    constexpr float strength = 1.0f;

    const float peak = std::max(color.x, std::max(color.y, color.z));

    if (peak <= 0.0f)
        return color;

    const float exposedPeak = peak * exposure;
    const float mappedPeak = exposedPeak / (1.0f + exposedPeak);

    const float reinhardScale = mappedPeak / peak;
    const float scale = 1.0f + (reinhardScale - 1.0f) * strength;

    return ImVec4(color.x * scale, color.y * scale, color.z * scale, color.w);
}

static ImVec4 toneMapColor(const ImVec4& color);

// Status colors shared by the whole menu
static ImVec4 StatusOk() { return toneMapColor(ImVec4(0.35f, 0.85f, 0.45f, 1.f)); }
static ImVec4 StatusWarn() { return toneMapColor(ImVec4(1.f, 0.8f, 0.f, 1.f)); }
static ImVec4 StatusError() { return toneMapColor(ImVec4(1.f, 0.f, 0.f, 1.f)); }

static ImVec4 toneMapColor(const ImVec4& color)
{
    const auto mode = getUiTargetMode();

    switch (mode)
    {
    case UiTargetMode::SDR:
        return color;

    case UiTargetMode::LinearHDR:
        return ImVec4(srgbToLinear(color.x), srgbToLinear(color.y), srgbToLinear(color.z), color.w);

    case UiTargetMode::ScRGB:
    {
        constexpr float scRgbReferenceWhiteNits = 80.0f;
        constexpr float hdrUiWhiteNits = 203.0f;

        const float uiWhiteNits = State::Instance().hdrOutputActive ? hdrUiWhiteNits : scRgbReferenceWhiteNits;

        const float scale = uiWhiteNits / scRgbReferenceWhiteNits;

        return ImVec4(srgbToLinear(color.x) * scale, srgbToLinear(color.y) * scale, srgbToLinear(color.z) * scale,
                      color.w);
    }

    case UiTargetMode::PQ:
        // Direct ImGui rendering into a nonlinear PQ target.
        //
        // Proper PQ encoding of vertex colors produces incorrect results
        // with the standard ImGui alpha blend state because blending then
        // happens in PQ space.
        //
        // Keep the known-good legacy compression until PQ rendering is
        // moved to a linear intermediate/composite pass.
        return legacyHdrToneMap(color);

    case UiTargetMode::HLG:
        // Same fundamental nonlinear-blending problem as PQ.
        // Conservative compatibility behavior for now.
        return legacyHdrToneMap(color);

    default:
        return color;
    }
}

static void MenuHdrCheck(ImGuiIO io)
{
    if (!_hdrTonemapApplied)
    {
        ImGuiStyle& style = ImGui::GetStyle();
        const auto mode = getUiTargetMode();

        LOG_INFO("Output HDR: {}, UI Mode: {}", State::Instance().hdrOutputActive, magic_enum::enum_name(mode));

        CopyMemory(SdrColors, style.Colors, sizeof(style.Colors));

        // Apply tone mapping to the ImGui style
        for (int i = 0; i < ImGuiCol_COUNT; ++i)
        {
            ImVec4 color = style.Colors[i];
            style.Colors[i] = toneMapColor(color);
        }

        _hdrTonemapApplied = true;
    }
}

static float MenuResolutionScale(ImGuiIO io)
{
    if (Config::Instance()->MenuScale.has_value())
        return Config::Instance()->MenuScale.value();

    // Calculate menu scale according to display resolution
    float y = State::Instance().screenHeight;

    if (io.DisplaySize.y != 0)
        y = (float) io.DisplaySize.y;

    // 1000p is minimum for 1.0 menu ratio
    float result = (float) ((int) (y / 108.0f)) / 10.0f;

    result = std::round(result * 10.0f) / 10.0f;

    if (result < 0.5f)
        result = 0.5f;

    if (result > 2.0f)
        result = 2.0f;

    return result;
}

inline static std::string GetSourceString(UINT source)
{
    switch (source)
    {
    case 1:
        return "RTV";
    case 2:
        return "SRV";
    case 4:
        return "UAV";
    case 8:
        return "OM";
    case 16:
        return "Ups";
    case 32:
        return "SCR";
    case 64:
        return "SGR";
    case 128:
        return "OMUAV";
    default:
        return std::format("{}", source);
    }
}

inline static std::string GetDispatchString(UINT source)
{
    switch (source)
    {
    case 0:
        return "-";
    case 512:
        return "DI";
    case 1024:
        return "DII";
    case 256:
        return "Disp";
    default:
        return std::format("{}", source);
    }
}

void MenuCommon::ApplyThemeStyle()
{
    if (ImGui::GetCurrentContext() == nullptr)
        return;

    ImGuiStyle& style = ImGui::GetStyle();

    auto conf = Config::Instance();
    bool lightTheme = conf->LightTheme.value_or_default();

    style.WindowRounding = conf->MenuRounding.value_or_default();
    style.ChildRounding = 2.0f;
    style.FrameRounding = 2.0f;
    style.PopupRounding = 2.0f;
    style.ScrollbarRounding = 2.0f;
    style.GrabRounding = 2.0f;
    style.TabRounding = 2.0f;

    style.WindowBorderSize = 1.0f;
    style.PopupBorderSize = 1.0f;

    style.FrameBorderSize = lightTheme ? 1.0f : 0.0f;
    style.TabBorderSize = lightTheme ? 1.0f : 0.0f;

    style.ScrollbarSize = 10.0f;
    style.GrabMinSize = 10.0f;

    auto Clamp01 = [](float v) { return std::max(0.0f, std::min(v, 1.0f)); };

    auto Mix = [](const ImVec4& a, const ImVec4& b, float t, float alpha = 1.0f)
    { return ImVec4(a.x + (b.x - a.x) * t, a.y + (b.y - a.y) * t, a.z + (b.z - a.z) * t, alpha); };

    auto Luminance = [](const ImVec4& c) { return c.x * 0.2126f + c.y * 0.7152f + c.z * 0.0722f; };

    auto Saturate = [&](const ImVec4& color, float amount)
    {
        float lum = Luminance(color);

        return ImVec4(Clamp01(lum + (color.x - lum) * amount), Clamp01(lum + (color.y - lum) * amount),
                      Clamp01(lum + (color.z - lum) * amount), color.w);
    };

    ImVec4 accent = ImVec4(conf->MenuAccentColorR.value_or_default(), conf->MenuAccentColorG.value_or_default(),
                           conf->MenuAccentColorB.value_or_default(), 1.0f);

    ImVec4 bgAccent = ImVec4(conf->MenuBGColorR.value_or_default(), conf->MenuBGColorG.value_or_default(),
                             conf->MenuBGColorB.value_or_default(), 1.0f);

    float luminance = Luminance(accent);

    const ImVec4 bgDark = lightTheme ? ImVec4(0.80f, 0.82f, 0.86f, 1.00f) : ImVec4(0.09f, 0.09f, 0.10f, 1.00f);
    const ImVec4 bgMid = lightTheme ? ImVec4(0.89f, 0.91f, 0.95f, 1.00f) : ImVec4(0.11f, 0.11f, 0.12f, 1.00f);
    const ImVec4 bgLight = lightTheme ? ImVec4(0.96f, 0.97f, 0.99f, 1.00f) : ImVec4(0.14f, 0.14f, 0.15f, 1.00f);

    const ImVec4 textPrimary = lightTheme ? ImVec4(0.05f, 0.06f, 0.08f, 1.00f) : ImVec4(0.90f, 0.93f, 0.95f, 1.00f);
    const ImVec4 textDim = lightTheme ? ImVec4(0.22f, 0.25f, 0.31f, 1.00f) : ImVec4(0.54f, 0.58f, 0.62f, 1.00f);

    const ImVec4 borderCol = lightTheme ? ImVec4(0.35f, 0.40f, 0.50f, 1.00f) : ImVec4(0.24f, 0.24f, 0.26f, 1.00f);
    const ImVec4 dimBg = lightTheme ? ImVec4(0.30f, 0.33f, 0.38f, 0.20f) : ImVec4(0.09f, 0.10f, 0.13f, 0.20f);
    const ImVec4 modalDimBg = lightTheme ? ImVec4(0.22f, 0.24f, 0.28f, 0.55f) : ImVec4(0.04f, 0.04f, 0.07f, 0.55f);

    // MenuBGColor: only background/surface tint.
    auto BgTint = [&](const ImVec4& base, float strength = 1.0f, float alpha = 1.0f)
    {
        float t = lightTheme ? (0.180f * strength) : (0.120f * strength);
        return Mix(base, bgAccent, t, alpha);
    };

    // MenuAccentColor: all visible interactive accent colors.
    auto AccentSoft = [&](float alpha = 1.0f)
    { return lightTheme ? Mix(bgLight, accent, 0.14f, alpha) : Mix(bgDark, accent, 0.32f, alpha); };

    auto AccentMed = [&](float alpha = 1.0f)
    { return lightTheme ? Mix(bgLight, accent, 0.42f, alpha) : Mix(bgDark, accent, 0.55f, alpha); };

    auto AccentStrong = [&](float alpha = 1.0f) { return ImVec4(accent.x, accent.y, accent.z, alpha); };

    const ImVec4 bgTitle = AccentSoft();

    auto SurfaceHover = [&](float alpha = 1.0f)
    { return lightTheme ? Mix(bgLight, accent, 0.12f, alpha) : Mix(bgLight, accent, 0.18f, alpha); };

    auto SurfaceActive = [&](float alpha = 1.0f)
    { return lightTheme ? Mix(bgLight, accent, 0.20f, alpha) : Mix(bgLight, accent, 0.28f, alpha); };

    auto TitleActive = [&](float alpha = 1.0f)
    { return lightTheme ? Mix(bgTitle, accent, 0.18f, alpha) : Mix(bgTitle, accent, 0.16f, alpha); };

    auto PlotAccent = [&](float alpha = 1.0f)
    {
        if (lightTheme)
        {
            // Darken slightly for contrast on light bg — no channel floors
            return Mix(accent, ImVec4(0.00f, 0.00f, 0.00f, 1.00f), 0.20f, alpha);
        }

        // Brighten slightly for visibility on dark bg — no channel floors
        return Mix(accent, ImVec4(1.00f, 1.00f, 1.00f, 1.00f), 0.35f, alpha);
    };

    auto PlotAccentHovered = [&](float alpha = 1.0f)
    {
        if (lightTheme)
        {
            return Mix(PlotAccent(alpha), ImVec4(0.00f, 0.00f, 0.00f, 1.00f), 0.15f, alpha);
        }

        return Mix(PlotAccent(alpha), ImVec4(1.00f, 1.00f, 1.00f, 1.00f), 0.25f, alpha);
    };

    auto AccentReadable = [&](float alpha = 1.0f)
    {
        // Apply saturation boost and luminance correction only here,
        // so AccentStrong / AccentMed / AccentSoft stay true to the user's pick.
        ImVec4 a = Saturate(accent, lightTheme ? 1.35f : 1.25f);
        float lum = Luminance(a);

        if (lightTheme && lum > 0.72f)
            a = Mix(a, ImVec4(0.0f, 0.0f, 0.0f, 1.0f), 0.35f, 1.0f);

        if (!lightTheme && lum < 0.25f)
            a = Mix(a, ImVec4(1.0f, 1.0f, 1.0f, 1.0f), 0.30f, 1.0f);

        return ImVec4(a.x, a.y, a.z, alpha);
    };

    ImVec4* c = ImGui::GetStyle().Colors;

    float minAlpha = Config::Instance()->MenuBGColorA.value_or_default() >= 0.5f
                         ? Config::Instance()->MenuBGColorA.value_or_default()
                         : 0.5f;

    c[ImGuiCol_Text] = textPrimary;
    c[ImGuiCol_TextDisabled] = textDim;
    c[ImGuiCol_TextLink] = AccentReadable();

    // MenuBGColor only.
    c[ImGuiCol_WindowBg] = BgTint(bgDark, 1.00f, Config::Instance()->MenuBGColorA.value_or_default());
    c[ImGuiCol_ChildBg] = BgTint(bgMid, 1.10f, minAlpha + 0.1f);
    c[ImGuiCol_PopupBg] =
        lightTheme ? BgTint(bgLight, 0.90f) : BgTint(ImVec4(0.09f, 0.10f, 0.13f, 0.97f), 0.90f, 0.97f);
    c[ImGuiCol_MenuBarBg] = BgTint(bgDark, 0.85f);
    c[ImGuiCol_DockingEmptyBg] = BgTint(bgDark, 0.75f);

    c[ImGuiCol_Border] = borderCol;
    c[ImGuiCol_BorderShadow] = ImVec4(0.00f, 0.00f, 0.00f, 0.00f);

    // Neutral background, not MenuBGColor.
    c[ImGuiCol_FrameBg] = BgTint(bgLight, 0.50f, minAlpha + 0.15f);
    c[ImGuiCol_FrameBgHovered] = SurfaceHover();
    c[ImGuiCol_FrameBgActive] = SurfaceActive();

    c[ImGuiCol_TitleBg] = BgTint(bgTitle, 0.40f);
    c[ImGuiCol_TitleBgActive] = TitleActive();
    c[ImGuiCol_TitleBgCollapsed] = ImVec4(bgTitle.x, bgTitle.y, bgTitle.z, 0.75f);

    c[ImGuiCol_ScrollbarBg] = BgTint(bgDark, 0.60f, minAlpha + 0.2f);
    c[ImGuiCol_ScrollbarGrab] = AccentSoft();
    c[ImGuiCol_ScrollbarGrabHovered] = AccentMed();
    c[ImGuiCol_ScrollbarGrabActive] = AccentStrong();

    c[ImGuiCol_CheckMark] = AccentReadable();
    c[ImGuiCol_SliderGrab] = AccentMed();
    c[ImGuiCol_SliderGrabActive] = AccentReadable();
    c[ImGuiCol_InputTextCursor] = AccentReadable();

    c[ImGuiCol_Button] = AccentSoft();
    c[ImGuiCol_ButtonHovered] = AccentMed();
    c[ImGuiCol_ButtonActive] = AccentStrong();

    c[ImGuiCol_Header] = AccentSoft(0.90f);
    c[ImGuiCol_HeaderHovered] = AccentMed(0.95f);
    c[ImGuiCol_HeaderActive] = AccentStrong();

    c[ImGuiCol_Separator] = borderCol;
    c[ImGuiCol_SeparatorHovered] = AccentMed(0.85f);
    c[ImGuiCol_SeparatorActive] = AccentStrong();

    c[ImGuiCol_ResizeGrip] = AccentSoft(0.30f);
    c[ImGuiCol_ResizeGripHovered] = AccentStrong(0.70f);
    c[ImGuiCol_ResizeGripActive] = AccentStrong(0.95f);

    c[ImGuiCol_Tab] = BgTint(bgDark, 0.60f);
    c[ImGuiCol_TabHovered] = AccentMed(0.85f);
    c[ImGuiCol_TabSelected] = AccentMed();
    c[ImGuiCol_TabSelectedOverline] = AccentStrong();
    c[ImGuiCol_TabDimmed] = BgTint(bgDark, 0.60f);
    c[ImGuiCol_TabDimmedSelected] = AccentSoft(0.75f);
    c[ImGuiCol_TabDimmedSelectedOverline] = borderCol;

    c[ImGuiCol_DockingPreview] = AccentStrong(0.70f);

    c[ImGuiCol_PlotLines] = PlotAccent();
    c[ImGuiCol_PlotLinesHovered] = PlotAccentHovered();
    c[ImGuiCol_PlotHistogram] = PlotAccent(0.85f);
    c[ImGuiCol_PlotHistogramHovered] = PlotAccentHovered();

    c[ImGuiCol_TableHeaderBg] = BgTint(bgMid, 0.80f, minAlpha + 0.25f);
    c[ImGuiCol_TableBorderStrong] = borderCol;
    c[ImGuiCol_TableBorderLight] = lightTheme ? ImVec4(0.68f, 0.72f, 0.80f, 1.00f) : AccentSoft();
    c[ImGuiCol_TableRowBg] = ImVec4(0.00f, 0.00f, 0.00f, 0.0f);
    c[ImGuiCol_TableRowBgAlt] = lightTheme ? ImVec4(0.00f, 0.00f, 0.00f, 0.045f) : ImVec4(1.00f, 1.00f, 1.00f, 0.03f);

    c[ImGuiCol_TreeLines] = borderCol;
    c[ImGuiCol_TextSelectedBg] = AccentMed(0.38f);
    c[ImGuiCol_DragDropTarget] = AccentStrong(0.90f);
    c[ImGuiCol_NavCursor] = AccentReadable();
    c[ImGuiCol_NavWindowingHighlight] = AccentStrong(0.70f);
    c[ImGuiCol_NavWindowingDimBg] = dimBg;
    c[ImGuiCol_ModalWindowDimBg] = modalDimBg;

    _hdrTonemapApplied = false;
    MenuHdrCheck(ImGui::GetIO());
}

static double lastTime = 0.0;
static double lastFrameTime = 0.0;
static UINT64 uwpTargetFrame = 0;

void MenuCommon::Present()
{
    _frameCount++;

    auto now = Util::MillisecondsNow();

    if (lastTime > 0.0)
        lastFrameTime = now - lastTime;

    lastTime = now;

    if (_handle != nullptr)
        UpdateManualInput(_handle);
}

static PresentRateMeter fgOutputPresentRate;

// Streamline can present real and generated frames from different threads.
// Measure the actual output-present cadence, not the number of presents
// observed between two (independently paced) menu frames.
void MenuCommon::FGOutputPresent() { fgOutputPresentRate.Present(Util::MillisecondsNow()); }

void MenuCommon::FGOutputPresents(bool presents) { fgOutputPresentRate.SetEnabled(presents); }

struct VersionCheckStatus
{
    bool completed = false;
    bool updateAvailable = false;
    std::string latestTag;
    std::string latestUrl;
    std::string error;
};

struct MenuCommon::RenderMenuContext
{
    State& state;
    decltype(Config::Instance()) config;
    ImGuiIO& io;
    IFeature* currentFeature = nullptr;

    double now = 0.0;
    double frameTime = 0.0;
    double frameRate = 0.0;
    // Current Vulkan FG output rate, kept separate from the long-term history.
    double fgOutputFrameTimeMs = 0.0;
    // The game's own frame time with an FG input, 0 when it isn't measured
    double baseFrameTimeMs = 0.0;
    float menuResScale = 1.0f;
    float fpsScale = 1.0f;
    float averageFrameTime = 0.0f;
    float averageUpscalerFT = 0.0f;

    bool frameTimesCalculated = false;
    bool newFrame = false;

    VersionCheckStatus versionStatus;
    std::string currentVersionText;

    // Cached when the menu is visible and shared by RenderMainMenuWindow section helpers.
    std::unique_ptr<std::decay_t<decltype(IdentifyGpu::getPrimaryGpu())>> primaryGpu;
};

static std::string splashMessage;

void MenuCommon::UpdateRenderTiming(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& now = ctx.now;
    auto& frameTime = ctx.frameTime;
    auto& frameRate = ctx.frameRate;

    if (config->OverlayMenu.value_or_default())
    {
        _frameCount++;

        // FPS & frame time calculation
        if (lastTime > 0.0)
        {
            frameTime = now - lastTime;
            frameRate = 1000.0 / frameTime;
        }

        lastTime = now;

        if (_handle != nullptr)
            UpdateManualInput(_handle);
    }
    else
    {
        if (state.activeFgInput == FGInput::NoFG || state.activeFgOutput == FGOutput::NoFG)
            MenuCommon::Present();

        frameTime = lastFrameTime;
        frameRate = 1000.0 / frameTime;
    }

    ctx.fgOutputFrameTimeMs = fgOutputPresentRate.FrameTime(now);

    state.gameFrameRate.SetEnabled(state.activeFgInput != FGInput::NoFG && state.activeFgInput != FGInput::ForceXeLL);
    ctx.baseFrameTimeMs = state.gameFrameRate.FrameTime(now);
    if (ctx.fgOutputFrameTimeMs > 0.0)
    {
        frameTime = ctx.fgOutputFrameTimeMs;
        frameRate = 1000.0 / frameTime;
    }

    state.frameTimes.pop_front();
    state.frameTimes.push_back(frameTime);
}

void MenuCommon::UpdateMenuInputMode(RenderMenuContext& ctx)
{
    auto& io = ctx.io;

    // Moved here to prevent gamepad key replay
    if (_isVisible)
    {
        if (hasGamepad)
            io.BackendFlags |= ImGuiBackendFlags_HasGamepad;

        io.ConfigFlags = ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    }
    else
    {
        capturingKey = false;
        hasGamepad = (io.BackendFlags & ImGuiBackendFlags_HasGamepad) != 0;
        io.BackendFlags &= ~ImGuiBackendFlags_HasGamepad;
        io.ConfigFlags = ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NoKeyboard;
    }
}

void MenuCommon::HandleMenuShortcuts(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& io = ctx.io;

    // Handle Inputs
    {
        if (inputFG)
        {
            inputFG = false;

            // DLSSG via Nvngx - activeFgOutput is NoFG, game's DLSSG is controlled by Override DLSSG Ratio
            if (state.activeFgInput == FGInput::NvngxFG)
            {
                if (state.dlssgMfgMax.has_value() && state.dlssgMfgMax.value() >= 1)
                {
                    auto& ratio = config->FGDLSSGOverrideInterpolationCount;
                    if (ratio.has_value() && ratio.value() == 0)
                    {
                        // Off by the override, go back to what it was
                        if (fgHotkeySavedOverride > 0)
                            ratio = fgHotkeySavedOverride;
                        else
                            ratio.reset();
                        LOG_DEBUG("FG toggle key pressed, DLSSG ratio override back to {}",
                                  ratio.has_value() ? std::to_string(ratio.value()) : std::string(LOC(Default)));
                    }
                    else
                    {
                        fgHotkeySavedOverride = ratio.has_value() ? ratio.value() : -1;
                        ratio = 0;
                        LOG_DEBUG("FG toggle key pressed, turning DLSSG off with the ratio override");
                    }
                    StreamlineHooks::updateDlssgOptions();
                }
                else
                {
                    LOG_DEBUG("FG toggle key pressed, DLSSG ratio override isn't available yet");
                }
            }

            else if (state.activeFgInput != FGInput::NoFG && state.activeFgOutput != FGOutput::NoFG &&
                     (state.FGSwapchainExists() || state.activeFgInput == FGInput::NvngxFG))
            {
                if (fgHotkeyLeftDefault)
                {
                    // Second press goes back to following the game
                    fgHotkeyLeftDefault = false;
                    config->FGEnabled = std::optional<bool> {};
                    LOG_DEBUG("FG toggle key pressed, following the game again");
                }
                else if (config->FGFollowsGame())
                {
                    // Volatile so the ini keeps auto
                    fgHotkeyLeftDefault = true;
                    config->FGEnabled.set_volatile_value(!config->IsFGEnabled());
                    LOG_DEBUG("FG toggle key pressed, overriding the game with {}", config->FGEnabled.value());
                }
                else
                {
                    config->FGEnabled = !config->IsFGEnabled();
                    LOG_DEBUG("FG toggle key pressed, setting FGEnabled to {}", config->FGEnabled.value());
                }

                if (config->IsFGEnabled())
                    state.fgChanged = true;
            }
        }

        if (inputFps)
        {
            inputFps = false;
            config->ShowFps = !config->ShowFps.value_or_default();
        }

        if (inputFpsCycle && config->ShowFps.value_or_default())
            config->FpsOverlayType = (FpsOverlay) ((config->FpsOverlayType.value_or_default() + 1) % FpsOverlay_COUNT);

        if (inputMenu)
        {
            inputMenu = false;
            _isVisible = !_isVisible;

            LOG_DEBUG("Menu key pressed, {0}", _isVisible ? "opening ImGui" : "closing ImGui");

            if (_isVisible)
            {
                io.ClearEventsQueue();
                io.ClearInputKeys();
                io.ClearInputMouse();

                OptiInput::ResetMenuInputTransientState();

                ApplyThemeStyle();

                refreshRate = Util::GetActiveRefreshRate(_handle);

                auto optiPath = std::filesystem::path(Config::Instance()->MainDllPath.value());
                state.artursFgFileAvailable = enablerExists.Get(optiPath / L"dlss-enabler-headless.dll");
                state.nukemsFgFileAvailable = nukemsExists.Get(optiPath / L"dlssg_to_fsr3_amd_is_better.dll");

                if (State::Instance().currentFeature != nullptr)
                {
                    auto upscalerType = State::Instance().currentFeature->GetUpscalerType();

                    if (upscalerType == Upscaler::DLSSD)
                        comboPreset = config->DLSSDRenderPresetForAll.value_or_default();
                    else if (upscalerType == Upscaler::DLSS || upscalerType == Upscaler::DLSS_on12)
                        comboPreset = config->RenderPresetForAll.value_or_default();
                }
            }
            else
            {
                ImGui::CloseCurrentPopup();

                _showMipmapCalcWindow = false;
                _showHudlessWindow = false;
            }

            io.MouseDrawCursor = _isVisible;
            io.WantCaptureKeyboard = _isVisible;
            io.WantCaptureMouse = _isVisible;
        }

        inputFpsCycle = false;
    }
}

void MenuCommon::UpdateVersionAndStartupNotifications(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& now = ctx.now;
    auto& versionStatus = ctx.versionStatus;

    constexpr double splashTime = 7000.0;
    constexpr int updateNoticeTime = 10000;

    // Version check state is copied while locked, then consumed by the UI render pass.
    {
        std::scoped_lock lock(state.versionCheckMutex);
        versionStatus.completed = state.versionCheckCompleted;
        versionStatus.updateAvailable = state.updateAvailable;
        versionStatus.latestTag = state.latestVersionTag;
        versionStatus.latestUrl = state.latestVersionUrl;
        versionStatus.error = state.versionCheckError;
    }

    ctx.currentVersionText = VersionCheck::CurrentVersionString();

    if (versionStatus.completed && versionStatus.updateAvailable && !versionStatus.latestTag.empty())
    {
        if (updateNoticeTag != versionStatus.latestTag)
        {
            updateNoticeTag = versionStatus.latestTag;
            updateNoticeUrl = versionStatus.latestUrl;
            const auto notice = [&]()
            {
                ImGuiToast updateNotification { ImGuiToastType::Error, updateNoticeTime };
                updateNotification.setTitle(LOC(TXT_OptiScaler_Update_available));
                updateNotification.setContent(
                    LOC(UI_Press_s_for_more_info),
                    Keybind::KeyNameFromVirtualKeyCode(config->ShortcutKey.value_or_default()).c_str());
                ImGui::InsertNotification(updateNotification);
                return true;
            };
            static auto res = notice();
        }
    }

    // One-shot startup warning notifications.
    if (!state.postDone)
    {
        if (state.postCodes & PostCode::SlPluginsAlreadyInMemory)
        {
            auto filename = Util::DllPath().filename().string();
            to_lower_in_place(filename);

            ImGuiToast notification { ImGuiToastType::Warning, 10000 };
            notification.setTitle(LOC(TXT_Late_Streamline_hook));
            notification.setContent(
                LOC(UI_Consider_renaming_OptiScaler_from_s_to_other_supported_name_),
                filename.c_str());
            ImGui::InsertNotification(notification);
        }

        if (state.postCodes & PostCode::TryingFsr4Fp8OnUnsupported)
        {
            ImGuiToast notification { ImGuiToastType::Warning, 10000 };
            notification.setTitle(LOC(TXT_Silly_goose));
            notification.setContent(LOC(TXT_FSR4_FP8_AMD_only));
            ImGui::InsertNotification(notification);
        }

        state.postDone = true;
    }

    static bool macTypeNoticeShown = false;
    if (!macTypeNoticeShown && LibraryLoadHooks::macTypeRejections.load(std::memory_order_relaxed) >= 20)
    {
        macTypeNoticeShown = true;

        ImGuiToast notification { ImGuiToastType::Warning, 20000 };
        notification.setTitle(LOC(UI_MacType_detected));
        notification.setContent("MacType is known to cause performance issues,\n"
                                "especially with high polling rate mice.\n"
                                "Please disable it while playing.");
        ImGui::InsertNotification(notification);
    }

    // Initialize splash timing and select the splash text once per process.
    if (splashLimit < 1.0f)
    {
        splashStart = now + 100.0;
        splashLimit = splashStart + splashTime;

        std::srand(static_cast<unsigned>(std::time(nullptr)));
        splashMessage = LocalizationManager::Instance().Get(splashKeys[std::rand() % splashKeys.size()]);
    }
}

void MenuCommon::BeginMenuFrameIfNeeded(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& io = ctx.io;
    auto& now = ctx.now;
    auto& newFrame = ctx.newFrame;

    // New frame check
    if ((!config->DisableSplash.value_or_default() && now > splashStart && now < splashLimit) ||
        config->ShowFps.value_or_default() || _isVisible || ImGui::notifications.size() > 0)
    {
        if (!_isUWP)
        {
            ImGui_ImplWin32_NewFrame();
        }
        else
        {
            ImVec2 displaySize { state.screenWidth, state.screenHeight };
            ImGui_ImplUwp_NewFrame(displaySize);
        }

        OptiInput::FeedImGui(_isVisible);

        MenuHdrCheck(io);
        UpdateOverlayFont();
        ImGui::NewFrame();

        newFrame = true;
    }
}

void MenuCommon::RenderSplashWindow(RenderMenuContext& ctx)
{
    auto config = ctx.config;
    auto& io = ctx.io;
    auto& now = ctx.now;

    constexpr double fadeTime = 1000.0;

    // Splash screen
    if (!config->DisableSplash.value_or_default())
    {
        if (now > splashStart && now < splashLimit)
        {

            ImGui::SetNextWindowSize({ 0.0f, 0.0f });
            ImGui::SetNextWindowBgAlpha(config->FpsOverlayAlpha.value_or_default());
            ImGui::SetNextWindowPos(splashPosition, ImGuiCond_Always);

            float windowAlpha = 1.0f;
            if (auto diff = now - splashStart; diff < fadeTime)
                windowAlpha = static_cast<float>(diff / fadeTime);
            else if (auto diff = splashLimit - now; diff < fadeTime)
                windowAlpha = static_cast<float>(diff / fadeTime);

            ImGui::PushStyleVar(ImGuiStyleVar_Alpha, windowAlpha);
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, ImVec2(12, 8));
            ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));
            ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0));

            if (!config->OverlaysUseTheme.value_or_default())
            {
                ImGui::PushStyleColor(ImGuiCol_WindowBg, ImVec4(0.0f, 0.0f, 0.0f, 1.0f));
                ImGui::PushStyleColor(ImGuiCol_Text, toneMapColor(ImVec4(1.0f, 1.0f, 1.0f, 1.0f)));
            }

            if (ImGui::Begin("Splash", nullptr,
                             ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration |
                                 ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing |
                                 ImGuiWindowFlags_NoNav))
            {
                float splashScale = 1.0f;
                float baseScaleHeight = 720.0f;

                if (io.DisplaySize.y > baseScaleHeight)
                    splashScale = io.DisplaySize.y / baseScaleHeight;

                if (config->UseHQFont.value_or_default())
                    ImGui::PushFontSize(std::round(splashScale * fontSize));
                else
                    ImGui::SetWindowFontScale(splashScale);

                ImGui::Text(LOC(UI_OptiScaler_s_for_menu),
                            Keybind::KeyNameFromVirtualKeyCode(config->ShortcutKey.value_or_default()).c_str());
                ImGui::TextColored(toneMapColor(ImVec4(1.0f, 1.0f, 1.0f, 0.7f)), splashMessage.c_str());

                splashSize = ImGui::GetWindowSize();

                if (config->UseHQFont.value_or_default())
                    ImGui::PopFontSize();

                ImGui::End();

                splashPosition.x = 0.0f; // io.DisplaySize.x - splashWinSize.x;
                splashPosition.y = io.DisplaySize.y - splashSize.y;
            }

            if (!config->OverlaysUseTheme.value_or_default())
                ImGui::PopStyleColor(4);
            else
                ImGui::PopStyleColor(2);

            ImGui::PopStyleVar(2);
        }
    }
}

void MenuCommon::RenderNotifications(RenderMenuContext& ctx)
{
    auto config = ctx.config;
    auto& io = ctx.io;

    // Notifications
    const UiTargetMode uiTargetMode = getUiTargetMode();
    const bool tonemapRequired = uiTargetMode != UiTargetMode::SDR;

    float screenHeight = State::Instance().screenHeight;
    if (io.DisplaySize.y != 0)
        screenHeight = io.DisplaySize.y;

    // Map resolution height to scale, 0.5 for 480p, 2.0 for 1440p
    constexpr float slope = (2.0f - 0.5f) / (1440.f - 480.f);
    float notificationScale = 0.5f + slope * (screenHeight - 480.f);
    notificationScale = std::clamp(notificationScale, 0.5f, 2.0f);

    if (config->UseHQFont.value_or_default())
        ImGui::PushFontSize(std::round(notificationScale * fontSize));

    // No fallback font, SetWindowFontScale needs to be called after Begin()

    ImGui::RenderNotifications(ImGuiToastPos::TopCenter, notificationScale, tonemapRequired);

    if (config->UseHQFont.value_or_default())
        ImGui::PopFontSize();
}

void MenuCommon::UpdateFrameTimeAverages(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& frameTime = ctx.frameTime;
    auto& frameRate = ctx.frameRate;
    auto& frameTimesCalculated = ctx.frameTimesCalculated;
    auto& menuResScale = ctx.menuResScale;
    auto& fpsScale = ctx.fpsScale;
    auto& averageFrameTime = ctx.averageFrameTime;
    auto& averageUpscalerFT = ctx.averageUpscalerFT;

    // FPS Overlay font
    fpsScale = config->FpsScale.value_or(menuResScale);

    // Update frame time & upscaler time averages
    averageFrameTime = 0.0f;
    averageUpscalerFT = 0.0f;

    if (config->ShowFps.value_or_default() || _isVisible)
    {
        float frameCnt = 0;
        frameTime = 0;
        for (size_t i = 299; i > 199; i--)
        {
            if (state.frameTimes[i] > 0.0)
            {
                frameTime += state.frameTimes[i];
                frameCnt++;
            }
        }

        frameTime /= frameCnt;
        frameRate = 1000.0 / frameTime;
        frameTimesCalculated = true;

        // Vulkan FG output timing is already averaged across the last 125 ms.
        // Use it directly for the current FPS readout; preserve the shared
        // history for the separately labelled average and frame-time graph.
        if (ctx.fgOutputFrameTimeMs > 0.0)
        {
            frameTime = ctx.fgOutputFrameTimeMs;
            frameRate = 1000.0 / frameTime;
        }

        float lastFT = static_cast<float>(state.frameTimes.empty() ? 0.0f : state.frameTimes.back());
        float lastUT = static_cast<float>(state.upscaleTimes.empty() ? 0.0f : state.upscaleTimes.back());

        if (!fgOutputPresentRate.TakeIntervals([](double ms) { gFrameTimes.Push(static_cast<float>(ms)); }))
            gFrameTimes.Push(lastFT);

        gUpscalerTimes.Push(lastUT);

        averageFrameTime = gFrameTimes.Average();
        averageUpscalerFT = gUpscalerTimes.Average();
    }
}

void MenuCommon::RenderPerformanceOverlay(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& io = ctx.io;
    auto& currentFeature = ctx.currentFeature;
    auto& now = ctx.now;
    auto& frameTime = ctx.frameTime;
    auto& frameRate = ctx.frameRate;
    auto& menuResScale = ctx.menuResScale;
    auto& fpsScale = ctx.fpsScale;
    auto& averageFrameTime = ctx.averageFrameTime;
    auto& averageUpscalerFT = ctx.averageUpscalerFT;

    // If Fps overlay is visible
    if (config->ShowFps.value_or_default())
    {
        bool stylePushed = false;

        const static auto defaultStyle = ImGuiStyle();

        // Rescale the fps overlay every frame because it shares style with the main menu
        if (config->FpsScale.has_value() && config->FpsScale.value() != menuResScale)
        {
            ImGui::PushStyleVar(ImGuiStyleVar_WindowPadding, defaultStyle.WindowPadding * fpsScale);
            ImGui::PushStyleVar(ImGuiStyleVar_FramePadding, defaultStyle.FramePadding * fpsScale);
            ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, defaultStyle.CellPadding * fpsScale);
            ImGui::PushStyleVar(ImGuiStyleVar_SeparatorTextPadding, defaultStyle.SeparatorTextPadding * fpsScale);

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, defaultStyle.ItemSpacing * fpsScale);
            ImGui::PushStyleVar(ImGuiStyleVar_ItemInnerSpacing, defaultStyle.ItemInnerSpacing * fpsScale);
            ImGui::PushStyleVar(ImGuiStyleVar_IndentSpacing, defaultStyle.IndentSpacing * fpsScale);

            stylePushed = true;
        }

        ImGui::PushStyleVar(ImGuiStyleVar_WindowRounding, config->FpsOverlayRounding.value_or_default() * fpsScale);

        // Set overlay position
        ImGui::SetNextWindowPos(overlayPosition, ImGuiCond_Always);

        // Set overlay window properties
        ImGui::PushStyleColor(ImGuiCol_Border, IM_COL32(0, 0, 0, 0));  // Transparent border
        ImGui::PushStyleColor(ImGuiCol_FrameBg, IM_COL32(0, 0, 0, 0)); // Transparent frame background

        if (!config->OverlaysUseTheme.value_or_default())
        {
            ImGui::PushStyleColor(ImGuiCol_Text,
                                  toneMapColor(RgbToColor(config->FpsOverlayLabelColor.value_or_default())));
            ImGui::PushStyleColor(ImGuiCol_WindowBg,
                                  toneMapColor(RgbToColor(config->FpsOverlayBGColor.value_or_default())));
        }

        // The background is drawn below, so a corner can stay square
        ImGui::SetNextWindowBgAlpha(0.0f);

        if (!config->OverlaysUseTheme.value_or_default())
        {
            ImGui::PushStyleColor(ImGuiCol_PlotLines,
                                  toneMapColor(RgbToColor(config->FpsOverlayGraphColor.value_or_default())));
        }

        if (ImGui::Begin(LOC(TXT_Performance_Overlay), nullptr,
                         ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoDecoration |
                             ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoFocusOnAppearing |
                             ImGuiWindowFlags_NoNav))
        {
            // Rounded corners, except one that touches two edges of the screen
            {
                const ImVec2 min = ImGui::GetWindowPos();
                const ImVec2 max = min + ImGui::GetWindowSize();
                const bool left = min.x <= 0.5f;
                const bool right = max.x >= io.DisplaySize.x - 0.5f;
                const bool top = min.y <= 0.5f;
                const bool bottom = max.y >= io.DisplaySize.y - 0.5f;

                ImDrawFlags corners = 0;
                if (!(top && left))
                    corners |= ImDrawFlags_RoundCornersTopLeft;
                if (!(top && right))
                    corners |= ImDrawFlags_RoundCornersTopRight;
                if (!(bottom && left))
                    corners |= ImDrawFlags_RoundCornersBottomLeft;
                if (!(bottom && right))
                    corners |= ImDrawFlags_RoundCornersBottomRight;

                auto background = ImGui::GetStyleColorVec4(ImGuiCol_WindowBg);
                background.w = config->FpsOverlayAlpha.value_or_default() * ImGui::GetStyle().Alpha;

                // The whole window, its content is clipped inside the padding
                auto drawList = ImGui::GetWindowDrawList();
                drawList->PushClipRect(min, max, false);
                drawList->AddRectFilled(min, max, ImGui::ColorConvertFloat4ToU32(background),
                                        ImGui::GetStyle().WindowRounding,
                                        corners != 0 ? corners : ImDrawFlags_RoundCornersNone);
                drawList->PopClipRect();
            }

            std::string api;
            if (IdentifyGpu::gameUsesDxvk())
            {
                if (state.swapchainInteropApi == SwapchainInteropApi::None)
                    api = "DXVK";
                else
                    api = LOC(UI_DXVK_w_Dx12);
            }
            else if (IdentifyGpu::gameUsesVkd3dProton())
            {
                api = "VKD3D";
            }
            else
            {
                switch (state.swapchainApi)
                {
                case Vulkan:
                    api = "VLK";
                    break;

                case DX11:
                    api = "D3D11";
                    break;

                case DX12:
                    if (state.swapchainInteropApi == SwapchainInteropApi::Dx11wDx12)
                        api = LOC(UI_D3D11_w_DX12);
                    else
                        api = "D3D12";

                    break;

                default:
                    switch (state.api)
                    {
                    case Vulkan:
                        api = "VLK";
                        break;

                    case DX11:
                        api = "D3D11";
                        break;

                    case DX12:
                        api = "D3D12";
                        break;

                    default:
                        api = "???";
                        break;
                    }

                    break;
                }
            }

            // Overlay font and size, the menu's unless set
            const float overlayFontPx = std::round(fpsScale * config->FpsOverlayFontSize.value_or(fontSize));

            if (gOverlayFont != nullptr)
                ImGui::PushFont(gOverlayFont, overlayFontPx);
            else
                ImGui::PushFontSize(overlayFontPx);

            auto fg = state.CurrentFG();
            std::string fgText;

            // Reprojection as the output generates no frames
            if (fg != nullptr && fg->IsGenerating())
                fgText = state.activeFgOutput == FGOutput::Reprojection
                             ? std::string(fg->Name())
                             : std::format("{} x{}", fg->Name(), fg->GetInterpolatedFrameCount() + 1);

            const int fakeFramesCount = state.DlssgDetectedInterpolationCount();

            // Nvngx/DLSSG paths report generation through the game's DLSSG frame count, not IsGenerating()
            bool nvngxFgGenerating = false;

            auto formatFg = [&](std::string_view name, int maxFakeFrames)
            {
                nvngxFgGenerating = fakeFramesCount > 0 && fakeFramesCount <= maxFakeFrames;

                if (fakeFramesCount > maxFakeFrames)
                    return std::format("{} Doesn't support more than {}x", name, maxFakeFrames);

                else if (fakeFramesCount == 0)
                    return std::format("{} off", name);

                return std::format("{} x{}", name, fakeFramesCount + 1);
            };

            const FGNvngxReplacement activeNvngxFg = state.activeFgNvngx;

            // Its frames come from the replacement, but the count is its own, not the game's DLSSG one
            if (state.activeFgOutput == FGOutput::FlipMeter)
            {
                if (fgText.empty())
                    fgText = LOC(UI_Flip_Meter_off);
            }
            else if (activeNvngxFg == FGNvngxReplacement::Arturs)
            {
                fgText = formatFg("Enabler", Nvngx_FG::getMaxFakeFramesCount());
            }
            else if (activeNvngxFg == FGNvngxReplacement::Nukems)
            {
                fgText = formatFg("Nukems", Nvngx_FG::getMaxFakeFramesCount());
            }
            else if (activeNvngxFg == FGNvngxReplacement::FFX)
            {
                fgText = formatFg("FFX", Nvngx_FG::getMaxFakeFramesCount());
            }
            else if (activeNvngxFg == FGNvngxReplacement::Combo)
            {
                fgText = formatFg("Combo", Nvngx_FG::getMaxFakeFramesCount());
            }
            else if (activeNvngxFg == FGNvngxReplacement::Custom)
            {
                fgText = formatFg(state.activeFgNvngxCustom, Nvngx_FG::getMaxFakeFramesCount());
            }
            else if (state.activeFgOutput == FGOutput::DLSSG && fg)
            {
                fgText = formatFg("DLSSG", fg->GetMaxInterpolationCount());
            }

            const auto overlayType = config->FpsOverlayType.value_or_default();
            const bool hasFeature = currentFeature && !currentFeature->IsFrozen();

            // Lines are made of labels (names, separators, info) and values, each with its own color and size
            struct OverlayPart
            {
                std::string text;
                bool value = false;
                float width = 0.0f;
                std::vector<int> padding; // Digits in front of each number of a value
            };

            struct OverlayLine
            {
                std::vector<OverlayPart> parts;
                ImVec2 size { 0.0f, 0.0f };
                int row = OverlayRow_Fps;
                // A graph row has no parts, nor the low latency row
                const char* graphId = nullptr;
                RingBuffer<float, plotWidth>* graphData = nullptr;
                float graphMax = 0.0f;
                bool lowLatency = false;

                OverlayLine& Label(std::string text)
                {
                    if (!text.empty())
                        parts.push_back({ std::move(text), false });

                    return *this;
                }

                // The id keeps the value's widest width across frames
                OverlayLine& Value(const char* id, std::string text)
                {
                    auto padding = OverlayValuePadding(id, text);
                    parts.push_back({ std::move(text), true, 0.0f, std::move(padding) });
                    return *this;
                }
            };

            const bool compact = config->FpsOverlayCompactLabels.value_or_default();
            auto L = [compact](const char* full, const char* shorter) { return std::string(compact ? shorter : full); };
            const bool averages = !config->FpsOverlayHideAverages.value_or_default();

            std::string featureText;

            if (hasFeature)
            {
                const bool usesDx12CompatLayer = currentFeature->IsWithDx12();

                featureText = StrFmt(LOC(UI_s_s_s_3), ApiUpscalerInputName(state.currentInputApiName).c_str(),
                                     NameWithVersion(currentFeature).c_str(), usesDx12CompatLayer ? LOC(UI_w_Dx12) : "");
            }

            // The values that change every frame, as their average over the update interval so they can be read
            struct ShownTimes
            {
                double frameRate = 0.0;
                double frameTime = 0.0;
                double averageFrameTime = 0.0;
                double lastFrameTime = 0.0;
                double lastUpscalerTime = 0.0;
                double averageUpscalerTime = 0.0;
                double baseFrameTime = 0.0;
            };

            static ShownTimes shown;
            static ShownTimes summed;
            static uint32_t summedFrames = 0;
            static uint32_t summedBaseFrames = 0;
            static double shownAt = 0.0;

            summed.frameRate += frameRate;
            summed.frameTime += frameTime;
            summed.averageFrameTime += averageFrameTime;
            summed.lastFrameTime += state.frameTimes.empty() ? 0.0 : state.frameTimes.back();
            summed.lastUpscalerTime += state.upscaleTimes.empty() ? 0.0 : state.upscaleTimes.back();
            summed.averageUpscalerTime += averageUpscalerFT;
            summedFrames++;

            if (ctx.baseFrameTimeMs > 0.0)
            {
                summed.baseFrameTime += ctx.baseFrameTimeMs;
                summedBaseFrames++;
            }

            const float updateIntervalMs = config->FpsOverlayUpdateInterval.value_or_default();

            if (updateIntervalMs < 1.0f || shownAt <= 0.001 || now < shownAt || now - shownAt >= updateIntervalMs)
            {
                const double frames = static_cast<double>(summedFrames);
                shown = { summed.frameRate / frames,
                          summed.frameTime / frames,
                          summed.averageFrameTime / frames,
                          summed.lastFrameTime / frames,
                          summed.lastUpscalerTime / frames,
                          summed.averageUpscalerTime / frames,
                          summedBaseFrames > 0 ? summed.baseFrameTime / summedBaseFrames : 0.0 };

                summed = {};
                summedFrames = 0;
                summedBaseFrames = 0;
                shownAt = now;
            }

            // With an FG input also the measured base frame rate, but only while FG is generating frames
            const bool fgGenerating = (fg != nullptr && fg->IsGenerating()) || nvngxFgGenerating;
            const std::string fpsValue = (shown.baseFrameTime > 0.0 && fgGenerating)
                                             ? StrFmt(LOC(UI_S_1f_1f), shown.frameRate, 1000.0 / shown.baseFrameTime)
                                             : StrFmt("%.1f", shown.frameRate);

            const auto overlayRows = OverlayRows(overlayType);
            std::array<bool, OverlayRow_COUNT> rowShown {};

            for (const auto& entry : overlayRows)
                rowShown[entry.row] = entry.shown;

            // Low latency timings, polled while their row is shown
            constexpr auto delayBetweenPollsMs = 500;
            static auto previousPoll = 0.0;
            static bool gotData = false;
            static TimingData timingData {};

            if (rowShown[OverlayRow_LowLatency] && (previousPoll <= 0.001 || previousPoll + delayBetweenPollsMs < now))
            {
                gotData = InputCommon::get_timing_data(timingData);
                previousPoll = now;
            }

            std::array<std::optional<OverlayLine>, OverlayRow_COUNT> rows;

            const auto fpsRow = OverlayFpsRow(overlayType);

            if (fpsRow == FpsOverlayFpsRow_JustFps)
            {
                rows[OverlayRow_Fps] = OverlayLine().Value("fps", fpsValue).Label(" ");
            }
            else
            {
                OverlayLine line;
                line.Label(api + L(LOC(UI_FPS), LOC(UI_FPS_2))).Value("fps", fpsValue);

                if (fpsRow == FpsOverlayFpsRow_FrameTime)
                    line.Label(", ").Value("fpsFrameTime", StrFmt(LOC(UI_S_2f_ms), shown.frameTime));
                else if (averages)
                    line.Label(L(LOC(UI_Avg), LOC(UI_Avg_2)))
                        .Value("fpsAverage", StrFmt("%.1f", 1000.0 / shown.averageFrameTime));

                if (!fgText.empty())
                    line.Label(" (" + fgText + ")");

                if (!featureText.empty())
                    line.Label(LOC(Str_001) + featureText);

                rows[OverlayRow_Fps] = std::move(line);
            }

            if (hasFeature && currentFeature->RenderWidth() > 0)
            {
                const float ratio = (float) currentFeature->TargetWidth() / (float) currentFeature->RenderWidth();

                OverlayLine line;
                line.Label(L(LOC(UI_Upscale), "UPS "))
                    .Value("upscale", StrFmt(LOC(UI_ux_u_ux_u_2fx), currentFeature->RenderWidth(),
                                             currentFeature->RenderHeight(), currentFeature->TargetWidth(),
                                             currentFeature->TargetHeight(), ratio));

                rows[OverlayRow_Upscale] = std::move(line);
            }

            auto graphRow = [](const char* id, RingBuffer<float, plotWidth>* data, float max)
            {
                OverlayLine line;
                line.graphId = id;
                line.graphData = data;
                line.graphMax = max;

                return line;
            };

            rows[OverlayRow_FrameTime] = OverlayLine()
                                             .Label(L(LOC(UI_Frame_Time), "FT "))
                                             .Value("frameTime", StrFmt(LOC(UI_S_2f_ms), shown.lastFrameTime));

            if (averages)
                rows[OverlayRow_FrameTime]
                    ->Label(L(LOC(UI_Avg), LOC(UI_Avg_2)))
                    .Value("frameTimeAverage", StrFmt(LOC(UI_S_2f_ms), shown.averageFrameTime));

            rows[OverlayRow_FrameTimeGraph] = graphRow(LOC(UI_FrameTimeGraph), &gFrameTimes, 66.6f);

            rows[OverlayRow_UpscalerTime] = OverlayLine()
                                                .Label(L(LOC(UI_Upscaler_Time), "UT "))
                                                .Value("upscalerTime", StrFmt(LOC(UI_S_2f_ms), shown.lastUpscalerTime));

            if (averages)
                rows[OverlayRow_UpscalerTime]
                    ->Label(L(LOC(UI_Avg), LOC(UI_Avg_2)))
                    .Value("upscalerTimeAverage", StrFmt(LOC(UI_S_2f_ms), shown.averageUpscalerTime));

            rows[OverlayRow_UpscalerTimeGraph] = graphRow(LOC(UI_UpscalerFrameTimeGraph), &gUpscalerTimes, 20.0f);

            if (gotData && timingData.timeRange.has_value())
            {
                rows[OverlayRow_LowLatency] = OverlayLine();
                rows[OverlayRow_LowLatency]->lowLatency = true;
            }

            std::vector<OverlayLine> lines;
            for (const auto& entry : overlayRows)
            {
                if (entry.shown && rows[entry.row].has_value())
                {
                    lines.push_back(std::move(rows[entry.row].value()));
                    lines.back().row = entry.row;
                }
            }

            ImFont* font = ImGui::GetFont();
            const float basePx = ImGui::GetFontSize();
            const float labelPx = std::round(basePx * config->FpsOverlayLabelScale.value_or_default());
            const float valuePx = std::round(basePx * config->FpsOverlayValueScale.value_or_default());

            const ImU32 labelColor = ImGui::GetColorU32(ImGuiCol_Text);
            const ImU32 valueColor = config->OverlaysUseTheme.value_or_default()
                                         ? labelColor
                                         : ImGui::ColorConvertFloat4ToU32(toneMapColor(
                                               RgbToColor(config->FpsOverlayValueColor.value_or_default())));

            // A stronger shadow is thicker and darker
            const auto shadowType = config->FpsOverlayShadowType.value_or_default();
            const float shadowStrength = config->FpsOverlayShadowStrength.value_or_default();
            const float shadowSize = std::max(1.0f, std::round(basePx / 14.0f * (0.5f + shadowStrength * 1.5f)));
            auto shadowVec = toneMapColor(RgbToColor(config->FpsOverlayShadowColor.value_or_default()));
            shadowVec.w = 0.35f + 0.65f * shadowStrength;
            const ImU32 shadowColor = ImGui::ColorConvertFloat4ToU32(shadowVec);

            // The outline is a filled circle of opaque copies around the text, translucent ones would add up unevenly
            std::vector<ImVec2> outlineOffsets;

            if (shadowType == FpsOverlayShadow_Outline)
            {
                const int radius = static_cast<int>(shadowSize);
                const int limit = radius * radius + radius;

                for (int dy = -radius; dy <= radius; dy++)
                {
                    for (int dx = -radius; dx <= radius; dx++)
                    {
                        if ((dx != 0 || dy != 0) && dx * dx + dy * dy <= limit)
                            outlineOffsets.emplace_back(static_cast<float>(dx), static_cast<float>(dy));
                    }
                }

                shadowVec.w = 1.0f;
            }

            const ImU32 outlineColor = ImGui::ColorConvertFloat4ToU32(shadowVec);

            auto partPx = [&](const OverlayPart& part) { return part.value ? valuePx : labelPx; };

            // Digits of the values take the widest digit's width, so changing numbers don't shift with fonts whose
            // digits differ in width
            float digitWidth = 0.0f;
            for (char digit = '0'; digit <= '9'; digit++)
                digitWidth = std::max(digitWidth, font->CalcTextSizeA(valuePx, FLT_MAX, 0.0f, &digit, &digit + 1).x);

            // Calls place(begin, end, x) for the part's text, the value's digits one at a time centered in their
            // width and each number after its padding. The part's width.
            auto layoutPart = [&](const OverlayPart& part, auto&& place)
            {
                const float px = partPx(part);
                const char* begin = part.text.c_str();
                const char* text = begin;
                const char* end = text + part.text.size();
                float x = 0.0f;
                size_t number = 0;

                auto isDigit = [&](const char* c) { return part.value && *c >= '0' && *c <= '9'; };

                while (text < end)
                {
                    if (isDigit(text))
                    {
                        if ((text == begin || !isDigit(text - 1)) && number < part.padding.size())
                            x += part.padding[number++] * digitWidth;

                        const float width = font->CalcTextSizeA(px, FLT_MAX, 0.0f, text, text + 1).x;
                        place(text, text + 1, x + std::round((digitWidth - width) * 0.5f));
                        x += digitWidth;
                        text++;
                        continue;
                    }

                    const char* runEnd = text;
                    while (runEnd < end && !isDigit(runEnd))
                        runEnd++;

                    place(text, runEnd, x);
                    x += font->CalcTextSizeA(px, FLT_MAX, 0.0f, text, runEnd).x;
                    text = runEnd;
                }

                return x;
            };

            auto measureLine = [&](OverlayLine& line)
            {
                ImVec2 size { 0.0f, 0.0f };

                for (auto& part : line.parts)
                {
                    const float px = partPx(part);
                    part.width = layoutPart(part, [](const char*, const char*, float) {});
                    size.x += part.width;
                    size.y = std::max(size.y, px);
                }

                line.size = size;
            };

            for (auto& line : lines)
                measureLine(line);

            // On whole pixels, the shadows of a line under all its text so they don't cover the next part
            auto drawText = [&](ImDrawList* drawList, float px, ImVec2 pos, ImU32 color, const char* text,
                                const char* end, bool shadowPass)
            {
                pos = ImVec2(std::round(pos.x), std::round(pos.y));

                if (!shadowPass)
                {
                    drawList->AddText(font, px, pos, color, text, end);
                }
                else if (shadowType == FpsOverlayShadow_Shadow)
                {
                    for (float d = 1.0f; d <= shadowSize; d++)
                        drawList->AddText(font, px, ImVec2(pos.x + d, pos.y + d), shadowColor, text, end);
                }
                else if (shadowType == FpsOverlayShadow_Outline)
                {
                    for (const auto& offset : outlineOffsets)
                        drawList->AddText(font, px, pos + offset, outlineColor, text, end);
                }
            };

            auto lineAscent = [&](const OverlayLine& line)
            {
                float ascent = 0.0f;
                for (const auto& part : line.parts)
                    ascent = std::max(ascent, font->GetFontBaked(partPx(part))->Ascent);

                return ascent;
            };

            // For a horizontal overlay, where the lines side by side share one baseline
            float sharedAscent = 0.0f;

            // Parts of different sizes share the baseline
            auto drawLine = [&](OverlayLine& line)
            {
                if (line.size.y == 0.0f && !line.parts.empty())
                    measureLine(line);

                const float maxAscent = std::max(lineAscent(line), sharedAscent);

                auto drawList = ImGui::GetWindowDrawList();
                const auto start = ImGui::GetCursorScreenPos();

                for (const bool shadowPass : { true, false })
                {
                    if (shadowPass && shadowType == FpsOverlayShadow_Off)
                        continue;

                    auto pos = start;

                    for (const auto& part : line.parts)
                    {
                        const float px = partPx(part);
                        const float y = pos.y + maxAscent - font->GetFontBaked(px)->Ascent;
                        const ImU32 color = part.value ? valueColor : labelColor;

                        layoutPart(part, [&](const char* text, const char* end, float x)
                                   { drawText(drawList, px, ImVec2(pos.x + x, y), color, text, end, shadowPass); });

                        pos.x += part.width;
                    }
                }

                ImGui::Dummy(ImVec2(line.size.x, line.size.y + maxAscent - lineAscent(line)));
            };

            const bool horizontal = config->FpsOverlayHorizontal.value_or_default();
            const float graphHeight = config->FpsOverlayGraphHeight.value_or_default();

            ImVec2 plotSize;
            if (horizontal)
            {
                plotSize = { fpsScale * 150, fpsScale * 16 * graphHeight };
            }
            else
            {
                // Graphs match the widest text line
                auto textWidth = 0.0f;
                for (const auto& line : lines)
                    textWidth = std::max(textWidth, line.size.x);

                plotSize = { std::max(textWidth, fpsScale * 300.0f), fpsScale * 30 * graphHeight };
            }

            // Rows are apart by the row spacing, by default as ImGui::Spacing() puts them
            const float itemSpacingY = ImGui::GetStyle().ItemSpacing.y;
            const float rowSpacing = config->FpsOverlayRowSpacing.has_value()
                                         ? config->FpsOverlayRowSpacing.value() * fpsScale
                                         : itemSpacingY * 2.0f;

            // The timings as lines of text, as wide as the graphs so their bars fit
            const ImVec2 itemSpacing = ImGui::GetStyle().ItemSpacing;

            auto drawLowLatency = [&]()
            {
                ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, itemSpacing);
                ImGui::BeginGroup();

                ImDrawList* drawList = ImGui::GetWindowDrawList();
                constexpr float offsetForText = 155;

                const auto& rangeInNs = timingData.timeRange.value().length;

                UINT64 localFrameCount = 0;

                if (fg != nullptr)
                    localFrameCount = fg->FrameCount();

                // Text lines with the overlay's shadow or outline under them
                auto shadowedText = [&](ImU32 color, const std::string& text)
                {
                    const char* begin = text.c_str();
                    const char* end = begin + text.size();
                    drawText(drawList, ImGui::GetFontSize(), ImGui::GetCursorScreenPos(), color, begin, end, true);

                    ImGui::PushStyleColor(ImGuiCol_Text, color);
                    ImGui::TextUnformatted(begin, end);
                    ImGui::PopStyleColor();
                };

                // OverlayLine frameIds;
                // frameIds.Label("FGId: ")
                //     .Value("lowLatencyFgId", StrFmt("%llu", localFrameCount))
                //     .Label(", RfxId: ")
                //     .Value("lowLatencyRfxId", StrFmt("%llu", state.reflexFrameId));
                // drawLine(frameIds);
                OverlayLine wholeFrame;
                wholeFrame.Label(LOC(UI_Low_latency_timings_whole_frame))
                    .Value("lowLatencyFrame", StrFmt(LOC(UI_S_1fms), rangeInNs / 1000.0));
                drawLine(wholeFrame);

                const auto maxWidth = horizontal ? fpsScale * 300.0f : plotSize.x;

                const auto drawTiming = [&](const auto& timingOpt, const char* desc, ImVec4 color)
                {
                    if (!timingOpt.has_value())
                        return;

                    auto toneMappedColor = toneMapColor(color);

                    const auto& timing = timingOpt.value();
                    float duration = static_cast<float>(timing.length * rangeInNs / 1000.0);

                    shadowedText(ImGui::ColorConvertFloat4ToU32(toneMappedColor),
                                 StrFmt(LOC(UI_S_12s_4_1fms), desc, duration));

                    auto leftLimit = ImGui::GetItemRectMin().x + offsetForText * fpsScale;

                    auto start = static_cast<float>(leftLimit + (ImGui::GetItemRectMin().x + maxWidth - leftLimit) *
                                                                    timing.position);

                    auto end =
                        static_cast<float>(start + (ImGui::GetItemRectMin().x + maxWidth - leftLimit) * timing.length);

                    auto pos = ImVec2(start, ImGui::GetItemRectMin().y);
                    auto size = ImVec2(end, ImGui::GetItemRectMax().y);

                    drawList->AddRectFilled(pos, size, ImGui::ColorConvertFloat4ToU32(toneMappedColor));
                };

                drawTiming(timingData.simulation, "Simulation", ImVec4(0.768f, 0.169f, 0.169f, 1.0f));
                drawTiming(timingData.renderSubmit, "RenderSubmit", ImVec4(0.235f, 0.705f, 0.294f, 1.0f));
                drawTiming(timingData.present, "Present", ImVec4(1.0f, 0.88f, 0.098f, 1.0f));
                drawTiming(timingData.driver, "Driver", ImVec4(0.263f, 0.388f, 0.847f, 1.0f));
                drawTiming(timingData.osRenderQueue, "RenderQueue", ImVec4(0.76f, 0.51f, 0.188f, 1.0f));
                drawTiming(timingData.gpuRender, "GpuRender", ImVec4(0.569f, 0.117f, 0.705f, 1.0f));

                // The bars are drawn without taking space
                ImGui::Dummy(ImVec2(maxWidth, 0.0f));

                ImGui::EndGroup();
                ImGui::PopStyleVar();
            };

            ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(itemSpacing.x, 0.0f));

            OverlayLine sepLine;
            if (horizontal)
            {
                sepLine.Label(LOC(Str_001));
                measureLine(sepLine);

                for (const auto& line : lines)
                    sharedAscent = std::max(sharedAscent, lineAscent(line));
            }

            // Draw the overlay
            for (size_t i = 0; i < lines.size(); i++)
            {
                auto& line = lines[i];

                // A graph right after its text row stays with it
                const bool attached = line.graphData != nullptr && i > 0 && lines[i - 1].row == line.row - 1;

                if (i > 0)
                {
                    if (horizontal)
                    {
                        ImGui::SameLine(0.0f, 0.0f);

                        if (!attached)
                        {
                            drawLine(sepLine);
                            ImGui::SameLine(0.0f, 0.0f);
                        }
                    }
                    else
                    {
                        ImGui::Dummy(ImVec2(0.0f, attached ? itemSpacingY : rowSpacing));
                    }
                }

                if (line.graphData != nullptr)
                {
                    ImGui::PlotLines(
                        line.graphId, [](void* rb, int idx) -> float
                        { return static_cast<RingBuffer<float, plotWidth>*>(rb)->At(idx); }, line.graphData, plotWidth,
                        0, nullptr, 0.0f, line.graphMax, plotSize);
                }
                else if (line.lowLatency)
                {
                    drawLowLatency();
                }
                else
                {
                    drawLine(line);
                }
            }

            ImGui::PopStyleVar();

            if (gOverlayFont != nullptr)
                ImGui::PopFont();
            else
                ImGui::PopFontSize();
        }

        // Restore the style
        if (!config->OverlaysUseTheme.value_or_default())
            ImGui::PopStyleColor(5);
        else
            ImGui::PopStyleColor(2);

        // Get size for postioning
        overlaySize = ImGui::GetWindowSize();

        ImGui::End();

        ImGui::PopStyleVar(); // Rounding

        if (stylePushed)
            ImGui::PopStyleVar(7);

        // Where the overlay sits in the free space for each FpsOverlayPos, 0 left/top to 1 right/bottom
        static constexpr ImVec2 alignments[] = { { 0.0f, 0.0f }, { 1.0f, 0.0f }, { 0.0f, 1.0f },
                                                 { 1.0f, 1.0f }, { 0.5f, 0.0f }, { 0.5f, 1.0f } };

        const auto position = config->FpsOverlayPosition.value_or_default();
        const float alignX =
            position == FpsOverlayPos_Custom ? config->FpsOverlayPosX.value_or_default() : alignments[position].x;
        const float alignY =
            position == FpsOverlayPos_Custom ? config->FpsOverlayPosY.value_or_default() : alignments[position].y;

        // Prevent overlapping with splash message at the bottom
        auto freeHeight = io.DisplaySize.y - overlaySize.y;
        if (alignY > 0.5f && !config->DisableSplash.value_or_default() && now > splashStart && now < splashLimit)
            freeHeight -= splashSize.y;

        overlayPosition.x = std::round(std::max(0.0f, io.DisplaySize.x - overlaySize.x) * alignX);
        overlayPosition.y = std::round(std::max(0.0f, freeHeight) * alignY);
    }
}

void MenuCommon::RenderMainMenuHeaderMessages(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& menuResScale = ctx.menuResScale;
    auto& versionStatus = ctx.versionStatus;
    auto& currentVersionText = ctx.currentVersionText;

    if (!_showMipmapCalcWindow && !_showHudlessWindow && !ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow))
        ImGui::SetWindowFocus();

    if (config->MenuScale.has_value())
    {
        _selectedScale = ((int) (menuResScale * 10.0f)) - 4;
    }
    else
    {
        _selectedScale = 0;
    }

    if (versionStatus.completed)
    {
        if (versionStatus.updateAvailable && !versionStatus.latestTag.empty())
        {
            ImGui::Spacing();
            ImGui::TextColored(StatusWarn(), LOC(UI_Update_available_s_current_s), versionStatus.latestTag.c_str(),
                               currentVersionText.c_str());

            if (!versionStatus.latestUrl.empty())
            {
                ImGui::SameLine();
                ImGui::TextLinkOpenURL(LOC(UI_Open_release_page), versionStatus.latestUrl.c_str());
            }

            ImGui::Spacing();
        }
        else if (!versionStatus.error.empty())
        {
            LOG_ERROR("Version check failed: {0}", versionStatus.error);
            versionStatus.error.clear();

            // The status is copied from State every frame
            std::scoped_lock lock(state.versionCheckMutex);
            state.versionCheckError.clear();
        }
        // Disabled error message
        // else if (!versionStatus.error.empty())
        //{
        //    ImGui::Spacing();
        //    ImGui::TextColored(toneMapColor(ImVec4(1.f, 0.4f, 0.f, 1.f)), "%s", versionStatus.error.c_str());
        //    ImGui::Spacing();
        //}
    }

    if (state.nvngxIniDetected)
    {
        ImGui::Spacing();
        ImGui::PushStyleColor(ImGuiCol_Text, StatusError());
        ImGui::TextWrapped(LOC(UI_nvngx_ini_detected_please_move_over_to_using_OptiScaler_ini_));
        ImGui::PopStyleColor();
        ImGui::Spacing();
    }
}

void MenuCommon::RenderUpscalerStateMessage(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;
    auto& menuResScale = ctx.menuResScale;
    auto& primaryGpu = *ctx.primaryGpu;

    // No active upscaler message
    if (currentFeature == nullptr || !currentFeature->IsInited())
    {
        ImGui::Spacing();

        if (config->UseHQFont.value_or_default())
            ImGui::PushFontSize(std::round(fontSize * menuResScale * 2.0f));
        else
            ImGui::SetWindowFontScale(menuResScale * 2.0f);

        if (state.nvngxExists || state.nvngxReplacement.has_value() ||
            (state.libxessExists || XeSSProxy::Module() != nullptr))
        {
            ImGui::Spacing();

            std::vector<std::string> upscalers;

            if (state.fsrHooks)
                upscalers.push_back("FSR");

            if (state.nvngxExists || state.nvngxReplacement.has_value() || primaryGpu.dlssCapable)
                upscalers.push_back("DLSS");

            if (state.libxessExists || XeSSProxy::Module() != nullptr)
                upscalers.push_back("XeSS");

            auto joined = upscalers | std::views::join_with(std::string { " or " });

            std::string joinedUpscalers(joined.begin(), joined.end());

            ImGui::PushTextWrapPos(0.0f);
            ImGui::Text("Please select %s as upscaler from game options and load a save game to enable Opti settings.\n"
                        "Upscalers don't always work in menus.",
                        joinedUpscalers.c_str());
            ImGui::PopTextWrapPos();

            if (config->UseHQFont.value_or_default())
                ImGui::PopFontSize();
            else
                ImGui::SetWindowFontScale(menuResScale);

            ImGui::Spacing();

            RenderDetectedCard(ctx);

            ImGui::Spacing();
        }
        else
        {
            ImGui::Spacing();
            ImGui::PushTextWrapPos(0.0f);
            ImGui::Text(LOC(TXT_Can_T_Find_Nvngx_10));
            ImGui::PopTextWrapPos();
            ImGui::Spacing();

            if (config->UseHQFont.value_or_default())
                ImGui::PopFont();
            else
                ImGui::SetWindowFontScale(menuResScale);

            RenderDetectedCard(ctx);
        }
    }
    else if (currentFeature->IsFrozen())
    {
        ImGui::Spacing();

        if (config->UseHQFont.value_or_default())
            ImGui::PushFontSize(std::round(fontSize * menuResScale * 2.0f));
        else
            ImGui::SetWindowFontScale(menuResScale * 2.0f);

        ImGui::PushTextWrapPos(0.0f);
        ImGui::Text(LOC(UI_s_active_but_currently_not_used_by_the_game_nPlease_load_int),
                    currentFeature->Name().c_str());
        ImGui::PopTextWrapPos();

        if (config->UseHQFont.value_or_default())
            ImGui::PopFont();
        else
            ImGui::SetWindowFontScale(menuResScale);
    }
}

static const char* ApiName(API api);

void MenuCommon::RenderActiveUpscalerSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;
    auto& menuResScale = ctx.menuResScale;
    auto& primaryGpu = *ctx.primaryGpu;

    // UPSCALERS -----------------------------
    SectionTitle(LOC(Upscalers));

    GetCurrentBackendInfo(state.api, currentBackend, &currentBackendName);

    ImGui::PushItemWidth(150.0f * menuResScale);

    const bool usesDlssd = currentFeature->GetUpscalerType() == Upscaler::DLSSD;
    const bool rayReconstruction = state.rrHandles.contains(currentFeature->Handle()->Id);

    // Ray reconstruction picks its DLSSD, DX11's runs only on NVIDIA's
    if (rayReconstruction && (state.api == DX12 || state.api == Vulkan))
    {
        RenderRRUpscalerCombo(state.api, currentFeature);
    }
    else if (usesDlssd)
    {
        ImGui::AlignTextToFramePadding();
        ImGui::TextUnformatted(NameWithVersion(currentFeature).c_str());
    }
    else
    {
        switch (state.api)
        {
        case DX11:
            AddDx11Backends(currentBackend);
            break;

        case DX12:
            AddDx12Backends(currentBackend);
            break;

        default:
            AddVulkanBackends(currentBackend);
        }
    }

    ImGui::PopItemWidth();

    ImGui::SameLine();
    ImGui::TextDisabled(LOC(UI_dx_d_dx_d_2fx), currentFeature->RenderWidth(), currentFeature->RenderHeight(),
                        currentFeature->TargetWidth(), currentFeature->TargetHeight(),
                        (float) currentFeature->TargetWidth() / (float) currentFeature->RenderWidth());

    // The game leaves ray traced effects noisy for ray reconstruction to clean up
    if (rayReconstruction && !usesDlssd && config->RRAllowRegularUpscalers.value_or_default())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, StatusWarn());
        ImGui::TextWrapped(
            "DLSSD is on in the game, but this upscaler can't denoise.\nImage may look grainy or noisy.");
        ImGui::PopStyleColor();
    }

    // Ray reconstruction's upscaler can be picked before the game starts it, shown for games that look for it
    if (!rayReconstruction && state.rrRequested && config->RRAllowRegularUpscalers.value_or_default() &&
        (state.api == DX12 || state.api == Vulkan))
    {
        ImGui::PushItemWidth(150.0f * menuResScale);
        RenderRROfferCombo(state.api);
        ImGui::PopItemWidth();
    }

    if (currentFeature->DynamicResolution())
    {
        ImGui::SameLine();
        ImGui::TextDisabled(LOC(UI_DRS));
    }

    if (!usesDlssd && !rayReconstruction)
    {
        if (state.newBackend != Upscaler::Reset && state.newBackend != currentBackend)
        {
            if (state.newBackend == Upscaler::XeSS)
            {
                // Reseting them for xess
                config->DisableReactiveMask.reset();
                config->DlssReactiveMaskBias.reset();
            }

            MARK_ALL_BACKENDS_CHANGED();
        }
    }

    if (primaryGpu.dlssCapable && !state.NVNGX_DLSS_Path.has_value())
    {
        ImGui::Spacing();
        ImGui::TextColored(StatusWarn(), LOC(UI_nvngx_dlss_dll_not_found_DLSS_disabled));
    }

    // Dx11 with Dx12
    if (state.api == DX11 && currentFeature->IsWithDx12())
    {
        ImGui::Spacing();
        if (auto ch = ScopedCollapsingHeader(LOC(Dx11_with_Dx12)); ch.IsHeaderOpen())
        {
            ScopedIndent indent {};
            ImGui::Spacing();

            if (bool dontUseNTShared = config->DontUseNTShared.value_or_default();
                ImGui::Checkbox(LOC(CHK_Don_T_Use_Ntshared_27), &dontUseNTShared))
                config->DontUseNTShared = dontUseNTShared;

            ImGui::Spacing();
            ImGui::Spacing();
        }
    }

    if (state.api == Vulkan && currentFeature->IsWithDx12())
    {
        ImGui::Spacing();
        if (auto ch = ScopedCollapsingHeader(LOC(Vulkan_with_Dx12)); ch.IsHeaderOpen())
        {
            ScopedIndent indent {};
            ImGui::Spacing();

            if (bool inputsUseCopy = config->VulkanUseCopyForInputs.value_or_default();
                ImGui::Checkbox(LOC(CHK_Use_Copyresource_For_Inputs_28), &inputsUseCopy))
                config->VulkanUseCopyForInputs = inputsUseCopy;

            if (bool outputUseCopy = config->VulkanUseCopyForOutput.value_or_default();
                ImGui::Checkbox(LOC(CHK_Use_Copyresource_For_Output_29), &outputUseCopy))
                config->VulkanUseCopyForOutput = outputUseCopy;

            ImGui::Spacing();
            ImGui::Spacing();
        }
    }

    // UPSCALER SPECIFIC -----------------------------

    // XeSS -----------------------------
    if (currentBackend == Upscaler::XeSS && !usesDlssd)
    {
        ImGui::Spacing();
        if (auto ch = ScopedCollapsingHeader(LOC(XeSS_Settings)); ch.IsHeaderOpen())
        {
            ScopedIndent indent {};
            ImGui::Spacing();

            const char* models[] = { "KPSS", "SPLAT", "MODEL_3", "MODEL_4", "MODEL_5", "MODEL_6" };
            auto configModes = config->NetworkModel.value_or_default();

            if (configModes < 0 || configModes > 5)
                configModes = 0;

            const char* selectedModel = models[configModes];

            if (ImGui::BeginCombo(LOC(LBL_Network_Models), selectedModel))
            {
                for (int n = 0; n < 6; n++)
                {
                    if (ImGui::Selectable(models[n], (config->NetworkModel.value_or_default() == n)))
                    {
                        config->NetworkModel = n;
                        state.newBackend = SameBackend(currentBackend);
                        MARK_ALL_BACKENDS_CHANGED();
                    }
                }

                ImGui::EndCombo();
            }
            ShowTooltip(LOC(TIP_Likely_Doesn_T_Do_202));

            if (bool dbg = state.xessDebug; ImGui::Checkbox(LOC(CHK_Dump_Shift_Del_30), &dbg))
                state.xessDebug = dbg;

            ImGui::SameLine(0.0f, 6.0f);
            int dbgCount = state.xessDebugFrames;

            ImGui::PushItemWidth(95.0f * menuResScale);
            if (ImGui::InputInt(LOC(UI_Frames), &dbgCount))
            {
                if (dbgCount < 4)
                    dbgCount = 4;
                else if (dbgCount > 999)
                    dbgCount = 999;

                state.xessDebugFrames = dbgCount;
            }

            ImGui::PopItemWidth();

            ImGui::Spacing();
            ImGui::Spacing();
        }
    }

    constexpr auto fsr3DebugViewTooltip = "Top left: Dilated Motion Vectors\n"
                                          "Top middle: Protected Areas\n"
                                          "Top right: Dilated Depth\n"
                                          "Middle: Upscaled frame\n"
                                          "Bottom left: Disocclusion mask\n"
                                          "Bottom middle: Reactiveness\n"
                                          "Bottom right: Detail Protection Takedown";

    // FFX -----------------
    if (!usesDlssd && (currentBackend == Upscaler::FFX || currentBackend == Upscaler::FFX_on12))
    {
        const auto customName = CustomProviders::Selected(state.api, currentBackend);
        const auto customUpscaler = SelectedCustomFfx(customName, currentBackend, state.api);
        auto& upscalerIndex = config->FfxUpscalerIndexFor(customName);

        // A custom upscaler only has a list once it got loaded
        if (auto versions = customName.empty()          ? GetFfxVersionList(currentBackend, state.api)
                            : customUpscaler != nullptr ? &customUpscaler->versions
                                                        : nullptr;
            versions != nullptr && !versions->names.empty())
        {
            ImGui::Spacing();

            const int ffxIndex = upscalerIndex.value_or_default();
            auto currentName =
                ffxIndex >= 0 && ffxIndex < versions->names.size() ? StrFmt(LOC(UI_FSR_s), versions->names[ffxIndex]) : "";

            ImGui::PushItemWidth(150.0f * menuResScale);

            if (ImGui::BeginCombo(LOC(LBL_FFX_Upscaler), currentName.c_str()))
            {
                for (auto n : FfxVersionsNewestFirst(*versions))
                {
                    if (ImGui::Selectable(StrFmt(LOC(UI_FSR_s_d), versions->names[n], n).c_str(), ffxIndex == n) &&
                        ffxIndex != n)
                    {
                        upscalerIndex = n;
                        state.newBackend = SameBackend(currentBackend);
                        MARK_ALL_BACKENDS_CHANGED();
                    }
                }

                ImGui::EndCombo();
            }

            ImGui::PopItemWidth();

            ShowTooltip(LOC(TIP_List_Of_Upscalers_Reported_203));
        }

        auto majorFsrVersion = currentFeature->Version().major;

        if (majorFsrVersion >= 4)
        {
            ImGui::Spacing();

            // Colorspaces
            const char* colorSpaces[] = { LOC(LBL_Linear_Default), "Non-Linear", LOC(LBL_Non_Linear_sRGB), LOC(UI_Non_Linear_PQ) };
            int currentColorSpace = 0;
            if (config->FsrNonLinearPQ.value_or_default())
                currentColorSpace = 3;
            else if (config->FsrNonLinearSRGB.value_or_default())
                currentColorSpace = 2;
            else if (config->FsrNonLinearColorSpace.value_or_default())
                currentColorSpace = 1;

            ImGui::SetNextItemWidth(150.0f * menuResScale);
            if (ImGui::Combo(LOC(UI_Input_Color_Space), &currentColorSpace, colorSpaces, IM_ARRAYSIZE(colorSpaces)))
            {
                bool isSrgb = (currentColorSpace == 2);
                bool isPq = (currentColorSpace == 3);

                config->FsrNonLinearSRGB = isSrgb;
                config->FsrNonLinearPQ = isPq;

                if (isSrgb || isPq)
                {
                    config->FsrNonLinearColorSpace.set_volatile_value(true);
                }
                else if (currentColorSpace == 1) // Just non-Linear
                {
                    config->FsrNonLinearColorSpace = true;
                }
                else // Linear
                {
                    config->FsrNonLinearColorSpace = false;
                }

                state.newBackend = SameBackend(currentBackend);
                MARK_ALL_BACKENDS_CHANGED();
            }
            ShowTooltip(LOC(Select_the_input));

            // FSR 4 Presets
            const char* presets[] = {
                LOC(Default), LOC(UI_Preset_0), LOC(UI_Preset_1), LOC(UI_Preset_2), LOC(UI_Preset_3), LOC(UI_Preset_4), LOC(UI_Preset_5)
            };
            int currentPresetIdx = config->Fsr4Preset.has_value() ? config->Fsr4Preset.value() + 1 : 0;

            if (currentPresetIdx < 0 || currentPresetIdx >= IM_ARRAYSIZE(presets))
                currentPresetIdx = 0;

            ImGui::SetNextItemWidth(150.0f * menuResScale);
            if (ImGui::Combo(LOC(UI_FSR4_Preset), &currentPresetIdx, presets, IM_ARRAYSIZE(presets)))
            {
                if (currentPresetIdx == 0)
                    config->Fsr4Preset.reset();
                else
                    config->Fsr4Preset = currentPresetIdx - 1;

                state.newBackend = SameBackend(currentBackend);
                MARK_ALL_BACKENDS_CHANGED();
            }
            ShowTooltip(LOC(Each_internal_FSR4));

            // Display the active preset right next to the combo box instead of using a table
            // The active preset can be left over from another module, so a custom one checks its own hook
            ImGui::SameLine();
            if (customUpscaler != nullptr && !FSR4Presets::IsModuleHooked(customUpscaler->module.dll))
                ImGui::TextDisabled(LOC(UI_Failed_to_hook));
            else if (state.currentFsr4Preset.has_value())
                ImGui::TextDisabled(LOC(UI_Active_d), state.currentFsr4Preset.value());
            else if (FSR4Presets::IsAnyModuleHooked())
                ImGui::TextColored(StatusWarn(), LOC(UI_Potential_FSR3_fallback));
            else
                ImGui::TextDisabled(LOC(UI_Failed_to_hook));

            // FSR 4 Models of the module running the selected upscaler, only listed when the GPU can run more than one
            HMODULE upscalerModule = nullptr;
            if (customUpscaler != nullptr)
                upscalerModule = customUpscaler->module.dll;
            else if (auto versions = GetFfxVersionList(currentBackend, state.api);
                     versions != nullptr && config->FfxUpscalerIndex.value_or_default() >= 0 &&
                     config->FfxUpscalerIndex.value_or_default() < versions->ids.size())
            {
                auto versionId = versions->ids[config->FfxUpscalerIndex.value_or_default()];

                if (versions == &state.ffxUpscalerVersionsDx12)
                    upscalerModule = FfxApiProxy::UpscalerModule(versionId);
                else if (versions == &state.ffxUpscalerVersionsVk)
                    upscalerModule = FfxApiProxy::UpscalerModuleVk(versionId);
            }

            if (auto models = FSR4Models::SelectableModels(upscalerModule); !models.empty())
            {
                auto& selectedModel = config->Fsr4Model;
                auto selectedName = selectedModel.has_value() ? FSR4Models::Name(selectedModel.value()) : LOC(UI_Auto);
                bool modelChanged = false;

                ImGui::SetNextItemWidth(150.0f * menuResScale);
                if (ImGui::BeginCombo(LOC(UI_FSR4_Model), selectedName))
                {
                    if (ImGui::Selectable(LOC(UI_Auto), !selectedModel.has_value()) && selectedModel.has_value())
                    {
                        selectedModel.reset();
                        modelChanged = true;
                    }

                    for (auto model : models)
                    {
                        if (ImGui::Selectable(FSR4Models::Name(model), selectedModel == model) &&
                            selectedModel != model)
                        {
                            selectedModel = model;
                            modelChanged = true;
                        }
                    }

                    ImGui::EndCombo();
                }

                if (modelChanged)
                {
                    state.newBackend = SameBackend(currentBackend);
                    MARK_ALL_BACKENDS_CHANGED();
                }

                ShowTooltip("FSR 4 network to run.\n"
                            "Auto lets FFX pick the best one the GPU supports.");

                ImGui::SameLine();
                if (auto activeModel = FSR4Models::ActiveModel(upscalerModule))
                    ImGui::TextDisabled(LOC(UI_Active_s), FSR4Models::Name(*activeModel));
                else
                    ImGui::TextColored(StatusWarn(), LOC(UI_Potential_FSR3_fallback));
            }
        }

        if (majorFsrVersion >= 3)
        {
            ImGui::Spacing();

            bool debugView = config->FsrDebugView.value_or_default();
            if (ImGui::Checkbox(LOC(CHK_Upscaler_Debug_View_31), &debugView))
            {
                config->FsrDebugView = debugView;

                // FSR 4's debug view requires backend reinit
                if (majorFsrVersion > 3)
                {
                    state.newBackend = SameBackend(currentBackend);
                    MARK_ALL_BACKENDS_CHANGED();
                }
            }

            if (majorFsrVersion > 3)
            {
                ShowTooltip(LOC(Top_left_Dilated));
            }
            else
            {
                ShowTooltip(fsr3DebugViewTooltip);
            }

            if (majorFsrVersion > 3)
            {
                ImGui::SameLine(0.0f, 20.0f * menuResScale);
                bool fsr4wm = config->Fsr4EnableWatermark.value_or_default();
                if (ImGui::Checkbox(LOC(Watermark), &fsr4wm))
                {
                    LOG_DEBUG("FSR4 Watermark set to {}", fsr4wm);
                    config->Fsr4EnableWatermark = fsr4wm;
                    Util::SetFlagEnvVar(L"MLSR-WATERMARK", fsr4wm);

                    // The driver reads the variable on context creation
                    state.newBackend = SameBackend(currentBackend);
                    MARK_ALL_BACKENDS_CHANGED();
                }
            }
        }

        if (currentFeature->Version() >= feature_version { 3, 1, 1 } &&
            currentFeature->Version() < feature_version { 4, 0, 0 })
        {
            ImGui::Spacing();

            if (currentFeature != nullptr)
            {
                ImGui::Text(LOC(TXT_Fsr_3_1_Presets_11));

                ImGui::SameLine(0.0f, 6.0f);

                // This will be applied by default
                if (ImGui::Button(LOC(Stability)))
                {
                    config->FsrVelocity = 0.5f;
                    config->FsrReactiveScale = 0.333f;
                    config->FsrShadingScale = 0.5f;
                    config->FsrAccAddPerFrame = 0.333f;
                    config->FsrMinDisOccAcc = 0.0f;
                }

                ImGui::SameLine(0.0f, 6.0f);

                if (ImGui::Button(LOC(Motion)))
                {
                    config->FsrVelocity = 1.0f;
                    config->FsrReactiveScale = 0.666f;
                    config->FsrShadingScale = 1.0f;
                    config->FsrAccAddPerFrame = 0.166f;
                    config->FsrMinDisOccAcc = 0.0f;
                }

                ImGui::SameLine(0.0f, 6.0f);

                if (ImGui::Button(LOC(Default)))
                {
                    config->FsrVelocity = 1.0f;
                    config->FsrReactiveScale = 1.0f;
                    config->FsrShadingScale = 1.0f;
                    config->FsrAccAddPerFrame = 0.333f;
                    config->FsrMinDisOccAcc = -0.333f;
                }
            }

            ImGui::Spacing();

            if (auto ch = ScopedCollapsingHeader(LOC(FSR_3_Upscaler)); ch.IsHeaderOpen())
            {
                ScopedIndent indent {};
                ImGui::Spacing();
                ImGui::Spacing();

                ImGui::PushItemWidth(220.0f * menuResScale);

                float velocity = config->FsrVelocity.value_or_default();
                if (ImGui::SliderFloat(LOC(UI_Velocity_Factor), &velocity, 0.00f, 1.0f, "%.2f"))
                    config->FsrVelocity = velocity;

                ShowTooltip("Value of 0.0f can improve temporal stability of bright pixels\n"
                            "Lower values are more stable with ghosting\n"
                            "Higher values are more pixelly, but less ghosting");

                if (currentFeature->Version() >= feature_version { 3, 1, 4 })
                {
                    // Reactive Scale
                    float reactiveScale = config->FsrReactiveScale.value_or_default();
                    if (ImGui::SliderFloat(LOC(UI_Reactive_Scale), &reactiveScale, 0.0f, 1.0f, "%.3f"))
                        config->FsrReactiveScale = reactiveScale;

                    ShowTooltip(LOC(Meant_for_development));

                    // Shading Scale
                    float shadingScale = config->FsrShadingScale.value_or_default();
                    if (ImGui::SliderFloat(LOC(UI_Shading_Scale), &shadingScale, 0.0f, 1.0f, "%.3f"))
                        config->FsrShadingScale = shadingScale;

                    ShowTooltip(LOC(Increasing_this_scales));

                    // Accumulation Added Per Frame
                    float accAddPerFrame = config->FsrAccAddPerFrame.value_or_default();
                    if (ImGui::SliderFloat(LOC(UI_Acc_Added_Per_Frame), &accAddPerFrame, 0.0f, 1.0f, "%.3f"))
                        config->FsrAccAddPerFrame = accAddPerFrame;

                    ShowTooltip(LOC(Corresponds_to_amount));

                    // Min Disocclusion Accumulation
                    float minDisOccAcc = config->FsrMinDisOccAcc.value_or_default();
                    if (ImGui::SliderFloat(LOC(UI_Min_Disocclusion_Acc), &minDisOccAcc, -1.0f, 1.0f, "%.3f"))
                        config->FsrMinDisOccAcc = minDisOccAcc;

                    ShowTooltip(LOC(Increasing_this_value));
                }

                ImGui::PopItemWidth();

                ImGui::Spacing();
                ImGui::Spacing();
            }
        }
    }

    // FSR 3.1 Dx11 -----------------
    if (!usesDlssd && currentBackend == Upscaler::FSR31)
    {
        ImGui::Spacing();

        bool debugView = config->FsrDebugView.value_or_default();
        if (ImGui::Checkbox(LOC(CHK_Upscaler_Debug_View_31), &debugView))
            config->FsrDebugView = debugView;

        ShowTooltip(fsr3DebugViewTooltip);
    }

    // DLSS -----------------
    if ((config->DLSSEnabled.value_or_default() &&
         (currentBackend == Upscaler::DLSS || currentBackend == Upscaler::DLSS_on12) &&
         currentFeature->Version().major > 2) ||
        usesDlssd)
    {
        ImGui::Spacing();

        auto overridden = usesDlssd ? state.dlssdPresetsOverriddenExternally : state.dlssPresetsOverriddenExternally;

        if (overridden)
        {
            ImGui::TextColored(StatusWarn(), LOC(UI_Presets_are_overridden_externally));
            ShowTooltip(LOC(This_usually_happens));
            // ImGui::Text(LOC(TT_External_Override_Note)
            //             "but you need to Save Settings and restart the game");

            ImGui::Spacing();
        }

        if (usesDlssd)
        {
            if (bool pOverride = config->DLSSDRenderPresetOverride.value_or_default();
                ImGui::Checkbox(LOC(CHK_Render_Presets_Override_32), &pOverride))
                config->DLSSDRenderPresetOverride = pOverride;

            ShowTooltip(LOC(Each_render_preset));

            /*
            auto currentPresetIndex = GetPresetIndex(currentFeature, true);

            if (currentPresetIndex == 0)
                ImGui::Text(LOC(TXT_Current_Preset_Default_12));
            else
                ImGui::Text(LOC(UI_Current_Preset_c), 64 + currentPresetIndex);
            */

            ImGui::BeginDisabled(!config->DLSSDRenderPresetOverride.value_or_default() /*|| overridden*/);
            ImGui::PushItemWidth(135.0f * menuResScale);

            AddDLSSDRenderPreset(LOC(UI_Override_Preset), &comboPreset);

            ImGui::PopItemWidth();
            ImGui::EndDisabled();
        }
        else
        {
            if (bool pOverride = config->RenderPresetOverride.value_or_default();
                ImGui::Checkbox(LOC(CHK_Render_Presets_Override_32), &pOverride))
                config->RenderPresetOverride = pOverride;

            ShowTooltip("Each render preset has it strengths and weaknesses\n"
                        "Override to potentially improve image quality\n"
                        "Press Apply after enable/disable");

            /*
            auto currentPresetIndex = GetPresetIndex(currentFeature, false);

            if (currentPresetIndex == 0)
                ImGui::Text(LOC(TXT_Current_Preset_Default_12));
            else
                ImGui::Text(LOC(UI_Current_Preset_c), 64 + currentPresetIndex);
            */

            ImGui::BeginDisabled(!config->RenderPresetOverride.value_or_default() /*|| overridden*/);

            ImGui::PushItemWidth(135.0f * menuResScale);

            AddDLSSRenderPreset(LOC(UI_Override_Preset), &comboPreset);

            ImGui::PopItemWidth();
            ImGui::EndDisabled();
        }

        ImGui::SameLine(0.0f, 6.0f);

        if (ImGui::Button(LOC(BTN_Apply_Changes_135)))
        {
            LOG_DEBUG("Applying DLSS/DLSSD preset override changes, preset index: {}", comboPreset.value_or_default());

            if (usesDlssd)
            {
                config->DLSSDRenderPresetForAll = comboPreset.value_or_default();

                // Ray reconstruction keeps its DLSSD, the upscalers their configured one
                state.newBackend = rayReconstruction ? Upscaler::Reset : Upscaler::DLSSD;
            }
            else
            {
                config->RenderPresetForAll = comboPreset.value_or_default();
                state.newBackend = SameBackend(currentBackend);
            }

            MARK_ALL_BACKENDS_CHANGED();
        }

        ImGui::Spacing();

        if (auto ch = ScopedCollapsingHeader(usesDlssd ? LOC(LBL_Adv_DLSSD_Settings) : LOC(LBL_Adv_DLSS_Settings));
            ch.IsHeaderOpen())
        {
            ScopedIndent indent {};
            ImGui::Spacing();

            bool appIdOverride = config->UseGenericAppIdWithDlss.value_or_default();
            if (ImGui::Checkbox(LOC(CHK_Use_Generic_App_Id_33), &appIdOverride))
                config->UseGenericAppIdWithDlss = appIdOverride;

            ShowTooltip("Use generic appid with NGX\n"
                        "Fixes OptiScaler preset override not working with certain games\n"
                        "Requires a game restart");

            ImGui::BeginDisabled(!config->RenderPresetOverride.value_or_default() || overridden);
            ImGui::Spacing();
            ImGui::PushItemWidth(135.0f * menuResScale);

            if (usesDlssd)
            {
                AddDLSSDRenderPreset(LOC(UI_DLAA_Preset), &config->DLSSDRenderPresetDLAA);
                AddDLSSDRenderPreset(LOC(UI_UltraQ_Preset), &config->DLSSDRenderPresetUltraQuality);
                AddDLSSDRenderPreset(LOC(UI_Quality_Preset), &config->DLSSDRenderPresetQuality);
                AddDLSSDRenderPreset(LOC(UI_Balanced_Preset), &config->DLSSDRenderPresetBalanced);
                AddDLSSDRenderPreset(LOC(UI_Perf_Preset), &config->DLSSDRenderPresetPerformance);
                AddDLSSDRenderPreset(LOC(UI_UltraP_Preset), &config->DLSSDRenderPresetUltraPerformance);
            }
            else
            {
                AddDLSSRenderPreset(LOC(UI_DLAA_Preset), &config->RenderPresetDLAA);
                AddDLSSRenderPreset(LOC(UI_UltraQ_Preset), &config->RenderPresetUltraQuality);
                AddDLSSRenderPreset(LOC(UI_Quality_Preset), &config->RenderPresetQuality);
                AddDLSSRenderPreset(LOC(UI_Balanced_Preset), &config->RenderPresetBalanced);
                AddDLSSRenderPreset(LOC(UI_Perf_Preset), &config->RenderPresetPerformance);
                AddDLSSRenderPreset(LOC(UI_UltraP_Preset), &config->RenderPresetUltraPerformance);
            }
            ImGui::PopItemWidth();
            ImGui::EndDisabled();

            ImGui::Spacing();
            ImGui::Spacing();
        }
    }
}

// Draws a line wherever an integer slider switches to the next step, so it's visible where to click
static void DrawSliderStepMarks(const ImVec2& framePos, float frameWidth, float frameHeight, int stepCount)
{
    if (stepCount < 2)
        return;

    constexpr float grabPadding = 2.0f; // Hardcoded in ImGui as well
    const float sliderSize = frameWidth - grabPadding * 2.0f;
    const float grabSize =
        std::min(std::max(sliderSize / (float) stepCount, ImGui::GetStyle().GrabMinSize), sliderSize);
    const float firstGrabCenter = framePos.x + grabPadding + grabSize * 0.5f;
    const float grabTravel = (sliderSize - grabSize) / (float) (stepCount - 1);
    const float inset = frameHeight * 0.2f;
    const ImU32 color = ImGui::GetColorU32(ImGuiCol_Text, 0.35f);
    auto drawList = ImGui::GetWindowDrawList();

    for (int step = 1; step < stepCount; step++)
    {
        // Snapped to the pixel grid to keep the 1 px line sharp, AddLine already centers it on the pixel
        const float x = std::floor(firstGrabCenter + grabTravel * ((float) step - 0.5f));
        drawList->AddLine(ImVec2(x, framePos.y + inset), ImVec2(x, framePos.y + frameHeight - inset), color);
    }
}

// Integer slider over [0, stepCount) with step marks and stepName(step) as the shown text
// Clicking a step applies it right away, while dragging only applies the step it's released on,
// so switching e.g. from Off to 4X doesn't go through every mode in between
// Returns true when step got changed
static bool RenderStepSlider(const char* label, int& step, int stepCount,
                             const std::function<std::string(int)>& stepName)
{
    // Step under the grab during a drag, -1 when not dragging
    auto storage = ImGui::GetStateStorage();
    const ImGuiID id = ImGui::GetID(label);
    const int draggedStep = storage->GetInt(id, -1);
    int shownStep = draggedStep >= 0 ? draggedStep : step;

    const ImVec2 sliderPos = ImGui::GetCursorScreenPos();
    const float sliderWidth = ImGui::CalcItemWidth();
    const float sliderHeight = ImGui::GetFrameHeight();
    ImGui::SliderInt(label, &shownStep, 0, stepCount - 1, stepName(shownStep).c_str(), ImGuiSliderFlags_NoInput);
    DrawSliderStepMarks(sliderPos, sliderWidth, sliderHeight, stepCount);

    // The click itself moves the grab on the activation frame, that's the only change applied immediately
    const bool apply = (ImGui::IsItemActivated() || ImGui::IsItemDeactivated()) && shownStep != step;

    if (ImGui::IsItemActive())
        storage->SetInt(id, shownStep);
    else if (draggedStep >= 0)
        storage->SetInt(id, -1);

    if (!apply)
        return false;

    step = shownStep;
    return true;
}

// Slider for overriding the game's FG ratio: Off, Default (no override), 2X, 3X... up to maxInterpolationCount
static bool RenderRatioOverrideSlider(const char* label, CustomOptional<int, NoDefault>& setting,
                                      int maxInterpolationCount)
{
    // 0 = Off, 1 = Default, 2+ = ratio (interpolated frame count + 1)
    int index = 1;
    if (setting.has_value())
        index = setting.value() == 0 ? 0 : std::min(setting.value(), maxInterpolationCount) + 1;

    auto stepName = [](int step) -> std::string
    {
        return step == 0 ? "Off" : step == 1 ? "Default" : std::format("{}X", step);
    };

    if (!RenderStepSlider(label, index, maxInterpolationCount + 2, stepName))
        return false;

    if (index == 1)
        setting.reset();
    else
        setting = index == 0 ? 0 : index - 1;

    return true;
}

// Generated frames per game frame, nullopt while FG is off. 0 is Dynamic MFG's 1X
static std::optional<int> FGGeneratedFrames(State& state)
{
    // Frames of the Nvngx replacements and DLSSG come from the game's DLSSG calls,
    // Flip Meter calls the replacement itself and has its own count
    if (state.activeFgOutput != FGOutput::FlipMeter &&
        (state.activeFgNvngx != FGNvngxReplacement::None || state.activeFgOutput == FGOutput::DLSSG))
    {
        if (state.DlssgDetectedInterpolationCount() > 0)
            return state.DlssgDetectedInterpolationCount();

        return std::nullopt;
    }

    if (auto fg = state.CurrentFG(); fg != nullptr && fg->IsGenerating())
        return (int) fg->GetInterpolatedFrameCount();

    return std::nullopt;
}

static void RenderFGStatus(State& state, Config* config)
{
    const auto input = state.activeFgInput;
    const bool dlssgInput = input == FGInput::DLSSG || input == FGInput::NvngxFG;

    if (dlssgInput && !ReflexHooks::isReflexHooked())
    {
        ImGui::TextColored(StatusError(), LOC(UI_Reflex_not_hooked));
        ImGui::Text(LOC(TXT_If_You_Are_Using_18));
        return;
    }

    const auto generated = FGGeneratedFrames(state);

    ImGui::Text(LOC(UI_Status));
    ImGui::SameLine();

    if (generated.has_value())
    {
        // The game's DLSSG count can be above what the Nvngx replacement generates
        const int replacementMax = Nvngx_FG::getMaxFakeFramesCount();
        const bool overReplacement = state.activeFgNvngx != FGNvngxReplacement::None &&
                                     state.activeFgOutput != FGOutput::FlipMeter && *generated > replacementMax;

        ImGui::TextColored(*generated > 0 && !overReplacement ? StatusOk() : StatusWarn(), LOC(UI_ON_dx), *generated + 1);

        if (overReplacement)
        {
            ImGui::SameLine();
            ImGui::TextColored(StatusWarn(), LOC(UI_the_replacement_supports_up_to_dx), replacementMax + 1);
        }

        return;
    }

    ImGui::TextColored(StatusError(), LOC(UI_OFF));

    // Game side FG that isn't being sent
    bool gameSends = true;
    if (input == FGInput::DLSSG)
        gameSends = (state.fgLastFrame - state.dlssgLastFrame) < 3;
    else if (input == FGInput::NvngxFG)
        gameSends = ReflexHooks::dlssgFrameCountToGenerate() != 0 ||
                    (state.dlssgGameDMFGSupported && config->FGDLSSGOverrideForceDMFG.value_or_default());
    else if (input == FGInput::FSRFG || input == FGInput::FSRFG30)
        gameSends = state.fsrfgInputActive;

    if (!gameSends)
    {
        const char* name = dlssgInput ? "DLSS" : "FSR";
        ImGui::Text("Please select %s Frame Generation in the game options\n"
                    "You might need to select %s first",
                    name, name);
    }
}

void MenuCommon::RenderFrameGenerationSelection(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& menuResScale = ctx.menuResScale;
    auto& primaryGpu = *ctx.primaryGpu;

    /// FG INPUTS

    static std::vector<MenuOption<FGInput>> inputOptions;
    inputOptions.clear();

    // clang-format off

    inputOptions = {
        { FGInput::NoFG, LOC(UI_None) },
        { FGInput::Upscaler, LOC(UI_OptiFG_Upscaler),
            "Upscaler must be enabled\n\nCan be used with any FG Output, but might be imperfect with some\nTo prevent UI glitching, HUDfix required" },
        { FGInput::DLSSG, LOC(UI_DLSSG_via_Streamline),
            "Can be used with any FG Output\n\nRequires enabling DLSS-FG in game settings\nSupports HUDless out of the box\n\nLimited to games that use Streamline" },
        { FGInput::NvngxFG, LOC(UI_DLSSG_via_Nvngx),
            "Limited to variants of FSR FG\n\nRequires enabling DLSS-FG in game settings\nSupports HUDless out of the box\nUses Streamline swapchain for pacing" },
        { FGInput::FSRFG, LOC(UI_FSR_3_1_FG),
            "Can be used with any FG Output\n\nRequires enabling FSR-FG in game settings\nSupports HUDless out of the box" },
        { FGInput::FSRFG30, LOC(UI_FSR_3_0_FG),
            "Can be used with any FG Output\n\nRequires enabling FSR-FG in game settings\nSupports HUDless out of the box" },
        { FGInput::XeFG, "XeFG",
            "Can be used with any FG Output\n\nRequires enabling XeSS-FG in game settings\nSupports HUDless out of the box" }
    };

    // clang-format on

    auto constexpr nvngxInputIndex = (uint32_t) FGInput::NvngxFG;

    // XeFG input requirements
    auto constexpr xefgInputIndex = (uint32_t) FGInput::XeFG;
    inputOptions[xefgInputIndex].set_disabled(state.swapchainApi != API::DX12, LOC(TXT_Unsupported_API));

    // OptiFG requirements
    auto constexpr optiFgIndex = (uint32_t) FGInput::Upscaler;
    // FSR FG's Vulkan dll is loaded with the device
    if (state.swapchainApi != API::Vulkan && !inputOptions[optiFgIndex].disabled &&
        state.activeFgOutput == FGOutput::FSRFG && !FfxApiProxy::IsFsrFgReady() && !ffxInitTried)
    {
        ffxInitTried = true;
        FfxApiProxy::InitFfxDx12();
        inputOptions[optiFgIndex].set_disabled(!FfxApiProxy::IsFsrFgReady(), LOC(TXT_Missing_amd_dll));
    }
    else if (!inputOptions[optiFgIndex].disabled && state.activeFgOutput == FGOutput::XeFG && !xefgInitTried &&
             XeFGProxy::Module() == nullptr)
    {
        xefgInitTried = true;
        XeFGProxy::InitXeFG();
        inputOptions[optiFgIndex].set_disabled(XeFGProxy::Module() == nullptr, LOC(TXT_Missing_libxess_fg));
    }

    // DLSSG inputs requirements
    auto constexpr dlssgInputIndex = (uint32_t) FGInput::DLSSG;
    // inputOptions[dlssgInputIndex].set_disabled(state.streamlineVersion.major == 0, LOC(UI_Game_doesn_t_use_streamline));
    inputOptions[dlssgInputIndex].set_disabled(state.swapchainApi == API::DX11, LOC(TXT_Unsupported_API));

    // FSRFG inputs requirements
    auto constexpr fsrfgInputIndex = (uint32_t) FGInput::FSRFG;
    inputOptions[fsrfgInputIndex].set_disabled(state.swapchainApi != API::DX12, LOC(TXT_Unsupported_API));

    // FSRFG30 inputs requirements
    auto constexpr fsrfg30InputIndex = (uint32_t) FGInput::FSRFG30;
    inputOptions[fsrfg30InputIndex].set_disabled(state.swapchainApi != API::DX12, LOC(TXT_Unsupported_API));

    if (!config->FGInput.has_value())
        config->FGInput = config->FGInput.value_or_default(); // need to have a value before combo

    /// FG OUTPUTS

    static std::vector<MenuOption<FGOutput>> outputOptions;
    outputOptions.clear();

    // clang-format off

    outputOptions = {
        { FGOutput::NoFG, LOC(UI_None) },
        { FGOutput::FSRFG, LOC(UI_FSR_FG), LOC(UI_FSR3_4_FG_RDNA4_autoupgrades_to_FSR4_FG_n_nFSR4_FG_sometimes) },
        { FGOutput::DLSSG, "DLSSG", LOC(UI_DLSSG_output_nCan_be_used_in_conjuction_with_Nukem_s_for_exa) },
        { FGOutput::XeFG, "XeFG", LOC(UI_XeFG_heaviest_but_best_universal_FG_n_nXeFG_3_overall_deals_) },
        { FGOutput::Reprojection, LOC(UI_Reprojection_WIP), LOC(UI_Reprojects_the_game_image_using_new_mouse_data_nKinda_like_R) },
        { FGOutput::FlipMeter, LOC(UI_Flip_Meter_WIP), LOC(UI_Frames_of_a_FG_Nvngx_Replacement_paced_by_AMD_s_driver_flip_) },
    };

    // clang-format on

    // DLSSG output requirements
    auto constexpr dlssgOutputIndex = (uint32_t) FGOutput::DLSSG;
    const bool maySupportDlssg = primaryGpu.vendorId == VendorId::Nvidia;
    const bool hasDlssgReplacement = state.nukemsFgFileAvailable || state.artursFgFileAvailable ||
                                     FfxApiProxy::IsFGReady(false) ||
                                     !CustomProviders::List(CustomProviders::Kind::Dlssg).empty();

    if (!maySupportDlssg && hasDlssgReplacement)
    {
        outputOptions[dlssgOutputIndex].tooltip =
            "No real DLSSG, unsupported hardware\nOnly Nvngx FG replacements available";
    }

    outputOptions[dlssgOutputIndex].set_disabled(!maySupportDlssg && !hasDlssgReplacement,
                                                 "Unsupported hardware and no replacements");

    // For that one case of DX11 DLSSG
    const auto streamlineVersion = state.streamlineVersion;
    const bool nukemsUnsupportedApi =
        state.swapchainApi == API::DX11 &&
        (streamlineVersion == feature_version { 0, 0, 0 } || streamlineVersion > feature_version { 2, 0, 1 });
    inputOptions[nvngxInputIndex].set_disabled(nukemsUnsupportedApi, LOC(TXT_Unsupported_API));

    // XeFG output requirements
    auto constexpr xefgOutputIndex = (uint32_t) FGOutput::XeFG;
    outputOptions[xefgOutputIndex].set_disabled(state.swapchainApi == API::Vulkan, LOC(TXT_Unsupported_API));

    // Flip meter output requirements, the driver is only asked once its swapchain gets created
    auto constexpr flipMeterOutputIndex = (uint32_t) FGOutput::FlipMeter;
    outputOptions[flipMeterOutputIndex].set_disabled(state.swapchainApi != API::DX12, LOC(TXT_Unsupported_API));
    outputOptions[flipMeterOutputIndex].set_disabled(primaryGpu.vendorId != VendorId::AMD, LOC(TXT_Unsupported_hardware));
    outputOptions[flipMeterOutputIndex].set_disabled(!state.amdFlipMeterSupported.value_or(true),
                                                     "Flip metering isn't supported by the driver");
    // Unsupported FG input selected
    const auto currentInputIndex = (uint32_t) state.activeFgInput;
    if (config->FGInput != FGInput::NoFG && inputOptions.size() > currentInputIndex &&
        inputOptions[currentInputIndex].disabled && state.activeFgInput == config->FGInput)
    {
        LOG_WARN("Resetting FGInput to NoFG: {}", inputOptions[currentInputIndex].label);
        config->FGInput = FGInput::NoFG;

        // Changing active can be dangerous but we are talking about an unsupported mode
        // which shouldn't even actually have taken affect
        state.activeFgInput = FGInput::NoFG;
    }

    // Unsupported FG output selected
    const auto currentOutputIndex = (uint32_t) state.activeFgOutput;
    if (config->FGOutput != FGOutput::NoFG && outputOptions.size() > currentOutputIndex &&
        outputOptions[currentOutputIndex].disabled && state.activeFgOutput == config->FGOutput)
    {
        LOG_WARN("Resetting FGOutput to NoFG: {}", outputOptions[currentOutputIndex].label);
        config->FGOutput = FGOutput::NoFG;
        state.activeFgOutput = FGOutput::NoFG;
    }

    if (!config->FGOutput.has_value())
        config->FGOutput = config->FGOutput.value_or_default(); // need to have a value before combo

    /// FG NVNGX REPLACEMENT

    static std::vector<MenuOption<FGNvngxReplacement>> nvngxOptions;
    nvngxOptions.clear();

    // clang-format off

    nvngxOptions = {
        { FGNvngxReplacement::None, LOC(UI_None_Real_DLSSG_2), LOC(UI_Real_DLSSG_For_RTX_40xx_and_above)},
        { FGNvngxReplacement::Nukems, LOC(UI_Nukem_s), LOC(UI_FSR_3_FG) },
        { FGNvngxReplacement::Arturs, "Enabler", LOC(UI_FSR_3_MFG_mod) },
        { FGNvngxReplacement::FFX, "FSR 3/4 FG", "FSR 3/4 FG using the FFX upgrade\n\n"
                                                 "Partially based on Nukems, uses SL swapchain\n"
                                                 "Possibly better performance and frame pacing compared to FSR-FG output"},
        { FGNvngxReplacement::Combo, "FFX + Enabler", "Use if FSR4-FG is supported, otherwise stick to Enabler\n\n"
                                                      "FFX used for the middle fake frame, Enabler for the rest\n\n"
                                                      "2x - FFX\n3x - Enabler\n4x - FFX + Enabler\n5x - Enabler\n6x - FFX + Enabler\n\n"
                                                      "Due to pacing, only odd number of fake frames are able to use FFX"},
    };

    // clang-format on

    // Custom ones go after the rest, each one is a value from FGNvngxReplacement::Custom on
    const auto customFgs = CustomProviders::List(CustomProviders::Kind::Dlssg);
    auto constexpr fgNvngxCustomIndex = (uint32_t) FGNvngxReplacement::Custom;

    for (uint32_t i = 0; i < customFgs.size(); i++)
        nvngxOptions.push_back({ (FGNvngxReplacement) (fgNvngxCustomIndex + i), customFgs[i],
                                 StrFmt(LOC(UI_Custom_DLSSG_from_custom_s_dlssg), customFgs[i].c_str()) });

    bool replaceFgOutputWithNvngx = false;
    bool showNvngxFgDowndown = false;

    if (config->FGInput == FGInput::NvngxFG)
    {
        config->FGOutput = FGOutput::NoFG;
        replaceFgOutputWithNvngx = true;
    }
    else if (config->FGOutput == FGOutput::DLSSG || config->FGOutput == FGOutput::FlipMeter)
    {
        showNvngxFgDowndown = true;
    }

    auto constexpr fgNvngxNoneIndex = (uint32_t) FGNvngxReplacement::None;
    nvngxOptions[fgNvngxNoneIndex].set_disabled(!maySupportDlssg, LOC(TXT_Unsupported_hardware));

    if (replaceFgOutputWithNvngx)
    {
        nvngxOptions[fgNvngxNoneIndex].label = LOC(UI_None);
        nvngxOptions[fgNvngxNoneIndex].set_hidden(true);
    }
    else if (config->FGOutput == FGOutput::FlipMeter)
    {
        // The flip meter output has no frames of its own
        nvngxOptions[fgNvngxNoneIndex].set_hidden(true);
    }

    auto constexpr fgNvngxNukemsIndex = (uint32_t) FGNvngxReplacement::Nukems;
    nvngxOptions[fgNvngxNukemsIndex].set_disabled(!state.nukemsFgFileAvailable,
                                                  "Missing dlssg_to_fsr3_amd_is_better.dll");

    auto constexpr fgNvngxArtursIndex = (uint32_t) FGNvngxReplacement::Arturs;
    nvngxOptions[fgNvngxArtursIndex].set_disabled(!state.artursFgFileAvailable, LOC(UI_Missing_dlss_enabler_headless_dll));

    auto constexpr fgNvngxFfxIndex = (uint32_t) FGNvngxReplacement::FFX;
    if (state.swapchainApi == API::Vulkan)
    {
        // The Vulkan FFX dlls are only loaded with the device when FSR FG is the output
        if (!ffxVkInitTried && !FfxApiProxy::IsFGReadyVk())
        {
            ffxVkInitTried = true;
            FfxApiProxy::InitFfxVk();
        }
        nvngxOptions[fgNvngxFfxIndex].set_disabled(!FfxApiProxy::IsFGReadyVk() && !FSR4DriverProviderVk::InitFg(),
                                                   "Missing amd_fidelityfx_framegeneration_vk.dll");
    }
    else
        nvngxOptions[fgNvngxFfxIndex].set_disabled(!FfxApiProxy::IsFGReady(false),
                                                   "Missing amd_fidelityfx_framegeneration_dx12.dll");

    auto constexpr fgNvngxComboIndex = (uint32_t) FGNvngxReplacement::Combo;
    nvngxOptions[fgNvngxComboIndex].set_disabled(state.swapchainApi == API::Vulkan, LOC(TXT_Unsupported_API));
    nvngxOptions[fgNvngxComboIndex].set_disabled(
        !FfxApiProxy::IsFGReady(false) || !state.artursFgFileAvailable,
        "Missing amd_fidelityfx_framegeneration_dx12.dll\nor missing dlss-enabler-headless.dll");

    // TODO: Automatically switch to any other option

    if (!config->FGNvngxReplacement.has_value())
        config->FGNvngxReplacement = config->FGNvngxReplacement.value_or_default(); // need to have a value before combo

    // Folder names in FGNvngxReplacement aren't case sensitive
    auto isActiveCustomFg = [&config, &state](const std::string& name)
    {
        return config->FGNvngxReplacement.value_or_default() == FGNvngxReplacement::Custom &&
               _stricmp(state.fgNvngxCustom.c_str(), name.c_str()) == 0;
    };

    auto nvngxCombo = [&](const char* label)
    {
        auto current = config->FGNvngxReplacement.value_or_default();

        if (current == FGNvngxReplacement::Custom)
            current = (FGNvngxReplacement) (fgNvngxCustomIndex +
                                            (std::ranges::find_if(customFgs, isActiveCustomFg) - customFgs.begin()));

        auto selected = current;
        PopulateCombo(label, selected, nvngxOptions);

        if (selected == current)
            return;

        if ((uint32_t) selected >= fgNvngxCustomIndex)
        {
            state.fgNvngxCustom = customFgs[(uint32_t) selected - fgNvngxCustomIndex];
            config->FGNvngxReplacement = FGNvngxReplacement::Custom;
        }
        else
        {
            state.fgNvngxCustom.clear();
            config->FGNvngxReplacement = selected;
        }
    };

    if (state.activeFgInput != FGInput::ForceXeLL)
    {
        SectionTitle(LOC(SEP_Frame_Generation_172));
        ShowTooltip(LOC(Which_copium_do));

        if (ImGui::BeginTable("fgSelection", 1, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();

            PopulateCombo(LOC(LBL_FG_Input), config->FGInput, inputOptions);
            ShowTooltip(LOC(The_data_source));

            ImGui::TableNextColumn();

            if (replaceFgOutputWithNvngx)
            {
                // Disable None?
                nvngxCombo(LOC(UI_FG_Nvngx));
                ShowTooltip(LOC(UI_What_backend_to_use_instead_of_the_real_DLSSG));
            }
            else
            {
                PopulateCombo(LOC(LBL_FG_Output), config->FGOutput, outputOptions);
                ShowTooltip(LOC(The_FG_that));
            }

            if (showNvngxFgDowndown)
            {
                ImGui::TableNextColumn();

                nvngxCombo(LOC(UI_FG_Nvngx_Replacement));
                ShowTooltip(config->FGOutput == FGOutput::FlipMeter ? "What backend generates the frames"
                                                                    : LOC(UI_What_backend_to_use_instead_of_the_real_DLSSG));
            }

            // Custom FFX FGs of the swapchain API, DX11 runs the DX12 one
            const auto customFsrFgs = CustomProviders::List(
                state.swapchainApi == API::Vulkan ? CustomProviders::Kind::FfxFgVk : CustomProviders::Kind::FfxFg);

            if (config->FGOutput == FGOutput::FSRFG && !customFsrFgs.empty())
            {
                ImGui::TableNextColumn();

                auto provider = config->FSRFGProvider.value_or("");

                if (ImGui::BeginCombo(LOC(UI_FSR_FG_Provider), provider.empty() ? LOC(Default) : provider.c_str()))
                {
                    if (ImGui::Selectable(LOC(Default), provider.empty()))
                        config->FSRFGProvider.reset();

                    for (auto& name : customFsrFgs)
                    {
                        if (ImGui::Selectable(name.c_str(), _stricmp(provider.c_str(), name.c_str()) == 0))
                            config->FSRFGProvider = name;
                    }

                    ImGui::EndCombo();
                }

                ShowTooltip("FFX frame generation and swapchain used by FSR FG\n"
                            "Custom ones are from custom\\<name>\\ffx_fg(_vk) or plugins");
            }

            ImGui::EndTable();
        }

        // Try to avoid having None selected when the gpu doesn't support DLSSG + some fallbacks
        if (!maySupportDlssg && (replaceFgOutputWithNvngx || showNvngxFgDowndown) &&
            config->FGNvngxReplacement.value_or_default() == FGNvngxReplacement::None)
        {
            if (state.nukemsFgFileAvailable)
                config->FGNvngxReplacement.set_volatile_value(FGNvngxReplacement::Nukems);

            else if (state.artursFgFileAvailable)
                config->FGNvngxReplacement.set_volatile_value(FGNvngxReplacement::Arturs);

            else if (FfxApiProxy::IsFGReady(false))
                config->FGNvngxReplacement.set_volatile_value(FGNvngxReplacement::FFX);

            else if (!customFgs.empty())
            {
                state.fgNvngxCustom = customFgs[0];
                config->FGNvngxReplacement.set_volatile_value(FGNvngxReplacement::Custom);
            }
        }

        const bool nvngxFgChanged =
            (replaceFgOutputWithNvngx || showNvngxFgDowndown) &&
            (state.activeFgNvngx != config->FGNvngxReplacement.value_or_default() ||
             (state.activeFgNvngx == FGNvngxReplacement::Custom && !isActiveCustomFg(state.activeFgNvngxCustom)));
        // XeFG input to XeFG output runs without OptiScaler's FG output
        auto configFgOutput = config->FGOutput.value_or_default();

        if (config->FGInput.value_or_default() == FGInput::XeFG && configFgOutput == FGOutput::XeFG)
            configFgOutput = FGOutput::NoFG;

        state.fgSettingsChanged =
            state.activeFgOutput != configFgOutput || state.activeFgInput != config->FGInput.value_or_default() ||
            nvngxFgChanged ||
            _stricmp(state.activeFsrFgProvider.c_str(), config->FSRFGProvider.value_or("").c_str()) != 0;

        if (state.fgSettingsChanged)
        {
            ImGui::Spacing();
            ImGui::TextColored(StatusError(), LOC(UI_Save_Settings_and_restart_to_apply_the_changes));
            ImGui::Spacing();
        }

        // One FG status for every input and output pair, Reprojection shows its own
        if ((state.activeFgOutput != FGOutput::NoFG && state.activeFgOutput != FGOutput::Reprojection) ||
            state.activeFgInput == FGInput::NvngxFG)
        {
            RenderFGStatus(state, config);
        }

        // XeFG input to XeFG output: the game's own XeFG, like Override DLSSG Ratio
        if (XeFGInputs::Passthrough() && XeFGInputs::MaxInterpolations() >= 1)
        {
            ImGui::PushItemWidth(150.0f * menuResScale);
            RenderRatioOverrideSlider(LOC(UI_Override_XeFG_Ratio), config->FGXeFGOverrideInterpolationCount,
                                      (int) XeFGInputs::MaxInterpolations());
            ImGui::PopItemWidth();
        }

        const bool dlssgInputOrOutput =
            state.activeFgOutput == FGOutput::DLSSG || state.activeFgInput == FGInput::DLSSG;

        ImGui::BeginDisabled(state.dlssgGameDMFGSupported && config->FGDLSSGOverrideForceDMFG.value_or_default());
        if (state.dlssgMfgMax.has_value() && state.dlssgMfgMax.value() >= 1 && !dlssgInputOrOutput)
        {
            ImGui::PushItemWidth(150.0f * menuResScale);

            if (RenderRatioOverrideSlider(LOC(UI_Override_DLSSG_Ratio), config->FGDLSSGOverrideInterpolationCount,
                                          state.dlssgMfgMax.value()))
            {
                LOG_DEBUG("DLSSG Interpolation Count set to: {}",
                          config->FGDLSSGOverrideInterpolationCount.has_value()
                              ? std::to_string(config->FGDLSSGOverrideInterpolationCount.value())
                              : std::string(LOC(Default)));

                StreamlineHooks::updateDlssgOptions();
            }

            ImGui::PopItemWidth();
        }

        ImGui::EndDisabled();

        if (state.dlssgGameDMFGSupported && !dlssgInputOrOutput)
        {
            ImGui::SameLine(0.0f, 16.0f);

            if (bool dynamicMFG = config->FGDLSSGOverrideForceDMFG.value_or_default();
                ImGui::Checkbox(LOC(CHK_Force_Dynamic_Mfg_34), &dynamicMFG))
            {
                config->FGDLSSGOverrideForceDMFG = dynamicMFG;
                StreamlineHooks::updateDlssgOptions();
            }

            ImGui::BeginDisabled(state.dlssgLastSetMode != sl::DLSSGMode::eDynamic);
            static float fpsTarget = config->FGDLSSGFramerateTargetDMFG.value_or_default();
            const float fpsSliderMax = (float) std::max(200, refreshRate);
            ImGui::SliderFloat(LOC(UI_DMFG_FPS_Target), &fpsTarget, 0, fpsSliderMax, "%.0f");

            ShowTooltip(LOC(TIP_An_Active_Limit_Of_204));

            if (ImGui::Button(LOC(BTN_Apply_Target_136)))
            {
                config->FGDLSSGFramerateTargetDMFG = fpsTarget;
                StreamlineHooks::updateDlssgOptions();
            }

            ImGui::SameLine(0.0f, 16.0f);

            if (ImGui::Button(LOC(BTN_Reset_Target_137)))
            {
                fpsTarget = 0.0f;
                config->FGDLSSGFramerateTargetDMFG.reset();
            }

            ImGui::EndDisabled();
        }

        auto fgOutput = state.CurrentFG();
        if (state.activeFgOutput != FGOutput::NoFG && state.activeFgInput != FGInput::NoFG &&
            state.activeFgInput != FGInput::NvngxFG && fgOutput)
        {
            // Reprojection has its own LOC(UI_Show_static_elements) for the same compare
            if (state.activeFgOutput != FGOutput::Reprojection)
            {
                ImGui::Checkbox(LOC(CHK_Show_Detected_Ui_35), &state.fgHudlessCompare);
                ShowTooltip(LOC(Needs_HUDless_texture));
            }

            const auto isUsingUIAny = fgOutput->IsUsingUIAny();

            ImGui::BeginDisabled(!isUsingUIAny);

            if (bool drawUIOverFG = config->FGDrawUIOverFG.value_or_default();
                ImGui::Checkbox(LOC(CHK_Draw_Ui_Over_36), &drawUIOverFG))
            {
                config->FGDrawUIOverFG = drawUIOverFG;
            }
            ShowTooltip(LOC(Draws_UI_resource));

            ImGui::EndDisabled();

            ImGui::SameLine(0.0f, 16.0f);

            ImGui::BeginDisabled(!isUsingUIAny || !config->FGDrawUIOverFG.value_or_default());

            if (bool uiPremultipliedAlpha = config->FGUIPremultipliedAlpha.value_or_default();
                ImGui::Checkbox(LOC(CHK_Ui_Premult_Alpha_37), &uiPremultipliedAlpha))
            {
                config->FGUIPremultipliedAlpha = uiPremultipliedAlpha;
            }
            ShowTooltip(LOC(UI_If_UI_is_too_bright_disable_this_option));

            ImGui::EndDisabled();
        }

        // Inputs where the game tags its own HUDless and UI for Opti's FG output
        const bool gameInput = state.activeFgInput == FGInput::DLSSG || state.activeFgInput == FGInput::FSRFG ||
                               state.activeFgInput == FGInput::FSRFG30 || state.activeFgInput == FGInput::XeFG;
        // The game calls the Nvngx replacement directly, there's no Opti FG output
        const bool nvngxInput = state.activeFgInput == FGInput::NvngxFG;
        const bool outputSettings = fgOutput != nullptr && state.activeFgInput != FGInput::NoFG;
        // FSR FG output and every Nvngx replacement run the HUD cutoff
        const bool showHudCutoff =
            nvngxInput || state.activeFgNvngx != FGNvngxReplacement::None || state.activeFgOutput == FGOutput::FSRFG;

        if (outputSettings || nvngxInput || showHudCutoff)
        {
            if (auto ch = ScopedCollapsingHeader(LOC(Advanced_FG_Settings)); ch.IsHeaderOpen())
            {
                ScopedIndent indent {};
                ImGui::Spacing();

                if ((outputSettings && gameInput) || nvngxInput)
                {
                    if (!nvngxInput)
                    {
                        bool disableUI = config->FGDisableUI.value_or_default();
                        ImGui::BeginDisabled(!fgOutput->IsUsingUIAny() && !disableUI);

                        if (ImGui::Checkbox(LOC(CHK_Disable_Ui_Texture_38), &disableUI))
                        {
                            config->FGDisableUI = disableUI;
                            fgOutput->UpdateTarget();
                        }

                        ShowTooltip("For when the game sends a UI texture, but you want to disable it\n"
                                    "Greyed out while the game doesn't send one");

                        ImGui::EndDisabled();
                        ImGui::SameLine(0.0f, 16.0f);
                    }

                    // With Nvngx as input there's no Opti FG output to tell if the game sends one
                    bool disableHudless = config->FGDisableHudless.value_or_default();
                    ImGui::BeginDisabled(!nvngxInput && !fgOutput->IsUsingHudlessAny() && !disableHudless);

                    if (ImGui::Checkbox(LOC(CHK_Disable_Hudless_39), &disableHudless))
                        config->FGDisableHudless = disableHudless;

                    ShowTooltip(nvngxInput ? "For when the game sends HUDless, but you want to disable it\n"
                                             "Might be required for some sets of DispatchFlags"
                                           : "For when the game sends HUDless, but you want to disable it\n"
                                             "Greyed out while the game doesn't send one");

                    ImGui::EndDisabled();
                }

                if (outputSettings && gameInput)
                {
                    // Read by the Streamline and FSR FG inputs
                    const bool validNowInput = state.activeFgInput != FGInput::XeFG;

                    if (validNowInput)
                    {
                        bool depthValidNow = config->FGDepthValidNow.value_or_default();
                        if (ImGui::Checkbox(LOC(CHK_Depth_As_Validnow_40), &depthValidNow))
                            config->FGDepthValidNow = depthValidNow;

                        ShowTooltip(LOC(Will_use_more));

                        ImGui::SameLine(0.0f, 16.0f);

                        bool velocityValidNow = config->FGVelocityValidNow.value_or_default();
                        if (ImGui::Checkbox(LOC(CHK_Velocity_As_Validnow_41), &velocityValidNow))
                            config->FGVelocityValidNow = velocityValidNow;

                        ShowTooltip(LOC(Will_use_more));

                        bool hudlessValidNow = config->FGHudlessValidNow.value_or_default();
                        if (ImGui::Checkbox(LOC(CHK_Hudless_As_Validnow_42), &hudlessValidNow))
                            config->FGHudlessValidNow = hudlessValidNow;

                        ShowTooltip(LOC(TIP_Will_Use_More_Vram_208));

                        ImGui::SameLine(0.0f, 16.0f);
                    }

                    bool firstHudless = config->FGOnlyAcceptFirstHudless.value_or_default();
                    if (ImGui::Checkbox(LOC(CHK_Accept_First_Hudless_43), &firstHudless))
                        config->FGOnlyAcceptFirstHudless = firstHudless;

                    ShowTooltip(LOC(TIP_If_Source_Tags_More_209));

                    // FFX API inputs can tag HUDless at both config and dispatch
                    if (state.activeFgInput == FGInput::FSRFG)
                    {
                        bool skipConfig = config->FSRFGSkipConfigForHudless.value_or_default();
                        if (ImGui::Checkbox(LOC(UI_Skip_Config_for_HUDless), &skipConfig))
                            config->FSRFGSkipConfigForHudless = skipConfig;

                        ShowTooltip(LOC(UI_Do_not_use_HUDless_set_at_ffxConfig));

                        ImGui::SameLine(0.0f, 16.0f);

                        bool skipDispatch = config->FSRFGSkipDispatchForHudless.value_or_default();
                        if (ImGui::Checkbox(LOC(UI_Skip_Dispatch_for_HUDless), &skipDispatch))
                            config->FSRFGSkipDispatchForHudless = skipDispatch;

                        ShowTooltip(LOC(UI_Do_not_use_HUDless_set_at_ffxDispatch));
                    }
                }

                if (nvngxInput)
                {
                    // Issue mostly shows up on AMD on Windows on pre-RDNA3 in some non-UE games
                    // Hide to reduce confusion, config is still read
                    const auto activeNvngxFg = state.activeFgNvngx;
                    const bool isUnrealEngine = state.NVNGX_Engine == NVSDK_NGX_ENGINE_TYPE_UNREAL ||
                                                state.gameQuirks & GameQuirk::ForceUnrealEngine;
                    const bool isDllProxyNvngxType = activeNvngxFg == FGNvngxReplacement::Nukems ||
                                                     activeNvngxFg == FGNvngxReplacement::Arturs ||
                                                     activeNvngxFg == FGNvngxReplacement::Custom;

                    if (state.swapchainApi == DX12 && isDllProxyNvngxType && !primaryGpu.dlssCapable &&
                        primaryGpu.fsr4Support == FSR4Support::None && !primaryGpu.d3d12IsVkd3dProton &&
                        !isUnrealEngine)
                    {
                        if (bool makeDepthCopy = config->NvngxFGMakeDepthCopy.value_or_default();
                            ImGui::Checkbox(LOC(CHK_Fix_Broken_Visuals_84), &makeDepthCopy))
                        {
                            config->NvngxFGMakeDepthCopy = makeDepthCopy;
                        }
                        ShowTooltip(LOC(Makes_a_copy));
                    }
                }

                if (outputSettings)
                {
                    if (bool skipReset = config->FGSkipReset.value_or_default();
                        ImGui::Checkbox(LOC(CHK_Skip_Reset_44), &skipReset))
                    {
                        config->FGSkipReset = skipReset;
                    }

                    ShowTooltip(LOC(TIP_Don_T_Use_Reset_210));

                    ImGui::PushItemWidth(80.0f * menuResScale);

                    auto frameAhead = config->FGAllowedFrameAhead.value_or_default();
                    if (ImGui::InputInt(LOC(UI_Frame_Ahead), &frameAhead, 1, 1) && frameAhead > 0 && frameAhead < 4)
                    {
                        config->FGAllowedFrameAhead = frameAhead;
                    }

                    ShowTooltip(LOC(Number_of_frames));

                    ImGui::PopItemWidth();

                    // Only FSR FG and XeFG take frame times
                    if (state.activeFgOutput == FGOutput::FSRFG || state.activeFgOutput == FGOutput::XeFG)
                    {
                        ImGui::SameLine(0.0f, 16.0f);

                        const char* ftSources[] = { LOC(UI_Input), "Opti", LOC(UI_Zero) };
                        const char* ftSourceInfos[] = { LOC(UI_Uses_frametimes_provided_by_nDLSSG_or_FSR_FG),
                                                        LOC(UI_Uses_frametimes_calculated_by_Opti),
                                                        LOC(UI_Let_XeFG_to_handle_frametimes) };

                        auto currentSet = (int) config->FTInput.value_or_default();
                        auto currentSourceCount = state.activeFgOutput == FGOutput::XeFG ? 3 : 2;

                        ImGui::PushItemWidth(95.0f * menuResScale);

                        if (ImGui::BeginCombo(LOC(LBL_FT_Input), ftSources[currentSet]))
                        {
                            for (size_t i = 0; i < currentSourceCount; i++)
                            {
                                if (ImGui::Selectable(ftSources[i], currentSet == i))
                                {
                                    LOG_DEBUG("FTInput has changed {} -> {}", ftSources[currentSet], ftSources[i]);
                                    config->FTInput = (FrameTimeSource) i;
                                }

                                if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
                                    ImGui::SetTooltip(ftSourceInfos[i]);
                            }

                            ImGui::EndCombo();
                        }

                        ImGui::PopItemWidth();

                        ShowTooltip(LOC(Select_source_for));
                    }
                }

                if (showHudCutoff)
                {
                    float fgHudCutoff = config->FGHudCutoff.value_or_default();
                    if (ImGui::SliderFloat(LOC(UI_Hud_Cutoff), &fgHudCutoff, 0.00f, 1.0f, "%.2f"))
                        config->FGHudCutoff = fgHudCutoff;

                    ShowTooltip(nvngxInput ? "Cutoffs transparency from UI to help with interpolation\n0.0 is auto"
                                           : LOC(Cutoffs_transparency_from));
                }
            }
        }
    }
}

// Replaces the Active checkbox + MFG combo of FG outputs with a single slider
// maxInterpolationCount of 0 means the output doesn't generate frames, LOC(UI_On) is shown instead of "2X"
void MenuCommon::RenderFGModeSlider(RenderMenuContext& ctx, const char* label, int maxInterpolationCount,
                                    CustomOptional<int>* countConfig, const char* tooltip)
{
    auto& state = ctx.state;
    auto config = ctx.config;

    // -1 = Default (follow game), 0 = Off, >0 = interpolated frame count
    auto modeName = [&](int mode) -> std::string
    {
        if (mode < 0)
            return LOC(Default);

        if (mode == 0)
            return LOC(UI_Off);

        if (maxInterpolationCount == 0)
            return LOC(UI_On);

        return std::format("{}X", mode + 1);
    };

    const int maxMode = std::max(maxInterpolationCount, 1);

    int current = 0;
    if (config->FGFollowsGame())
        current = -1;
    else if (config->FGEnabled.value_or_default())
        current = countConfig != nullptr ? std::clamp(countConfig->value_or_default(), 1, maxMode) : 1;

    // Slider order: Off, (Default), 2X, 3X... up to the output's max
    std::vector<int> modes { 0 };
    if (Config::FGCanFollowGame())
        modes.push_back(-1);

    for (int mode = 1; mode <= maxMode; mode++)
        modes.push_back(mode);

    int index = (int) (std::find(modes.begin(), modes.end(), current) - modes.begin());
    if (index >= (int) modes.size())
        index = 0;

    auto stepName = [&](int step) -> std::string
    {
        const int mode = modes[step];
        if (mode >= 0)
            return modeName(mode);

        const int gameMode =
            state.dlssgInputGameEnabled ? std::min(state.dlssgInputGameInterpolationCount, maxMode) : 0;
        return std::format("{} ({})", modeName(mode), modeName(gameMode));
    };

    ImGui::PushItemWidth(150.0f * ctx.menuResScale);

    if (RenderStepSlider(label, index, (int) modes.size(), stepName) && modes[index] != current)
    {
        const int mode = modes[index];
        fgHotkeyLeftDefault = false;

        if (mode < 0)
        {
            config->FGEnabled = std::optional<bool> {};
        }
        else
        {
            config->FGEnabled = mode > 0;

            if (mode > 0 && countConfig != nullptr)
                *countConfig = mode;
        }

        LOG_DEBUG("FG mode set to: {}", modeName(mode));

        if (config->IsFGEnabled())
            state.fgChanged = true;
    }

    ImGui::PopItemWidth();

    if (tooltip != nullptr)
        ShowTooltip(tooltip);
    else if (Config::FGCanFollowGame())
        ShowTooltip(LOC(UI_Frame_Generation_mode_nDefault_follows_the_game_s_DLSS_FG_se));
    else
        ShowTooltip(LOC(UI_Frame_Generation_mode));
}

void MenuCommon::RenderFrameGenerationRuntimeSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;
    auto& menuResScale = ctx.menuResScale;
    auto& primaryGpu = *ctx.primaryGpu;
    auto fgOutput = state.currentFG;

    const UiTargetMode uiTargetMode = getUiTargetMode();
    const bool outputIsHdr = uiTargetMode != UiTargetMode::SDR;

    // FSR FG controls
    if (ScopedCard::Shows("fg_fsrfg") && state.activeFgOutput == FGOutput::FSRFG &&
        state.activeFgInput != FGInput::NoFG && state.FGSwapchainExists())
    {
        if (state.activeFgInput != FGInput::Upscaler ||
            (currentFeature != nullptr && !currentFeature->IsFrozen()) &&
                (state.currentVkFG != nullptr || FfxApiProxy::IsFsrFgReady()))
        {
            ScopedCard card { LOC(UI_FSR_FG_Settings) };

            // The output resets an index from a game with more FFX versions
            if (_ffxFGIndex < 0 || _ffxFGIndex >= (int) state.ffxFGVersionNames.size())
                _ffxFGIndex = config->FfxFGIndex.value_or_default();

            if (state.ffxFGVersionNames.size() > 0)
            {
                ImGui::PushItemWidth(115.0f * menuResScale);

                auto currentName = StrFmt(LOC(UI_FSR_s), state.ffxFGVersionNames[_ffxFGIndex]);
                if (ImGui::BeginCombo(LOC(UI_FFX_FG_Version), currentName.c_str()))
                {
                    for (int n = 0; n < state.ffxFGVersionIds.size(); n++)
                    {
                        auto name = StrFmt(LOC(UI_FSR_s), state.ffxFGVersionNames[n]);
                        if (ImGui::Selectable(name.c_str(), config->FfxFGIndex.value_or_default() == n))
                        {
                            _ffxFGIndex = n;
                            config->FfxFGIndex = _ffxFGIndex;
                            state.fgChanged = true;
                            state.scChanged = true;
                        }
                    }

                    ImGui::EndCombo();
                }
                ImGui::PopItemWidth();

                ShowTooltip(LOC(TIP_List_Of_Fgs_Reported_211));
            }

            RenderFGModeSlider(ctx, LOC(UI_Frame_Gen_fsrfg), 1, nullptr);

            bool fgAsync = config->FGAsync.value_or_default();
            if (ImGui::Checkbox(LOC(CHK_Allow_Async_46), &fgAsync))
            {
                config->FGAsync = fgAsync;

                if (config->IsFGEnabled())
                {
                    state.fgChanged = true;
                    state.scChanged = true;
                    LOG_DEBUG("Async set FGChanged");
                }
            }
            ShowTooltip(LOC(TIP_Enable_Async_For_Better_213));

            if (state.amdFlipMeterCreated)
            {
                ImGui::SameLine(0.0f, 16.0f);

                bool disableFlipMetering = config->FSRFGDisableFlipMetering.value_or_default();
                if (ImGui::Checkbox(LOC(UI_Disable_HW_Flip_Metering), &disableFlipMetering))
                {
                    config->FSRFGDisableFlipMetering = disableFlipMetering;
                    Amdxc64Hooks::ApplyFlipMeterOverride();
                }
                ShowTooltip(LOC(UI_Overrides_the_AMD_hardware_flip_metering_requested_by_FSR));

                ImGui::SameLine(0.0f, 16.0f);

                if (state.amdFlipMeterMode != 0)
                    ImGui::TextDisabled(LOC(UI_Mode_u), state.amdFlipMeterMode);
                else
                    ImGui::TextDisabled(LOC(UI_Off_2));

                ShowTooltip("Flip metering mode applied in the driver\nFSR only requests it in exclusive or "
                            "borderless fullscreen");
            }

            ImGui::Spacing();

            if (auto ch = ScopedCollapsingHeader(LOC(Advanced_FSR_FG)); ch.IsHeaderOpen())
            {
                ScopedIndent indent {};
                ImGui::Spacing();

                if (ImGui::BeginTable("fsrfgAdvanced", 2, ImGuiTableFlags_SizingStretchSame))
                {
                    ImGui::TableNextColumn();
                    if (ConfigCheckbox(LOC(CHK_Debug_View_47), config->FGDebugView,
                                       LOC(Enable_FSR3_1)) &&
                        config->IsFGEnabled())
                    {
                        state.fgChanged = true;
                        LOG_DEBUG("DebugView set FGChanged");
                    }

                    if (state.currentFG && state.currentFG->Version().major > 3)
                    {
                        ImGui::TableNextColumn();
                        if (ConfigCheckbox(LOC(CHK_Enable_Watermark_48), config->FSRFGEnableWatermark,
                                           "Enables the FSR FG watermark"))
                        {
                            Util::SetFlagEnvVar(L"MLFI-WATERMARK", config->FSRFGEnableWatermark.value_or_default());

                            // The driver reads the variable on context creation, SCchanged also required
                            if (config->IsFGEnabled())
                            {
                                state.fgChanged = true;
                                state.scChanged = true;
                            }
                        }
                    }

                    ImGui::TableNextColumn();
                    ImGui::Checkbox(LOC(CHK_Fg_Only_Generated_49), &state.fgOnlyGenerated);
                    ShowTooltip(LOC(TIP_Display_Only_Fsr_3_214));

                    ImGui::TableNextColumn();
                    ConfigCheckbox(LOC(CHK_Debug_Reset_Lines_50), config->FGDebugResetLines,
                                   LOC(TIP_Enables_Drawing_Of_Interpolation_215));

                    ImGui::TableNextColumn();
                    ConfigCheckbox(LOC(CHK_Debug_Tear_Lines_51), config->FGDebugTearLines,
                                   LOC(TIP_Enables_Drawing_Of_Tear_216));

                    ImGui::TableNextColumn();
                    ConfigCheckbox(LOC(CHK_Debug_Pacing_Lines_52), config->FGDebugPacingLines, LOC(TIP_Enables_Drawing_Of_Pacing_217));

                    ImGui::EndTable();
                }

                ImGui::Spacing();
                if (ImGui::TreeNode(LOC(UI_FG_Rectangle_Settings)))
                {
                    ImGui::PushItemWidth(95.0f * menuResScale);
                    int rectLeft = config->FGRectLeft.value_or(0);
                    if (ImGui::InputInt(LOC(UI_Rect_Left), &rectLeft))
                        config->FGRectLeft = rectLeft;

                    ImGui::SameLine(0.0f, 16.0f);
                    int rectTop = config->FGRectTop.value_or(0);
                    if (ImGui::InputInt(LOC(UI_Rect_Top), &rectTop))
                        config->FGRectTop = rectTop;

                    int rectWidth = config->FGRectWidth.value_or(0);
                    if (ImGui::InputInt(LOC(UI_Rect_Width), &rectWidth))
                        config->FGRectWidth = rectWidth;

                    ImGui::SameLine(0.0f, 16.0f);
                    int rectHeight = config->FGRectHeight.value_or(0);
                    if (ImGui::InputInt(LOC(UI_Rect_Height), &rectHeight))
                        config->FGRectHeight = rectHeight;

                    ImGui::PopItemWidth();
                    ShowTooltip(LOC(TIP_Frame_Generation_Rectangle_Adjust_218));

                    ImGui::BeginDisabled(!config->FGRectLeft.has_value() && !config->FGRectTop.has_value() &&
                                         !config->FGRectWidth.has_value() && !config->FGRectHeight.has_value());

                    if (ImGui::Button(LOC(BTN_Reset_Fg_Rect_139)))
                    {
                        config->FGRectLeft.reset();
                        config->FGRectTop.reset();
                        config->FGRectWidth.reset();
                        config->FGRectHeight.reset();
                    }

                    ShowTooltip(LOC(TIP_Resets_Frame_Generation_Rectangle_219));

                    ImGui::EndDisabled();
                    ImGui::TreePop();
                }

                // Vulkan FSR-FG reports FFX SDK versions, SDK 1.1.3 is FSR 3.1.3
                auto fg = state.CurrentFG();
                auto fptMinVersion =
                    state.currentVkFG != nullptr ? feature_version { 1, 1, 3 } : feature_version { 3, 1, 3 };
                if (fg != nullptr && strcmp(fg->Name(), "FSR-FG") == 0 && fg->Version() >= fptMinVersion)
                {
                    ImGui::Spacing();

                    if (ImGui::TreeNode(LOC(UI_Frame_Pacing_Tuning)))
                    {
                        auto fptEnabled = config->FGFramePacingTuning.value_or_default();
                        if (ImGui::Checkbox(LOC(CHK_Enable_Tuning_53), &fptEnabled))
                        {
                            config->FGFramePacingTuning = fptEnabled;
                            state.fsrfgFramePaceTuningChanged = true;
                        }

                        ImGui::BeginDisabled(!config->FGFramePacingTuning.value_or_default());

                        ImGui::PushItemWidth(115.0f * menuResScale);
                        auto fptSafetyMargin = config->FGFPTSafetyMarginInMs.value_or_default();
                        if (ImGui::InputFloat(LOC(UI_Safety_Margins_in_ms), &fptSafetyMargin, 0.01f, 0.1f, "%.2f"))
                            config->FGFPTSafetyMarginInMs = fptSafetyMargin;
                        ShowTooltip(LOC(Safety_margins_in));

                        auto fptVarianceFactor = config->FGFPTVarianceFactor.value_or_default();
                        if (ImGui::SliderFloat(LOC(UI_Variance_Factor), &fptVarianceFactor, 0.0f, 1.0f, "%.2f"))
                            config->FGFPTVarianceFactor = fptVarianceFactor;
                        ShowTooltip(LOC(Variance_factor_nFSR));
                        ImGui::PopItemWidth();

                        // FFX's Vulkan swapchain only reads the safety margin and variance factor
                        ImGui::BeginDisabled(state.currentVkFG != nullptr);

                        auto fpHybridSpin = config->FGFPTAllowHybridSpin.value_or_default();
                        if (ImGui::Checkbox(LOC(CHK_Enable_Hybrid_Spin_54), &fpHybridSpin))
                            config->FGFPTAllowHybridSpin = fpHybridSpin;
                        ShowTooltip(LOC(Allows_pacing_spinlock));

                        ImGui::PushItemWidth(115.0f * menuResScale);
                        auto fptHybridSpinTime = config->FGFPTHybridSpinTime.value_or_default();
                        if (ImGui::SliderInt(LOC(UI_Hybrid_Spin_Time), &fptHybridSpinTime, 0, 100))
                            config->FGFPTHybridSpinTime = fptHybridSpinTime;
                        ShowTooltip(LOC(How_long_to));
                        ImGui::PopItemWidth();

                        auto fpWaitForSingleObjectOnFence =
                            config->FGFPTAllowWaitForSingleObjectOnFence.value_or_default();
                        if (ImGui::Checkbox(LOC(CHK_Enable_Waitforsingleobjectonfence_55), &fpWaitForSingleObjectOnFence))
                        {
                            config->FGFPTAllowWaitForSingleObjectOnFence = fpWaitForSingleObjectOnFence;
                        }
                        ShowTooltip(LOC(TIP_Allows_Waitforsingleobject_Instead_Of_220));

                        ImGui::EndDisabled();

                        if (ImGui::Button(LOC(BTN_Apply_Timing_Changes_140)))
                            state.fsrfgFramePaceTuningChanged = true;

                        ImGui::EndDisabled();
                        ImGui::TreePop();
                    }
                }

                ImGui::Spacing();
                ImGui::Spacing();
            }
        }
    }

    // XeFG controls
    if (ScopedCard::Shows("fg_xefg") && state.activeFgOutput == FGOutput::XeFG &&
        state.activeFgInput != FGInput::NoFG && state.activeFgInput != FGInput::ForceXeLL &&
        state.currentFGSwapchain != nullptr && XeFGProxy::InitXeFG() && fgOutput)
    {
        ScopedCard card { LOC(UI_XeFG_Settings) };

        bool ignoreChecks = config->FGXeFGIgnoreInitChecks.value_or_default();

        bool nativeAA = false;
        if (state.activeFgInput == FGInput::Upscaler && currentFeature != nullptr)
            nativeAA = currentFeature->RenderWidth() == currentFeature->DisplayWidth();

        const bool correctMVs = fgOutput->IsLowResMV() || nativeAA ||
                                (State::Instance().gameQuirks & GameQuirk::ForceFGRenderSizeMVs) || ignoreChecks;

        if (!correctMVs || state.realExclusiveFullscreen)
        {
            config->FGEnabled.set_volatile_value(false);
            config->FGXeFGDebugView.reset();
        }

        const bool restartNeeded = config->FGXeFGDepthInverted.value_or_default() != fgOutput->IsInvertedDepth() ||
                                   config->FGXeFGJitteredMV.value_or_default() != fgOutput->IsJitteredMVs() ||
                                   config->FGXeFGHighResMV.value_or_default() == fgOutput->IsLowResMV();

        bool cantActivate = false;
        if (restartNeeded)
        {
            ImGui::TextColored(StatusWarn(), LOC(UI_Restart_the_game_to_apply_correct_XeFG_settings));
        }
        else
        {
            if (!correctMVs)
                ImGui::TextColored(StatusError(), LOC(UI_Requires_disabling_dilated_motion_vectors));

            if (!ignoreChecks && state.realExclusiveFullscreen)
            {
                cantActivate = true;
                ImGui::TextColored(StatusError(), LOC(UI_Borderless_display_mode_required));
            }

            if (!ignoreChecks && outputIsHdr)
            {
                if (state.currentSwapchainDesc.BufferDesc.Format >= DXGI_FORMAT_R32G32B32A32_TYPELESS &&
                    state.currentSwapchainDesc.BufferDesc.Format <= DXGI_FORMAT_R16G16B16A16_SINT)
                {
                    cantActivate = true;
                    ImGui::TextColored(StatusError(), LOC(UI_XeFG_only_supports_HDR10));
                }
            }
        }

        if (!correctMVs || cantActivate || ignoreChecks)
        {
            if (ImGui::Checkbox(LOC(CHK_Ignore_Init_Checks_56), &ignoreChecks))
                config->FGXeFGIgnoreInitChecks = ignoreChecks;

            ShowTooltip(LOC(Ignores_all_prechecks));
        }

        const int maxCount = fgOutput->GetMaxInterpolationCount();
        const bool dynamicMFG = maxCount >= 1 && config->FGXeFGDynamicMFG.value_or_default();

        // Frame Gen at 2X: Dynamic MFG only switches between off and 2X
        const bool dynamicOnOff =
            std::clamp(config->FGXeFGInterpolationCount.value_or_default(), 1, std::max(maxCount, 1)) == 1;

        ImGui::BeginDisabled(!correctMVs || cantActivate);

        // Dynamic MFG: the Frame Gen setting is the most, the multiplier follows the game's frame rate
        if (dynamicMFG)
            RenderFGModeSlider(ctx, LOC(UI_Max_Frame_Gen_xefgMode), maxCount, &config->FGXeFGInterpolationCount,
                               "The highest multiplier Dynamic MFG may use");
        else
            RenderFGModeSlider(ctx, LOC(UI_Frame_Gen_xefgMode), maxCount, &config->FGXeFGInterpolationCount);

        if (maxCount >= 1)
        {
            ImGui::SameLine(0.0f, 16.0f);

            if (bool dynamic = dynamicMFG; ImGui::Checkbox(LOC(UI_Dynamic_MFG_xefg), &dynamic))
                config->FGXeFGDynamicMFG = dynamic;

            ShowTooltip("Changes the multiplier on the fly to hit the target FPS,\n"
                        "up to the Frame Gen setting (at 2X: FG on or off).");
        }

        if (dynamicMFG)
        {
            // Applied on release, dragging doesn't restart Dynamic MFG every frame.
            // The slider's lowest step (9) is Auto, the lowest target is 10.
            constexpr float autoStep = 9.0f;
            static float xefgFpsTarget = std::max(config->FGXeFGFramerateTargetDMFG.value_or_default(), autoStep);
            const float fpsSliderMax = (float) std::max(200, refreshRate);

            ImGui::SetNextItemWidth(150.0f * menuResScale);
            ImGui::SliderFloat(LOC(UI_Target_FPS_xefg), &xefgFpsTarget, autoStep, fpsSliderMax,
                               xefgFpsTarget < autoStep + 0.5f ? LOC(UI_Auto) : "%.0f", ImGuiSliderFlags_AlwaysClamp);

            if (ImGui::IsItemDeactivatedAfterEdit())
            {
                if (xefgFpsTarget < autoStep + 0.5f)
                    config->FGXeFGFramerateTargetDMFG.reset();
                else
                    config->FGXeFGFramerateTargetDMFG = xefgFpsTarget;
            }

            ShowTooltip("Presented FPS to aim for, never above the FPS limit.\n"
                        "Auto: the FPS limit, or just under the refresh rate.");

            // The cap picks from 2X up, it needs a higher Frame Gen setting
            const bool targetAsCap = config->FGXeFGTargetAsCapDMFG.value_or_default() && !dynamicOnOff;

            if (!dynamicOnOff)
            {
                ImGui::SameLine(0.0f, 16.0f);

                if (bool cap = targetAsCap; ImGui::Checkbox(LOC(UI_Use_as_FPS_limit_xefg), &cap))
                    config->FGXeFGTargetAsCapDMFG = cap;

                ShowTooltip("Also limits FPS to the target and uses the lowest\n"
                            "multiplier that reaches it.");
            }

            // The cap holds the target with its own count logic
            ImGui::BeginDisabled(targetAsCap);

            const char* roundings[] = { LOC(UI_At_most_target), LOC(UI_Around_target), LOC(UI_At_least_target) };
            int rounding = std::clamp(config->FGXeFGRoundingDMFG.value_or_default(), 0, 2);

            ImGui::SetNextItemWidth(150.0f * menuResScale);
            if (ImGui::Combo(LOC(UI_xefgRounding), &rounding, roundings, IM_ARRAYSIZE(roundings)))
                config->FGXeFGRoundingDMFG = rounding;

            ShowTooltip("Where FPS may land, as multipliers only come in steps:\n"
                        "at most, around or at least the target FPS.");

            // At 2X, 1X is the only other step
            if (!dynamicOnOff)
            {
                ImGui::SameLine(0.0f, 16.0f);

                if (bool allow1X = config->FGXeFGDynamicAllow1X.value_or_default();
                    ImGui::Checkbox(LOC(UI_Allow_1X_xefg), &allow1X))
                {
                    config->FGXeFGDynamicAllow1X = allow1X;
                }

                ShowTooltip("Turns frame generation off when the game\n"
                            "alone gets close enough to the target.");
            }

            ImGui::EndDisabled();
        }

        bool fgDV = config->FGXeFGDebugView.value_or_default();
        if (ImGui::Checkbox(LOC(CHK_Debug_View_47), &fgDV))
        {
            config->FGXeFGDebugView = fgDV;

            if (config->FGXeFGDebugView.value_or_default())
            {
                state.fgChanged = true;
                LOG_DEBUG("DebugView set FGChanged");
            }
        }
        ShowTooltip(LOC(TIP_Enable_Xefg_Debug_View_222));

        ImGui::EndDisabled();

        ImGui::SameLine(0.0f, 16.0f);
        bool fgBorderless = config->FGXeFGForceBorderless.value_or_default();
        if (ImGui::Checkbox(LOC(CHK_Force_Borderless_59), &fgBorderless))
            config->FGXeFGForceBorderless = fgBorderless;

        ShowTooltip(LOC(Forces_Borderless_display));

        ImGui::SameLine(0.0f, 16.0f);
        ImGui::BeginDisabled(!fgOutput->IsUsingHudlessAny() || XeFGProxy::SetUiCompositionState() == nullptr);
        bool fgCompositeUI = config->FGXeFGUIComposition.value_or_default();
        if (ImGui::Checkbox(LOC(CHK_Ui_Composition_58), &fgCompositeUI))
            config->FGXeFGUIComposition = fgCompositeUI;

        ShowTooltip(LOC(Disable_HUD_UI));
        ImGui::EndDisabled();

        // Disable this for now
        // ImGui::SameLine(0.0f, 16.0f);
        // ImGui::Checkbox(LOC(CHK_Only_Generated_60), &state.fgOnlyGenerated);
        // ShowTooltip(LOC(TIP_Display_Only_Xefg_Generated_223));

        ImGui::Spacing();
        if (auto ch = ScopedCollapsingHeader(LOC(UI_Extended_XeFG_Settings)); ch.IsHeaderOpen())
        {
            ImGui::Spacing();
            if (ImGui::TreeNode(LOC(UI_Rectangle_Settings)))
            {
                ImGui::PushItemWidth(95.0f * menuResScale);
                int rectLeft = config->FGRectLeft.value_or(0);
                if (ImGui::InputInt(LOC(UI_Rect_Left_2), &rectLeft))
                    config->FGRectLeft = rectLeft;

                ImGui::SameLine(0.0f, 16.0f);
                int rectTop = config->FGRectTop.value_or(0);
                if (ImGui::InputInt(LOC(UI_Rect_Top_2), &rectTop))
                    config->FGRectTop = rectTop;

                int rectWidth = config->FGRectWidth.value_or(0);
                if (ImGui::InputInt(LOC(UI_Rect_Width_2), &rectWidth))
                    config->FGRectWidth = rectWidth;

                ImGui::SameLine(0.0f, 16.0f);
                int rectHeight = config->FGRectHeight.value_or(0);
                if (ImGui::InputInt(LOC(UI_Rect_Height_2), &rectHeight))
                    config->FGRectHeight = rectHeight;

                ImGui::PopItemWidth();
                ShowTooltip(LOC(TIP_Frame_Generation_Rectangle_Adjust_224));

                ImGui::BeginDisabled(!config->FGRectLeft.has_value() && !config->FGRectTop.has_value() &&
                                     !config->FGRectWidth.has_value() && !config->FGRectHeight.has_value());

                if (ImGui::Button(LOC(BTN_Reset_Fg_Rect_141)))
                {
                    config->FGRectLeft.reset();
                    config->FGRectTop.reset();
                    config->FGRectWidth.reset();
                    config->FGRectHeight.reset();
                }

                ShowTooltip(LOC(TIP_Resets_Frame_Generation_Rectangle_225));

                ImGui::EndDisabled();
                ImGui::TreePop();
            }

            ImGui::Spacing();
            ImGui::Spacing();
        }
    }

    // DLSSG controls
    if (auto dlssg = state.CurrentFG(); ScopedCard::Shows("fg_dlssg") && state.activeFgOutput == FGOutput::DLSSG &&
                                        state.activeFgInput != FGInput::NoFG && state.FGSwapchainExists() &&
                                        StreamlineProxy::LoadStreamline() && dlssg)
    {
        ScopedCard card { LOC(UI_DLSSG_Settings) };

        if (state.activeFgNvngx == FGNvngxReplacement::None && (state.hdrOutputActive && outputIsHdr))
        {
            if (state.currentSwapchainDesc.BufferDesc.Format >= DXGI_FORMAT_R32G32B32A32_TYPELESS &&
                state.currentSwapchainDesc.BufferDesc.Format <= DXGI_FORMAT_R16G16B16A16_SINT)
            {
                ImGui::TextColored(StatusError(), LOC(UI_DLSSG_only_supports_HDR10));
            }
        }

        RenderFGModeSlider(ctx, LOC(UI_Frame_Gen_dlssg), dlssg->GetMaxInterpolationCount(),
                           &config->FGDLSSGInterpolationCount);

        if (dlssg->GetMaxInterpolationCount() > 1)
        {
            if (dlssg->GetDMFGSupport())
            {
                ImGui::SameLine(0.0f, 16.0f);

                if (bool dynamicMFG = config->FGDLSSGForceDMFG.value_or_default();
                    ImGui::Checkbox(LOC(CHK_Force_Dynamic_Mfg_34), &dynamicMFG))
                {
                    config->FGDLSSGForceDMFG = dynamicMFG;
                    state.fgChanged = true;
                }

                ImGui::BeginDisabled(!config->FGDLSSGForceDMFG.value_or_default());
                static float fpsTarget = config->FGDLSSGFramerateTargetDMFG.value_or_default();
                float fpsSliderMax = (float) std::max(200, refreshRate);
                ImGui::SliderFloat(LOC(UI_DMFG_FPS_Target), &fpsTarget, 0, fpsSliderMax, "%.0f");

                ShowTooltip(LOC(TIP_An_Active_Limit_Of_204));

                if (ImGui::Button(LOC(BTN_Apply_Target_136)))
                {
                    config->FGDLSSGFramerateTargetDMFG = fpsTarget;
                }

                ImGui::SameLine(0.0f, 16.0f);

                if (ImGui::Button(LOC(BTN_Reset_Target_137)))
                {
                    fpsTarget = 0.0f;
                    config->FGDLSSGFramerateTargetDMFG.reset();
                }

                ImGui::EndDisabled();
            }
        }

        bool useGamesMarkers = config->FGDLSSGUseGamesReflexMarkers.value_or_default();
        // Vulkan games' markers don't reach OptiScaler's Streamline
        ImGui::BeginDisabled(!ReflexHooks::gameIsSendingMarkers() || state.swapchainApi == API::Vulkan);
        if (ImGui::Checkbox(LOC(CHK_Use_Game_S_Reflex_62), &useGamesMarkers))
        {
            config->FGDLSSGUseGamesReflexMarkers = useGamesMarkers;
            LOG_DEBUG("Changed set FGDLSSGUseGamesReflexMarkers: {}", useGamesMarkers);
        }
        ImGui::EndDisabled();
    }

    // Flip meter controls, the generated frames come from the Nvngx replacement
    if (ScopedCard::Shows("fg_flipmeter") && state.activeFgOutput == FGOutput::FlipMeter &&
        state.activeFgInput != FGInput::NoFG && state.FGSwapchainExists() && state.CurrentFG() != nullptr)
    {
        ScopedCard card { LOC(UI_Flip_Meter_Settings) };

        auto flipMeter = dynamic_cast<FlipMeterFG_Dx12*>(state.CurrentFG());
        auto swapChain = flipMeter != nullptr ? flipMeter->SwapChain() : nullptr;

        RenderFGModeSlider(ctx, LOC(UI_Frame_Gen_flipmeter), flipMeter != nullptr ? flipMeter->MaxInterpolationCount() : 1,
                           &config->FGFlipMeterInterpolationCount);

        if (swapChain != nullptr)
        {
            const auto status = swapChain->GetStatus();

            // While no frames are generated the FG status already says it's off
            if (status.metering && FAILED(status.presentResult))
            {
                ImGui::TextColored(StatusError(), LOC(UI_Driver_pacing_the_driver_doesn_t_pace_the_flips_08X_status_u),
                                   (UINT) status.presentResult, status.presentStatus);

                switch (status.presentStatus)
                {
                case AmdExtFlipMeterStatus_WindowNotEligible:
                    ImGui::TextWrapped("The driver doesn't see the game as a fullscreen app flipped straight to the "
                                       "display. Try exclusive fullscreen and closing overlays (Discord, Steam, Game "
                                       "Bar, RTSS) and windows on top of the game.");
                    ShowTooltip("Status 7: the driver's window manager doesn't report the swapchain's window\n"
                                "in its fullscreen state, the window is likely composed by DWM\n"
                                "or shown on an overlay plane");
                    break;

                case AmdExtFlipMeterStatus_NoPacingInfo:
                    ImGui::TextWrapped(LOC(UI_The_driver_got_flips_without_usable_pacing_info));
                    ShowTooltip("Status 8: a flip came without a flip meter Present, with a target time of 0\n"
                                "or with more generated frames than configured");
                    break;

                case AmdExtFlipMeterStatus_Retry:
                    ImGui::TextWrapped(LOC(UI_The_driver_has_no_pacing_config_for_the_swapchain));
                    break;

                default:
                    ImGui::TextWrapped(LOC(UI_The_driver_hasn_t_reported_on_the_flips_yet));
                    break;
                }
            }
            else if (status.metering)
            {
                ImGui::TextColored(StatusOk(), LOC(UI_Driver_pacing_s),
                                   status.mode == AmdExtFlipMeterMode_FrameIndex ? LOC(UI_frame_index_mode)
                                                                                 : LOC(UI_adaptive_mode));
                ShowTooltip("AMD driver flip metering holds the flips of the generated frames\n"
                            "Adaptive mode is FSR's and paces one generated frame,\n"
                            "frame index mode paces more");
            }
            else if (status.generatedFrames != 0)
            {
                ImGui::TextColored(StatusError(), LOC(UI_Driver_pacing_off_only_the_game_s_frames_are_shown));
            }

            if (!status.coversOutput)
                ImGui::TextColored(StatusError(), LOC(UI_Needs_exclusive_fullscreen_or_a_window_covering_the_whole_di));

            if (FAILED(status.configResult))
            {
                ImGui::TextColored(StatusError(), LOC(UI_The_driver_rejected_the_config_08X_status_u),
                                   (UINT) status.configResult, status.configStatus);
            }

            const int providerMax = FlipMeterFG_Dx12::ProviderMaxInterpolationCount();
            const int driverMax = (int) swapChain->MaxGeneratedFrames();

            if (driverMax < providerMax)
            {
                // The file's path or why it couldn't be written, kept while the menu is open
                static std::optional<std::expected<std::filesystem::path, std::string>> regFile;

                // At least 6X, like DLSSG, so other FG replacements don't need a new one. The driver allows 15.
                const UINT regFrames = (UINT) std::clamp(std::max(providerMax, 5), 1, 15);

                ImGui::TextColored(StatusWarn(), LOC(UI_The_driver_allows_up_to_dX_the_FG_replacement_dX), driverMax + 1,
                                   providerMax + 1);
                ImGui::TextWrapped("AMD's driver only paces 2X by default. More needs a registry setting.\n"
                                   "Button below creates a .reg file next to the game's exe.\n"
                                   "Run it and restart your computer.");

                if (ImGui::Button(LOC(UI_Create_reg_file_flipmeter)))
                    regFile = swapChain->WriteMaxGenFramesRegFile(regFrames);

                ShowTooltip(std::format("Sets the DWORD Gmx_DriverFramePacingMaxGenFrames to {} ({}X) in the GPU's\n"
                                        "driver key under HKLM\\SYSTEM\\CurrentControlSet\\Control\\Class\\\n"
                                        "{{4d36e968-e325-11ce-bfc1-08002be10318}}",
                                        regFrames, regFrames + 1)
                                .c_str());

                if (regFile.has_value() && regFile->has_value())
                {
                    ImGui::TextColored(StatusOk(), LOC(UI_Saved_s), wstring_to_string(regFile->value().wstring()).c_str());
                    ImGui::TextWrapped("Double-click it and allow the change (needs admin), then restart Windows. "
                                       "The file says how to undo it.");
                }
                else if (regFile.has_value())
                {
                    ImGui::TextColored(StatusError(), LOC(UI_Couldn_t_create_it_s), regFile->error().c_str());
                }
            }
        }
    }

    if (auto reprojectionOutput = state.CurrentFG();
        ScopedCard::Shows("fg_reprojection") && reprojectionOutput && reprojectionOutput->HasReprojection())
    {
        ScopedCard card { LOC(UI_Reprojection_Settings) };

        if (reprojectionOutput->IsActive() && reprojectionOutput->IsReprojectionActive())
        {
            ImGui::Text(LOC(UI_Updated_camera_rotation_by_1fms),
                        (float) reprojectionOutput->GetLastTimeSinceSimStartNs() / 1'000'000.f);
        }
        else if (reprojectionOutput->IsActive() && reprojectionOutput->GetReproject() &&
                 reprojectionOutput->GetReproject()->IsBypassed())
        {
            ImGui::TextColored(StatusWarn(), LOC(UI_Paused_3rd_person_detected));
        }
        else
        {
            ImGui::TextDisabled(LOC(UI_Not_updating_camera_rotation));
        }

        if (state.activeFgOutput == FGOutput::Reprojection)
        {
            RenderFGModeSlider(ctx, LOC(UI_Reprojection_mode), 0, nullptr);
        }
        else
        {
            bool reprojectionActive = config->FGReprojectionEnabled.value_or_default();
            if (ImGui::Checkbox(LOC(UI_Active_5), &reprojectionActive))
            {
                config->FGReprojectionEnabled = reprojectionActive;
                LOG_DEBUG("Reprojection enabled: {}", reprojectionActive);
            }
            ShowTooltip(LOC(UI_Enable_reprojection));
        }

        ImGui::SameLine();

        ImGui::Checkbox(LOC(UI_Show_static_elements), &state.fgHudlessCompare);
        ShowTooltip("For fine tuning the depth cutoff\n"
                    "Shows UI and depth cutoff areas\n"
                    "Adjust depth cutoff so that only stuff like your gun and hands are marked");

        ImGui::Spacing();

        // clang-format off
        static std::vector<MenuOption<ReprojectionFill>> fillModes = {
            { ReprojectionFill::SoftEdge, LOC(UI_Soft_edge) },
            { ReprojectionFill::StrechEdge, LOC(UI_Strech_edge) },
            { ReprojectionFill::Dithering, "Dithering" },
            { ReprojectionFill::Noise, "Noise" },
            { ReprojectionFill::Debug, LOC(UI_LogLevel_Debug) }
        };
        // clang-format on

        // need to have a value before combo
        if (!config->ReprojectionFillMode.has_value())
            config->ReprojectionFillMode = config->ReprojectionFillMode.value_or_default();

        PopulateCombo(LOC(UI_Edge_fill_mode), config->ReprojectionFillMode, fillModes);
        ShowTooltip("Soft edge and strech fill the screen edges with the edge pixels,\n"
                    "soft edge blurs them more the further out it goes\n"
                    "Dither and noise use the unprojected image as fill\n"
                    "and then some blending on the edges to fool the eye");

        float cutoff = config->ReprojectionDepthCutoff.value_or_default();
        if (ImGui::SliderFloat(LOC(UI_Depth_cutoff), &cutoff, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_Logarithmic))
            config->ReprojectionDepthCutoff = cutoff;
        ShowTooltip("Selects how many elements close to the camera\n"
                    "should be shown over the reprojected image.\n"
                    "This prevents your gun from being moved in weird ways.\n\n"
                    "Use \"Show static elements\" to help you adjust it\n"
                    "Unreal Engine games are usually around 0.10\n"
                    "Cyberpunk is around 0.02");

        uint32_t cutoffExpandPx = config->ReprojectionCutoffExpand.value_or_default();
        if (SliderUInt(LOC(UI_Cutoff_expand), &cutoffExpandPx, 0, 8))
            config->ReprojectionCutoffExpand = cutoffExpandPx;
        ShowTooltip("Expands the cutoff area by this many depth pixels\n"
                    "Use only when you see an outline left by the cutoff process");

        if (bool lateLatch = config->ReprojectionLateLatch.value_or_default();
            ImGui::Checkbox(LOC(UI_Late_latch_mouse), &lateLatch))
        {
            config->ReprojectionLateLatch = lateLatch;
        }
        ShowTooltip("Reads the mouse right when the GPU starts the reprojection\n"
                    "instead of when the frame is presented on the CPU.\n"
                    "Lowers latency, especially when GPU bound");

        if (bool directionCache = config->ReprojectionDirectionCache.value_or_default();
            ImGui::Checkbox(LOC(UI_Direction_cache), &directionCache))
        {
            config->ReprojectionDirectionCache = directionCache;
        }
        ShowTooltip("Remembers what was around the camera in previous frames\n"
                    "and uses it to fill the screen edges and the area under your hands.\n"
                    "Edge fill mode is only used where the cache has nothing.\n"
                    "Shown with a blue tint in debug mode");

        const auto reproject = reprojectionOutput->GetReproject();
        const bool hasMotionStats = reproject && reproject->HasMotionStats();

        bool parallaxAuto = config->ReprojectionParallaxAuto.value_or_default();
        const float parallaxRowStart = ImGui::GetCursorPosX();

        if (ImGui::Checkbox(LOC(UI_Auto_parallax), &parallaxAuto))
            config->ReprojectionParallaxAuto = parallaxAuto;
        ShowTooltip("Estimates the parallax scale from the game's motion vectors\n"
                    "Needs some walking around, until then the cache only follows rotation");

        ImGui::SameLine();

        // The checkbox takes part of the row, the slider gets what is left so the label still fits
        const float parallaxSliderWidth = ImGui::CalcItemWidth() - (ImGui::GetCursorPosX() - parallaxRowStart);

        if (parallaxAuto)
        {
            float estimate = reproject ? reproject->ParallaxScaleEstimate() : 0.0f;

            ImGui::BeginDisabled();
            ImGui::SetNextItemWidth(parallaxSliderWidth);
            ImGui::SliderFloat(LOC(UI_Cache_parallax_scale), &estimate, 0.0f, 10000.0f,
                               estimate > 0.0f ? "%.1f"
                                               : (hasMotionStats ? "Estimating..." : LOC(UI_No_usable_motion_vectors)),
                               ImGuiSliderFlags_Logarithmic);
            ImGui::EndDisabled();
        }
        else
        {
            float parallaxScale = config->ReprojectionParallaxScale.value_or_default();
            ImGui::SetNextItemWidth(parallaxSliderWidth);
            if (ImGui::SliderFloat(LOC(UI_Cache_parallax_scale), &parallaxScale, 0.0f, 10000.0f, "%.1f",
                                   ImGuiSliderFlags_Logarithmic))
            {
                config->ReprojectionParallaxScale = parallaxScale;
            }
        }
        ShowTooltip("Converts depth distances into the units of the camera position\n"
                    "so the cache can follow the camera when you walk.\n"
                    "0 = only follow rotation\n"
                    "Wrong values make the cache fill wobble while walking\n"
                    "Also used by the 3rd person detection\n"
                    "Unreal Engine is usually around 1000");

        if (bool thirdPersonDetection = config->ReprojectionThirdPersonDetection.value_or_default();
            ImGui::Checkbox(LOC(UI_Pause_in_3rd_person), &thirdPersonDetection))
        {
            config->ReprojectionThirdPersonDetection = thirdPersonDetection;
        }
        ShowTooltip("Reprojection would swing the character around with the camera,\n"
                    "so it gets paused while something in the middle of the screen follows the camera.\n"
                    "Uses the game's motion vectors and the cache parallax scale.\n"
                    "Vehicle cockpits can count as 3rd person too.\n"
                    "Disable to keep reprojection always active");

        ImGui::SameLine();

        if (!hasMotionStats)
        {
            ImGui::TextDisabled(LOC(UI_no_usable_motion_vectors));
        }
        else if (reproject->ParallaxScale() <= 0.0f)
        {
            ImGui::TextDisabled(LOC(UI_needs_the_parallax_scale));
        }
        else if (reproject->ThirdPersonDetected())
        {
            ImGui::TextColored(StatusWarn(), LOC(UI_S_3rd_person_detected_2f_orbit_2f), reproject->ThirdPersonScore(),
                               reproject->OrbitScore());
        }
        else
        {
            ImGui::TextDisabled(LOC(UI_S_1st_person_2f_orbit_2f), reproject->ThirdPersonScore(),
                                reproject->OrbitScore());
        }
    }

    // OptiFG
    if (ScopedCard::Shows("fg_optifg") && state.api != API::Vulkan && state.currentFGSwapchain != nullptr &&
        state.activeFgInput == FGInput::Upscaler)
    {
        ScopedCard card { LOC(UI_OptiFG_Settings) };

        if (currentFeature != nullptr && !currentFeature->IsFrozen() &&
            ((state.activeFgOutput == FGOutput::FSRFG && FfxApiProxy::IsFsrFgReady()) ||
             (state.activeFgOutput == FGOutput::XeFG && XeFGProxy::Module() != nullptr) ||
             (state.activeFgOutput == FGOutput::DLSSG && StreamlineProxy::Module() != nullptr) ||
             state.activeFgOutput == FGOutput::Reprojection || state.activeFgOutput == FGOutput::FlipMeter))
        {
            const bool dx11HudfixTracking = state.swapchainInteropApi == SwapchainInteropApi::Dx11wDx12;
            const bool hudfixTrackingSupported =
                !Config::Instance()->FGDisableHUDFix.value_or_default() &&
                (state.swapchainInteropApi == SwapchainInteropApi::None || dx11HudfixTracking);

            if (hudfixTrackingSupported)
            {
                bool fgHudfix = config->FGHUDFix.value_or_default();

                if (ImGui::Checkbox(LOC(HUDFix), &fgHudfix))
                {
                    config->FGHUDFix = fgHudfix;
                    LOG_DEBUG("Enabled set FGHUDFix: {}", fgHudfix);
                    state.clearCapturedHudlesses = true;
                    state.fgChanged = true;
                }

                ShowTooltip(LOC(TIP_Enable_Hud_Stability_Fix_227));

                ImGui::BeginDisabled(!config->FGHUDFix.value_or_default());

                ImGui::SameLine(0.0f, 16.0f);
                ImGui::PushItemWidth(95.0f * menuResScale);
                int hudFixLimit = config->FGHUDLimit.value_or_default();
                if (ImGui::InputInt(LOC(UI_Limit), &hudFixLimit))
                {
                    if (hudFixLimit < 1)
                        hudFixLimit = 1;
                    else if (hudFixLimit > 999)
                        hudFixLimit = 999;

                    config->FGHUDLimit = hudFixLimit;
                    LOG_DEBUG("Enabled set FGHUDLimit: {}", hudFixLimit);
                }
                ShowTooltip(LOC(TIP_Delay_Hudless_Capture_High_228));

                ImGui::SameLine(0.0f, 16.0f);
                if (ImGui::Button(LOC(BTN_Res_142)))
                    _showHudlessWindow = !_showHudlessWindow;

                ImGui::EndDisabled();

                auto hudExtended = config->FGHUDFixExtended.value_or_default();
                if (ImGui::Checkbox(LOC(Extended), &hudExtended))
                {
                    LOG_DEBUG("Enabled set FGHUDFixExtended: {}", hudExtended);
                    config->FGHUDFixExtended = hudExtended;
                }
                ShowTooltip(LOC(UI_Extended_format_checks_for_possible_HUDless_nMight_cause_cra));
                ImGui::SameLine(0.0f, 16.0f);

                ImGui::BeginDisabled(!config->FGHUDFix.value_or_default());

                auto immediate = config->FGImmediateCapture.value_or_default();
                if (ImGui::Checkbox(LOC(CHK_Immediate_Capture_63), &immediate))
                {
                    LOG_DEBUG("Enabled set FGImmediateCapture: {}", immediate);
                    config->FGImmediateCapture = immediate;
                }
                ShowTooltip("Enables capturing of resources before shader execution.\nIncrease HUDless "
                            "capture chances, but might cause capturing of unnecessary resources.");

                ImGui::PopItemWidth();

                ImGui::EndDisabled();
            }

            bool depthScale = config->FGEnableDepthScale.value_or_default();
            if (ImGui::Checkbox(LOC(CHK_Scale_Depth_To_Fix_64), &depthScale))
                config->FGEnableDepthScale = depthScale;
            ShowTooltip(LOC(TIP_Fix_For_Dlss_D_230));

            bool resourceFlip = config->FGResourceFlip.value_or_default();
            if (ImGui::Checkbox(LOC(CHK_Flip_Unity_65), &resourceFlip))
                config->FGResourceFlip = resourceFlip;
            ShowTooltip(LOC(TIP_Flip_Velocity_Depth_Resources_231));

            ImGui::SameLine(0.0f, 16.0f);

            bool hudlessFlip = config->FGHudlessFlip.value_or_default();
            if (ImGui::Checkbox(LOC(UI_Flip_HUDless), &hudlessFlip))
                config->FGHudlessFlip = hudlessFlip;
            ShowTooltip(LOC(UI_Flip_the_HUDless_texture_nGames_can_also_request_it_with_the));

            ImGui::SameLine(0.0f, 16.0f);

            bool resourceFlipOffset = config->FGResourceFlipOffset.value_or_default();
            if (ImGui::Checkbox(LOC(CHK_Flip_Use_Offset_66), &resourceFlipOffset))
                config->FGResourceFlipOffset = resourceFlipOffset;
            ShowTooltip(LOC(TIP_Use_Height_Difference_As_232));

            if (auto ch = ScopedCollapsingHeader(LOC(Advanced_OptiFG_Settings)); ch.IsHeaderOpen())
            {
                ScopedIndent indent {};

                ImGui::Spacing();

                auto disablehudfix = config->FGDisableHUDFix.value_or_default();
                {
                    if (ImGui::Checkbox(LOC(UI_Disable_HUDFix_Resource_Tracking), &disablehudfix))
                    {
                        config->FGDisableHUDFix = disablehudfix;
                        LOG_DEBUG("Enabled FGDisableHUDFix: {}", disablehudfix);
                    }
                    ShowTooltip("Disables HUDFix resource tracking and\n"
                                "reduces CPU overhead.\n"
                                "Useful for MLFG which deals well with the HUD.\n\n"
                                "After changing, please Save Settings and restart.");
                }

                if (hudfixTrackingSupported)
                {
                    ImGui::Spacing();

                    auto rb = config->FGResourceBlocking.value_or_default();
                    if (ImGui::Checkbox(LOC(CHK_Resource_Blocking_67), &rb))
                    {
                        config->FGResourceBlocking = rb;
                        LOG_DEBUG("Enabled set FGResourceBlocking: {}", rb);
                    }
                    ShowTooltip("Block rarely used resources from using as HUDless \n"
                                "to prevent flickers and other issues\n\n"
                                "HUDfix enable/disable will reset the block list!");

                    ImGui::SameLine(0.0f, 16.0f);

                    auto rrc = config->FGRelaxedResolutionCheck.value_or_default();
                    if (ImGui::Checkbox(LOC(CHK_Relaxed_Resource_Check_68), &rrc))
                    {
                        config->FGRelaxedResolutionCheck = rrc;
                        LOG_DEBUG("Enabled set FGRelaxedResolutionCheck: {}", rrc);
                    }
                    ShowTooltip("Relax resolution checks for HUDless by 32 pixels \n"
                                "Helps games which use black borders for some \n"
                                "resolutions and screen ratios (e.g. Witcher 3)");

                    ImGui::BeginDisabled(state.fgResetCapturedResources);
                    ImGui::PushItemWidth(95.0f * menuResScale);
                    if (ImGui::Checkbox(LOC(CHK_Fg_Create_List_69), &state.fgCaptureResources))
                    {
                        if (!state.fgCaptureResources)
                            config->FGHUDLimit = 1;
                        else
                            state.fgOnlyUseCapturedResources = false;
                    }

                    ImGui::SameLine(0.0f, 16.0f);
                    if (ImGui::Checkbox(LOC(CHK_Fg_Use_List_70), &state.fgOnlyUseCapturedResources))
                    {
                        if (state.fgCaptureResources)
                        {
                            state.fgCaptureResources = false;
                            config->FGHUDLimit = 1;
                        }
                    }

                    ImGui::SameLine(0.0f, 8.0f);
                    ImGui::Text(LOC(TXT_Captured), state.fgCapturedResourceCount);

                    ImGui::PopItemWidth();

                    ImGui::SameLine(0.0f, 16.0f);

                    if (ImGui::Button(LOC(BTN_Reset_List_143)))
                    {
                        LOG_DEBUG("Resetting captured resource list");

                        state.fgResetCapturedResources = true;
                        state.fgOnlyUseCapturedResources = false;
                    }

                    ImGui::EndDisabled();

                    ImGui::Spacing();
                    ImGui::Spacing();
                    if (ImGui::TreeNode(LOC(UI_Tracking_Settings)))
                    {
                        ImGui::BeginDisabled(dx11HudfixTracking);

                        auto ath = config->FGAlwaysTrackHeaps.value_or_default();
                        if (ImGui::Checkbox(LOC(CHK_Always_Track_Heaps_71), &ath))
                        {
                            config->FGAlwaysTrackHeaps = ath;
                            LOG_DEBUG("Enabled set FGAlwaysTrackHeaps: {}", ath);
                        }
                        ImGui::EndDisabled();

                        ShowTooltip(dx11HudfixTracking
                                        ? "D3D12 only; not applicable to DX11."
                                        : LOC(Always_track_resources));

                        auto disableRTV = config->FGHudfixDisableRTV.value_or_default();
                        if (ImGui::Checkbox(LOC(CHK_Disable_Rtv_Tracking_72), &disableRTV))
                            config->FGHudfixDisableRTV = disableRTV;
                        ShowTooltip("Disable tracking of CreateRenderTargetView\n"
                                    "This might help filtering of wrong HUDless resources");

                        ImGui::SameLine(0.0f, 16.0f);

                        auto disableSRV = config->FGHudfixDisableSRV.value_or_default();
                        if (ImGui::Checkbox(LOC(CHK_Disable_Srv_Tracking_73), &disableSRV))
                            config->FGHudfixDisableSRV = disableSRV;
                        ShowTooltip("Disable tracking of CreateShaderResourceView\n"
                                    "This might help filtering of wrong HUDless resources");

                        auto disableUAV = config->FGHudfixDisableUAV.value_or_default();
                        if (ImGui::Checkbox(LOC(CHK_Disable_Uav_Tracking_74), &disableUAV))
                            config->FGHudfixDisableUAV = disableUAV;
                        ShowTooltip("Disable tracking of CreateUnorderedAccessView\n"
                                    "This might help filtering of wrong HUDless resources");

                        ImGui::SameLine(0.0f, 16.0f);

                        auto disableOM = config->FGHudfixDisableOM.value_or_default();
                        if (ImGui::Checkbox(LOC(CHK_Disable_Om_Tracking_75), &disableOM))
                            config->FGHudfixDisableOM = disableOM;
                        ShowTooltip("Disable tracking of OMSetRenderTargets\n"
                                    "This might help filtering of wrong HUDless resources");

                        auto disableSCR = config->FGHudfixDisableSCR.value_or_default();
                        if (ImGui::Checkbox(LOC(CHK_Disable_Scr_Tracking_76), &disableSCR))
                            config->FGHudfixDisableSCR = disableSCR;
                        ShowTooltip("Disable tracking of SetComputeRootDescriptorTable\n"
                                    "This might help filtering of wrong HUDless resources");

                        ImGui::SameLine(0.0f, 16.0f);

                        auto disableSGR = config->FGHudfixDisableSGR.value_or_default();
                        if (ImGui::Checkbox(LOC(CHK_Disable_Sgr_Tracking_77), &disableSGR))
                            config->FGHudfixDisableSGR = disableSGR;
                        ShowTooltip("Disable tracking of SetGraphicsRootDescriptorTable\n"
                                    "This might help filtering of wrong HUDless resources");

                        ImGui::Spacing();

                        auto disableDI = config->FGHudfixDisableDI.value_or_default();
                        if (ImGui::Checkbox(LOC(CHK_Disable_Di_Tracking_78), &disableDI))
                            config->FGHudfixDisableDI = disableDI;
                        ShowTooltip("Disable tracking of DrawInstanced\n"
                                    "This might help filtering of wrong HUDless resources");

                        ImGui::SameLine(0.0f, 16.0f);

                        auto disableDII = config->FGHudfixDisableDII.value_or_default();
                        if (ImGui::Checkbox(LOC(CHK_Disable_Dii_Tracking_79), &disableDII))
                            config->FGHudfixDisableDII = disableDII;
                        ShowTooltip("Disable tracking of DrawIndexedInstanced\n"
                                    "This might help filtering of wrong HUDless resources");

                        auto disableDispatch = config->FGHudfixDisableDispatch.value_or_default();
                        if (ImGui::Checkbox(LOC(CHK_Disable_Dispatch_Tracking_80), &disableDispatch))
                            config->FGHudfixDisableDispatch = disableDispatch;
                        ShowTooltip("Disable tracking of Dispatch\n"
                                    "This might help filtering of wrong HUDless resources");

                        ImGui::TreePop();
                    }
                }

                ImGui::Spacing();
                if (ImGui::TreeNode(LOC(UI_Resource_Settings)))
                {
                    bool makeMVCopies = config->FGMakeMVCopy.value_or_default();
                    if (ImGui::Checkbox(LOC(CHK_Fg_Make_Mv_Copies_81), &makeMVCopies))
                        config->FGMakeMVCopy = makeMVCopies;
                    ShowTooltip(LOC(Make_a_copy));

                    bool makeDepthCopies = config->FGMakeDepthCopy.value_or_default();
                    if (ImGui::Checkbox(LOC(CHK_Fg_Make_Depth_Copies_82), &makeDepthCopies))
                        config->FGMakeDepthCopy = makeDepthCopies;
                    ShowTooltip(LOC(Make_a_copy_2));

                    ImGui::PushItemWidth(115.0f * menuResScale);
                    float depthScaleMax = config->FGDepthScaleMax.value_or_default();
                    if (ImGui::InputFloat(LOC(UI_FG_Scale_Depth_Max), &depthScaleMax, 10.0f, 100.0f, "%.1f"))
                        config->FGDepthScaleMax = depthScaleMax;
                    ShowTooltip(LOC(TIP_Depth_Values_Will_Be_233));
                    ImGui::PopItemWidth();

                    ImGui::TreePop();
                }

                ImGui::Spacing();
                if (ImGui::TreeNode(LOC(UI_Syncing_Settings)))
                {
                    bool useMutexForPresent = config->FGUseMutexForSwapchain.value_or_default();
                    if (ImGui::Checkbox(LOC(CHK_Fg_Use_Mutex_For_83), &useMutexForPresent))
                        config->FGUseMutexForSwapchain = useMutexForPresent;
                    ShowTooltip(LOC(Use_mutex_to));

                    ImGui::TreePop();
                }

                ImGui::Spacing();
                ImGui::Spacing();
            }
        }
        else if (currentFeature == nullptr || currentFeature->IsFrozen())
        {
            ImGui::Text(LOC(TXT_Upscaler_Is_Not_Active_14)); // Probably never will be visible
        }
        else if (state.activeFgOutput == FGOutput::FSRFG && !FfxApiProxy::IsFsrFgReady())
        {
            ImGui::TextColored(toneMapColor({ 1.0f, 0.0f, 0.0f, 1.0f }),
                               LOC(UI_amd_fidelityfx_dx12_dll_is_missing)); // Probably never will be visible
        }
        else if (state.activeFgOutput == FGOutput::XeFG && XeFGProxy::Module() == nullptr)
        {
            ImGui::TextColored(toneMapColor({ 1.0f, 0.0f, 0.0f, 1.0f }),
                               LOC(UI_libxess_fg_dll_is_missing)); // Probably never will be visible
        }
    }

    if (ScopedCard::Shows("fg_optifg") && state.api == API::Vulkan && state.currentVkFG != nullptr &&
        state.activeFgInput == FGInput::Upscaler)
    {
        ScopedCard card { LOC(UI_OptiFG_Settings) };

        if (!config->FGDisableHUDFix.value_or_default())
        {
            bool fgHudfix = config->FGHUDFix.value_or_default();
            if (ImGui::Checkbox(LOC(HUDFix), &fgHudfix))
                config->FGHUDFix = fgHudfix;
            ShowTooltip(LOC(TIP_Enable_Hud_Stability_Fix_227));

            ImGui::BeginDisabled(!fgHudfix);

            ImGui::SameLine(0.0f, 16.0f);
            ImGui::PushItemWidth(95.0f * menuResScale);
            int hudFixLimit = config->FGHUDLimit.value_or_default();
            if (ImGui::InputInt(LOC(UI_Limit), &hudFixLimit))
                config->FGHUDLimit = std::clamp(hudFixLimit, 1, 999);
            ShowTooltip(LOC(TIP_Delay_Hudless_Capture_High_228));
            ImGui::PopItemWidth();

            ImGui::SameLine(0.0f, 16.0f);
            auto hudExtended = config->FGHUDFixExtended.value_or_default();
            if (ImGui::Checkbox(LOC(Extended), &hudExtended))
                config->FGHUDFixExtended = hudExtended;
            ShowTooltip(LOC(UI_Accept_HUDless_candidates_of_any_format));

            auto uiTexture = config->FGHUDFixUITexture.value_or_default();
            if (ImGui::Checkbox(LOC(UI_UI_Texture), &uiTexture))
                config->FGHUDFixUITexture = uiTexture;
            ShowTooltip(LOC(UI_Use_the_UI_texture_the_game_blends_on_the_frame_in_its_last_));

            const bool dlssgOutput = state.activeFgOutput == FGOutput::DLSSG;
            ImGui::BeginDisabled(!uiTexture || dlssgOutput);
            ImGui::SameLine(0.0f, 16.0f);

            auto uiUnblend = !dlssgOutput && config->FGHUDFixUIUnblend.value_or(true);
            if (ImGui::Checkbox(LOC(UI_Unblend_UI), &uiUnblend))
                config->FGHUDFixUIUnblend = uiUnblend;
            ShowTooltip("Rebuild the HUDless by removing the UI texture from the frame\n"
                        "FSR-FG gets the UI only this way, DLSSG takes the UI texture directly");

            ImGui::EndDisabled();

            ImGui::EndDisabled();
        }

        bool resourceFlip = config->FGResourceFlip.value_or_default();
        if (ImGui::Checkbox(LOC(CHK_Flip_Unity_65), &resourceFlip))
            config->FGResourceFlip = resourceFlip;
        ShowTooltip(LOC(TIP_Flip_Velocity_Depth_Resources_231));

        ImGui::SameLine(0.0f, 16.0f);

        bool resourceFlipOffset = config->FGResourceFlipOffset.value_or_default();
        if (ImGui::Checkbox(LOC(CHK_Flip_Use_Offset_66), &resourceFlipOffset))
            config->FGResourceFlipOffset = resourceFlipOffset;
        ShowTooltip(LOC(TIP_Use_Height_Difference_As_232));
    }

    const FGNvngxReplacement activeNvngxFg = state.activeFgNvngx;

    bool nvngxLoaded = false;
    if (state.swapchainApi == Vulkan)
        nvngxLoaded = Nvngx_FG::isVulkanAvailable();
    if (state.swapchainApi == DX12)
        nvngxLoaded = Nvngx_FG::isDx12Available();

    // Status and the shared settings are in the Frame Generation card, skip a card with only a title
    const bool nvngxHasSettings =
        activeNvngxFg == FGNvngxReplacement::Arturs ||
        (activeNvngxFg == FGNvngxReplacement::Nukems && !state.nukemsFgFileAvailable) ||
        (nvngxLoaded && (Nvngx_FG::supportsDispatchFlags() || activeNvngxFg == FGNvngxReplacement::Nukems ||
                         activeNvngxFg == FGNvngxReplacement::FFX || activeNvngxFg == FGNvngxReplacement::Combo));

    if (ScopedCard::Shows("fg_nvngx") && activeNvngxFg != FGNvngxReplacement::None && nvngxHasSettings)
    {
        ScopedCard card;

        if (activeNvngxFg == FGNvngxReplacement::Nukems)
        {
            SeparatorWithHelpMarker(LOC(SEP_Frame_Generation_Fsr3_Fg_198),
                                    "Requires Nukem's dlssg_to_fsr3 dll");

            if (!state.nukemsFgFileAvailable)
            {
                ImGui::TextColored(StatusError(), LOC(UI_Please_put_dlssg_to_fsr3_amd_is_better_dll_into_OptiScaler_f));
            }
        }
        else if (activeNvngxFg == FGNvngxReplacement::Arturs)
        {
            SeparatorWithHelpMarker(LOC(SEP_Frame_Generation_Fsr3_Mfg_197),
                                    "DLSS Enabler as dlss-enabler-headless.dll");

            if (!state.artursFgFileAvailable)
            {
                ImGui::TextColored(StatusError(), LOC(UI_Please_put_dlss_enabler_headless_dll_into_OptiScaler_folder));
            }

            ImGui::TextColored(StatusWarn(), LOC(UI_Using_a_subset_of_features_from_DLSS_Enabler));
        }
        else if (activeNvngxFg == FGNvngxReplacement::FFX)
        {
            SeparatorWithHelpMarker(LOC(UI_Frame_Generation_FSRFG_via_FFX), LOC(UI_FFX_using_the_DLSSG_swapchain));
        }
        else if (activeNvngxFg == FGNvngxReplacement::Custom)
        {
            SeparatorWithHelpMarker(StrFmt(LOC(UI_Frame_Generation_s), state.activeFgNvngxCustom.c_str()).c_str(),
                                    "Custom DLSSG from the custom folder");
        }
        else if (activeNvngxFg == FGNvngxReplacement::Combo)
        {
            SeparatorWithHelpMarker(LOC(UI_Frame_Generation_Enabler_FFX),
                                    "FFX for middle fake frames, and Enabler for the rest\n\n2x - FFX\n"
                                    "3x - Enabler\n4x - FFX + Enabler\n5x - Enabler\n6x - FFX + Enabler");
        }

        if (nvngxLoaded)
        {
            // DLSS Enabler's own, or any custom provider reporting its antighosting version
            if (Nvngx_FG::supportsDispatchFlags())
            {
                auto antighostingVer = Nvngx_FG::extraVersion();

                if (activeNvngxFg == FGNvngxReplacement::Arturs || activeNvngxFg == FGNvngxReplacement::Combo)
                {
                    auto featureVer = Nvngx_FG::version();
                    ImGui::Text(LOC(UI_DE_Ver_d_d_d_d_GB_Ver_d_d), featureVer.major, featureVer.minor,
                                featureVer.patch, featureVer.reserved, antighostingVer.major, antighostingVer.minor);
                }
                else
                {
                    ImGui::Text(LOC(UI_GB_Ver_d_d), antighostingVer.major, antighostingVer.minor);
                }

                static std::vector<FlagDefinition> common_flags = {
                    { LOC(UI_Antighosting_GB), 0x00100000, LOC(UI_Enable_anti_ghosting_correction) },
                    { LOC(UI_Temporal_HUD_pin), 0x04000000, LOC(UI_Enable_temporal_HUD_pinning_present_backbuffer_stability) }
                };

                static std::vector<FlagDefinition> uncommon_flags = {
                    //{ "Hudless UI mask", 0x02000000, LOC(UI_Use_HUD_less_as_UI_mask_DL2_inverted_semantics) },
                    { LOC(UI_HUD_interpolation), 0x08000000, LOC(UI_HUD_OF_interpolation_0_legacy_pin_present_1_OF_warp) },
                    { LOC(UI_Ignore_UI_texture), 0x10000000, LOC(UI_Ignore_dedicated_DLSSG_UI_texture_force_legacy_HUD_path) },
                    //{ "Dp4a active", 0x20000000, LOC(UI_OF_pipeline_using_dp4a_accelerated_SSD_SM_6_4) },
                    { LOC(UI_Pin_backbuffer), 0x40000000, LOC(UI_Pin_DLSSG_Backbuffer_to_subframe_1_snapshot_across_MFG_frame) }
                };

                static std::vector<FlagDefinition> debug_flags = {
                    { LOC(UI_Antighosting_red_tint), 0x00200000, LOC(UI_Debug_red_tint_on_corrected_pixels) },
                    { LOC(UI_Antighosting_split_screen), 0x00400000, LOC(UI_Debug_split_screen_comparison) },
                    { LOC(UI_Frame_index_line), 0x00010000, "" },
                    { LOC(UI_HUD_detection), 0x00020000, "" },
                    { LOC(UI_Disocclusion_tint), 0x00040000, "" },
                    { LOC(UI_Artifacts_detection), 0x00080000, "" },
                    { LOC(UI_Camera_MV_debug), 0x00800000, LOC(UI_Debug_blue_tint_where_camera_MV_fallback_is_used) },
                    { LOC(UI_Generic_visualization), 0x01000000, LOC(UI_Debug_trapezoid_zone_visualization) }
                };

                uint32_t temp_flags = config->NvngxFGDispatchFlags.value_or_default();
                bool changed = false;

                ImGui::Text(LOC(TXT_Raw_Dispatchflags_16));
                changed |= ImGui::InputScalar(LOC(UI_RawFlags), ImGuiDataType_U32, &temp_flags, NULL, NULL, "%08X",
                                              ImGuiInputTextFlags_CharsHexadecimal);

                ImGui::SameLine(0.0f, 20.0f * menuResScale);
                if (bool showDebug = config->NvngxFGShowDebug.value_or_default();
                    ImGui::Checkbox(LOC(CHK_Show_Debug_86), &showDebug))
                {
                    config->NvngxFGShowDebug = showDebug;
                }
                ShowTooltip(LOC(TIP_Required_For_Debug_Flags_235));

                ImGui::Spacing();

                if (auto ch = ScopedCollapsingHeader(LOC(Active_DispatchFlags)); ch.IsHeaderOpen())
                {
                    ScopedIndent indent {};

                    auto render_flags = [&](const std::vector<FlagDefinition>& flags)
                    {
                        for (const auto& flag : flags)
                        {
                            changed |= ImGui::CheckboxFlags(flag.name.c_str(), &temp_flags, flag.mask);

                            if (ImGui::IsItemHovered() && !flag.description.empty())
                            {
                                ImGui::SetTooltip("%s", flag.description.c_str());
                            }
                        }
                    };

                    ImGui::TextDisabled("Common");
                    render_flags(common_flags);

                    ImGui::Spacing();
                    ImGui::TextDisabled("Uncommon");
                    render_flags(uncommon_flags);

                    if (config->NvngxFGShowDebug.value_or_default())
                    {
                        ImGui::Spacing();
                        ImGui::TextDisabled(LOC(UI_LogLevel_Debug));
                        render_flags(debug_flags);
                    }
                }

                if (changed)
                {
                    config->NvngxFGDispatchFlags = temp_flags;
                }
            }

            if (activeNvngxFg == FGNvngxReplacement::Nukems)
            {
                if (ImGui::Checkbox(LOC(CHK_Enable_Debug_View_87), &state.dlssgDebugView))
                {
                    Nvngx_FG::setDebugView(state.dlssgDebugView);
                }
                if (ImGui::Checkbox(LOC(CHK_Interpolated_Frames_Only_88), &state.dlssgInterpolatedOnly))
                {
                    Nvngx_FG::setInterpolatedOnly(state.dlssgInterpolatedOnly);
                }
            }

            if (activeNvngxFg == FGNvngxReplacement::FFX || activeNvngxFg == FGNvngxReplacement::Combo)
            {
                if (_ffxFGIndex < 0 || _ffxFGIndex >= (int) state.ffxFGVersionNames.size())
                    _ffxFGIndex = config->FfxFGIndex.value_or_default();

                if (state.ffxFGVersionNames.size() > 0)
                {
                    ImGui::PushItemWidth(135.0f * menuResScale);

                    auto currentName = StrFmt(LOC(UI_FSR_s), state.ffxFGVersionNames[_ffxFGIndex]);
                    if (ImGui::BeginCombo(LOC(LBL_FFX_FG), currentName.c_str()))
                    {
                        for (int n = 0; n < state.ffxFGVersionIds.size(); n++)
                        {
                            auto name = StrFmt(LOC(UI_FSR_s), state.ffxFGVersionNames[n]);
                            if (ImGui::Selectable(name.c_str(), config->FfxFGIndex.value_or_default() == n))
                                _ffxFGIndex = n;
                        }

                        ImGui::EndCombo();
                    }
                    ImGui::PopItemWidth();

                    ShowTooltip(LOC(TIP_List_Of_Fgs_Reported_211));

                    ImGui::SameLine(0.0f, 6.0f);

                    if (ImGui::Button(LOC(BTN_Change_Fg_138)) && _ffxFGIndex != config->FfxFGIndex.value_or_default())
                    {
                        config->FfxFGIndex = _ffxFGIndex;
                        state.fgChanged = true;
                    }
                }

                bool fgAsync = config->FGAsync.value_or_default();
                if (ImGui::Checkbox(LOC(UI_Allow_Async_2), &fgAsync))
                {
                    config->FGAsync = fgAsync;

                    if (config->IsFGEnabled())
                    {
                        state.fgChanged = true;
                        LOG_DEBUG("Async set FGChanged");
                    }
                }
                ShowTooltip(LOC(TIP_Enable_Async_For_Better_213));

                ImGui::SameLine(0.0f, 20.0f * menuResScale);
                bool fgDV = config->FGDebugView.value_or_default();
                if (ImGui::Checkbox(LOC(UI_Debug_View_3), &fgDV))
                {
                    config->FGDebugView = fgDV;

                    if (config->IsFGEnabled())
                    {
                        state.fgChanged = true;
                        LOG_DEBUG("DebugView set FGChanged");
                    }
                }
                ShowTooltip(LOC(Enable_FSR3_1));

                if (Nvngx_FG::version().major > 3)
                {
                    ImGui::SameLine(0.0f, 20.0f * menuResScale);
                    if (bool fgwm = config->FSRFGEnableWatermark.value_or_default();
                        ImGui::Checkbox(LOC(CHK_Enable_Watermark_48), &fgwm))
                    {
                        LOG_DEBUG("FSRFGEnableWatermark set FGWatermark: {}", fgwm);
                        config->FSRFGEnableWatermark = fgwm;
                        Util::SetFlagEnvVar(L"MLFI-WATERMARK", fgwm);

                        // The driver reads the variable on context creation
                        if (config->IsFGEnabled())
                            state.fgChanged = true;
                    }
                }
            }
        }
    }
}

void MenuCommon::RenderFsrCommonSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;

    if (currentFeature != nullptr && !currentFeature->IsFrozen())
    {
        // FSR Common -----------------
        if (currentFeature != nullptr && !currentFeature->IsFrozen() &&
            (state.activeFgOutput == FGOutput::FSRFG || IsFsr(currentBackend)))
        {
            SeparatorWithHelpMarker(LOC(SEP_Fsr_Common_Settings_201), LOC(UI_Affects_both_FSR_FG_Upscalers));

            bool useFsrVales = config->FsrUseFsrInputValues.value_or_default();
            if (ImGui::Checkbox(LOC(CHK_Use_Fsr_Input_Values_91), &useFsrVales))
                config->FsrUseFsrInputValues = useFsrVales;

            ImGui::Spacing();
            if (auto ch = ScopedCollapsingHeader(LOC(FoV_Camera_Values)); ch.IsHeaderOpen())
            {
                ScopedIndent indent {};
                ImGui::Spacing();

                bool useVFov = config->FsrVerticalFov.has_value() || !config->FsrHorizontalFov.has_value();

                float vfov = config->FsrVerticalFov.value_or_default();
                float hfov = config->FsrHorizontalFov.value_or(90.0f);

                if (useVFov && !config->FsrVerticalFov.has_value())
                    config->FsrVerticalFov = vfov;
                else if (!useVFov && !config->FsrHorizontalFov.has_value())
                    config->FsrHorizontalFov = hfov;

                if (ImGui::RadioButton(LOC(UI_Use_Vert_Fov), useVFov))
                {
                    config->FsrHorizontalFov.reset();
                    config->FsrVerticalFov = vfov;
                    useVFov = true;
                }

                ImGui::SameLine(0.0f, 6.0f);

                if (ImGui::RadioButton(LOC(UI_Use_Horz_Fov), !useVFov))
                {
                    config->FsrVerticalFov.reset();
                    config->FsrHorizontalFov = hfov;
                    useVFov = false;
                }

                if (useVFov)
                {
                    if (ImGui::SliderFloat(LOC(UI_Vert_FOV), &vfov, 0.0f, 180.0f, "%.1f"))
                        config->FsrVerticalFov = vfov;

                    ShowTooltip(LOC(TIP_Might_Help_Achieve_Better_238));
                }
                else
                {
                    if (ImGui::SliderFloat(LOC(UI_Horz_FOV), &hfov, 0.0f, 180.0f, "%.1f"))
                        config->FsrHorizontalFov = hfov;

                    ShowTooltip(LOC(TIP_Might_Help_Achieve_Better_238));
                }

                float cameraNear;
                float cameraFar;

                cameraNear = config->FsrCameraNear.value_or_default();
                cameraFar = config->FsrCameraFar.value_or_default();

                if (ImGui::SliderFloat(LOC(UI_Camera_Near), &cameraNear, 0.1f, 500000.0f, "%.1f"))
                    config->FsrCameraNear = cameraNear;
                ShowTooltip(LOC(Might_help_achieve_2));

                if (ImGui::SliderFloat(LOC(UI_Camera_Far), &cameraFar, 0.1f, 500000.0f, "%.1f"))
                    config->FsrCameraFar = cameraFar;
                ShowTooltip(LOC(Might_help_achieve_2));

                if (ImGui::Button(LOC(BTN_Reset_Camera_Values_144)))
                {
                    config->FsrVerticalFov.reset();
                    config->FsrHorizontalFov.reset();
                    config->FsrCameraNear.reset();
                    config->FsrCameraFar.reset();
                }

                ImGui::SameLine(0.0f, 6.0f);
                ImGui::Text(LOC(UI_Near_1f_Far_1f),
                            state.lastFsrCameraNear < 500000.0f ? state.lastFsrCameraNear : 500000.0f,
                            state.lastFsrCameraFar < 500000.0f ? state.lastFsrCameraFar : 500000.0f);

                ImGui::Spacing();
                ImGui::Spacing();
            }
        }
    }
}

void MenuCommon::RenderFramerateSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& menuResScale = ctx.menuResScale;

    // Framerate ---------------------
    if (state.reflexLimitsFps || config->OverlayMenu.value_or_default())
    {
        SeparatorWithHelpMarker(LOC(Framerate_Title), LOC(Uses_Reflex_when_possible));

        static std::string currentMethod {};
        const LowLatencyMode activeOutput = InputCommon::active_output();

        // The Reflex output limits through the driver's Reflex
        if (state.reflexLimitsFps)
        {
            currentMethod = "Reflex";
        }
        else if (InputCommon::can_limit_fps())
        {
            // The game's XeFG with its own XeLL
            if (InputXeLL::IsNative())
                currentMethod = LOC(UI_Game_s_XeLL);
            else if (activeOutput == LowLatencyMode::AntiLag2)
                currentMethod = LOC(UI_FSR_Anti_Lag_2_0);
            else if (activeOutput == LowLatencyMode::LatencyFlex)
                currentMethod = "LatencyFlex";
            else if (activeOutput == LowLatencyMode::AntiLagVk)
                currentMethod = LOC(UI_Vulkan_AntiLag);
            else if (activeOutput == LowLatencyMode::Reflex)
                currentMethod = "Reflex";
            else if (activeOutput == LowLatencyMode::FpsLimiter)
                currentMethod = LOC(UI_FPS_Limiter);
            else
                currentMethod = "XeLL";
        }
        else
        {
            currentMethod = LOC(UI_Fallback);
        }

        if (state.rtssReflexInjection)
            currentMethod.append(LOC(UI_RTSS));

        ImGui::Text(LOC(UI_Current_method_s), currentMethod.c_str());

        if (activeOutput == LowLatencyMode::AntiLag2)
            ShowTooltip(LOC(TIP_Fsr_Anti_Lag_2_239));

        if (state.reflexShowWarning)
        {
            ImGui::TextColored(StatusError(), LOC(UI_Using_Reflex_s_limit_with_FSR_FG_has_performance_overhead));

            ImGui::Spacing();
        }

        // set initial value
        if (std::isinf(_limitFps))
            _limitFps = config->FramerateLimit.value_or_default();

        float fpsSliderMax = (float) std::max(200, refreshRate);
        ImGui::SliderFloat(LOC(UI_FPS_Limit), &_limitFps, 0, fpsSliderMax, "%.0f");

        if (ImGui::Button(LOC(BTN_Apply_Limit_145)))
        {
            config->FramerateLimit = _limitFps;
        }

        ImGui::SameLine(0.0f, 16.0f);

        if (ImGui::Button(LOC(BTN_Reset_Limit_146)))
        {
            _limitFps = 0.0f;
            config->FramerateLimit = _limitFps;
        }

        ImGui::Spacing();
        if (auto ch = ScopedCollapsingHeader(LOC(VRR_Frame_Cap)); ch.IsHeaderOpen())
        {
            ScopedIndent indent {};
            ImGui::Spacing();

            ImGui::PushItemWidth(105.0f * menuResScale);
            ImGui::InputInt(LOC(UI_Refresh_Rate), &refreshRate, 1, 1, ImGuiInputTextFlags_None);
            ImGui::PopItemWidth();

            float refreshRateF = static_cast<float>(refreshRate);
            // it's fine to use with real reflex, we only care about antilag
            auto fpsLimitTech = InputCommon::active_output();
            constexpr float margin = 0.3f; // in ms
            float frameCap = std::round(10000.f / (1000.f / refreshRateF + margin)) / 10.f;

            if (fpsLimitTech == LowLatencyMode::AntiLag2 || fpsLimitTech == LowLatencyMode::AntiLagVk)
                frameCap = std::round(frameCap);

            ImGui::Text(LOC(UI_Calculated_Cap_1f), frameCap);

            ImGui::SameLine(0.0f, 16.0f);

            if (ImGui::Button(LOC(BTN_Set_As_Fps_Limit_147)))
            {
                _limitFps = frameCap;
                config->FramerateLimit = _limitFps;
            }
        }
    }
}

template <typename T> std::string GetMenuOptionLabel(const std::vector<MenuOption<T>>& options, T targetValue)
{
    auto it = std::find_if(options.begin(), options.end(),
                           [targetValue](const MenuOption<T>& option) { return option.value == targetValue; });

    if (it != options.end())
    {
        return it->label;
    }

    return LOC(UI_Unknown);
}

void MenuCommon::RenderLowLatencySettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;

    // Low Latency ---------------------------
    SectionTitle(LOC(SEP_Low_Latency_177));

    static std::vector<MenuOption<LowLatencyInput>> lowLatencyInput = {
        { LowLatencyInput::None, LOC(UI_None) },
        { LowLatencyInput::Auto, LOC(Default) },
        { LowLatencyInput::AntiLag2, LOC(UI_FSR_Anti_Lag_2_0) },
        { LowLatencyInput::Reflex, "Reflex" },
        { LowLatencyInput::XeLL, "XeLL" },
        { LowLatencyInput::UeLowLatency, "UeLowLatency" },
        { LowLatencyInput::OptiScaler, "OptiScaler" },
    };

    static std::vector<MenuOption<LowLatencyMode>> lowLatencyOutput = {
        { LowLatencyMode::None, LOC(UI_None) },
        { LowLatencyMode::Auto, LOC(UI_Auto) },
        { LowLatencyMode::LatencyFlex, "LatencyFlex" },
        { LowLatencyMode::AntiLag2, LOC(UI_FSR_Anti_Lag_2_0) },
        { LowLatencyMode::XeLL, "XeLL" },
        { LowLatencyMode::AntiLagVk, LOC(UI_AntiLag_Vk) },
        { LowLatencyMode::Reflex, "Reflex" },
        { LowLatencyMode::FpsLimiter, LOC(UI_FPS_Limiter), LOC(UI_Only_limits_the_FPS) },
    };

    LowLatencyInput activeInput {};
    LowLatencyMode activeOutput {};

    if (ImGui::BeginTable("lowLatencyActive", 2, ImGuiTableFlags_SizingStretchSame))
    {
        InputCommon::get_currently_active(activeInput, activeOutput);

        ImGui::TableNextColumn();

        ImGui::Text(LOC(UI_Active_input_s), GetMenuOptionLabel(lowLatencyInput, activeInput).c_str());

        ImGui::TableNextColumn();

        ImGui::Text(LOC(UI_Active_output_s), GetMenuOptionLabel(lowLatencyOutput, activeOutput).c_str());

        ImGui::EndTable();
    }

    if (ImGui::BeginTable("lowLatencySelection", 2, ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableNextColumn();

        auto avalibleInputs = InputCommon::get_avaliable_inputs();

        // Inputs become available as the game starts sending them
        for (auto type : { LowLatencyInput::AntiLag2, LowLatencyInput::Reflex, LowLatencyInput::XeLL,
                           LowLatencyInput::UeLowLatency })
        {
            auto& option = lowLatencyInput[(uint32_t) type];
            option.disabled = !avalibleInputs[type];
            option.tooltip = option.disabled ? LOC(UI_The_game_doesn_t_send_it) : "";
        }

        auto defaultInput = InputCommon::default_input();
        lowLatencyInput[(uint32_t) LowLatencyInput::None].set_hidden(true);

        // Only the default when the game has nothing of its own
        lowLatencyInput[(uint32_t) LowLatencyInput::OptiScaler].set_hidden(true);
        lowLatencyInput[(uint32_t) LowLatencyInput::Auto].label = LOC(UI_Auto);
        lowLatencyInput[(uint32_t) LowLatencyInput::Auto].tooltip =
            "The best input the game sends: Reflex, XeLL when enabled in the game, AntiLag 2, UE, XeLL.\n"
            "AntiLag 2 first while the game's FSR-FG reports its frames through it.\n"
            "Without any, OptiScaler's own with the DLSSG output.";

        auto forcedInput = InputCommon::forced_input();
        auto input = forcedInput.value_or(config->LowLatencyInput.value_or_default());

        if (input == LowLatencyInput::None)
            input = LowLatencyInput::Auto;

        auto selectedInput = input;

        ImGui::BeginDisabled(forcedInput.has_value());
        PopulateCombo(LOC(UI_Input), selectedInput, lowLatencyInput);
        ImGui::EndDisabled();

        if (forcedInput.has_value() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip(LOC(UI_Set_by_XeFG_the_game_s_own_keeps_its_XeLL));
        else if (selectedInput != input)
            config->LowLatencyInput = selectedInput;

        ImGui::TableNextColumn();

        lowLatencyOutput[(uint32_t) LowLatencyMode::None].set_hidden(true);

        // Each API has its own, the other API's selection maps to it. The API can change, so no sticky setters.
        const auto api = InputCommon::uses_api();
        const bool vulkan = api == API::Vulkan;
        const bool outputForced = InputCommon::forced_output().has_value();

        // OptiScaler's XeFG paces with XeLL on D3D12 (also for D3D11 games through interop), the GPU's Reflex or
        // AntiLag 2 output can sleep instead while XeFG keeps an XeLL context of its own
        const bool optiXefg = state.activeFgOutput == FGOutput::XeFG;

        lowLatencyOutput[(uint32_t) LowLatencyMode::AntiLag2].hidden = vulkan;
        lowLatencyOutput[(uint32_t) LowLatencyMode::XeLL].hidden =
            !outputForced && !optiXefg && (vulkan || api == API::DX11);
        lowLatencyOutput[(uint32_t) LowLatencyMode::AntiLagVk].hidden = !vulkan;

        auto disable = [](MenuOption<LowLatencyMode>& option, bool condition, const char* reason)
        {
            option.disabled = condition;
            option.tooltip = condition ? reason : "";
        };

        for (auto mode : { LowLatencyMode::Reflex, LowLatencyMode::AntiLag2, LowLatencyMode::AntiLagVk })
        {
            auto reason = InputCommon::output_unavailable(mode, api);
            disable(lowLatencyOutput[(uint32_t) mode], reason != nullptr, reason);
        }

        disable(lowLatencyOutput[(uint32_t) LowLatencyMode::LatencyFlex], optiXefg, LOC(UI_Not_with_XeFG));

        lowLatencyOutput[(uint32_t) LowLatencyMode::Auto].label = LOC(UI_Auto);
        lowLatencyOutput[(uint32_t) LowLatencyMode::Auto].tooltip =
            vulkan ? "The GPU's own: AntiLag on AMD, Reflex on Nvidia, LatencyFlex otherwise"
            : api == API::DX11
                ? "The GPU's own: AntiLag 2 on AMD, Reflex on Nvidia, LatencyFlex otherwise"
                : LOC(UI_The_GPU_s_own_AntiLag_2_on_AMD_Reflex_on_Nvidia_XeLL_on_Inte);

        if (optiXefg)
        {
            auto& autoTooltip = lowLatencyOutput[(uint32_t) LowLatencyMode::Auto].tooltip;
            autoTooltip = LOC(UI_XeLL_XeFG_paces_with_it);

            if (!lowLatencyOutput[(uint32_t) LowLatencyMode::Reflex].disabled)
                autoTooltip += LOC(UI_nReflex_can_do_the_sleeping_instead_XeFG_keeps_an_XeLL_conte);
            else if (!lowLatencyOutput[(uint32_t) LowLatencyMode::AntiLag2].disabled)
                autoTooltip += LOC(UI_nAntiLag_2_can_do_the_sleeping_instead_XeFG_keeps_an_XeLL_co);
        }

        // Frame generation decides the output it works with
        auto forcedOutput = InputCommon::forced_output();
        auto output = forcedOutput.value_or(config->LowLatencyOutput.value_or_default());

        // An output the GPU can't run works as Auto
        if (output == LowLatencyMode::None || InputCommon::output_unavailable(output, api))
            output = LowLatencyMode::Auto;

        // The other API's AntiLag
        if (vulkan && output == LowLatencyMode::AntiLag2)
            output = LowLatencyMode::AntiLagVk;
        else if (!vulkan && output == LowLatencyMode::AntiLagVk)
            output = LowLatencyMode::AntiLag2;

        // No XeLL on D3D11 unless OptiScaler's XeFG runs it on D3D12, Auto picks the GPU's own
        if (!forcedOutput.has_value() && !optiXefg && api == API::DX11 && output == LowLatencyMode::XeLL)
            output = LowLatencyMode::Auto;

        auto selectedOutput = output;

        ImGui::BeginDisabled(forcedOutput.has_value());
        PopulateCombo(LOC(UI_Debug_Output), selectedOutput, lowLatencyOutput);
        ImGui::EndDisabled();

        if (forcedOutput.has_value() && ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip(LOC(UI_Set_by_XeFG_the_game_s_own_keeps_its_XeLL_only_the_FPS_limit));
        else if (selectedOutput != output)
            config->LowLatencyOutput = selectedOutput;

        ImGui::EndTable();
    }

    if (InputCommon::xefg_output_change_pending())
    {
        ImGui::PushStyleColor(ImGuiCol_Text, StatusWarn());
        ImGui::TextWrapped(LOC(UI_Save_Settings_and_restart_the_game_XeFG_keeps_the_low_latenc));
        ImGui::PopStyleColor();
    }

    if (auto reason = InputCommon::incompatibility(activeInput, activeOutput))
    {
        ImGui::PushStyleColor(ImGuiCol_Text, StatusWarn());
        ImGui::TextWrapped(LOC(UI_Incompatible_s), reason);
        ImGui::PopStyleColor();
    }

    if (activeOutput == LowLatencyMode::LatencyFlex)
    {
        static const std::vector<MenuOption<LFXMode>> lfx_modes = {
            { LFXMode::Conservative, LOC(UI_Conservative), LOC(UI_The_safest_but_might_not_reduce_latency_well) },
            { LFXMode::Aggressive, LOC(UI_Aggressive),
              LOC(UI_Improves_latency_but_in_some_cases_will_lower_FPS_more_than_) },
            { LFXMode::ReflexIDs, LOC(UI_Reflex_ID),
              LOC(TT_Conservative_Mode) }
        };

        PopulateCombo(LOC(UI_LatencyFlex_mode), config->FN_LatencyFlexMode, lfx_modes);
    }

    static std::vector<MenuOption<ForceReflex>> lowlatency_states = { { ForceReflex::InGame, LOC(LBL_Follow_in_game) },
                                                                      { ForceReflex::ForceDisable, LOC(UI_Force_Disable) },
                                                                      { ForceReflex::ForceEnable, LOC(UI_Force_Enable) } };

    ImGui::SetNextItemWidth(150.0f * ctx.menuResScale);
    PopulateCombo(LOC(UI_Force_State), config->FN_ForceReflex, lowlatency_states);
}

void MenuCommon::RenderActiveImageSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;
    auto& menuResScale = ctx.menuResScale;

    bool rcasEnabled = false;

    // SHARPNESS -----------------------------
    SectionTitle(LOC(Sharpness));

    if (bool overrideSharpness = config->OverrideSharpness.value_or_default();
        ImGui::Checkbox(LOC(Override), &overrideSharpness))
    {
        config->OverrideSharpness = overrideSharpness;

        if ((currentBackend == Upscaler::DLSS || currentBackend == Upscaler::DLSS_on12) &&
            currentFeature->Version().major < 3)
        {
            state.newBackend = SameBackend(currentBackend);
            MARK_ALL_BACKENDS_CHANGED();
        }
    }
    ShowTooltip(LOC(Ignores_the_value));

    ImGui::SameLine(0.0f, 16.0f * menuResScale);

    float featuresCurrentSharpness = currentFeature->Sharpness();
    if (featuresCurrentSharpness > 0.0f)
        ImGui::TextDisabled(LOC(UI_Current_3f), featuresCurrentSharpness);
    else
        ImGui::TextDisabled(LOC(UI_Current_disabled));

    ImGui::BeginDisabled(!config->OverrideSharpness.value_or_default());

    float sharpness = config->Sharpness.value_or_default();

    if (ImGui::SliderFloat(LOC(UI_Strength), &sharpness, 0.0f, 1.0f))
        config->Sharpness = sharpness;

    ImGui::EndDisabled();

    // RCAS
    {
        // xess or dlss version >= 2.5.1
        constexpr feature_version requiredDlssVersion = { 2, 5, 1 };
        const bool isDlss = currentBackend == Upscaler::DLSS || currentBackend == Upscaler::DLSS_on12;
        rcasEnabled = currentBackend == Upscaler::XeSS || (isDlss && currentFeature->Version() >= requiredDlssVersion);

        // Determine current mode for the dropdown
        bool currentRcasEnabled = config->RcasEnabled.value_or(rcasEnabled);
        auto currentShader = Config::Instance()->SharpnessShader.value_or_default();

        int selectedMode = 0; // 0 = Upscaler's Default
        if (currentRcasEnabled)
        {
            if (currentShader == SharpenShader::RCAS)
                selectedMode = 1;
            else if (currentShader == SharpenShader::DepthAware)
                selectedMode = 2;
            else if (currentShader == SharpenShader::DepthAwareDAS)
                selectedMode = 3;
            else if (currentShader == SharpenShader::BabaNeural)
                selectedMode = 4;
            else if (currentShader == SharpenShader::OptiSharpen)
                selectedMode = 5;
        }

        const char* sharpenModes[] = { LOC(UI_Upscaler_s_default), "RCAS",          LOC(UI_Depth_Aware_RCAS),
                                       LOC(UI_Depth_Aware_DAS),  LOC(UI_Neural_BaBa), LOC(UI_OptiSharpen) };

        if (ImGui::Combo(LOC(UI_Method), &selectedMode, sharpenModes, IM_ARRAYSIZE(sharpenModes)))
        {
            if (selectedMode == 0)
            {
                config->RcasEnabled = false;
            }
            else
            {
                config->RcasEnabled = true;
                if (selectedMode == 1)
                    Config::Instance()->SharpnessShader = SharpenShader::RCAS;
                else if (selectedMode == 2)
                    Config::Instance()->SharpnessShader = SharpenShader::DepthAware;
                else if (selectedMode == 3)
                    Config::Instance()->SharpnessShader = SharpenShader::DepthAwareDAS;
                else if (selectedMode == 4)
                    Config::Instance()->SharpnessShader = SharpenShader::BabaNeural;
                else if (selectedMode == 5)
                    Config::Instance()->SharpnessShader = SharpenShader::OptiSharpen;
            }
        }

        ShowTooltip("Select the sharpening filter to apply.\n\n"
                    "Upscaler's default: Uses the sharpening provided by the upscaler.\n"
                    "RCAS: AMD's RCAS, modified for Contrast and MAS.\n"
                    "Depth Aware: Smarter sharpening with fewer artifacts (heavier).\n"
                    "Neural (BaBa): Small neural network from the BaBa ReShade shaders. Only changes the luma and\n"
                    "skips aliased edges and fireflies. 0.5 strength is the default intensity of the ReShade shader.\n"
                    "OptiSharpen: Small neural network trained for OptiScaler. Only changes the luma, works the same\n"
                    "for SDR and HDR. 0.5 strength is the trained strength.\n"
                    "Neural shaders use RCAS when their shader isn't available for the current API.\n\n"
                    "By default uses a sharpening value provided by the game.\n"
                    "Select 'Override' under 'Sharpness' and adjust the slider to change it.");

        // Only show MAS and Advanced parameters if a custom sharpness mode is active
        if (selectedMode != 0)
        {
            ImGui::Spacing();

            if (bool overrideMotionSharpness = config->MotionSharpnessEnabled.value_or_default();
                ImGui::Checkbox(LOC(CHK_Enable_Motion_Adaptive_Sharpness_95), &overrideMotionSharpness))
                config->MotionSharpnessEnabled = overrideMotionSharpness;
            ShowTooltip(LOC(TIP_Enables_Sharpness_Adjustments_According_240));

            // Shared by the DA and Neural shaders
            auto ClampOutputCheckbox = [config]()
            {
                if (bool clamp = config->DAClampOutput.value_or(false); ImGui::Checkbox(LOC(CHK_Clamp_Output_97), &clamp))
                {
                    if (clamp)
                        config->DAClampOutput = true;
                    else
                        config->DAClampOutput.reset();
                }

                ShowTooltip("Clamps the final image to the [0, 1] range.\n\n"
                            "Prevents overshoot artifacts such as bright halos or negative colors.\n"
                            "Recommended for LDR pipelines; optional for HDR depending on tone-mapping.\n\n"
                            "Off when not set, games that report SDR can still send HDR color");
            };

            if (Config::Instance()->SharpnessShader.value_or_default() == SharpenShader::BabaNeural)
            {
                if (bool overrideMSDebug = config->MotionSharpnessDebug.value_or_default();
                    ImGui::Checkbox(LOC(CHK_Mas_Debug_99), &overrideMSDebug))
                    config->MotionSharpnessDebug = overrideMSDebug;

                ShowTooltip(LOC(Areas_that_are));

                if (auto ch = ScopedCollapsingHeader(LOC(UI_Neural_Sharpening_Parameters)); ch.IsHeaderOpen())
                {
                    ScopedIndent indent {};
                    ImGui::Spacing();

                    const char* babaModels[] = { LOC(UI_Model_A_standard), LOC(UI_Model_B_edge_enhanced) };
                    int babaModel = std::clamp(config->BabaNeuralModel.value_or_default(), 0, 1);

                    if (ImGui::Combo(LOC(UI_Neural_Model), &babaModel, babaModels, IM_ARRAYSIZE(babaModels)))
                        config->BabaNeuralModel = babaModel;

                    ShowTooltip("Model A: Standard sharpening.\n"
                                "Model B: Edge-enhanced.");

                    float edgeResponse = config->BabaNeuralEdgeResponse.value_or_default();
                    if (ImGui::SliderFloat(LOC(UI_Edge_Response), &edgeResponse, -1.0f, 1.0f, "%.2f",
                                           ImGuiSliderFlags_AlwaysClamp))
                        config->BabaNeuralEdgeResponse = edgeResponse;

                    ShowTooltip("Controls how the sharpening adapts to the detected edges.\n\n"
                                "Negative values smooth the edges.\n"
                                "Positive values enhance the edges.\n"
                                "0 is uniform.");

                    const char* babaViews[] = { "Normal", LOC(UI_Sharpening_map), LOC(UI_Residual_map) };
                    int babaView = std::clamp(config->BabaNeuralDebugView.value_or_default(), 0, 2);

                    if (ImGui::Combo(LOC(UI_Debug_View), &babaView, babaViews, IM_ARRAYSIZE(babaViews)))
                        config->BabaNeuralDebugView = babaView;

                    ShowTooltip("Normal: The final image.\n"
                                "Sharpening map: The sharpening that gets applied, plain gray means no change.\n"
                                "Residual map: Where the network wants to change the image.");

                    ClampOutputCheckbox();

                    if (ImGui::Button(LOC(UI_Reset_Neural_Values)))
                    {
                        config->BabaNeuralModel.reset();
                        config->BabaNeuralEdgeResponse.reset();
                        config->BabaNeuralDebugView.reset();
                    }
                }
            }
            else if (Config::Instance()->SharpnessShader.value_or_default() == SharpenShader::OptiSharpen)
            {
                if (bool overrideMSDebug = config->MotionSharpnessDebug.value_or_default();
                    ImGui::Checkbox(LOC(CHK_Mas_Debug_99), &overrideMSDebug))
                    config->MotionSharpnessDebug = overrideMSDebug;

                ShowTooltip(LOC(Areas_that_are));

                if (auto ch = ScopedCollapsingHeader(LOC(UI_OptiSharpen_Parameters)); ch.IsHeaderOpen())
                {
                    ScopedIndent indent {};
                    ImGui::Spacing();

                    const char* optiModels[] = { LOC(UI_Natural_D10), LOC(UI_Crisp_D10_p50), LOC(UI_Natural_D10_fp16_packed),
                                                 "Crisp D10 p50 (fp16 packed)" };
                    int optiModel = std::clamp(config->OptiSharpenModel.value_or_default(), 0, 3);

                    if (ImGui::Combo("Model", &optiModel, optiModels, IM_ARRAYSIZE(optiModels)))
                        config->OptiSharpenModel = optiModel;

                    ShowTooltip("Natural D10 (default): Natural sharpening.\n"
                                "Crisp D10 p50: Stronger.\n"
                                "(fp16 packed): The same networks in half precision, written for packed math.\n"
                                "Faster on Dx12 / Vulkan, Dx11 runs them in fp32.\n"
                                "All use a 25 tap (5x5) window and overshoot the local range by up to 15%.\n"
                                "Each model scales the strength, the top of the slider is its best point.");

                    const char* optiViews[] = { "Normal", LOC(UI_Sharpening_map), LOC(UI_Change_magnitude) };
                    int optiView = std::clamp(config->OptiSharpenDebugView.value_or_default(), 0, 2);

                    if (ImGui::Combo(LOC(UI_Debug_View), &optiView, optiViews, IM_ARRAYSIZE(optiViews)))
                        config->OptiSharpenDebugView = optiView;

                    ShowTooltip("Normal: The final image.\n"
                                "Sharpening map: The relative luma change, plain gray means no change.\n"
                                "Change magnitude: How much each pixel changes, black means no change.");

                    ClampOutputCheckbox();

                    if (ImGui::Button(LOC(UI_Reset_OptiSharpen_Values)))
                    {
                        config->OptiSharpenModel.reset();
                        config->OptiSharpenDebugView.reset();
                    }
                }
            }
            else if (Config::Instance()->SharpnessShader.value_or_default() != SharpenShader::RCAS)
            {
                if (bool overrideMSDebug = config->MotionSharpnessDebug.value_or_default();
                    ImGui::Checkbox(LOC(CHK_Da_Mas_Debug_96), &overrideMSDebug))
                    config->MotionSharpnessDebug = overrideMSDebug;

                ShowTooltip(LOC(Enable_DA_MAS));

                if (auto ch = ScopedCollapsingHeader(LOC(Advanced_DA_Parameters)); ch.IsHeaderOpen())
                {
                    ScopedIndent indent {};
                    ImGui::Spacing();

                    ClampOutputCheckbox();

                    if (currentFeature->DepthLinear())
                    {
                        float depthBias = config->DADepthBias.value_or(0.0015f);
                        if (ImGui::SliderFloat(LOC(UI_Depth_Bias), &depthBias, 0.005f, 0.03f, "%.4f"))
                            config->DADepthBias = depthBias;

                        ShowTooltip(LOC(Ignores_small_depth));

                        float depthScale = config->DADepthScale.value_or(250.0f);
                        if (ImGui::SliderFloat(LOC(UI_Depth_Scale), &depthScale, 100.0f, 600.0f, "%.1f"))
                            config->DADepthScale = depthScale;

                        ShowTooltip(LOC(Controls_how_strongly));
                    }
                    else
                    {
                        float depthBias = config->DADepthBias.value_or(0.001f);
                        if (ImGui::SliderFloat(LOC(UI_Depth_Bias), &depthBias, 0.0001f, 0.003f, "%.4f"))
                            config->DADepthBias = depthBias;

                        ShowTooltip(LOC(Ignores_small_depth));

                        float depthScale = config->DADepthScale.value_or(35.0f);
                        if (ImGui::SliderFloat(LOC(UI_Depth_Scale), &depthScale, 25.0f, 400.0f, "%.1f"))
                            config->DADepthScale = depthScale;

                        ShowTooltip(LOC(Controls_how_strongly));
                    }

                    if (ImGui::Button(LOC(BTN_Reset_Depth_Values_148)))
                    {
                        config->DADepthBias.reset();
                        config->DADepthScale.reset();
                    }
                }
            }
            else
            {
                if (bool contrastEnabled = config->ContrastEnabled.value_or_default();
                    ImGui::Checkbox(LOC(CHK_Contrast_Enabled_98), &contrastEnabled))
                    config->ContrastEnabled = contrastEnabled;

                ShowTooltip(LOC(TIP_Controls_Sharpness_At_High_241));

                ImGui::BeginDisabled(!config->ContrastEnabled.value_or_default());

                float contrast = config->Contrast.value_or_default();
                if (ImGui::SliderFloat(LOC(UI_Contrast), &contrast, 0.0f, 2.0f, "%.2f"))
                    config->Contrast = contrast;

                ShowTooltip(LOC(UI_Lowers_sharpness_at_high_contrast_areas));

                ImGui::EndDisabled();
            }

            ImGui::Spacing();

            if (auto ch = ScopedCollapsingHeader(LOC(Motion_Adaptive_Sharpness)); ch.IsHeaderOpen())
            {
                ScopedIndent indent {};
                ImGui::Spacing();

                ImGui::BeginDisabled(!config->MotionSharpnessEnabled.value_or_default());

                if (Config::Instance()->SharpnessShader.value_or_default() == SharpenShader::RCAS)
                {
                    if (bool overrideMSDebug = config->MotionSharpnessDebug.value_or_default();
                        ImGui::Checkbox(LOC(CHK_Mas_Debug_99), &overrideMSDebug))
                        config->MotionSharpnessDebug = overrideMSDebug;
                    ShowTooltip(LOC(Areas_that_are));
                }

                float motionSharpness = config->MotionSharpness.value_or_default();
                if (ImGui::SliderFloat(LOC(UI_MotionSharpness), &motionSharpness, -1.0f, 1.0f, "%.3f"))
                    config->MotionSharpness = motionSharpness;

                ShowTooltip(LOC(Maximum_amount_of));

                float motionThreshod = config->MotionThreshold.value_or_default();
                if (ImGui::SliderFloat(LOC(UI_MotionThreshod), &motionThreshod, 0.0f, 100.0f, "%.2f"))
                    config->MotionThreshold = motionThreshod;

                ShowTooltip(LOC(Minimum_motion_required));

                float motionScale = config->MotionScaleLimit.value_or_default();
                if (ImGui::SliderFloat(LOC(UI_MotionRange), &motionScale, 0.01f, 100.0f, "%.2f"))
                    config->MotionScaleLimit = motionScale;

                ShowTooltip(LOC(Defines_the_motion));

                ImGui::EndDisabled();

                ImGui::Spacing();
                ImGui::Spacing();
            }
        }
    }
}

void MenuCommon::RenderUpscaleRatioSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;
    auto& menuResScale = ctx.menuResScale;

    // UPSCALE RATIO OVERRIDE -----------------

    auto minSliderLimit = config->ExtendedLimits.value_or_default() ? 0.1f : 1.0f;
    auto maxSliderLimit = config->ExtendedLimits.value_or_default() ? 6.0f : 3.0f;

    SectionTitle(LOC(SEP_Upscale_Ratio_Override_178));

    if (ImGui::BeginTable("ratioOverride", 2, ImGuiTableFlags_SizingFixedFit))
    {
        ImGui::TableNextColumn();

        if (bool upOverride = config->UpscaleRatioOverrideEnabled.value_or_default();
            ImGui::Checkbox(LOC(CHK_Override_All_100), &upOverride))
        {
            config->UpscaleRatioOverrideEnabled = upOverride;

            if (upOverride)
                config->QualityRatioOverrideEnabled = false;
        }
        ShowTooltip(LOC(Overrides_every_upscaler));

        ImGui::TableNextColumn();

        if (bool qOverride = config->QualityRatioOverrideEnabled.value_or_default();
            ImGui::Checkbox(LOC(CHK_Override_Per_Quality_Preset_101), &qOverride))
        {
            config->QualityRatioOverrideEnabled = qOverride;

            if (qOverride)
                config->UpscaleRatioOverrideEnabled = false;
        }

        ShowTooltip(LOC(Lets_you_override));

        ImGui::EndTable();
    }

    if (config->UpscaleRatioOverrideEnabled.value_or_default())
    {
        float urOverride = config->UpscaleRatioOverrideValue.value_or_default();
        ImGui::SliderFloat(LOC(UI_All_Ratios), &urOverride, minSliderLimit, maxSliderLimit, "%.3f");
        config->UpscaleRatioOverrideValue = urOverride;
    }

    if (config->QualityRatioOverrideEnabled.value_or_default())
    {
        float qDlaa = config->QualityRatio_DLAA.value_or_default();
        if (ImGui::SliderFloat(LOC(UI_DLAA), &qDlaa, minSliderLimit, maxSliderLimit, "%.3f"))
            config->QualityRatio_DLAA = qDlaa;

        float qUq = config->QualityRatio_UltraQuality.value_or_default();
        if (ImGui::SliderFloat(LOC(UI_Ultra_Quality), &qUq, minSliderLimit, maxSliderLimit, "%.3f"))
            config->QualityRatio_UltraQuality = qUq;

        float qQ = config->QualityRatio_Quality.value_or_default();
        if (ImGui::SliderFloat(LOC(UI_Quality), &qQ, minSliderLimit, maxSliderLimit, "%.3f"))
            config->QualityRatio_Quality = qQ;

        float qB = config->QualityRatio_Balanced.value_or_default();
        if (ImGui::SliderFloat(LOC(UI_Balanced), &qB, minSliderLimit, maxSliderLimit, "%.3f"))
            config->QualityRatio_Balanced = qB;

        float qP = config->QualityRatio_Performance.value_or_default();
        if (ImGui::SliderFloat(LOC(UI_Performance), &qP, minSliderLimit, maxSliderLimit, "%.3f"))
            config->QualityRatio_Performance = qP;

        float qUp = config->QualityRatio_UltraPerformance.value_or_default();
        if (ImGui::SliderFloat(LOC(UI_Ultra_Performance), &qUp, minSliderLimit, maxSliderLimit, "%.3f"))
            config->QualityRatio_UltraPerformance = qUp;
    }
}

void MenuCommon::RenderOutputScalingSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;
    auto& menuResScale = ctx.menuResScale;

    // OUTPUT SCALING -----------------------------
    // if (state.api == DX12 || state.api == DX11)
    {
        // if motion vectors are not display size
        ImGui::BeginDisabled(!currentFeature->LowResMV() &&
                             currentFeature->RenderWidth() != currentFeature->DisplayWidth());

        SectionTitle(LOC(SEP_Output_Scaling_179));

        float defaultRatio = 1.5f;

        if (_ssRatio == 0.0f)
        {
            _ssRatio = config->OutputScalingMultiplier.value_or(defaultRatio);
            _ssEnabled = config->OutputScalingEnabled.value_or_default();
            _ssDownsampler = config->OutputScalingDownscaler.value_or_default();
        }

        ImGui::BeginDisabled((currentBackend == Upscaler::XeSS || currentBackend == Upscaler::DLSS ||
                              currentBackend == Upscaler::DLSS_on12) &&
                             currentFeature->RenderWidth() > currentFeature->DisplayWidth());
        ImGui::Checkbox(LOC(Enable), &_ssEnabled);
        ImGui::EndDisabled();

        ShowTooltip(LOC(Upscales_the_image));

        ImGui::SameLine(0.0f, 6.0f);

        ImGui::BeginDisabled(!_ssEnabled);
        {
            ImGui::PushItemWidth(105.0f * menuResScale);

            // clang-format off
                    std::vector<MenuOption<Scaler>> ds_options = {
                        { Scaler::FSR1, "FSR1",
                            LOC(UI_Default_option_nGood_enough_image_quality_and_very_fast) },
                        { Scaler::Bicubic, "Bicubic",
                            LOC(UI_Fastest_traditional_option_nProduces_a_very_soft_blurry_imag) },
                        { Scaler::CatmullRom, "Catmull-Rom",
                            LOC(UI_Designed_primarily_for_downscaling_nRetains_good_contrast_wi) },
                        { Scaler::Lanczos2, "Lanczos2",
                            LOC(UI_Lighter_and_faster_than_Lanczos3_nLess_prone_to_ringing_arte) },
                        { Scaler::Lanczos3, "Lanczos3",
                            "Heavier version of Lanczos2.\nOffers the sharpest image, but is the most prone to ringing.\nConsidered the best along with Kaiser3." },
                        { Scaler::Kaiser2, "Kaiser2",
                            LOC(UI_Similar_to_Lanczos2_nSmoother_and_less_prone_to_artefacts_th) },
                        { Scaler::Kaiser3, "Kaiser3",
                            "Similar to Lanczos3.\nFar less prone to artefacting than Lanczos3, but much heavier on the GPU.\nConsidered the best along with Lanczos3." },
                        { Scaler::Magic, "MAGIC",
                            LOC(UI_Specialised_to_prevent_artifacts_nEliminates_harsh_halos_for) }
                    };
            // clang-format on

            const bool isUpsampleRatio = _ssRatio < 1.0f;
            const std::string disabledReason = LOC(UI_Only_FSR1_and_Bicubic_are_supported_when_Ratio_is_below_1_0);

            for (auto& opt : ds_options)
            {
                if (isUpsampleRatio && opt.value > Scaler::Bicubic)
                    opt.set_disabled(true, opt.tooltip + "\n\n" + disabledReason);
            }

            if (isUpsampleRatio && _ssDownsampler > Scaler::Bicubic)
                _ssDownsampler = Scaler::FSR1;

            PopulateCombo(LOC(LBL_Downscaler), _ssDownsampler, ds_options);

            ImGui::PopItemWidth();
        }
        ImGui::EndDisabled();

        bool applyEnabled = _ssEnabled != config->OutputScalingEnabled.value_or_default() ||
                            _ssRatio != config->OutputScalingMultiplier.value_or(defaultRatio) ||
                            _ssDownsampler != config->OutputScalingDownscaler.value_or_default();

        ImGui::BeginDisabled(!applyEnabled);
        if (ImGui::Button(LOC(BTN_Apply_Change_149)))
        {
            config->OutputScalingEnabled = _ssEnabled;
            config->OutputScalingMultiplier = _ssRatio;

            if (_ssRatio < 1.0f && _ssDownsampler > Scaler::Bicubic)
                _ssDownsampler = Scaler::FSR1;

            config->OutputScalingDownscaler = _ssDownsampler;

            // Ray reconstruction keeps its DLSSD, the upscalers their configured one
            if (state.rrHandles.contains(currentFeature->Handle()->Id))
                state.newBackend = Upscaler::Reset;
            else if (currentFeature->GetUpscalerType() == Upscaler::DLSSD)
                state.newBackend = Upscaler::DLSSD;
            else
                state.newBackend = SameBackend(currentBackend);

            MARK_ALL_BACKENDS_CHANGED();
        }
        ImGui::EndDisabled();

        ImGui::BeginDisabled(!_ssEnabled || currentFeature->RenderWidth() > currentFeature->DisplayWidth());
        ImGui::SliderFloat(LOC(UI_Ratio), &_ssRatio, 0.5f, 3.0f, "%.2f");
        ImGui::EndDisabled();

        if (currentFeature != nullptr && !currentFeature->IsFrozen())
        {
            ImGui::Text(LOC(UI_Output_Scaling_is_s_Target_Res_dx_d_2f_nJitter_Count_d),
                        config->OutputScalingEnabled.value_or_default() ? LOC(UI_ENABLED) : LOC(UI_DISABLED),
                        (uint32_t) (currentFeature->DisplayWidth() * _ssRatio),
                        (uint32_t) (currentFeature->DisplayHeight() * _ssRatio),
                        ((float) currentFeature->DisplayWidth() * _ssRatio) / (float) currentFeature->RenderWidth(),
                        currentFeature->JitterCount());
        }

        ImGui::EndDisabled();
    }
}

void MenuCommon::RenderInitFlagsSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;

    if (currentFeature == nullptr || currentFeature->IsFrozen())
        return;

    // INIT -----------------------------
    SectionTitle(LOC(SEP_Init_Flags_180));

    if (ImGui::BeginTable("init", 2, ImGuiTableFlags_SizingStretchSame))
    {
        ImGui::TableNextColumn();

        // AutoExposure is always enabled for XeSS with native Dx11
        bool autoExposureDisabled = state.api == API::DX11 && currentBackend == Upscaler::XeSS;
        ImGui::BeginDisabled(autoExposureDisabled);

        if (bool autoExposure = currentFeature->AutoExposure(); ImGui::Checkbox(LOC(CHK_Auto_Exposure_102), &autoExposure))
        {
            config->AutoExposure = autoExposure;
            ReInitUpscaler();
        }
        ShowTooltip(LOC(Some_Unreal_Engine));
        ShowResetButton(&config->AutoExposure, "R");

        ImGui::EndDisabled();

        ImGui::TableNextColumn();
        if (bool depth = currentFeature->DepthInverted(); ImGui::Checkbox(LOC(CHK_Depth_Inverted_104), &depth))
        {
            config->DepthInverted = depth;
            ReInitUpscaler();
        }
        ShowTooltip(LOC(TIP_You_Shouldn_T_Need_243));
        ShowResetButton(&config->DepthInverted, "R##2");

        ImGui::TableNextColumn();
        if (bool hdr = currentFeature->IsHdr(); ImGui::Checkbox(LOC(HDR), &hdr))
        {
            config->HDR = hdr;
            ReInitUpscaler();
        }
        ShowTooltip(LOC(TIP_Might_Help_With_Purple_244));
        ShowResetButton(&config->HDR, "R##1");

        ImGui::TableNextColumn();
        if (bool jitter = currentFeature->JitteredMV(); ImGui::Checkbox(LOC(CHK_Jitter_Cancellation_106), &jitter))
        {
            config->JitterCancellation = jitter;
            ReInitUpscaler();
        }
        ShowTooltip(LOC(TIP_Fix_For_Games_That_245));
        ShowResetButton(&config->JitterCancellation, "R##3");

        ImGui::TableNextColumn();
        if (bool mv = !currentFeature->LowResMV(); ImGui::Checkbox(LOC(CHK_Display_Res_Mv_105), &mv))
        {
            config->DisplayResolution = mv;

            // Disable output scaling when
            // Display res MV is active
            if (mv)
            {
                config->OutputScalingEnabled = false;
                _ssEnabled = false;
            }

            ReInitUpscaler();
        }
        ShowTooltip(LOC(Mostly_a_fix));
        ShowResetButton(&config->DisplayResolution, "R##4");

        ImGui::TableNextColumn();
        ImGui::BeginDisabled(currentBackend != Upscaler::FFX && currentBackend != Upscaler::FFX_on12);

        if (bool drs = currentFeature->DynamicResolution(); ImGui::Checkbox(LOC(UI_Dynamic_Res_DRS), &drs))
        {
            config->DynamicResolution = drs;
            ReInitUpscaler();
        }
        ShowTooltip("Enable if the game uses dynamic resolution\n"
                    "Prevents FSR 4 from switching models when the render size changes");
        ShowResetButton(&config->DynamicResolution, "R##5");

        ImGui::EndDisabled();

        ImGui::TableNextColumn();
        auto accessToReactiveMask = currentFeature->AccessToReactiveMask();
        ImGui::BeginDisabled(!accessToReactiveMask);

        bool canUseReactiveMask =
            accessToReactiveMask && currentBackend != Upscaler::DLSS && currentBackend != Upscaler::DLSS_on12 &&
            (currentBackend != Upscaler::XeSS || currentFeature->Version() >= feature_version { 2, 0, 1 });

        bool disableReactiveMask = config->DisableReactiveMask.value_or(!canUseReactiveMask);

        if (ImGui::Checkbox(LOC(CHK_Disable_Reactive_Mask_103), &disableReactiveMask))
        {
            config->DisableReactiveMask = disableReactiveMask;

            if (currentBackend == Upscaler::XeSS)
            {
                state.newBackend = SameBackend(currentBackend);
                MARK_ALL_BACKENDS_CHANGED();
            }
        }

        ImGui::EndDisabled();

        if (accessToReactiveMask)
            ShowTooltip(LOC(Allows_the_use));
        else
            ShowTooltip(LOC(TIP_Option_Disabled_Because_The_242));

        ImGui::EndTable();
    }

    if (currentFeature->AccessToReactiveMask())
    {
        ImGui::BeginDisabled(config->DisableReactiveMask.value_or(false));

        auto useAsTransparency = config->FsrUseMaskForTransparency.value_or_default();
        if (ImGui::Checkbox(LOC(CHK_Use_Reactive_Mask_As_26), &useAsTransparency))
            config->FsrUseMaskForTransparency = useAsTransparency;

        ImGui::EndDisabled();
    }

    if (currentFeature->AccessToReactiveMask() && currentBackend != Upscaler::DLSS &&
        currentBackend != Upscaler::DLSS_on12)
    {
        ImGui::BeginDisabled(config->DisableReactiveMask.value_or(currentBackend == Upscaler::XeSS));

        bool binaryMask = state.api == Vulkan || currentBackend == Upscaler::XeSS;
        auto defaultBias = binaryMask ? 0.0f : 0.45f;
        auto maskBias = config->DlssReactiveMaskBias.value_or(defaultBias);

        if (!binaryMask)
        {
            if (ImGui::SliderFloat(LOC(UI_React_Mask_Bias), &maskBias, 0.0f, 0.9f, "%.2f"))
                config->DlssReactiveMaskBias = maskBias;

            ShowTooltip(LOC(TIP_Values_Above_0_Activate_246));
        }
        else
        {
            bool useRM = maskBias > 0.0f;
            if (ImGui::Checkbox(LOC(CHK_Use_Binary_Reactive_Mask_107), &useRM))
            {
                if (useRM)
                    config->DlssReactiveMaskBias = 0.45f;
                else
                    config->DlssReactiveMaskBias.reset();
            }
        }

        ImGui::EndDisabled();
    }
}

void MenuCommon::RenderMagnifierSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;

    // Magnifier -----------------------------
    ImGui::Spacing();
    if (auto ch = ScopedCollapsingHeader(LOC(Magnifier)); ch.IsHeaderOpen())
    {
        ScopedIndent indent {};
        ImGui::Spacing();

        bool magnifierEnabled = config->MagnifierEnabled.value_or_default();
        if (ImGui::Checkbox(LOC(CHK_Enable_Magnifier_108), &magnifierEnabled))
            config->MagnifierEnabled = magnifierEnabled;

        ImGui::BeginDisabled(!magnifierEnabled);

        float magnifierSize = config->MagnifierSize.value_or_default();
        if (ImGui::SliderFloat(LOC(UI_Size), &magnifierSize, 5.0f, 50.0f, LOC(UI_S_1f_of_screen)))
            config->MagnifierSize = magnifierSize;

        int zoomFactor = config->MagnifierZoomFactor.value_or_default();
        if (ImGui::SliderInt(LOC(UI_Zoom_Factor), &zoomFactor, 2, 20, "%dx"))
            config->MagnifierZoomFactor = zoomFactor;

        float borderSize = config->MagnifierBorderSize.value_or_default();
        if (ImGui::SliderFloat(LOC(UI_Border_Size), &borderSize, 0.0f, 2.0f, LOC(UI_S_2f_of_screen)))
            config->MagnifierBorderSize = borderSize;

        ImGui::Separator();
        ImGui::Text(LOC(Positioning));

        bool staticMode = config->MagnifierStaticPosX.has_value() && config->MagnifierStaticPosY.has_value();
        if (staticMode)
        {
            float staticX = config->MagnifierStaticPosX.value();
            if (ImGui::SliderFloat(LOC(UI_Static_Pos_X), &staticX, 0.0f, 100.0f, LOC(UI_S_1f)))
                config->MagnifierStaticPosX = staticX;

            float staticY = config->MagnifierStaticPosY.value();
            if (ImGui::SliderFloat(LOC(UI_Static_Pos_Y), &staticY, 0.0f, 100.0f, LOC(UI_S_1f)))
                config->MagnifierStaticPosY = staticY;

            if (ImGui::Button(LOC(BTN_Reset_Static_Position_Follow_150)))
            {
                config->MagnifierStaticPosX.reset();
                config->MagnifierStaticPosY.reset();
            }
        }
        else
        {
            // Button to initialize static position mode
            if (ImGui::Button(LOC(BTN_Set_Static_Position_151)))
            {
                config->MagnifierStaticPosX = 50.0f;
                config->MagnifierStaticPosY = 50.0f;
            }
            ImGui::SameLine();
            ImGui::TextDisabled(LOC(UI_Currently_following_cursor));

            float offsetX = config->MagnifierCursorOffsetX.value_or_default();
            if (ImGui::SliderFloat(LOC(UI_Cursor_Offset_X), &offsetX, -300.0f, 300.0f, LOC(UI_S_0f_px)))
                config->MagnifierCursorOffsetX = offsetX;

            float offsetY = config->MagnifierCursorOffsetY.value_or_default();
            if (ImGui::SliderFloat(LOC(UI_Cursor_Offset_Y), &offsetY, -300.0f, 300.0f, LOC(UI_S_0f_px)))
                config->MagnifierCursorOffsetY = offsetY;
        }

        ImGui::EndDisabled();
        ImGui::Spacing();
    }
}

// List loaded ASI plugins, with their version when the file has one
static std::vector<std::string> ListedAsiPlugins(State& state)
{
    std::vector<std::string> names;
    std::lock_guard<std::mutex> lock(state.loadedAsiPluginsMutex);
    for (const auto& plugin : state.loadedAsiPlugins)
    {
        if (plugin.version.empty())
            names.push_back(plugin.name);
        else
            names.push_back(std::format("{} ({})", plugin.name, plugin.version));
    }
    return names;
}

// Modular Quirks / ASI tab
static bool QuirksTabVisible(State& state) { return !state.detectedQuirks.empty() || !ListedAsiPlugins(state).empty(); }
static const char* QuirksTabLabel(State& state)
{
    const bool quirks = !state.detectedQuirks.empty();
    const bool asi = !ListedAsiPlugins(state).empty();
    if (quirks && asi)
        return LOC(UI_Quirks_and_ASI);
    return asi ? LOC(UI_ASI) : LOC(UI_Quirks);
}

void MenuCommon::RenderQuirksSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;

    const auto asiPlugins = ListedAsiPlugins(state);

    // QUIRKS -----------------------------
    if (ScopedCard::Shows("active_quirks") && state.detectedQuirks.size() > 0)
    {
        ScopedCard card { LOC(Active_Quirks) };

        for (const auto& quirk : state.detectedQuirks)
        {
            ImGui::TextWrapped("%s", quirk.c_str());
        }
    }

    // ASI PLUGINS -----------------------------
    if (ScopedCard::Shows("loaded_asi_plugins") && asiPlugins.size() > 0)
    {
        ScopedCard card { LOC(UI_Loaded_ASI_Plugins) };
        for (const auto& name : asiPlugins)
        {
            ImGui::TextWrapped("%s", name.c_str());
        }
    }
}

void MenuCommon::RenderAdvancedSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;

    const bool upscalerActive = currentFeature != nullptr && !currentFeature->IsFrozen();
    const bool nonDlssActive =
        upscalerActive && currentBackend != Upscaler::DLSS && currentBackend != Upscaler::DLSS_on12;

    // SPOOFING/HOOKING -----------------------------
    if (ScopedCard::Shows("spoofing") && nonDlssActive)
    {
        ScopedCard card { "Spoofing" };

        if (ImGui::BeginTable("hooking", 2, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();
            ConfigCheckbox(LOC(UI_DXGI_Spoofing), config->DxgiSpoofing,
                           "Enable Nvidia GPU spoofing for DXGI adapter\n"
                           "Detailed modifications available in the INI",
                           true);

            ImGui::TableNextColumn();
            ConfigCheckbox(LOC(UI_Ntdll_Hooks), config->UseNtdllHooks,
                           "Only hook ntdll.dll methods\n"
                           "Disable for switching back to kernel hooks",
                           true);

            if (state.api == Vulkan)
            {
                ImGui::TableNextColumn();
                ConfigCheckbox(LOC(UI_VLK_Spoofing), config->VulkanSpoofing,
                               "Enable Nvidia GPU spoofing for Vulkan\n"
                               "Detailed modifications available in the INI",
                               true);

                ImGui::TableNextColumn();
                ConfigCheckbox(LOC(UI_VLK_Extension_Spoofing), config->VulkanExtensionSpoofing,
                               "Enable Nvidia Extension spoofing for Vulkan\n"
                               "Detailed modifications available in the INI",
                               true);
            }

            ImGui::TableNextColumn();
            ConfigCheckbox(LOC(UI_Disable_Overlays), config->DisableOverlays,
                           "Disable supported overlays (autoenabled with OptiFG)\n"
                           "Including Steam Input (controller issues)",
                           true);

            ImGui::EndTable();
        }
    }

    // PLUGINS/MISC -----------------------------
    if (ScopedCard::Shows("plugins"))
    {
        ScopedCard card { "Startup" };

        if (ImGui::BeginTable("plugins", 2, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();
            ConfigCheckbox(LOC(UI_Load_ASI_plugins), config->LoadAsiPlugins,
                           "Let OptiScaler load *.asi files from plugins folder", true);

            ImGui::TableNextColumn();
            ConfigCheckbox(LOC(UI_Disable_Splash_message), config->DisableSplash,
                           "Disables Startup Splash message (bottom left corner)", true);

            ImGui::TableNextColumn();
            ConfigCheckbox(LOC(UI_Check_for_Update), config->CheckForUpdate,
                           "Enable checking Github for latest version\n"
                           "Only works on stable/final releases",
                           true);

            ImGui::EndTable();
        }
    }

    // SHADERS, LIMITS AND DRS -----------------------------
    if (ScopedCard::Shows("shaders_limits"))
    {
        ScopedCard card { "Shaders" };

        if (ImGui::BeginTable("limits", 2, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();
            if (ConfigCheckbox(LOC(CHK_Use_Precompiled_Shaders_110), config->UsePrecompiledShaders))
            {
                state.newBackend = SameBackend(currentBackend);
                MARK_ALL_BACKENDS_CHANGED();
            }

            // DRS
            ImGui::TableNextColumn();
            ConfigCheckbox(LOC(UI_DRS_Override_Minimum), config->DrsMinOverrideEnabled,
                           "Dynamic Resolution Scaling\nFix for games ignoring official DRS limits");

            ImGui::TableNextColumn();
            if (upscalerActive)
                ConfigCheckbox(LOC(CHK_Enable_Extended_Limits_109), config->ExtendedLimits,
                               LOC(Extended_sliders_limit));

            ImGui::TableNextColumn();
            ConfigCheckbox(LOC(UI_DRS_Override_Maximum), config->DrsMaxOverrideEnabled,
                           "Dynamic Resolution Scaling\nFix for games ignoring official DRS limits");

            ImGui::EndTable();
        }
    }

    if (!nonDlssActive)
        return;

    // BARRIERS -----------------------------
    if (ScopedCard::Shows("barriers"))
    {
        ScopedCard card { LOC(Resource_Barriers) };

        if (ImGui::BeginTable("barriers", 2, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();
            AddResourceBarrier(LOC(UI_Debug_Color), &config->ColorResourceBarrier);

            ImGui::TableNextColumn();
            AddResourceBarrier(LOC(UI_Debug_Depth), &config->DepthResourceBarrier);

            ImGui::TableNextColumn();
            AddResourceBarrier(LOC(Motion), &config->MVResourceBarrier);

            ImGui::TableNextColumn();
            AddResourceBarrier(LOC(UI_Debug_Exposure), &config->ExposureResourceBarrier);

            ImGui::TableNextColumn();
            AddResourceBarrier(LOC(UI_Debug_Mask), &config->MaskResourceBarrier);

            ImGui::TableNextColumn();
            AddResourceBarrier(LOC(UI_Debug_Output), &config->OutputResourceBarrier);

            ImGui::EndTable();
        }
    }

    // HOTFIXES -----------------------------
    if (ScopedCard::Shows("root_signatures") && state.api == DX12)
    {
        ScopedCard card { LOC(Root_Signatures) };

        if (ImGui::BeginTable("rootSignatures", 2, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableNextColumn();
            ConfigCheckbox(LOC(CHK_Restore_Compute_Root_Signature_113), config->RestoreComputeSignature);

            ImGui::TableNextColumn();
            ConfigCheckbox(LOC(CHK_Restore_Graphic_Root_Signature_114), config->RestoreGraphicSignature);

            ImGui::EndTable();
        }
    }
}

void MenuCommon::RenderLoggingSettings(RenderMenuContext& ctx)
{
    auto config = ctx.config;

    // LOGGING -----------------------------
    SectionTitle(LOC(Logging));

    if (config->LogToConsole.value_or_default() || config->LogToFile.value_or_default() ||
        config->LogToNGX.value_or_default())
        spdlog::default_logger()->set_level((spdlog::level::level_enum) config->LogLevel.value_or_default());
    else
        spdlog::default_logger()->set_level(spdlog::level::off);

    if (bool toFile = config->LogToFile.value_or_default(); ImGui::Checkbox(LOC(CHK_To_File_115), &toFile))
    {
        config->LogToFile = toFile;
        PrepareLogger();
    }

    ImGui::SameLine(0.0f, 6.0f);
    if (bool toConsole = config->LogToConsole.value_or_default(); ImGui::Checkbox(LOC(CHK_To_Console_116), &toConsole))
    {
        config->LogToConsole = toConsole;
        PrepareLogger();
    }

    ImGui::SameLine(0.0f, 6.0f);
    if (auto SingleFile = config->LogSingleFile.value_or_default(); ImGui::Checkbox(LOC(UI_Single_File), &SingleFile))
    {
        config->LogSingleFile = SingleFile;
        PrepareLogger();
    }

    const char* logLevels[] = { LOC(UI_LogLevel_Trace), LOC(UI_LogLevel_Debug), LOC(UI_LogLevel_Information), LOC(UI_LogLevel_Warning), LOC(UI_LogLevel_Error) };
    const char* selectedLevel = logLevels[std::clamp(config->LogLevel.value_or_default(), 0, 4)];

    if (ImGui::BeginCombo(LOC(LBL_Log_Level), selectedLevel))
    {
        for (int n = 0; n < 5; n++)
        {
            if (ImGui::Selectable(logLevels[n], (config->LogLevel.value_or_default() == n)))
            {
                config->LogLevel = n;
                spdlog::default_logger()->set_level((spdlog::level::level_enum) config->LogLevel.value_or_default());
            }
        }

        ImGui::EndCombo();
    }
}

void MenuCommon::RenderThemeSettings(RenderMenuContext& ctx)
{
    auto config = ctx.config;

    // THEME -----------------------------
    ImGui::Spacing();
    if (auto ch = ScopedCollapsingHeader(LOC(Menu_Theme_and)); ch.IsHeaderOpen())
    {
        ScopedIndent indent {};
        ImGui::Spacing();

        bool lightTheme = config->LightTheme.value_or_default();

        static constexpr std::pair<const char*, ImVec4> presets[] = {
            { "Blue", { 0.00f, 0.40f, 0.77f, 1.0f } },   { "Teal", { 0.00f, 1.00f, 0.91f, 1.0f } },
            { "Gray", { 0.54f, 0.54f, 0.54f, 1.0f } },   { "Yellow", { 1.00f, 0.89f, 0.00f, 1.0f } },
            { "Green", { 0.25f, 1.00f, 0.00f, 1.0f } },  { "Red", { 1.00f, 0.00f, 0.00f, 1.0f } },
            { "Orange", { 1.00f, 0.52f, 0.00f, 1.0f } }, { "Purple", { 0.576f, 0.00f, 1.00f, 1.0f } },
        };

        // Row of clickable swatches, same presets for the accent and the background
        auto presetSwatches = [&](const char* id, auto& r, auto& g, auto& b)
        {
            const float size = ImGui::GetFrameHeight();

            ImGui::PushID(id);
            for (const auto& [name, color] : presets)
            {
                if (name != presets[0].first)
                    ImGui::SameLine(0.0f, 4.0f);

                if (ImGui::ColorButton(name, toneMapColor(color), ImGuiColorEditFlags_NoTooltip, ImVec2(size, size)))
                {
                    r = color.x;
                    g = color.y;
                    b = color.z;
                    ApplyThemeStyle();
                }

                ShowTooltip(name);
            }
            ImGui::PopID();
        };

        if (ImGui::Checkbox(LOC(CHK_Light_Theme_117), &lightTheme))
        {
            config->LightTheme = lightTheme;
            ApplyThemeStyle();
        }

        SectionTitle(LOC(SEP_Accent_Colour_182));

        presetSwatches("accent", config->MenuAccentColorR, config->MenuAccentColorG, config->MenuAccentColorB);

        float accentColor[3] = { config->MenuAccentColorR.value_or_default(),
                                 config->MenuAccentColorG.value_or_default(),
                                 config->MenuAccentColorB.value_or_default() };

        if (ImGui::ColorEdit3(LOC(UI_Custom_Accent_Color), accentColor))
        {
            config->MenuAccentColorR = accentColor[0];
            config->MenuAccentColorG = accentColor[1];
            config->MenuAccentColorB = accentColor[2];
            ApplyThemeStyle();
        }

        ImGui::Spacing();

        if (ImGui::Button(LOC(BTN_Reset_Accent_Color_152)))
        {
            config->MenuAccentColorR.reset();
            config->MenuAccentColorG.reset();
            config->MenuAccentColorB.reset();
            ApplyThemeStyle();
        }

        ImGui::Spacing();

        SectionTitle(LOC(SEP_Background_Colour_183));

        presetSwatches("bg", config->MenuBGColorR, config->MenuBGColorG, config->MenuBGColorB);

        float bgColor[3] = { config->MenuBGColorR.value_or_default(), config->MenuBGColorG.value_or_default(),
                             config->MenuBGColorB.value_or_default() };

        if (ImGui::ColorEdit3(LOC(UI_Custom_BG_Colour), bgColor))
        {
            config->MenuBGColorR = bgColor[0];
            config->MenuBGColorG = bgColor[1];
            config->MenuBGColorB = bgColor[2];
            ApplyThemeStyle();
        }

        ImGui::Spacing();

        auto alpha = config->MenuBGColorA.value_or_default();
        if (ImGui::SliderFloat(LOC(UI_Background_Alpha), &alpha, 0.0f, 1.0f))
        {
            config->MenuBGColorA = alpha;
            ApplyThemeStyle();
        }

        auto rounding = config->MenuRounding.value_or_default();
        if (ImGui::SliderFloat(LOC(UI_Corner_Rounding), &rounding, 0.0f, 16.0f, rounding < 0.5f ? "Square" : LOC(UI_S_0f_px)))
        {
            config->MenuRounding = rounding;
            lastMenuScale = 0.0f; // Rescales the style with this rounding
        }

        if (_bgBlurTexture != ImTextureID_Invalid)
        {
            ConfigCheckbox(LOC(UI_Background_Blur), config->MenuBlur,
                           "Blur the game behind the menu\nOnly visible when Background Alpha is below 1");

            ImGui::BeginDisabled(!config->MenuBlur.value_or_default());

            auto blurStrength = config->MenuBlurStrength.value_or_default();
            if (ImGui::SliderFloat(LOC(UI_Blur_Strength), &blurStrength, 0.0f, 4.0f, "%.2f"))
                config->MenuBlurStrength = blurStrength;

            ImGui::EndDisabled();
        }

        ImGui::Spacing();

        if (ImGui::Button(LOC(BTN_Reset_Bg_Colour_161)))
        {
            config->MenuBGColorR.reset();
            config->MenuBGColorG.reset();
            config->MenuBGColorB.reset();
            config->MenuBGColorA.reset();
            ApplyThemeStyle();
        }

        ImGui::Spacing();
    }
}

void MenuCommon::RenderFpsOverlaySettings(RenderMenuContext& ctx)
{
    auto config = ctx.config;

    // FPS OVERLAY -----------------------------
    SectionTitle(LOC(FPS_Overlay));
    {
        ScopedID sectionId { LOC(FPS_Overlay) };

        bool fpsEnabled = config->ShowFps.value_or_default();
        if (ImGui::Checkbox(LOC(CHK_Fps_Overlay_Enabled_118), &fpsEnabled))
            config->ShowFps = fpsEnabled;

        ImGui::SameLine(0.0f, 6.0f);

        bool fpsHorizontal = config->FpsOverlayHorizontal.value_or_default();
        if (ImGui::Checkbox(LOC(Horizontal), &fpsHorizontal))
            config->FpsOverlayHorizontal = fpsHorizontal;

        const char* fpsPosition[] = { LOC(UI_Top_Left),   LOC(UI_Top_Right),     LOC(UI_Bottom_Left), LOC(UI_Bottom_Right),
                                      "Top Center", "Bottom Center", "Custom" };
        const char* selectedPosition = fpsPosition[config->FpsOverlayPosition.value_or_default()];

        if (ImGui::BeginCombo(LOC(LBL_Overlay_Position), selectedPosition))
        {
            for (int n = 0; n < std::size(fpsPosition); n++)
            {
                if (ImGui::Selectable(fpsPosition[n], (config->FpsOverlayPosition.value_or_default() == n)))
                    config->FpsOverlayPosition = (FpsOverlayPos) n;
            }

            ImGui::EndCombo();
        }

        if (config->FpsOverlayPosition.value_or_default() == FpsOverlayPos_Custom)
        {
            float posX = config->FpsOverlayPosX.value_or_default();
            if (ImGui::SliderFloat(LOC(UI_Horizontal_Position), &posX, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp))
                config->FpsOverlayPosX = posX;

            float posY = config->FpsOverlayPosY.value_or_default();
            if (ImGui::SliderFloat(LOC(UI_Vertical_Position), &posY, 0.0f, 1.0f, "%.3f", ImGuiSliderFlags_AlwaysClamp))
                config->FpsOverlayPosY = posY;
        }

        const char* fpsType[] = {
            LOC(UI_Just_FPS), LOC(UI_Simple), LOC(UI_Detailed), LOC(UI_Detailed_Graph), LOC(UI_Full), LOC(UI_Full_Graph), "Low latency timings",
            "Custom"
        };
        static_assert(std::size(fpsType) == FpsOverlay_COUNT);
        const auto currentType = config->FpsOverlayType.value_or_default();

        if (ImGui::BeginCombo(LOC(LBL_Overlay_Type), fpsType[currentType]))
        {
            for (int n = 0; n < std::size(fpsType); n++)
            {
                if (ImGui::Selectable(fpsType[n], currentType == n))
                {
                    // Custom starts from the preset's rows the first time
                    if (n == FpsOverlay_Custom && currentType != FpsOverlay_Custom &&
                        !config->FpsOverlayRows.has_value())
                    {
                        SetOverlayRows(OverlayRows(currentType));
                        config->FpsOverlayFpsRowType = OverlayFpsRow(currentType);
                    }

                    config->FpsOverlayType = (FpsOverlay) n;
                }
            }

            ImGui::EndCombo();
        }
        ShowTooltip("Presets of the rows, Custom shows the ones chosen in Rows.\n"
                    "The Overlay Cycle key switches between them.");

        // 0 is the menu's scale, then 0.5 to 3.0 in 0.1 steps
        int currentIndex = config->FpsScale.has_value()
                               ? std::clamp((int) std::lround(config->FpsScale.value() * 10.0f) - 4, 1, 26)
                               : 0;
        auto scaleText = currentIndex == 0 ? StrFmt(LOC(UI_Same_as_menu_1f), ctx.menuResScale)
                                           : StrFmt("%.1f", 0.4f + currentIndex / 10.0f);

        if (ImGui::SliderInt(LOC(UI_Scale), &currentIndex, 0, 26, scaleText.c_str(), ImGuiSliderFlags_ClampOnInput))
        {
            if (currentIndex == 0)
                config->FpsScale.reset();
            else
                config->FpsScale = 0.4f + currentIndex / 10.0f;
        }
        ShowTooltip(LOC(UI_Size_of_the_FPS_overlay_independent_from_the_menu_scale_unle));

        auto optionCheckbox = [&](const char* label, CustomOptional<bool>& setting, const char* tip)
        {
            bool value = setting.value_or_default();
            if (ImGui::Checkbox(label, &value))
                setting = value;

            ShowTooltip(tip);
        };

        auto statusText = [](const std::string& text)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
            ImGui::TextWrapped("%s", text.c_str());
            ImGui::PopStyleColor();
        };

        // Which rows are shown and their order, from top to bottom (left to right when horizontal)
        if (ImGui::TreeNode("Rows"))
        {
            const bool custom = currentType == FpsOverlay_Custom;
            auto rows = OverlayRows(currentType);
            bool rowsChanged = false;

            if (!custom)
            {
                statusText(StrFmt(LOC(UI_The_rows_of_s_set_Overlay_Type_to_Custom_to_change_them), fpsType[currentType]));
            }

            ImGui::BeginDisabled(!custom);

            if (ImGui::BeginTable("overlayRows", 2, ImGuiTableFlags_SizingFixedFit))
            {
                ImGui::TableSetupColumn("Order");
                ImGui::TableSetupColumn("Row", ImGuiTableColumnFlags_WidthStretch);

                // Drawn as they were at the frame's start, a click changes one row
                const auto shownRows = rows;

                for (size_t i = 0; i < shownRows.size(); i++)
                {
                    const int row = shownRows[i].row;
                    ScopedID rowId { EnumToCode((OverlayRow) row).c_str() };

                    ImGui::TableNextRow();
                    ImGui::TableNextColumn();

                    ImGui::BeginDisabled(i == 0);
                    if (ImGui::ArrowButton("##Up", ImGuiDir_Up))
                    {
                        std::swap(rows[i], rows[i - 1]);
                        rowsChanged = true;
                    }
                    ImGui::EndDisabled();

                    ImGui::SameLine(0.0f, 2.0f);

                    ImGui::BeginDisabled(i + 1 == shownRows.size());
                    if (ImGui::ArrowButton(LOC(UI_Down), ImGuiDir_Down))
                    {
                        std::swap(rows[i], rows[i + 1]);
                        rowsChanged = true;
                    }
                    ImGui::EndDisabled();

                    ImGui::TableNextColumn();

                    // A click here means no row moved this frame
                    bool rowOn = shownRows[i].shown;
                    if (ImGui::Checkbox(overlayRowNames[row], &rowOn))
                    {
                        rows[i].shown = rowOn;
                        rowsChanged = true;
                    }

                    ShowTooltip(overlayRowTips[row]);
                }

                ImGui::EndTable();
            }

            if (rowsChanged)
                SetOverlayRows(rows);

            const char* fpsRowTypes[] = { LOC(UI_Just_FPS), LOC(UI_With_Frame_Time), LOC(UI_With_Average_FPS) };
            static_assert(std::size(fpsRowTypes) == FpsOverlayFpsRow_COUNT);

            int fpsRowType = OverlayFpsRow(currentType);
            if (ImGui::Combo(LOC(UI_FPS_Row), &fpsRowType, fpsRowTypes, IM_ARRAYSIZE(fpsRowTypes)))
                config->FpsOverlayFpsRowType = (FpsOverlayFpsRow) fpsRowType;

            ShowTooltip(LOC(UI_Just_the_number_or_with_the_API_frame_time_or_average_FPS_fr));

            ImGui::EndDisabled();

            ImGui::BeginDisabled(custom);
            optionCheckbox(LOC(UI_Upscale_Ratio_in_Presets), config->FpsOverlayShowRatio,
                           "The Upscale Ratio row in the presets from Simple on, Custom has it in its rows");
            ImGui::EndDisabled();

            ImGui::SameLine(0.0f, 6.0f);

            optionCheckbox(LOC(UI_Hide_Averages), config->FpsOverlayHideAverages,
                           "Only the current values, without the averages of FPS, frame time and upscaler time");

            ImGui::Spacing();

            // -1 is the default spacing
            float rowSpacing = config->FpsOverlayRowSpacing.value_or(-1.0f);
            if (ImGui::SliderFloat(LOC(UI_Row_Spacing), &rowSpacing, -1.0f, 32.0f,
                                   config->FpsOverlayRowSpacing.has_value() ? LOC(UI_S_0f_px) : LOC(Default),
                                   ImGuiSliderFlags_AlwaysClamp))
            {
                if (rowSpacing < -0.5f)
                    config->FpsOverlayRowSpacing.reset();
                else
                    config->FpsOverlayRowSpacing = std::round(std::max(rowSpacing, 0.0f));
            }

            if (ImGui::Button(custom ? LOC(UI_Reset_Rows_Spacing) : LOC(UI_Reset_Spacing)))
            {
                if (custom)
                {
                    config->FpsOverlayRows.reset();
                    config->FpsOverlayFpsRowType.reset();
                }

                config->FpsOverlayRowSpacing.reset();
            }

            if (custom)
                ShowTooltip(LOC(UI_Custom_s_rows_back_to_the_ones_of_Full_Graph));

            ImGui::TreePop();
        }

        if (ImGui::TreeNode("Appearance"))
        {
            auto scaleSlider = [&](const char* label, CustomOptional<float>& setting)
            {
                float value = setting.value_or_default();
                if (ImGui::SliderFloat(label, &value, 0.5f, 2.0f, LOC(UI_S_2fx), ImGuiSliderFlags_AlwaysClamp))
                    setting = value;
            };

            // Ready made looks, as the text of a settings code: what a preset doesn't list goes back to the default
            SectionTitle("Presets");

            struct OverlayPreset
            {
                const char* name;
                const char* tip;
                const char* settings;
            };

            static const OverlayPreset presets[] = {
                { LOC(Default), LOC(UI_OptiScaler_s_overlay_every_row_with_the_graphs_grey_labels_w), "" },
                { "Classic", LOC(UI_OptiScaler_s_earlier_overlay_just_the_FPS_in_white_no_shadow),
                  "LabelColor=0xFFFFFF;Shadow=0;Rounding=2" },
                { "Afterburner", LOC(UI_As_MSI_Afterburner_Unispace_Bold_orange_labels_outlined_no_b),
                  "OverlaysUseTheme=false;Type=3;CompactLabels=true;Font=unispace bd.ttf;Alpha=0;Rounding=0;"
                  "LabelColor=0xFF9C00;ValueColor=0xFFFFFF;GraphColor=0xFF9C00;Shadow=2;ShadowStrength=0.5;"
                  "ShadowColor=0x000000;UpdateInterval=500" },
                { "Strip", LOC(UI_A_strip_at_the_top_center_Bahnschrift_FPS_and_upscaler_time_),
                  "Pos=4;Type=7;Horizontal=true;Rows=fps,-upscale,-frametime,-frametimegraph,upscaler,"
                  "-upscalergraph,-lowlatency;FpsRow=2;UpdateInterval=250;Alpha=0.35;Font=bahnschrift.ttf;"
                  "LetterSpacing=0.5;LabelScale=0.85;ValueScale=1.1;LabelColor=0xFFFFFF;ValueColor=0xFF9A00;"
                  "GraphColor=0x00E5FF;BGColor=0x0A0F14;Rounding=10;HideAverages=true" },
                { LOC(UI_Lemon_zest),
                  "A winning submission from user Voodoo: a top center strip in Unispace Bold, lime values",
                  "Pos=4;Type=7;Horizontal=true;Rows=upscale,-frametime,-frametimegraph,fps,upscaler,"
                  "-upscalergraph,-lowlatency;FpsRow=1;Alpha=0.12;Font=unispace bd.ttf;ValueColor=0xC9DF21;"
                  "ShadowStrength=0.59;ShadowColor=0x866767;Rounding=7;HideAverages=true" },
            };

            const float presetCount = static_cast<float>(std::size(presets));
            const float buttonWidth =
                (ImGui::GetContentRegionAvail().x - ImGui::GetStyle().ItemSpacing.x * (presetCount - 1.0f)) /
                presetCount;

            for (size_t i = 0; i < std::size(presets); i++)
            {
                if (i > 0)
                    ImGui::SameLine();

                if (ImGui::Button(presets[i].name, ImVec2(buttonWidth, 0.0f)))
                    config->ApplyOverlaySettings(presets[i].settings);

                ShowTooltip(presets[i].tip);
            }

            SectionTitle("Layout");

            optionCheckbox(LOC(UI_Compact_Labels), config->FpsOverlayCompactLabels,
                           "Shorter labels for a narrower overlay: UPS, FT, UT");

            float fpsAlpha = config->FpsOverlayAlpha.value_or_default();
            if (ImGui::SliderFloat(LOC(UI_Background_Alpha), &fpsAlpha, 0.0f, 1.0f, "%.2f"))
                config->FpsOverlayAlpha = fpsAlpha;

            float graphHeight = config->FpsOverlayGraphHeight.value_or_default();
            if (ImGui::SliderFloat(LOC(UI_Graph_Height), &graphHeight, 0.5f, 3.0f, LOC(UI_S_1fx), ImGuiSliderFlags_AlwaysClamp))
                config->FpsOverlayGraphHeight = graphHeight;

            float rounding = config->FpsOverlayRounding.value_or_default();
            if (ImGui::SliderFloat(LOC(UI_Corner_Rounding), &rounding, 0.0f, 16.0f, rounding < 0.5f ? "Square" : LOC(UI_S_0f_px),
                                   ImGuiSliderFlags_AlwaysClamp))
            {
                config->FpsOverlayRounding = std::round(rounding);
            }

            SectionTitle("Text");

            // Checked once, not every frame: fonts this Windows doesn't have aren't offered
            static const std::vector<bool> overlayFontExists = []()
            {
                std::vector<bool> exists;
                exists.reserve(IM_ARRAYSIZE(overlayFonts));
                for (const auto& [name, file] : overlayFonts)
                    exists.push_back(std::filesystem::exists(OverlayFontPath(file)));
                return exists;
            }();

            auto fontName = config->FpsOverlayFont.value_or("");
            std::string selectedFont = fontName.empty() ? LOC(UI_Menu_font) : fontName;

            for (const auto& [name, file] : overlayFonts)
            {
                if (_stricmp(file, fontName.c_str()) == 0)
                    selectedFont = name;
            }

            if (ImGui::BeginCombo("Font", selectedFont.c_str()))
            {
                if (ImGui::Selectable(LOC(UI_Menu_font), fontName.empty()))
                    config->FpsOverlayFont.reset();

                for (size_t i = 0; i < IM_ARRAYSIZE(overlayFonts); ++i)
                {
                    if (!overlayFontExists[i])
                        continue;

                    const auto& [name, file] = overlayFonts[i];
                    if (ImGui::Selectable(name, _stricmp(file, fontName.c_str()) == 0))
                        config->FpsOverlayFont = std::string(file);
                }

                ImGui::EndCombo();
            }
            ShowTooltip(LOC(UI_Choose_a_Windows_system_font_or_enter_any_custom_ttf_otf_fil));

            char customFontBuf[260] {};
            std::snprintf(customFontBuf, sizeof(customFontBuf), "%s", fontName.c_str());
            if (ImGui::InputText(LOC(UI_Custom_Font_File), customFontBuf, sizeof(customFontBuf),
                                 ImGuiInputTextFlags_EnterReturnsTrue))
            {
                std::string entered(customFontBuf);
                if (entered.empty())
                    config->FpsOverlayFont.reset();
                else
                    config->FpsOverlayFont = entered;
            }
            ShowTooltip(LOC(UI_A_file_in_C_Windows_Fonts_or_a_full_path_Enter_applies_it));

            // 7, below the range, is the menu's font size
            float fontSizeValue = config->FpsOverlayFontSize.value_or(7.0f);
            if (ImGui::SliderFloat(LOC(UI_Font_Size), &fontSizeValue, 7.0f, 48.0f,
                                   config->FpsOverlayFontSize.has_value() ? LOC(UI_S_0f_px) : LOC(UI_Menu_font_size),
                                   ImGuiSliderFlags_AlwaysClamp))
            {
                if (fontSizeValue < 8.0f)
                    config->FpsOverlayFontSize.reset();
                else
                    config->FpsOverlayFontSize = std::round(fontSizeValue);
            }
            ShowTooltip(LOC(UI_Before_the_overlay_s_scale_the_far_left_is_the_menu_s_font_s));

            float letterSpacing = config->FpsOverlayLetterSpacing.value_or_default();
            if (ImGui::SliderFloat(LOC(UI_Letter_Spacing), &letterSpacing, -2.0f, 8.0f, LOC(UI_S_1f_px),
                                   ImGuiSliderFlags_AlwaysClamp))
            {
                config->FpsOverlayLetterSpacing = letterSpacing;
            }
            ShowTooltip(LOC(UI_Extra_horizontal_tracking_in_pixels_between_characters));

            scaleSlider(LOC(UI_Label_Size), config->FpsOverlayLabelScale);
            ShowTooltip(LOC(UI_Names_like_FPS_and_Avg));

            scaleSlider(LOC(UI_Value_Size), config->FpsOverlayValueScale);
            ShowTooltip(LOC(UI_The_numbers));

            float updateInterval = config->FpsOverlayUpdateInterval.value_or_default();
            if (ImGui::SliderFloat(LOC(UI_Update_Interval), &updateInterval, 0.0f, 1000.0f,
                                   updateInterval < 1.0f ? LOC(UI_Every_frame) : LOC(UI_S_0f_ms), ImGuiSliderFlags_AlwaysClamp))
            {
                config->FpsOverlayUpdateInterval = std::round(updateInterval / 10.0f) * 10.0f;
            }
            ShowTooltip(LOC(UI_How_often_FPS_and_times_change_averaged_in_between_Higher_is));

            const char* shadowTypes[] = { LOC(UI_Off), "Shadow", "Outline" };
            int shadowType = config->FpsOverlayShadowType.value_or_default();
            if (ImGui::Combo(LOC(UI_Text_Shadow), &shadowType, shadowTypes, IM_ARRAYSIZE(shadowTypes)))
                config->FpsOverlayShadowType = (FpsOverlayShadow) shadowType;

            ImGui::BeginDisabled(shadowType == FpsOverlayShadow_Off);
            float shadowStrength = config->FpsOverlayShadowStrength.value_or_default();
            if (ImGui::SliderFloat(LOC(UI_Shadow_Strength), &shadowStrength, 0.0f, 1.0f, "%.2f",
                                   ImGuiSliderFlags_AlwaysClamp))
            {
                config->FpsOverlayShadowStrength = shadowStrength;
            }
            ShowTooltip(LOC(UI_Thickness_and_darkness_of_the_shadow_thickness_of_the_outlin));
            ImGui::EndDisabled();

            SectionTitle("Colors");

            // Side by side, an item that doesn't fit goes to the next line
            const float rowRight = ImGui::GetCursorScreenPos().x + ImGui::GetContentRegionAvail().x;
            bool firstColorItem = true;

            auto colorRowItem = [&](const char* label)
            {
                const auto& style = ImGui::GetStyle();
                const float gap = style.ItemSpacing.x * 2.0f;
                const float width = ImGui::GetFrameHeight() + style.ItemInnerSpacing.x + ImGui::CalcTextSize(label).x;

                if (!firstColorItem && ImGui::GetItemRectMax().x + gap + width <= rowRight)
                    ImGui::SameLine(0.0f, gap);

                firstColorItem = false;
            };

            auto colorEdit = [&](const char* label, CustomOptional<uint32_t>& setting)
            {
                colorRowItem(label);

                auto color = RgbToColor(setting.value_or_default());
                float rgb[3] = { color.x, color.y, color.z };

                if (ImGui::ColorEdit3(label, rgb, ImGuiColorEditFlags_NoInputs))
                    setting = ColorToRgb(rgb);
            };

            bool useTheme = config->OverlaysUseTheme.value_or_default();
            if (ImGui::Checkbox(LOC(UI_Use_Theme), &useTheme))
                config->OverlaysUseTheme = useTheme;

            ShowTooltip(LOC(UI_The_menu_theme_s_colors_for_the_labels_values_graph_and_back));

            ImGui::BeginDisabled(useTheme);
            colorEdit("Labels", config->FpsOverlayLabelColor);
            colorEdit("Values", config->FpsOverlayValueColor);
            colorEdit("Graph", config->FpsOverlayGraphColor);
            colorEdit("Background", config->FpsOverlayBGColor);
            ImGui::EndDisabled();

            ImGui::BeginDisabled(shadowType == FpsOverlayShadow_Off);
            colorEdit("Shadow", config->FpsOverlayShadowColor);
            ImGui::EndDisabled();

            ImGui::Spacing();

            if (ImGui::Button(LOC(UI_Reset_Appearance)))
            {
                config->FpsOverlayCompactLabels.reset();
                config->FpsOverlayFont.reset();
                config->FpsOverlayFontSize.reset();
                config->FpsOverlayLetterSpacing.reset();
                config->FpsOverlayUpdateInterval.reset();
                config->FpsOverlayLabelColor.reset();
                config->FpsOverlayValueColor.reset();
                config->FpsOverlayLabelScale.reset();
                config->FpsOverlayValueScale.reset();
                config->FpsOverlayGraphColor.reset();
                config->FpsOverlayBGColor.reset();
                config->FpsOverlayShadowType.reset();
                config->FpsOverlayShadowStrength.reset();
                config->FpsOverlayShadowColor.reset();
                config->FpsOverlayGraphHeight.reset();
                config->FpsOverlayRounding.reset();
                config->FpsOverlayAlpha.reset();
            }

            ImGui::TreePop();
        }

        // The overlay's settings as a code, kept outside the ini across updates and shared with others
        if (ImGui::TreeNode(LOC(UI_Share_Settings)))
        {
            static std::string settingsCodeStatus;

            if (ImGui::Button(LOC(UI_Copy_Code)))
            {
                ImGui::SetClipboardText(config->ExportOverlaySettings().c_str());
                settingsCodeStatus = LOC(UI_Copied_to_the_clipboard);
            }

            ShowTooltip(LOC(UI_Copies_your_overlay_settings_as_a_code_to_keep_or_share));

            ImGui::SameLine();

            if (ImGui::Button(LOC(UI_Paste_Code)))
            {
                const char* clipboard = ImGui::GetClipboardText();

                if (clipboard != nullptr && config->ImportOverlaySettings(clipboard))
                    settingsCodeStatus = LOC(UI_Applied_Save_Settings_to_keep_it);
                else
                    settingsCodeStatus = LOC(UI_No_FPS_overlay_code_in_the_clipboard);
            }

            ShowTooltip(LOC(UI_Applies_a_copied_code_Settings_it_doesn_t_list_go_back_to_th));

            if (!settingsCodeStatus.empty())
                statusText(settingsCodeStatus);

            ImGui::TreePop();
        }
    }
}

void MenuCommon::RenderUpscalerInputsSettings(RenderMenuContext& ctx)
{
    auto config = ctx.config;

    // UPSCALER INPUTS -----------------------------
    SectionTitle(LOC(Upscaler_Inputs));
    {
        ScopedID sectionId { LOC(Upscaler_Inputs) };

        // Values when the menu was first drawn, enabling and pattern matching apply on next launch
        static const bool startFsr2 = config->EnableFsr2Inputs.value_or_default();
        static const bool startFsr3 = config->EnableFsr3Inputs.value_or_default();
        static const bool startFfx = config->EnableFfxInputs.value_or_default();
        static const bool startFsr2Pattern = config->Fsr2Pattern.value_or_default();
        static const bool startFsr3Pattern = config->Fsr3Pattern.value_or_default();

        if (ImGui::BeginTable("upscalerInputs", 4, ImGuiTableFlags_SizingStretchSame))
        {
            ImGui::TableSetupColumn(LOC(UI_Input), ImGuiTableColumnFlags_WidthStretch, 0.7f);
            ImGui::TableSetupColumn(LOC(Enable), ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Use", ImGuiTableColumnFlags_WidthStretch, 1.0f);
            ImGui::TableSetupColumn("Pattern", ImGuiTableColumnFlags_WidthStretch, 1.0f);

            ImGui::TableNextRow();
            for (int i = 0; i < 4; i++)
            {
                ImGui::TableSetColumnIndex(i);
                ImGui::TextDisabled("%s", ImGui::TableGetColumnName(i));

                if (i == 1)
                    ShowTooltip(LOC(UI_Hook_the_inputs_of_this_API_needed_before_they_can_be_used));
                else if (i == 2)
                    ShowTooltip(LOC(UI_Take_the_upscaler_inputs_from_this_API));
                else if (i == 3)
                    ShowTooltip(LOC(UI_Find_the_inputs_by_pattern_matching));
            }

            auto row = [&](const char* name, auto& enable, auto& use, auto pattern)
            {
                ScopedID rowId { name };

                ImGui::TableNextRow();
                ImGui::TableNextColumn();
                ImGui::AlignTextToFramePadding();
                ImGui::TextUnformatted(name);

                ImGui::TableNextColumn();
                ConfigCheckbox(LOC(UI_enable), enable, LOC(UI_Hook_the_inputs_of_this_API), true);

                ImGui::BeginDisabled(!enable.value_or_default());

                ImGui::TableNextColumn();
                ConfigCheckbox(LOC(UI_use), use, LOC(UI_Take_the_upscaler_inputs_from_this_API));

                ImGui::TableNextColumn();
                if constexpr (!std::is_null_pointer_v<decltype(pattern)>)
                    ConfigCheckbox(LOC(UI_pattern), *pattern, LOC(UI_Find_the_inputs_by_pattern_matching), true);

                ImGui::EndDisabled();
            };

            row("FSR2", config->EnableFsr2Inputs, config->UseFsr2Inputs, &config->Fsr2Pattern);
            row("FSR3", config->EnableFsr3Inputs, config->UseFsr3Inputs, &config->Fsr3Pattern);
            row("FFX", config->EnableFfxInputs, config->UseFfxInputs, nullptr);

            ImGui::EndTable();
        }

        const bool restartNeeded = startFsr2 != config->EnableFsr2Inputs.value_or_default() ||
                                   startFsr3 != config->EnableFsr3Inputs.value_or_default() ||
                                   startFfx != config->EnableFfxInputs.value_or_default() ||
                                   startFsr2Pattern != config->Fsr2Pattern.value_or_default() ||
                                   startFsr3Pattern != config->Fsr3Pattern.value_or_default();

        if (restartNeeded)
        {
            ImGui::PushStyleColor(ImGuiCol_Text, StatusWarn());
            ImGui::TextWrapped(LOC(UI_Save_Settings_and_restart_the_game_for_Enable_and_Pattern_ch));
            ImGui::PopStyleColor();
        }
    }
}

void MenuCommon::RenderVsyncSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;
    auto& menuResScale = ctx.menuResScale;

    // DX11 & DX12 -----------------------------
    if (state.swapchainApi != Vulkan)
    {
        // V-SYNC -----------------------------
        SectionTitle(LOC(V_Sync_Settings));
        {
            ScopedID sectionId { LOC(V_Sync_Settings) };

            auto OverrideVsync = config->OverrideVsync.value_or_default();
            {
                if (ImGui::Checkbox(LOC(UI_Override_Vsync), &OverrideVsync))
                {
                    config->OverrideVsync = OverrideVsync;
                    LOG_DEBUG("Enabled OverrideVsync: {}", OverrideVsync);
                }
                ShowTooltip("Force override the game's V-Sync settings.\n\n"
                            "Useful for example when XeFG is locked to V-sync\n"
                            "frame cap despite toggling Vsync off (e.g. Nioh 3)\n\n"
                            "After changing, please Save Settings and restart.");
            }

            auto forceVsyncOn = config->ForceVsync.has_value() && config->ForceVsync.value();
            auto forceVsyncOff = config->ForceVsync.has_value() && !config->ForceVsync.value();
            bool vsyncChanged = false;

            if (ImGui::Checkbox(LOC(CHK_V_Sync_On_125), &forceVsyncOn))
            {
                if (forceVsyncOn)
                {
                    config->ForceVsync = true;
                    vsyncChanged = true;
                }
                else
                {
                    config->ForceVsync.reset();
                    vsyncChanged = true;
                }
            }
            ImGui::SameLine(0.0f, 16.0f);

            if (ImGui::Checkbox(LOC(CHK_V_Sync_Off_126), &forceVsyncOff))
            {
                if (forceVsyncOff)
                {
                    config->ForceVsync = false;
                    vsyncChanged = true;
                }
                else
                {
                    config->ForceVsync.reset();
                    vsyncChanged = true;
                }
            }
            ImGui::SameLine(0.0f, 16.0f);

            ImGui::BeginDisabled(!forceVsyncOn);

            ImGui::PushItemWidth(50.0f * menuResScale);

            auto vsyncBuf = StrFmt("%d", config->VsyncInterval.value_or_default());
            if (ImGui::BeginCombo(LOC(LBL_Sync_Int), vsyncBuf.c_str()))
            {
                if (ImGui::Selectable("0", config->VsyncInterval.value_or_default() == 0))
                {
                    config->VsyncInterval = 0;
                    vsyncChanged = true;
                }

                if (ImGui::Selectable("1", config->VsyncInterval.value_or_default() == 1))
                {
                    config->VsyncInterval = 1;
                    vsyncChanged = true;
                }

                if (ImGui::Selectable("2", config->VsyncInterval.value_or_default() == 2))
                {
                    config->VsyncInterval = 2;
                    vsyncChanged = true;
                }

                if (ImGui::Selectable("3", config->VsyncInterval.value_or_default() == 3))
                {
                    config->VsyncInterval = 3;
                    vsyncChanged = true;
                }

                ImGui::EndCombo();
            }
            ImGui::PopItemWidth();

            ShowTooltip(LOC(Controls_the_DXGI));

            ImGui::EndDisabled();
            ImGui::SameLine(0.0f, 16.0f);

            if (ImGui::Button(LOC(BTN_Reset_162)))
            {
                config->ForceVsync.reset();
                vsyncChanged = true;
            }

            ShowTooltip(LOC(TIP_Force_V_Sync_On_248));

            if (vsyncChanged && state.activeFgOutput == FGOutput::XeFG && state.currentFG != nullptr)
            {
                // To prevent XeLL issues
                LOG_DEBUG("V-Sync change detected, forcing XeFG reset");
                state.WAR_xefgRequestFGToggle = true;
            }
        }
    }
    else
    {
        SectionTitle(LOC(V_Sync_Settings));
        {
            ScopedID sectionId { LOC(V_Sync_Settings) };

            const auto& forced = config->VulkanPresentMode;
            const char* preview = forced.has_value() ? VulkanHooks::PresentModeName(forced.value()) : LOC(UI_Game_s);
            bool changed = false;

            if (ImGui::BeginCombo(LOC(UI_Present_Mode), preview))
            {
                if (ImGui::Selectable(LOC(UI_Game_s), !forced.has_value()))
                {
                    config->VulkanPresentMode.reset();
                    changed = true;
                }

                for (uint32_t i = 0; i < std::size(VulkanHooks::PresentModes); i++)
                {
                    const auto& entry = VulkanHooks::PresentModes[i];

                    if ((state.vkPresentModeMask & (1u << i)) == 0)
                        continue;

                    if (ImGui::Selectable(entry.name, forced.has_value() && forced.value() == entry.mode))
                    {
                        config->VulkanPresentMode = (int) entry.mode;
                        changed = true;
                    }
                }

                ImGui::EndCombo();
            }

            ShowTooltip("Overrides the game's Vulkan present mode, the game recreates its swapchain to apply it.\n\n"
                        "Immediate: no V-Sync, tears.\n"
                        "Mailbox: no tearing, newest frame replaces the queued one.\n"
                        "FIFO: V-Sync.\n"
                        "FIFO Relaxed: V-Sync, tears when below the refresh rate.\n"
                        "FIFO Latest Ready: V-Sync, shows the newest ready frame at each refresh.\n\n"
                        "Mailbox and FIFO Latest Ready can drop generated frames above the refresh rate.\n"
                        "Only modes the display supports are listed.");

            if (state.vkGamePresentMode >= 0)
                ImGui::Text(LOC(UI_Game_s_present_mode_s), VulkanHooks::PresentModeName(state.vkGamePresentMode));

            if (changed)
                state.vkPresentModeChanged = true;
        }
    }
}

void MenuCommon::RenderApiAndTextureSettings(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;
    auto& menuResScale = ctx.menuResScale;

    // DX11 & DX12 -----------------------------
    if (state.swapchainApi != Vulkan)
    {
        // MIPMAP BIAS & Anisotropy -----------------------------
        if (ScopedCard::Shows("mipmap"))
        {
            ScopedCard card { LOC(Mipmap_Bias) };
            ScopedID sectionId { LOC(Mipmap_Bias) };
            if (config->MipmapBiasOverride.has_value() && _mipBias == 0.0f)
                _mipBias = config->MipmapBiasOverride.value();

            ImGui::SliderFloat(LOC(UI_Mipmap_Bias_2), &_mipBias, -15.0f, 15.0f, "%.6f");
            ShowTooltip(LOC(Can_help_with));

            ImGui::BeginDisabled(!config->MipmapBiasOverride.has_value());
            {
                ImGui::BeginDisabled(config->MipmapBiasScaleOverride.has_value() &&
                                     config->MipmapBiasScaleOverride.value());
                {
                    bool mbFixed = config->MipmapBiasFixedOverride.value_or_default();
                    if (ImGui::Checkbox(LOC(CHK_Mb_Fixed_Override_127), &mbFixed))
                    {
                        config->MipmapBiasScaleOverride.reset();
                        config->MipmapBiasFixedOverride = mbFixed;
                    }

                    ShowTooltip(LOC(TIP_Apply_Same_Override_Value_249));
                }
                ImGui::EndDisabled();

                ImGui::SameLine(0.0f, 6.0f);

                ImGui::BeginDisabled(config->MipmapBiasFixedOverride.has_value() &&
                                     config->MipmapBiasFixedOverride.value());
                {
                    bool mbScale = config->MipmapBiasScaleOverride.value_or_default();
                    if (ImGui::Checkbox(LOC(CHK_Mb_Scale_Override_128), &mbScale))
                    {
                        config->MipmapBiasFixedOverride.reset();
                        config->MipmapBiasScaleOverride = mbScale;
                    }

                    ShowTooltip(LOC(Apply_override_value));
                }
                ImGui::EndDisabled();

                bool mbAll = config->MipmapBiasOverrideAll.value_or_default();
                if (ImGui::Checkbox(LOC(CHK_Mb_Override_All_Textures_129), &mbAll))
                    config->MipmapBiasOverrideAll = mbAll;

                ShowTooltip(LOC(Override_all_textures));
            }
            ImGui::EndDisabled();

            ImGui::BeginDisabled(config->MipmapBiasOverride.has_value() &&
                                 config->MipmapBiasOverride.value() == _mipBias);
            {
                if (ImGui::Button(LOC(Set)))
                {
                    config->MipmapBiasOverride = _mipBias;
                    state.lastMipBias = 100.0f;
                    state.lastMipBiasMax = -100.0f;
                }
            }
            ImGui::EndDisabled();

            ImGui::SameLine(0.0f, 6.0f);

            ImGui::BeginDisabled(!config->MipmapBiasOverride.has_value());
            {
                if (ImGui::Button(LOC(Reset)))
                {
                    config->MipmapBiasOverride.reset();
                    _mipBias = 0.0f;
                    state.lastMipBias = 100.0f;
                    state.lastMipBiasMax = -100.0f;
                }
            }
            ImGui::EndDisabled();

            if (currentFeature != nullptr && !currentFeature->IsFrozen())
            {
                ImGui::SameLine(0.0f, 6.0f);

                if (ImGui::Button(LOC(BTN_Calculate_Mipmap_Bias_163)))
                    _showMipmapCalcWindow = true;
            }

            if (config->MipmapBiasOverride.has_value())
            {
                if (config->MipmapBiasFixedOverride.value_or_default())
                {
                    ImGui::Text(LOC(UI_Current_3f_3f_Target_3f), state.lastMipBias, state.lastMipBiasMax,
                                config->MipmapBiasOverride.value());
                }
                else if (config->MipmapBiasScaleOverride.value_or_default())
                {
                    ImGui::Text(LOC(UI_Current_3f_3f_Target_Base_3f), state.lastMipBias, state.lastMipBiasMax,
                                config->MipmapBiasOverride.value());
                }
                else
                {
                    ImGui::Text(LOC(UI_Current_3f_3f_Target_Base_3f_2), state.lastMipBias, state.lastMipBiasMax,
                                config->MipmapBiasOverride.value());
                }
            }
            else
            {
                ImGui::Text(LOC(UI_Current_3f_3f), state.lastMipBias, state.lastMipBiasMax);
            }

            ImGui::Text(LOC(TXT_Will_Be_Applied_After_21));
        }

        if (ScopedCard::Shows("anisotropy"))
        {
            ScopedCard card { LOC(Anisotropic_Filtering) };
            ScopedID sectionId { LOC(Anisotropic_Filtering) };
            ImGui::PushItemWidth(65.0f * menuResScale);

            auto selectedAF =
                config->AnisotropyOverride.has_value() ? std::to_string(config->AnisotropyOverride.value()) : LOC(UI_Auto);
            if (ImGui::BeginCombo(LOC(LBL_Force_Aniso), selectedAF.c_str()))
            {
                if (ImGui::Selectable(LOC(UI_Auto), !config->AnisotropyOverride.has_value()))
                    config->AnisotropyOverride.reset();

                if (ImGui::Selectable("1", config->AnisotropyOverride.value_or(0) == 1))
                    config->AnisotropyOverride = 1;

                if (ImGui::Selectable("2", config->AnisotropyOverride.value_or(0) == 2))
                    config->AnisotropyOverride = 2;

                if (ImGui::Selectable("4", config->AnisotropyOverride.value_or(0) == 4))
                    config->AnisotropyOverride = 4;

                if (ImGui::Selectable("8", config->AnisotropyOverride.value_or(0) == 8))
                    config->AnisotropyOverride = 8;

                if (ImGui::Selectable("16", config->AnisotropyOverride.value_or(0) == 16))
                    config->AnisotropyOverride = 16;

                ImGui::EndCombo();
            }

            ImGui::PopItemWidth();

            bool afComp = config->AnisotropyModifyComp.value_or_default();
            if (ImGui::Checkbox(LOC(CHK_Modify_Compare_130), &afComp))
                config->AnisotropyModifyComp = afComp;

            ShowTooltip(LOC(TIP_Update_Comparison_Filters_250));

            ImGui::SameLine(0.0f, 6.0f);

            bool afMinMax = config->AnisotropyModifyMinMax.value_or_default();
            if (ImGui::Checkbox(LOC(CHK_Modify_Min_Max_131), &afMinMax))
                config->AnisotropyModifyMinMax = afMinMax;

            ShowTooltip(LOC(TIP_Update_Min_Max_Filters_251));

            bool afSkipPoint = config->AnisotropySkipPointFilter.value_or_default();
            if (ImGui::Checkbox(LOC(CHK_Skip_Point_Filters_132), &afSkipPoint))
                config->AnisotropySkipPointFilter = afSkipPoint;

            ShowTooltip(LOC(TIP_Skip_Updating_Of_Point_252));

            ImGui::Text(LOC(TXT_Will_Might_Be_Applied_22));
        }
    }
}

void MenuCommon::RenderKeybindSettings(RenderMenuContext& ctx)
{
    auto config = ctx.config;

    SectionTitle(LOC(Keybinds));
    {
        ScopedID sectionId { LOC(Keybinds) };

        ImGui::Text(LOC(TXT_Key_Combinations_Are_Currently_23));
        ImGui::Text(LOC(TXT_Escape_To_Cancel_Backspace_24));
        ImGui::Spacing();

        static auto menu = Keybind(LOC(UI_Menu), 10);
        static auto fpsOverlay = Keybind(LOC(FPS_Overlay), 11);
        static auto fpsOverlayCycle = Keybind(LOC(UI_FPS_Overlay_Cycle), 12);
        static auto fgEnable = Keybind(LOC(SEP_Frame_Generation_172), 13);

        menu.Render(config->ShortcutKey);
        fpsOverlay.Render(config->FpsShortcutKey);
        fpsOverlayCycle.Render(config->FpsCycleShortcutKey);
        fgEnable.Render(config->FGShortcutKey);
    }
}

void MenuCommon::RenderMenuScaleSettings(RenderMenuContext& ctx)
{
    auto config = ctx.config;
    auto& menuResScale = ctx.menuResScale;

    SectionTitle(LOC(UI_Menu));

    ImGui::PushItemWidth(100.0f * menuResScale);

    auto autoText = config->MenuScale.has_value() ? LOC(UI_Auto) : StrFmt(LOC(TXT_Auto_fmt), menuResScale);
    // clang-format off
    const char* uiScales[] = { autoText.c_str(), "0.5", "0.6", "0.7", "0.8", "0.9", "1.0", "1.1",
                               "1.2", "1.3", "1.4", "1.5", "1.6", "1.7", "1.8", "1.9", "2.0" };
    // clang-format on

    const char* selectedScaleName = uiScales[_selectedScale];

    if (ImGui::BeginCombo(LOC(LBL_Menu_Scale), selectedScaleName))
    {
        for (int n = 0; n < std::size(uiScales); n++)
        {
            if (ImGui::Selectable(uiScales[n], (_selectedScale == n)))
            {
                _selectedScale = n;

                if (n == 0)
                    config->MenuScale.reset();
                else
                    config->MenuScale = 0.4f + (float) n / 10.0f;
            }
        }

        ImGui::EndCombo();
    }

    ImGui::PopItemWidth();
}

static const char* ApiName(API api)
{
    if (api == DX11)
        return "D3D11";

    if (api == DX12)
        return "D3D12";

    if (api == Vulkan)
        return "Vulkan";

    return LOC(UI_Unknown);
}

// Set by the DLSS inputs, before an upscaler is created the swapchain shows it
static API GameApi(const State& state)
{
    if (state.api != NotSelected)
        return state.api;

    if (state.swapchainInteropApi == SwapchainInteropApi::Dx11wDx12)
        return DX11;

    return state.swapchainApi;
}

// Filled dot for on / found, hollow dot for off / missing. Goes into the next table cell
static void StatusFlag(const char* label, bool on, bool warnWhenOn = false)
{
    ImGui::TableNextColumn();

    const float lineHeight = ImGui::GetTextLineHeight();
    const float radius = lineHeight * 0.28f;
    const ImVec2 pos = ImGui::GetCursorScreenPos();
    const ImVec2 center(pos.x + lineHeight * 0.5f, pos.y + lineHeight * 0.5f);
    auto drawList = ImGui::GetWindowDrawList();

    if (on)
        drawList->AddCircleFilled(center, radius, ImGui::GetColorU32(warnWhenOn ? StatusWarn() : StatusOk()));
    else
        drawList->AddCircle(center, radius, ImGui::GetColorU32(ImGuiCol_TextDisabled), 0, 1.5f);

    ImGui::Dummy(ImVec2(lineHeight, lineHeight));
    ImGui::SameLine(0.0f, 0.0f);

    if (on)
        ImGui::TextUnformatted(label);
    else
        ImGui::TextDisabled("%s", label);
}

// Which libraries and GPU features were found. Used by the Status tab and when no upscaler is available
void MenuCommon::RenderDetectedInfo(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto& primaryGpu = *ctx.primaryGpu;

    if (!ScopedCard::Shows("status_detected"))
        return;

    ScopedCard card { "Detected" };

    if (ImGui::BeginTable("statusDetected", 4, ImGuiTableFlags_SizingStretchProp))
    {
        StatusFlag("nvngx.dll", state.nvngxExists);
        StatusFlag("nvngx_dlss", state.NVNGX_DLSS_Path.has_value());
        StatusFlag("nvngx_dlssd", state.NVNGX_DLSSD_Path.has_value());
        StatusFlag(LOC(UI_nvngx_replacement), state.nvngxReplacement.has_value());
        StatusFlag("libxess", state.libxessExists || XeSSProxy::Module() != nullptr);
        StatusFlag(LOC(UI_FSR_hooks), state.fsrHooks);
        StatusFlag(LOC(UI_FFX_3_1), FfxApiProxy::Dx12Module() != nullptr);
        StatusFlag(LOC(UI_FFX_SR), FfxApiProxy::Dx12Module_SR() != nullptr);
        StatusFlag(LOC(LBL_FFX_FG), FfxApiProxy::Dx12Module_FG() != nullptr);
        StatusFlag(LOC(UI_FFX_Vk), FfxApiProxy::VkModule() != nullptr);
        StatusFlag(LOC(UI_FFX_Vk_SR), FfxApiProxy::VkModule_SR() != nullptr);
        StatusFlag(LOC(UI_FFX_Vk_FG), FfxApiProxy::VkModule_FG() != nullptr);
        StatusFlag("OptiPatcher", state.isOptiPatcherSucceed);
        StatusFlag("Linux", state.isRunningOnLinux);
        StatusFlag(LOC(UI_DLSS_capable_GPU), primaryGpu.dlssCapable);
        StatusFlag(LOC(UI_FSR4_capable_GPU), primaryGpu.fsr4Support != FSR4Support::None, IsFsr4Int8(primaryGpu.fsr4Support));

        ImGui::EndTable();
    }
}

// Same dense look as the Status tab
void MenuCommon::RenderDetectedCard(RenderMenuContext& ctx)
{
    const auto& style = ImGui::GetStyle();

    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing, ImVec2(style.ItemSpacing.x, style.ItemSpacing.y * 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(style.CellPadding.x, 1.0f * ctx.menuResScale));

    RenderDetectedInfo(ctx);

    ImGui::PopStyleVar(2);
}

static const char* NvngxFgName(const State& state)
{
    switch (state.activeFgNvngx)
    {
    case FGNvngxReplacement::None:
        return LOC(UI_Off);
    case FGNvngxReplacement::Nukems:
        return "Nukems";
    case FGNvngxReplacement::Arturs:
        return "Enabler";
    case FGNvngxReplacement::FFX:
        return "FFX";
    case FGNvngxReplacement::Custom:
        return state.activeFgNvngxCustom.c_str();
    default:
        return "Combo";
    }
}

void MenuCommon::RenderStatusInfo(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;
    auto& menuResScale = ctx.menuResScale;
    auto& primaryGpu = *ctx.primaryGpu;

    const bool active = currentFeature != nullptr && !currentFeature->IsFrozen();

    // Dense on purpose, the whole page is meant to fit in a single screenshot
    ImGui::PushStyleVar(ImGuiStyleVar_ItemSpacing,
                        ImVec2(ImGui::GetStyle().ItemSpacing.x, ImGui::GetStyle().ItemSpacing.y * 0.5f));
    ImGui::PushStyleVar(ImGuiStyleVar_CellPadding, ImVec2(ImGui::GetStyle().CellPadding.x, 1.0f * menuResScale));

    // Label and value, two pairs per row
    auto pair = [&](const char* label, const std::string& value, bool dim = false)
    {
        ImGui::TableNextColumn();
        ImGui::TextDisabled("%s", label);

        ImGui::TableNextColumn();
        if (dim)
            ImGui::TextDisabled("%s", value.c_str());
        else
            ImGui::TextUnformatted(value.c_str());
    };

    auto flag = [](const char* label, bool on, bool warnWhenOn = false) { StatusFlag(label, on, warnWhenOn); };

    auto setupPairs = []()
    {
        ImGui::TableSetupColumn("##l1", ImGuiTableColumnFlags_WidthStretch, 0.9f);
        ImGui::TableSetupColumn("##v1", ImGuiTableColumnFlags_WidthStretch, 1.1f);
        ImGui::TableSetupColumn("##l2", ImGuiTableColumnFlags_WidthStretch, 0.9f);
        ImGui::TableSetupColumn("##v2", ImGuiTableColumnFlags_WidthStretch, 1.1f);
    };

    // SYSTEM -----------------------------
    if (ScopedCard::Shows("status_system"))
    {
        ScopedCard card { "System" };

        // Long values get the whole row
        if (ImGui::BeginTable("statusSystem", 2, ImGuiTableFlags_SizingFixedFit))
        {
            ImGui::TableSetupColumn(LOC(UI_label));
            ImGui::TableSetupColumn(LOC(UI_value), ImGuiTableColumnFlags_WidthStretch);

            auto row = [&](const char* label, const std::string& value)
            {
                ImGui::TableNextColumn();
                ImGui::TextDisabled("%s", label);
                ImGui::TableNextColumn();
                ImGui::TextUnformatted(value.c_str());
            };

            row("GPU", primaryGpu.name);
            row("Game", state.gameExe + (state.gameName.empty() ? "" : " (" + state.gameName + ")"));

            if (state.detectedQuirks.size() > 0)
            {
                std::string quirks;
                for (const auto& quirk : state.detectedQuirks)
                    quirks += quirks.empty() ? quirk : ", " + quirk;

                ImGui::TableNextColumn();
                ImGui::TextDisabled(LOC(UI_Quirks));
                ImGui::TableNextColumn();
                ImGui::TextWrapped("%s", quirks.c_str());
            }

            const auto asiPlugins = ListedAsiPlugins(state);
            if (asiPlugins.size() > 0)
            {
                std::string asi;
                for (const auto& name : asiPlugins)
                    asi += asi.empty() ? name : ", " + name;
                ImGui::TableNextColumn();
                ImGui::TextDisabled(LOC(UI_ASI));
                ImGui::TableNextColumn();
                ImGui::TextWrapped("%s", asi.c_str());
            }

            ImGui::EndTable();
        }

        std::string ratioOverride = LOC(UI_Off);
        if (config->UpscaleRatioOverrideEnabled.value_or_default())
            ratioOverride = StrFmt(LOC(UI_All_2f), config->UpscaleRatioOverrideValue.value_or_default());
        else if (config->QualityRatioOverrideEnabled.value_or_default())
            ratioOverride = LOC(UI_Per_preset);

        if (ImGui::BeginTable("statusRuntime", 4, ImGuiTableFlags_SizingStretchProp))
        {
            setupPairs();

            pair("API", StrFmt("%s%s", ApiName(GameApi(state)),
                               IdentifyGpu::gameUsesDxvk()          ? LOC(UI_DXVK_2)
                               : IdentifyGpu::gameUsesVkd3dProton() ? " (VKD3D)"
                                                                    : ""));
            pair("Swapchain", ApiName(state.swapchainApi));
            pair(LOC(UI_Input), ApiUpscalerInputName(state.currentInputApiName));

            if (active)
            {
                pair(LOC(Upscaler), NameWithVersion(currentFeature) +
                                     ((currentFeature->IsWithDx12() && state.api != DX12) ? LOC(UI_w_Dx12) : ""));
                pair("Render", StrFmt(LOC(UI_dx_d), currentFeature->RenderWidth(), currentFeature->RenderHeight()));
                pair(LOC(UI_Debug_Output), StrFmt(LOC(UI_dx_d_2fx), currentFeature->TargetWidth(), currentFeature->TargetHeight(),
                                      (float) currentFeature->TargetWidth() / (float) currentFeature->RenderWidth()));
                pair("Display", StrFmt(LOC(UI_dx_d_2fx), currentFeature->DisplayWidth(), currentFeature->DisplayHeight(),
                                       (float) currentFeature->DisplayWidth() / (float) currentFeature->RenderWidth()));
                pair("Frames", std::to_string(currentFeature->FrameCount()));
            }
            else
            {
                pair(LOC(Upscaler), LOC(UI_None), true);
                pair("Render", "-", true);
                pair(LOC(UI_Debug_Output), "-", true);
                pair("Display", "-", true);
                pair("Frames", "-", true);
            }

            auto fgInput = state.activeFgInput;
            if (fgInput != FGInput::NoFG)
                pair(LOC(LBL_FG_Input), EnumToCode(fgInput));
            else
                pair(LOC(LBL_FG_Input), LOC(UI_Off), true);

            auto fgOutput = state.activeFgOutput;
            if (fgOutput != FGOutput::NoFG)
                pair(LOC(LBL_FG_Output), EnumToCode(fgOutput));
            else
                pair(LOC(LBL_FG_Output), LOC(UI_Off), true);

            pair(LOC(UI_FG_Nvngx), NvngxFgName(state), state.activeFgNvngx == FGNvngxReplacement::None);
            pair(LOC(UI_Ratio_override), ratioOverride, ratioOverride == LOC(UI_Off));
            pair(LOC(UI_Mipmap_bias),
                 config->MipmapBiasOverride.has_value() ? StrFmt("%.3f", config->MipmapBiasOverride.value()) : LOC(UI_Off),
                 !config->MipmapBiasOverride.has_value());
            pair("Anisotropy",
                 config->AnisotropyOverride.has_value() ? std::to_string(config->AnisotropyOverride.value()) : LOC(UI_Auto),
                 !config->AnisotropyOverride.has_value());

            ImGui::EndTable();
        }
    }

    // DETECTED -----------------------------
    RenderDetectedInfo(ctx);

    // HOOKS AND CONFIG -----------------------------
    if (ScopedCard::Shows("status_hooks"))
    {
        ScopedCard card { "Hooks" };

        if (ImGui::BeginTable("statusConfig", 4, ImGuiTableFlags_SizingStretchSame))
        {
            flag(LOC(UI_DXGI_spoofing), config->DxgiSpoofing.value_or_default());
            flag(LOC(UI_VLK_spoofing), config->VulkanSpoofing.value_or_default());
            flag(LOC(UI_VLK_ext_spoofing), config->VulkanExtensionSpoofing.value_or_default());
            flag(LOC(UI_Ntdll_hooks), config->UseNtdllHooks.value_or_default());
            flag(LOC(UI_Disable_overlays), config->DisableOverlays.value_or_default(), true);
            flag(LOC(UI_ASI_plugins), config->LoadAsiPlugins.value_or_default());
            flag(LOC(UI_Precomp_shaders), config->UsePrecompiledShaders.value_or_default());
            flag(LOC(UI_Extended_limits), config->ExtendedLimits.value_or_default(), true);
            flag(LOC(UI_DRS_min_override), config->DrsMinOverrideEnabled.value_or_default());
            flag(LOC(UI_DRS_max_override), config->DrsMaxOverrideEnabled.value_or_default());
            flag(LOC(UI_Compute_root_sig), config->RestoreComputeSignature.value_or_default());
            flag(LOC(UI_Graphic_root_sig), config->RestoreGraphicSignature.value_or_default());

            ImGui::EndTable();
        }

        // Only shown when something was overridden
        std::string barriers;
        auto addBarrier = [&](const char* name, bool set)
        {
            if (!set)
                return;

            barriers += barriers.empty() ? name : std::string(", ") + name;
        };
        addBarrier(LOC(UI_Debug_Color), config->ColorResourceBarrier.has_value());
        addBarrier(LOC(UI_Debug_Depth), config->DepthResourceBarrier.has_value());
        addBarrier(LOC(Motion), config->MVResourceBarrier.has_value());
        addBarrier(LOC(UI_Debug_Exposure), config->ExposureResourceBarrier.has_value());
        addBarrier(LOC(UI_Debug_Mask), config->MaskResourceBarrier.has_value());
        addBarrier(LOC(UI_Debug_Output), config->OutputResourceBarrier.has_value());

        if (!barriers.empty())
        {
            ImGui::TextDisabled(LOC(UI_Barriers_overridden));
            ImGui::SameLine();
            ImGui::TextUnformatted(barriers.c_str());
        }
    }

    // INIT FLAGS -----------------------------
    if (ScopedCard::Shows("status_init") && active)
    {
        ScopedCard card { LOC(SEP_Init_Flags_180) };

        if (ImGui::BeginTable("statusInit", 4, ImGuiTableFlags_SizingStretchSame))
        {
            flag(LOC(UI_Auto_exposure), currentFeature->AutoExposure());
            flag(LOC(UI_Depth_inverted), currentFeature->DepthInverted());
            flag(LOC(HDR), currentFeature->IsHdr());
            flag(LOC(UI_Display_res_MV), !currentFeature->LowResMV());
            flag(LOC(UI_Jitter_cancel), currentFeature->JitteredMV());
            flag(LOC(UI_Dynamic_res), currentFeature->DynamicResolution());
            flag(LOC(UI_Output_scaling), config->OutputScalingEnabled.value_or_default());
            flag(LOC(UI_Sharp_override), config->OverrideSharpness.value_or_default());
            flag(LOC(UI_V_Sync_override), config->OverrideVsync.value_or_default());

            ImGui::EndTable();
        }
    }

    ImGui::PopStyleVar(2);
}

static bool IsUpscalerActive(IFeature* feature) { return feature != nullptr && !feature->IsFrozen(); }

static float FooterPlotHeight() { return ImGui::GetFrameHeight() * 1.0f; }

// Everything RenderMainMenuFooter draws, every item is followed by the item spacing
static float FooterHeight()
{
    const auto& style = ImGui::GetStyle();

    return 1.0f + style.ItemSpacing.y * 5.0f + ImGui::GetTextLineHeight() + FooterPlotHeight() +
           ImGui::GetFrameHeight();
}

// CUSTOM TAB -----------------------------
struct MenuCommon::MenuBox
{
    const char* id;   // Saved to the config
    const char* tab;  // Label of the tab it belongs to
    const char* name; // Shown in the Custom tab picker
    void (*draw)(RenderMenuContext&);
    // Function that draws several boxes, ScopedCard::Shows(id) selects the one to draw
    void (*source)(RenderMenuContext&) = nullptr;
    bool pinnable = true;
};

// Every box of every tab in the order they are drawn
const MenuCommon::MenuBox* MenuCommon::GetMenuBoxes(size_t& count)
{
    // clang-format off
    static const MenuBox boxes[] = {
        { "upscaler", "Upscaling", LOC(Upscalers),
          [](RenderMenuContext& c) { if (IsUpscalerActive(c.currentFeature)) RenderCard(c, RenderActiveUpscalerSettings); } },
        { "sharpness", "Upscaling", LOC(Sharpness),
          [](RenderMenuContext& c) { if (IsUpscalerActive(c.currentFeature)) RenderCard(c, RenderActiveImageSettings); } },
        { "upscale_ratio", "Upscaling", LOC(SEP_Upscale_Ratio_Override_178),
          [](RenderMenuContext& c) { if (IsUpscalerActive(c.currentFeature)) RenderCard(c, RenderUpscaleRatioSettings); } },
        { "output_scaling", "Upscaling", LOC(SEP_Output_Scaling_179),
          [](RenderMenuContext& c) { if (IsUpscalerActive(c.currentFeature)) RenderCard(c, RenderOutputScalingSettings); } },
        { "shader_times", "Upscaling", LOC(UI_Per_shader_GPU_times),
          [](RenderMenuContext& c) { if (IsUpscalerActive(c.currentFeature)) RenderCard(c, RenderShaderTimes); } },
        { "magnifier", "Upscaling", LOC(Magnifier),
          [](RenderMenuContext& c) { if (IsUpscalerActive(c.currentFeature)) RenderCard(c, RenderMagnifierSettings); } },

        { "fg_selection", LOC(UI_Frame_Gen), LOC(SEP_Frame_Generation_172), [](RenderMenuContext& c) { RenderCard(c, RenderFrameGenerationSelection); } },
        { "fg_fsrfg", LOC(UI_Frame_Gen), LOC(UI_FSR_FG_Settings), nullptr, RenderFrameGenerationRuntimeSettings },
        { "fg_xefg", LOC(UI_Frame_Gen), LOC(UI_XeFG_Settings), nullptr, RenderFrameGenerationRuntimeSettings },
        { "fg_dlssg", LOC(UI_Frame_Gen), LOC(UI_DLSSG_Settings), nullptr, RenderFrameGenerationRuntimeSettings },
        { "fg_flipmeter", LOC(UI_Frame_Gen), LOC(UI_Flip_Meter_Settings), nullptr, RenderFrameGenerationRuntimeSettings },
        { "fg_reprojection", LOC(UI_Frame_Gen), LOC(UI_Reprojection_Settings), nullptr, RenderFrameGenerationRuntimeSettings },
        { "fg_optifg", LOC(UI_Frame_Gen), LOC(UI_OptiFG_Settings), nullptr, RenderFrameGenerationRuntimeSettings },
        { "fg_nvngx", LOC(UI_Frame_Gen), LOC(UI_Nvngx_FG_replacement), nullptr, RenderFrameGenerationRuntimeSettings },

        { "framerate", "Latency", LOC(Framerate_Title), [](RenderMenuContext& c) { RenderCard(c, RenderFramerateSettings); } },
        { "low_latency", "Latency", LOC(SEP_Low_Latency_177), [](RenderMenuContext& c) { RenderCard(c, RenderLowLatencySettings); } },
        { "vsync", "Latency", "V-Sync", [](RenderMenuContext& c) { RenderCard(c, RenderVsyncSettings); } },

        { "mipmap", "Textures", LOC(Mipmap_Bias), nullptr, RenderApiAndTextureSettings },
        { "anisotropy", "Textures", LOC(Anisotropic_Filtering), nullptr, RenderApiAndTextureSettings },

        { "init_flags", "Compatibility", LOC(SEP_Init_Flags_180), [](RenderMenuContext& c) { RenderCard(c, RenderInitFlagsSettings); } },
        { "fsr_common", "Compatibility", LOC(SEP_Fsr_Common_Settings_201), [](RenderMenuContext& c) { RenderCard(c, RenderFsrCommonSettings); } },
        { "upscaler_inputs", "Compatibility", LOC(Upscaler_Inputs), [](RenderMenuContext& c) { RenderCard(c, RenderUpscalerInputsSettings); } },
        { "spoofing", "Compatibility", "Spoofing", nullptr, RenderAdvancedSettings },
        { "plugins", "Compatibility", "Startup", nullptr, RenderAdvancedSettings },
        { "shaders_limits", "Compatibility", "Shaders", nullptr, RenderAdvancedSettings },
        { "barriers", "Compatibility", LOC(Resource_Barriers), nullptr, RenderAdvancedSettings },
        { "root_signatures", "Compatibility", LOC(Root_Signatures), nullptr, RenderAdvancedSettings },

        { "menu_scale", LOC(UI_Menu), LOC(LBL_Menu_Scale), [](RenderMenuContext& c) { RenderCard(c, RenderMenuScaleSettings); } },
        { "theme", LOC(UI_Menu), LOC(UI_Theme_and_Color), [](RenderMenuContext& c) { RenderCard(c, RenderThemeSettings); } },
        { "fps_overlay", LOC(UI_Menu), LOC(FPS_Overlay), [](RenderMenuContext& c) { RenderCard(c, RenderFpsOverlaySettings); } },
        { "keybinds", LOC(UI_Menu), LOC(Keybinds), [](RenderMenuContext& c) { RenderCard(c, RenderKeybindSettings); } },
        { "custom_tab", LOC(UI_Menu), LOC(UI_Custom_Tab), [](RenderMenuContext& c) { RenderCard(c, RenderCustomTabSettings); }, nullptr, false },

        { "active_quirks", LOC(UI_Quirks_ASI), LOC(Active_Quirks), nullptr, RenderQuirksSettings },
        { "loaded_asi_plugins", LOC(UI_Quirks_ASI), LOC(UI_Loaded_ASI_Plugins), nullptr, RenderQuirksSettings },

        { "status_system", "Status", "System", nullptr, RenderStatusInfo },
        { "status_detected", "Status", "Detected", nullptr, RenderStatusInfo },
        { "status_hooks", "Status", "Hooks", nullptr, RenderStatusInfo },
        { "status_init", "Status", LOC(UI_Init_Flags_active), nullptr, RenderStatusInfo },
        { "logging", "Status", LOC(Logging), [](RenderMenuContext& c) { RenderCard(c, RenderLoggingSettings); } },
    };
    // clang-format on

    count = std::size(boxes);
    return boxes;
}

// Draws a single box, even when it's one of many drawn by the same function
void MenuCommon::RenderMenuBox(RenderMenuContext& ctx, const MenuBox& box)
{
    if (box.draw != nullptr)
    {
        box.draw(ctx);
        return;
    }

    ScopedCard::SetFilter(box.id);
    box.source(ctx);
    ScopedCard::SetFilter(nullptr);
}

// Ids are kept in a comma separated list
static bool IsBoxPinned(const std::string& pinned, const char* id)
{
    const size_t length = strlen(id);

    for (size_t pos = pinned.find(id); pos != std::string::npos; pos = pinned.find(id, pos + 1))
    {
        const bool startOk = pos == 0 || pinned[pos - 1] == ',';
        const bool endOk = pos + length == pinned.size() || pinned[pos + length] == ',';

        if (startOk && endOk)
            return true;
    }

    return false;
}

void MenuCommon::RenderCustomTab(RenderMenuContext& ctx)
{
    auto config = ctx.config;

    const std::string pinned = config->CustomTabCards.value_or_default();

    size_t count = 0;
    const MenuBox* boxes = GetMenuBoxes(count);

    bool anyPinned = false;
    const float startY = ImGui::GetCursorPosY();

    for (size_t i = 0; i < count; i++)
    {
        if (!boxes[i].pinnable || !IsBoxPinned(pinned, boxes[i].id))
            continue;

        anyPinned = true;

        ImGui::PushID(boxes[i].id);

        // Explains why the upscaler settings are missing, the Upscaling tab draws it above its boxes
        if (strcmp(boxes[i].id, "upscaler") == 0)
        {
            auto feature = ctx.currentFeature;
            if (feature == nullptr || !feature->IsInited() || feature->IsFrozen())
                RenderCard(ctx, RenderUpscalerStateMessage);
        }

        RenderMenuBox(ctx, boxes[i]);
        ImGui::PopID();
    }

    // Boxes only show up when their feature is in use
    if (!anyPinned)
        ImGui::TextDisabled(LOC(UI_Nothing_picked_yet_choose_the_boxes_in_Menu_Custom_Tab));
    else if (ImGui::GetCursorPosY() == startY)
        ImGui::TextDisabled(LOC(UI_None_of_the_picked_boxes_are_available_right_now));

    // Hint pinned to the bottom of the tab
    const float hintHeight = ImGui::GetFrameHeight();
    const float gap = ImGui::GetContentRegionAvail().y - hintHeight - ImGui::GetStyle().ItemSpacing.y;
    if (gap > 0.0f)
        ImGui::Dummy(ImVec2(0.0f, gap));
    else
        ImGui::Spacing();

    ImGui::AlignTextToFramePadding();
    ImGui::PushStyleColor(ImGuiCol_Text, ImGui::GetStyleColorVec4(ImGuiCol_TextDisabled));
    ImGui::PushStyleColor(ImGuiCol_Button, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    if (ImGui::SmallButton(LOC(UI_Customise_this_tab)))
        requestedMenuTab = LOC(UI_Menu);
    ImGui::PopStyleColor(2);
}

void MenuCommon::RenderCustomTabSettings(RenderMenuContext& ctx)
{
    auto config = ctx.config;

    SectionTitle(LOC(UI_Custom_Tab));

    ConfigCheckbox(LOC(UI_Show_Custom_tab), config->CustomTabEnabled,
                   "Adds a tab on top of the list that shows only the picked boxes");

    if (!config->CustomTabEnabled.value_or_default())
        return;

    ImGui::Spacing();

    if (auto ch = ScopedCollapsingHeader(LOC(UI_Boxes_to_show)); ch.IsHeaderOpen())
    {
        const std::string pinned = config->CustomTabCards.value_or_default();
        std::string newPinned;
        bool changed = false;

        size_t count = 0;
        const MenuBox* boxes = GetMenuBoxes(count);

        const char* lastTab = nullptr;
        bool tableOpen = false;

        for (size_t i = 0; i < count; i++)
        {
            const MenuBox& box = boxes[i];

            if (!box.pinnable)
                continue;

            if (lastTab == nullptr || strcmp(lastTab, box.tab) != 0)
            {
                if (tableOpen)
                    ImGui::EndTable();

                ImGui::TextDisabled("%s", box.tab);
                tableOpen = ImGui::BeginTable(box.tab, 2, ImGuiTableFlags_SizingStretchSame);
                lastTab = box.tab;
            }

            bool on = IsBoxPinned(pinned, box.id);

            if (tableOpen)
            {
                ImGui::TableNextColumn();

                ImGui::PushID(box.id);
                changed |= ImGui::Checkbox(box.name, &on);
                ImGui::PopID();
            }

            if (on)
            {
                if (!newPinned.empty())
                    newPinned += ',';

                newPinned += box.id;
            }
        }

        if (tableOpen)
            ImGui::EndTable();

        if (changed)
            config->CustomTabCards = newPinned;

        ImGui::Spacing();

        if (ImGui::Button(LOC(UI_Reset_Custom_Tab)))
            config->CustomTabCards.reset();
    }
}

void MenuCommon::RenderMainMenuTabs(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& currentFeature = ctx.currentFeature;
    auto& menuResScale = ctx.menuResScale;

    using DrawFn = void (*)(RenderMenuContext&);

    struct Tab
    {
        const char* group; // Groups are split by a line in the sidebar
        const char* label;
        DrawFn draw = nullptr;                                     // Drawn above the boxes of the tab
        bool (*visible)(RenderMenuContext&) = nullptr;             // Tab hidden when returned false
        const char* (*displayLabel)(RenderMenuContext&) = nullptr; // Name shown in the sidebar
    };

    // Boxes of a tab come from GetMenuBoxes, by the label of the tab
    // clang-format off
    static const Tab tabs[] = {
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
    };
    // clang-format on

    constexpr int customTab = 0;
    const bool customTabEnabled = config->CustomTabEnabled.value_or_default();

    // Tabs registered by plugins come after the built-in ones
    constexpr int builtinTabCount = (int) std::size(tabs);
    const auto pluginTabs = PluginApi::GetTabs();
    const int tabCount = builtinTabCount + (int) pluginTabs.size();

    auto isTabVisible = [&](int i)
    {
        if (i >= builtinTabCount)
            return true;
        if (i == customTab)
            return customTabEnabled;
        return tabs[i].visible == nullptr || tabs[i].visible(ctx);
    };

    if (requestedMenuTab != nullptr)
    {
        for (int i = 0; i < (int) std::size(tabs); i++)
        {
            if (strcmp(tabs[i].label, requestedMenuTab) == 0)
                selectedMenuTab = i;
        }

        requestedMenuTab = nullptr;
    }

    selectedMenuTab = std::clamp(selectedMenuTab, 0, tabCount - 1);

    // Selected tab can disappear, pick the first one that's visible
    if (!isTabVisible(selectedMenuTab))
    {
        for (int i = 0; i < builtinTabCount; i++)
        {
            if (isTabVisible(i))
            {
                selectedMenuTab = i;
                break;
            }
        }
    }

    const auto& style = ImGui::GetStyle();

    // Space kept free below for the frametime graph and the buttons
    const float footerHeight = FooterHeight();
    const float sidebarWidth = 130.0f * menuResScale;
    const float tabHeight = ImGui::GetFrameHeight() * 1.35f;

    // Sidebar -----------------------------
    ImGui::PushStyleColor(ImGuiCol_ChildBg, ImVec4(0.0f, 0.0f, 0.0f, 0.0f));
    ImGui::BeginChild(LOC(UI_MenuSidebar), ImVec2(sidebarWidth, -footerHeight),
                      ImGuiChildFlags_Borders | ImGuiChildFlags_NavFlattened);
    {
        const char* lastGroup = nullptr;

        for (int i = 0; i < tabCount; i++)
        {
            if (!isTabVisible(i))
                continue;

            const bool pluginTab = i >= builtinTabCount;
            const char* group = pluginTab ? "PLUGINS" : tabs[i].group;

            if (lastGroup != nullptr && strcmp(lastGroup, group) != 0)
                ImGui::Separator();

            lastGroup = group;

            const bool selected = selectedMenuTab == i;

            ImGui::PushID(i);
            if (ImGui::Selectable(LOC(UI_Tab), selected, 0, ImVec2(0.0f, tabHeight)))
                selectedMenuTab = i;
            ImGui::PopID();

            // Description comes from the plugin, so it isn't used as a format string
            if (pluginTab && !pluginTabs[i - builtinTabCount].description.empty() &&
                ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            {
                ImGui::BeginTooltip();
                ImGui::TextUnformatted(pluginTabs[i - builtinTabCount].description.c_str());
                ImGui::EndTooltip();
            }

            // Label is drawn manually to center it vertically
            const ImVec2 rectMin = ImGui::GetItemRectMin();
            const ImVec2 rectMax = ImGui::GetItemRectMax();
            auto drawList = ImGui::GetWindowDrawList();

            const char* label = pluginTab                         ? pluginTabs[i - builtinTabCount].name.c_str()
                                : tabs[i].displayLabel != nullptr ? tabs[i].displayLabel(ctx)
                                                                  : tabs[i].label;

            // Long plugin names are cut at the edge of the sidebar
            drawList->PushClipRect(rectMin, rectMax, true);
            drawList->AddText(
                ImVec2(rectMin.x + 6.0f * menuResScale, rectMin.y + (tabHeight - ImGui::GetTextLineHeight()) * 0.5f),
                ImGui::GetColorU32(ImGuiCol_Text), label);
            drawList->PopClipRect();
        }

        // OptiPatcher status only visible when used
        const bool optiPatcher = state.isOptiPatcherSucceed;

        // Compact state of the upscaler and FG pinned to the bottom of the sidebar
        const float statusLines = 3.0f + (optiPatcher ? 1.0f : 0.0f);
        const float statusHeight = 1.0f + ImGui::GetTextLineHeightWithSpacing() * statusLines + style.ItemSpacing.y;
        const float gap = ImGui::GetContentRegionAvail().y - statusHeight - style.ItemSpacing.y;
        if (gap > 0.0f)
            ImGui::Dummy(ImVec2(0.0f, gap));

        ImGui::Separator();
        ImGui::Spacing();

        ImGui::TextDisabled("SR");
        ImGui::SameLine(0.0f, 8.0f * menuResScale);
        if (currentFeature == nullptr || !currentFeature->IsInited())
            ImGui::TextDisabled(LOC(UI_None));
        else if (currentFeature->IsFrozen())
            ImGui::TextColored(StatusWarn(), "Idle");
        else
        {
            const float renderWidth = (float) currentFeature->RenderWidth();
            const float upscaleRatio = renderWidth > 0.0f ? (float) currentFeature->TargetWidth() / renderWidth : 0.0f;

            ImGui::TextColored(StatusOk(), LOC(UI_s_2f), currentFeature->ShortName().c_str(), upscaleRatio);
        }

        ImGui::TextDisabled("FG");
        ImGui::SameLine(0.0f, 8.0f * menuResScale);
        auto fg = state.CurrentFG();

        // Nvngx replacements don't use currentFG, have to use the DLSSG state detected from the game.
        // Flip Meter calls the replacement itself and counts its own frames
        const FGNvngxReplacement activeNvngxFg = state.activeFgNvngx;

        if (activeNvngxFg != FGNvngxReplacement::None && state.activeFgOutput != FGOutput::FlipMeter)
        {
            const char* nvngxName = NvngxFgName(state);
            const int detectedCount = state.DlssgDetectedInterpolationCount();

            if (detectedCount <= 0)
            {
                ImGui::TextDisabled(LOC(UI_Off));
            }

            else
            {
                const bool unsupported = detectedCount > Nvngx_FG::getMaxFakeFramesCount();
                ImGui::TextColored(unsupported ? StatusWarn() : StatusOk(), LOC(UI_s_dx), nvngxName, detectedCount + 1);
            }
        }

        else if (fg != nullptr && fg->IsGenerating())
        {
            // Reprojection as the output generates no frames
            if (state.activeFgOutput == FGOutput::Reprojection)
                ImGui::TextColored(StatusOk(), "%s", fg->Name());
            else
                ImGui::TextColored(StatusOk(), LOC(UI_s_ux), fg->Name(), fg->GetInterpolatedFrameCount() + 1);
        }
        else
            ImGui::TextDisabled(LOC(UI_Off));

        const bool dxgiSpoofing = config->DxgiSpoofing.value_or_default();
        const bool vkSpoofing = config->VulkanSpoofing.value_or_default();
        const bool vkExtSpoofing = config->VulkanExtensionSpoofing.value_or_default();

        ImGui::TextDisabled("Spoof");
        ImGui::SameLine(0.0f, 8.0f * menuResScale);
        if (vkSpoofing && vkExtSpoofing)
            ImGui::TextColored(StatusOk(), LOC(UI_VLK_Ext));
        else if (vkSpoofing)
            ImGui::TextColored(StatusOk(), "VLK");
        else if (vkExtSpoofing)
            ImGui::TextColored(StatusOk(), LOC(UI_VLK_Ext_2));
        else if (dxgiSpoofing)
            ImGui::TextColored(StatusOk(), "DXGI");
        else
            ImGui::TextDisabled("No");

        if (optiPatcher)
        {
            ImGui::TextDisabled("OP");
            ImGui::SameLine(0.0f, 8.0f * menuResScale);
            ImGui::TextColored(StatusOk(), "Yes");
        }
    }

    ImGui::EndChild();

    ImGui::SameLine();

    // Scrollable content of the tab -----------------------------
    ImGui::BeginChild(LOC(UI_MenuContent), ImVec2(0.0f, -footerHeight), ImGuiChildFlags_NavFlattened,
                      ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse);
    if (selectedMenuTab >= builtinTabCount)
    {
        // Own ID per tab, so every tab remembers its scroll position
        ImGui::PushID(selectedMenuTab);
        ImGui::BeginChild(LOC(UI_MenuTabBody), ImVec2(0.0f, 0.0f));

        PluginApi::DrawTab(selectedMenuTab - builtinTabCount, menuResScale, toneMapColor);

        ImGui::EndChild();
        ImGui::PopID();
    }
    else
    {
        const Tab& tab = tabs[selectedMenuTab];

        // Own ID per tab, so every tab remembers its scroll position
        ImGui::PushID(selectedMenuTab);
        ImGui::BeginChild(LOC(UI_MenuTabBody), ImVec2(0.0f, 0.0f));

        if (tab.draw != nullptr)
            tab.draw(ctx);

        size_t boxCount = 0;
        const MenuBox* boxes = GetMenuBoxes(boxCount);
        DrawFn lastSource = nullptr;

        for (size_t i = 0; i < boxCount; i++)
        {
            if (strcmp(boxes[i].tab, tab.label) != 0)
                continue;

            // A function with several boxes is drawn once for all of them
            if (boxes[i].draw == nullptr)
            {
                if (boxes[i].source != lastSource)
                    boxes[i].source(ctx);
            }
            else
            {
                boxes[i].draw(ctx);
            }

            lastSource = boxes[i].source;
        }

        ImGui::EndChild();
        ImGui::PopID();
    }
    ImGui::EndChild();
    ImGui::PopStyleColor();
}

void MenuCommon::PlotLinesWithRange(const char* label, float (*values_getter)(void* data, int idx), void* data,
                                    int values_count, int values_offset, const char* overlay_text, ImVec2 graph_size,
                                    float padding)
{
    float v_min = FLT_MAX;
    float v_max = -FLT_MAX;
    for (int i = 0; i < plotWidth; i++)
    {
        const float v = values_getter(data, i);
        if (v != v) // Ignore NaN values
            continue;
        v_min = ImMin(v_min, v);
        v_max = ImMax(v_max, v);
    }

    ImGui::PlotEx(ImGuiPlotType_Lines, label, values_getter, data, values_count, values_offset, overlay_text, v_min,
                  v_max, graph_size);

    if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
        ImGui::SetTooltip(LOC(UI_Range_2f_ms_2f_ms), v_min, v_max);
}

void MenuCommon::RenderFrameTimeGraph(RenderMenuContext& ctx)
{
    auto& frameTime = ctx.frameTime;
    auto& frameRate = ctx.frameRate;
    auto& menuResScale = ctx.menuResScale;
    auto& averageFrameTime = ctx.averageFrameTime;

    ImGui::Text(LOC(UI_Frametime_2fms_1ffps_avg_1ffps), frameTime, frameRate, 1000.f / averageFrameTime);

    PlotLinesWithRange(
        "##FrameTime",
        [](void* rb, int idx) -> float { return static_cast<RingBuffer<float, plotWidth>*>(rb)->At(idx); },
        &gFrameTimes, plotWidth, 0, nullptr, ImVec2(0.0f, FooterPlotHeight()), 4.0f * menuResScale);
}

void MenuCommon::RenderUpscalerTimeGraph(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto& currentFeature = ctx.currentFeature;
    auto& menuResScale = ctx.menuResScale;
    auto& averageUpscalerFT = ctx.averageUpscalerFT;

    if (!IsUpscalerActive(currentFeature))
        return;

    ImGui::Text(LOC(UI_Upscaler_GPU_time_2fms_avg_2fms), state.upscaleTimes.back(), averageUpscalerFT);

    PlotLinesWithRange(
        "##UpscalerTime",
        [](void* rb, int idx) -> float { return static_cast<RingBuffer<float, plotWidth>*>(rb)->At(idx); },
        &gUpscalerTimes, plotWidth, 0, nullptr, ImVec2(0.0f, FooterPlotHeight()), 4.0f * menuResScale);
}

void MenuCommon::RenderMainMenuFooter(RenderMenuContext& ctx)
{
    ImGui::Separator();
    ImGui::Spacing();

    // Placed by hand instead of using a table or SameLine. Both of them carry the text baseline of the
    // first graph over to the second one, which made it sit a few pixels lower
    const float spacing = ImGui::GetStyle().ItemSpacing.x;
    const float graphWidth = (ImGui::GetContentRegionAvail().x - spacing) * 0.5f;
    const ImVec2 startPos = ImGui::GetCursorPos();

    ImGui::PushItemWidth(graphWidth);

    RenderFrameTimeGraph(ctx);
    const float endY = ImGui::GetCursorPosY();

    // The indent is needed because the cursor goes back to the left edge after every item
    ImGui::SetCursorPosY(startPos.y);
    ImGui::Indent(graphWidth + spacing);
    RenderUpscalerTimeGraph(ctx);
    ImGui::Unindent(graphWidth + spacing);

    ImGui::PopItemWidth();

    ImGui::SetCursorPosY(endY);

    RenderMainMenuBottomBar(ctx);
}

void MenuCommon::RenderShaderTimes(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto& currentFeature = ctx.currentFeature;

    if (!IsUpscalerActive(currentFeature) || state.detailedGpuTimes.empty())
        return;

    SectionTitle(LOC(UI_Per_shader_GPU_times));

    if (ImGui::BeginTable("ShaderTimes", 2, ImGuiTableFlags_SizingFixedSame))
    {
        bool hasExtra = false;

        for (auto& [name, time, includedInUpscalerTime] : state.detailedGpuTimes)
        {
            if (!includedInUpscalerTime)
            {
                hasExtra = true;
                continue;
            }

            auto formattedTime = StrFmt(LOC(UI_S_7_2f_ms), time);

            ImGui::TableNextColumn();
            ImGui::Text("%s", name.c_str());

            ImGui::TableNextColumn();
            ImGui::Text("%s", formattedTime.c_str());
        }

        if (hasExtra)
        {
            ImGui::TableNextRow();
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::TextDisabled(LOC(UI_Extra_shaders));
            ImGui::TableNextColumn();
            ImGui::TextDisabled("");
            for (auto& [name, time, includedInUpscalerTime] : state.detailedGpuTimes)
            {
                if (includedInUpscalerTime)
                    continue;

                auto formattedTime = StrFmt(LOC(UI_S_7_2f_ms), time);

                ImGui::TableNextColumn();
                ImGui::Text("%s", name.c_str());

                ImGui::TableNextColumn();
                ImGui::Text("%s", formattedTime.c_str());
            }
        }

        ImGui::EndTable();
    }
}

void MenuCommon::RenderMainMenuBottomBar(RenderMenuContext& ctx)
{
    auto config = ctx.config;
    auto& io = ctx.io;

    // BOTTOM LINE ---------------
    if (ImGui::Button(LOC(BTN_Open_Wiki_165)))
    {
        auto pIO = &ImGui::GetPlatformIO();
        auto ctx = ImGui::GetCurrentContext();
        pIO->Platform_OpenInShellFn(ctx, LOC(UI_https_github_com_optiscaler_OptiScaler_wiki));
    }
    ShowTooltip(LOC(UI_Compatibility_list_known_issues_and_FG_options_explained));

    ImGui::SameLine();

    const auto saveLabel = LOC(BTN_Save_Settings_164);
    const auto closeLabel = LOC(Close);

    auto& style = ImGui::GetStyle();
    float rightButtonsWidth = ImGui::CalcTextSize(saveLabel, nullptr, true).x +
                              ImGui::CalcTextSize(closeLabel, nullptr, true).x + style.FramePadding.x * 4.0f + 6.0f;

    float avail = ImGui::GetContentRegionAvail().x;
    ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - rightButtonsWidth);

    if (ImGui::Button(saveLabel))
        config->SaveIni();

    ImGui::SameLine(0.0f, 6.0f);

    if (ImGui::Button(closeLabel))
    {
        _isVisible = false;
        hasGamepad = (io.BackendFlags | ImGuiBackendFlags_HasGamepad) > 0;
        io.BackendFlags &= 30;
        io.ConfigFlags = ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NoKeyboard;

        _showMipmapCalcWindow = false;
        _showHudlessWindow = false;
        io.MouseDrawCursor = false;
        io.WantCaptureKeyboard = false;
        io.WantCaptureMouse = false;
    }
}

void MenuCommon::RenderMipmapBiasWindow(RenderMenuContext& ctx, ImGuiWindowFlags flags)
{
    auto config = ctx.config;
    auto& io = ctx.io;
    auto& currentFeature = ctx.currentFeature;

    // Metrics window (for debug)
    // ImGui::ShowMetricsWindow();

    // Mipmap calculation window
    if (_showMipmapCalcWindow && currentFeature != nullptr && !currentFeature->IsFrozen() && currentFeature->IsInited())
    {
        auto posX = (io.DisplaySize.x - 450.0f) / 2.0f;
        auto posY = (io.DisplaySize.y - 200.0f) / 2.0f;

        ImGui::SetNextWindowPos(ImVec2 { posX, posY }, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2 { 450.0f, 200.0f }, ImGuiCond_FirstUseEver);

        if (_displayWidth == 0)
        {
            if (config->OutputScalingEnabled.value_or_default())
            {
                _displayWidth = static_cast<uint32_t>(currentFeature->DisplayWidth() *
                                                      config->OutputScalingMultiplier.value_or_default());
            }
            else
            {
                _displayWidth = currentFeature->DisplayWidth();
            }

            _renderWidth = static_cast<uint32_t>(_displayWidth / 3.0f);
            _mipmapUpscalerQuality = 0;
            _mipmapUpscalerRatio = 3.0f;
            _mipBiasCalculated = log2((float) _renderWidth / (float) _displayWidth);
        }

        if (ImGui::Begin(LOC(Mipmap_Bias), nullptr, flags))
        {
            if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow))
                ImGui::SetWindowFocus();

            if (ImGui::InputScalar(LOC(UI_Display_Width), ImGuiDataType_U32, &_displayWidth, NULL, NULL, "%u"))
            {
                if (_displayWidth <= 0)
                {
                    if (config->OutputScalingEnabled.value_or_default())
                    {
                        _displayWidth = static_cast<uint32_t>(currentFeature->DisplayWidth() *
                                                              config->OutputScalingMultiplier.value_or_default());
                    }
                    else
                    {
                        _displayWidth = currentFeature->DisplayWidth();
                    }
                }

                _renderWidth = static_cast<uint32_t>(_displayWidth / _mipmapUpscalerRatio);
                _mipBiasCalculated = log2((float) _renderWidth / (float) _displayWidth);
            }

            const char* q[] = { LOC(UI_Ultra_Performance), LOC(UI_Performance), LOC(UI_Balanced), LOC(UI_Quality), LOC(UI_Ultra_Quality), LOC(UI_DLAA) };
            float fr[] = { 3.0f, 2.0f, 1.7f, 1.5f, 1.3f, 1.0f };
            auto configQ = _mipmapUpscalerQuality;

            const char* selectedQ = q[configQ];

            ImGui::BeginDisabled(config->UpscaleRatioOverrideEnabled.value_or_default());

            if (ImGui::BeginCombo(LOC(UI_Upscaler_Quality), selectedQ))
            {
                for (int n = 0; n < 6; n++)
                {
                    if (ImGui::Selectable(q[n], (_mipmapUpscalerQuality == n)))
                    {
                        _mipmapUpscalerQuality = n;

                        float ov = -1.0f;

                        if (config->QualityRatioOverrideEnabled.value_or_default())
                        {
                            switch (n)
                            {
                            case 0:
                                ov = config->QualityRatio_UltraPerformance.value_or(-1.0f);
                                break;

                            case 1:
                                ov = config->QualityRatio_Performance.value_or(-1.0f);
                                break;

                            case 2:
                                ov = config->QualityRatio_Balanced.value_or(-1.0f);
                                break;

                            case 3:
                                ov = config->QualityRatio_Quality.value_or(-1.0f);
                                break;

                            case 4:
                                ov = config->QualityRatio_UltraQuality.value_or(-1.0f);
                                break;
                            }
                        }

                        if (ov > 0.0f)
                            _mipmapUpscalerRatio = ov;
                        else
                            _mipmapUpscalerRatio = fr[n];

                        _renderWidth = static_cast<uint32_t>(_displayWidth / _mipmapUpscalerRatio);
                        _mipBiasCalculated = log2((float) _renderWidth / (float) _displayWidth);
                    }
                }

                ImGui::EndCombo();
            }

            ImGui::EndDisabled();

            auto minLimit = config->ExtendedLimits.value_or_default() ? 0.1f : 1.0f;
            auto maxLimit = config->ExtendedLimits.value_or_default() ? 6.0f : 3.0f;
            if (ImGui::SliderFloat(LOC(UI_Upscaler_Ratio), &_mipmapUpscalerRatio, minLimit, maxLimit, "%.2f"))
            {
                _renderWidth = static_cast<uint32_t>(_displayWidth / _mipmapUpscalerRatio);
                _mipBiasCalculated = log2((float) _renderWidth / (float) _displayWidth);
            }

            if (ImGui::InputScalar(LOC(UI_Render_Width), ImGuiDataType_U32, &_renderWidth, NULL, NULL, "%u"))
                _mipBiasCalculated = log2((float) _renderWidth / (float) _displayWidth);

            ImGui::SliderFloat(LOC(Mipmap_Bias), &_mipBiasCalculated, -15.0f, 0.0f, "%.6f");

            // BOTTOM LINE
            ImGui::Spacing();
            ImGui::Separator();
            ImGui::Spacing();

            ImGui::SameLine();
            ImGui::Spacing();

            constexpr float spacing = 6.0f;
            auto textSize = ImGui::CalcTextSize(LOC(BTN_Use_Value_166));
            textSize += ImGui::CalcTextSize(LOC(Close));
            textSize.x += ImGui::GetStyle().FramePadding.x * 5.0f + spacing; // 2 sides * 2 buttons + 1

            float avail = ImGui::GetContentRegionAvail().x;
            ImGui::SetCursorPosX(ImGui::GetCursorPosX() + avail - textSize.x);

            if (ImGui::Button(LOC(BTN_Use_Value_166)))
            {
                _mipBias = _mipBiasCalculated;
                _showMipmapCalcWindow = false;
            }

            ImGui::SameLine(0.0f, spacing);

            if (ImGui::Button(LOC(Close)))
                _showMipmapCalcWindow = false;

            ImGui::Spacing();
            ImGui::Separator();

            ImGui::End();
        }
    }
}

void MenuCommon::RenderHudlessResourcesWindow(RenderMenuContext& ctx, ImGuiWindowFlags flags)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& io = ctx.io;

    auto fg = state.currentFG;
    if (_showHudlessWindow && config->FGHUDFix.value_or_default() && fg != nullptr && fg->IsActive())
    {
        auto posX = (io.DisplaySize.x - 400.0f) / 2.0f;
        auto posY = (io.DisplaySize.y - 300.0f) / 2.0f;

        ImGui::SetNextWindowPos(ImVec2 { posX, posY }, ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowSize(ImVec2 { 400.0f, 300.0f });

        if (ImGui::Begin(LOC(UI_HUDless_Resources), nullptr, flags))
        {
            if (!ImGui::IsWindowFocused(ImGuiFocusedFlags_AnyWindow))
                ImGui::SetWindowFocus();

            int btnCount = 100;

            if (ImGui::BeginTable("HUDlessTable", 2, ImGuiTableFlags_SizingFixedFit))
            {
                ImGui::TableSetupColumn("##1", ImGuiTableColumnFlags_WidthStretch);
                ImGui::TableSetupColumn("##2", ImGuiTableColumnFlags_WidthFixed);

                ankerl::unordered_dense::map<void*, CapturedHudlessInfo>::iterator it;

                // Render threads add and remove entries
                std::scoped_lock hudlessLock(state.capturedHudlessesMutex);

                for (it = state.capturedHudlesses.begin(); it != state.capturedHudlesses.end(); it++)
                {
                    ImGui::TableNextRow();

                    ImGui::TableSetColumnIndex(0);

                    ImGui::Text(LOC(UI_S_08x_s_s_Count_llu_s), (size_t) it->first,
                                GetSourceString(it->second.captureInfo & 0xFF).c_str(),
                                GetDispatchString(it->second.captureInfo & 0xFF00).c_str(), it->second.usageCount,
                                it->second.enabled ? LOC(UI_Active) : LOC(UI_Passive));

                    ImGui::TableSetColumnIndex(1);

                    btnCount++;
                    std::string text;

                    if (it->second.enabled)
                        text = StrFmt(LOC(LBL_Disable_fmt), btnCount);
                    else
                        text = StrFmt(LOC(LBL_Enable_fmt), btnCount);

                    if (ImGui::Button(text.c_str()))
                    {
                        LOG_DEBUG("HUDless {:X}: {}", (size_t) it->first,
                                  it->second.enabled ? LOC(UI_Disabling) : LOC(UI_Enabling));
                        it->second.enabled = !it->second.enabled;
                    }
                }

                ImGui::EndTable();
            }

            if (ImGui::Button(LOC(BTN_Clear_167)))
            {
                LOG_DEBUG("Clearing captured HUDless resources");
                state.clearCapturedHudlesses = true;
            }

            ImGui::SameLine(0.0f, 8.0f);

            if (ImGui::Button(LOC(BTN_Close_168)))
                _showHudlessWindow = false;

            ImGui::End();
        }
    }
}

// API | GPU | Input, above the tabs
static void RenderGameInfoRow(State& state, const GpuInformation& primaryGpu)
{
    // Game name is in the window title
    ScopedCard card;
    std::string line = StrFmt(LOC(UI_s_s_s_Input_s), ApiName(GameApi(state)),
                              IdentifyGpu::gameUsesDxvk()          ? LOC(UI_DXVK_2)
                              : IdentifyGpu::gameUsesVkd3dProton() ? " (VKD3D)"
                                                                   : "",
                              primaryGpu.name.c_str(), ApiUpscalerInputName(state.currentInputApiName).c_str());

    const float avail = ImGui::GetContentRegionAvail().x;
    const float textWidth = ImGui::CalcTextSize(line.c_str()).x;
    if (textWidth < avail)
    {
        ImGui::SetCursorPosX(ImGui::GetCursorPosX() + (avail - textWidth) * 0.5f);
        ImGui::TextUnformatted(line.c_str());
    }
    else
    {
        ImGui::TextWrapped("%s", line.c_str());
    }
}

void MenuCommon::RenderMainMenuWindow(RenderMenuContext& ctx)
{
    auto& state = ctx.state;
    auto config = ctx.config;
    auto& frameTime = ctx.frameTime;
    auto& frameRate = ctx.frameRate;
    auto& frameTimesCalculated = ctx.frameTimesCalculated;
    auto& menuResScale = ctx.menuResScale;
    auto& io = ctx.io;

    if (!_isVisible)
        return;

    // Check for GPU support once and reuse the result in all menu sections.
    // DXVK might call Vulkan device creation, which would destroy our objects.
    State::Instance().vulkanSkipHooks = true;
    ctx.primaryGpu =
        std::make_unique<std::decay_t<decltype(IdentifyGpu::getPrimaryGpu())>>(IdentifyGpu::getPrimaryGpu());
    State::Instance().vulkanSkipHooks = false;

    // Overlay font
    if (config->UseHQFont.value_or_default())
        ImGui::PushFontSize(std::round(menuResScale * fontSize));

    // If overlay is not visible frame needs to be inited
    if (!frameTimesCalculated)
    {
        float frameCnt = 0;
        frameTime = 0;
        for (size_t i = 299; i > 199; i--)
        {
            if (state.frameTimes[i] > 0.0)
            {
                frameTime += state.frameTimes[i];
                frameCnt++;
            }
        }

        frameTime /= frameCnt;
        frameRate = 1000.0 / frameTime;
    }

    ImGuiWindowFlags flags = 0;
    flags |= ImGuiWindowFlags_NoSavedSettings;
    flags |= ImGuiWindowFlags_NoCollapse;
    flags |= ImGuiWindowFlags_AlwaysAutoResize;

    // Fixed width, only the tab content scrolls. Keeps the menu from growing or shrinking when tabs change
    // Height can be changed by the user with the resize grip and is stored unscaled in the config
    ImGuiWindowFlags mainFlags = ImGuiWindowFlags_NoSavedSettings | ImGuiWindowFlags_NoCollapse |
                                 ImGuiWindowFlags_NoScrollbar | ImGuiWindowFlags_NoScrollWithMouse;

    const float menuWidth = std::min(680.0f * menuResScale, io.DisplaySize.x - 20.0f);
    const float maxMenuHeight = io.DisplaySize.y - 20.0f;
    const float minMenuHeight = std::min(300.0f * menuResScale, maxMenuHeight);
    const float menuHeight =
        std::clamp(config->MenuHeight.value_or(815.0f) * menuResScale, minMenuHeight, maxMenuHeight);

    ImGui::SetNextWindowSize(ImVec2(menuWidth, menuHeight), ImGuiCond_Always);
    ImGui::SetNextWindowSizeConstraints(ImVec2(menuWidth, minMenuHeight), ImVec2(menuWidth, maxMenuHeight));

    // Main menu window, fixed ID as the game name can show up later
    windowTitle = StrFmt(LOC(UI_s_s_OptiScalerMenu), BuildInfo::ProductName(),
                         state.gameName.empty() ? state.gameExe.c_str() : state.gameName.c_str());

    if (ImGui::Begin(windowTitle.c_str(), NULL, mainFlags))
    {
        // Update and config notices shown above the tabs.
        RenderMainMenuHeaderMessages(ctx);

        // Game name, API, GPU and input, above the tabs.
        RenderGameInfoRow(state, *ctx.primaryGpu);

        // Tabs on the left, tab content on the right.
        RenderMainMenuTabs(ctx);

        // Graphs and buttons, always visible regardless of the selected tab.
        RenderMainMenuFooter(ctx);

        auto winSize = ImGui::GetWindowSize();
        auto winPos = ImGui::GetWindowPos();

        // Menu was resized by the user
        if (std::abs(winSize.y - menuHeight) > 0.5f)
            config->MenuHeight = std::round(winSize.y / menuResScale);

        if (lastPosition.x < -900.0f || (lastPosition.x >= winPos.x - 1.0f && lastPosition.y >= winPos.y - 1.0f &&
                                         lastPosition.x <= winPos.x + 1.0f && lastPosition.y <= winPos.y + 1.0f))
        {
            float posX;
            float posY;

            posX = ((float) io.DisplaySize.x - winSize.x) / 2.0f;
            posY = ((float) io.DisplaySize.y - winSize.y) / 2.0f;

            // don't position menu outside of screen
            if (posX < 0.0 || posY < 0.0)
            {
                posX = 50;
                posY = 50;
            }

            ImGui::SetWindowPos(ImVec2 { posX, posY });
            lastPosition.x = posX;
            lastPosition.y = posY;
        }

        // Blurred game image behind the window, the semi transparent window background tints it
        if (_bgBlurTexture != ImTextureID_Invalid && config->MenuBlur.value_or_default() &&
            ImGui::GetStyleColorVec4(ImGuiCol_WindowBg).w < 1.0f)
        {
            const ImVec2 blurMin = ImGui::GetWindowPos();
            const ImVec2 blurMax = ImVec2(blurMin.x + ImGui::GetWindowWidth(), blurMin.y + ImGui::GetWindowHeight());
            const ImVec2 uvMul = ImVec2(_bgBlurUVScale.x / io.DisplaySize.x, _bgBlurUVScale.y / io.DisplaySize.y);

            ImGui::GetBackgroundDrawList()->AddImageRounded(
                _bgBlurTexture, blurMin, blurMax, ImVec2(blurMin.x * uvMul.x, blurMin.y * uvMul.y),
                ImVec2(blurMax.x * uvMul.x, blurMax.y * uvMul.y), IM_COL32_WHITE, ImGui::GetStyle().WindowRounding);

            _bgBlurUsed = true;
        }

        ImGui::End();
    }

    // Detached utility windows owned by the main menu.
    RenderMipmapBiasWindow(ctx, flags);
    RenderHudlessResourcesWindow(ctx, flags);

    if (config->UseHQFont.value_or_default())
        ImGui::PopFontSize();
}

void KeyUp(UINT vKey)
{
    inputMenu = vKey == Config::Instance()->ShortcutKey.value_or_default();
    inputFps = vKey == Config::Instance()->FpsShortcutKey.value_or_default();
    inputFG = vKey == Config::Instance()->FGShortcutKey.value_or_default();
    inputFpsCycle = vKey == Config::Instance()->FpsCycleShortcutKey.value_or_default();
}

bool MenuCommon::RenderMenu()
{
    if (!_isInited)
        return false;

    RenderMenuContext ctx { State::Instance(), Config::Instance(), ImGui::GetIO() };
    ctx.now = Util::MillisecondsNow();
    ctx.currentFeature = ctx.state.currentFeature;

    // Applied here, the feature isn't safe to read from the plugin's thread, kept until there is one
    if (ctx.currentFeature != nullptr && PluginApi::TakeReInitRequest())
        ReInitUpscaler();

    // 1) Collect timing and input state before any ImGui drawing.
    UpdateRenderTiming(ctx);
    UpdateMenuInputMode(ctx);
    HandleMenuShortcuts(ctx);

    // 2) Prepare one-shot notifications and start a new ImGui frame only when needed.
    UpdateVersionAndStartupNotifications(ctx);
    BeginMenuFrameIfNeeded(ctx);
    OptiInput::EndFrame(_isVisible);

    // 3) Draw lightweight overlay windows first, preserving the original order.
    ctx.menuResScale = MenuResolutionScale(ctx.io);

    // The overlays share the style with the menu, so it's scaled before them even when the menu is closed
    if (ctx.newFrame && lastMenuScale != ctx.menuResScale)
    {
        lastMenuScale = ctx.menuResScale;

        // if UI scale is changed rescale the style
        ImGuiStyle& style = ImGui::GetStyle();
        ImGuiStyle styleold = style; // Backup colors
        style = ImGuiStyle();        // IMPORTANT: ScaleAllSizes will change the original size,
                                     // so we should reset all style config

        ApplyThemeStyle();

        style.ScaleAllSizes(ctx.menuResScale);
        style.MouseCursorScale = 1.0f;
        CopyMemory(style.Colors, styleold.Colors, sizeof(style.Colors)); // Restore colors
    }

    RenderSplashWindow(ctx);
    RenderNotifications(ctx);
    UpdateFrameTimeAverages(ctx);
    RenderPerformanceOverlay(ctx);

    // 4) Draw the full settings menu last so popups and child windows keep their existing behavior.
    RenderMainMenuWindow(ctx);

    if (ctx.newFrame)
        ImGui::EndFrame();

    return ctx.newFrame;
}

void MenuCommon::Init(HWND InHwnd, bool isUWP)
{
    LocalizationManager::Instance().Init();

    // Reset shutdown flag in case of re-init
    State::Instance().isShuttingDown = false;

    HWND oldHandle = nullptr;

    if (_handle != nullptr)
    {
        oldHandle = _handle;
        LOG_DEBUG("Old Handle: {:X}, ImGui Handle: {:X}", (size_t) oldHandle,
                  (size_t) ImGui::GetMainViewport()->PlatformHandleRaw);
    }

    _handle = InHwnd;
    _isVisible = false;
    _isUWP = isUWP;
    lastPosition = { -1000.0f, -1000.0f };

    LOG_DEBUG("Handle: {0:X}", (size_t) _handle);

    // In case d3d12 wasn't yet used up to this point, try to update GPU info late here
    IdentifyGpu::updateD3d12Capabilities();

    // Setup Dear ImGui context
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGui::StyleColorsDark();

    ImGuiIO& io = ImGui::GetIO();
    (void) io;

    hasGamepad = (io.BackendFlags | ImGuiBackendFlags_HasGamepad) > 0;
    io.BackendFlags &= 30;
    io.ConfigFlags = ImGuiConfigFlags_NoMouse | ImGuiConfigFlags_NoMouseCursorChange | ImGuiConfigFlags_NoKeyboard;

    io.MouseDrawCursor = _isVisible;
    io.WantCaptureKeyboard = _isVisible;
    io.WantCaptureMouse = _isVisible;
    io.WantSetMousePos = _isVisible;

    io.IniFilename = io.LogFilename = nullptr;

    bool initResult = false;

    if (io.BackendPlatformUserData == nullptr)
    {
        if (!isUWP)
        {
            initResult = ImGui_ImplWin32_Init(InHwnd);
            LOG_DEBUG("ImGui_ImplWin32_Init result: {0}", initResult);
        }
        else
        {
            initResult = ImGui_ImplUwp_Init(InHwnd);
            ImGui_BindUwpKeyUp(KeyUp);
            LOG_DEBUG("ImGui_ImplUwp_Init result: {0}", initResult);
        }
    }

    if (io.Fonts->Fonts.empty() && Config::Instance()->UseHQFont.value_or_default())
    {
        ImFontAtlas* atlas = io.Fonts;
        atlas->Clear();

        // This automatically becomes the next default font
        ImFontConfig fontConfig;

        if (Config::Instance()->FontSize.has_value())
            fontSize = Config::Instance()->FontSize.value();

        if (Config::Instance()->TTFFontPath.has_value())
        {
            io.FontDefault =
                atlas->AddFontFromFileTTF(wstring_to_string(Config::Instance()->TTFFontPath.value()).c_str(), fontSize,
                                          &fontConfig, io.Fonts->GetGlyphRangesDefault());
        }
        else
        {
            io.FontDefault = atlas->AddFontFromMemoryCompressedBase85TTF(hack_compressed_compressed_data_base85,
                                                                         fontSize, &fontConfig);
        
        // FIX-CJK: 中文字体加载（覆盖 Latin + CJK）
        {
            static const ImWchar cjk_ranges[] = {
                0x0020, 0x00FF, 0x2000, 0x206F, 0x3000, 0x30FF,
                0xFF00, 0xFFEF, 0x4E00, 0x9FFF, 0,
            };
            auto dllDir = Util::DllPath().parent_path();
            std::vector<std::filesystem::path> fontPaths = {
                dllDir / "font" / "SarasaGothicSC-Regular.ttf",
                dllDir / "font" / "wqy-microhei.ttc",
                dllDir / "font" / "NotoSansSC-Regular.ttf",
            };
            for (const auto& cjkPath : fontPaths) {
                if (std::filesystem::exists(cjkPath)) {
                    ImFontConfig cfg{};
                    ImFont* cjkFont = atlas->AddFontFromFileTTF(wstring_to_string(cjkPath.wstring()).c_str(),
                                                                fontSize, &cfg, cjk_ranges);
                    if (cjkFont) {
                        io.FontDefault = cjkFont;
                        LOG_INFO("CHS: CJK font loaded from {}", cjkPath.filename().string());
                    }
                    break;
                }
            }
        }
}
    }

    if (!Config::Instance()->OverlayMenu.value_or_default())
    {
        _hdrTonemapApplied = false;
    }

    DWORD hwndPid = 0;
    DWORD hwndTid = GetWindowThreadProcessId(_handle, &hwndPid);

    LOG_DEBUG("HWND: {:X}, IsWindow: {}, HWND PID: {}, Current PID: {}, HWND TID: {}, Current TID: {}",
              (ULONG64) _handle, IsWindow(_handle), hwndPid, GetCurrentProcessId(), hwndTid, GetCurrentThreadId());

    OptiInput::Initialize(_handle, isUWP);

    ApplyThemeStyle();
    _isInited = true;
}

void MenuCommon::Shutdown()
{
    if (!MenuCommon::_isInited)
        return;

    // if (_oWndProc != nullptr)
    //{
    //     auto handle = (HWND) ImGui::GetMainViewport()->PlatformHandleRaw;
    //     SetLastError(0);
    //     auto restoreResult = SetWindowLongPtr(handle, GWLP_WNDPROC, (LONG_PTR) _oWndProc);
    //     auto error = GetLastError();

    //    if (restoreResult == 0 && error != 0)
    //    {
    //        LOG_ERROR("Failed to restore old WndProc. Error: {:X}", error);
    //    }

    //    _oWndProc = nullptr;
    //}

    if (!_isUWP)
        ImGui_ImplWin32_Shutdown();
    else
        ImGui_ImplUwp_Shutdown();

    ImGui::DestroyContext();

    // The font was in the destroyed context's atlas
    gOverlayFont = nullptr;
    gOverlayFontName.clear();
    gOverlayLetterSpacing = 0.0f;

    _handle = nullptr;
    _isInited = false;
    _isVisible = false;
}

void MenuCommon::HideMenu()
{
    if (!_isVisible)
        return;

    _isVisible = false;

    ImGuiIO& io = ImGui::GetIO();
    (void) io;

    _showMipmapCalcWindow = false;
    _showHudlessWindow = false;

    io.MouseDrawCursor = _isVisible;
    io.WantCaptureKeyboard = _isVisible;
    io.WantCaptureMouse = _isVisible;
}
