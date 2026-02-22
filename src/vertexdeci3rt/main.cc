//
// Copyright (C) 2026 PHTNC<>.
// Licensed under LGPLv3.0+
//

#include <vertexdeci3rt/config.hh>
#include <vertexdeci3rt/init.hh>

#include <sdk/api.h>
#include <sdk/ui.h>

#include <algorithm>
#include <array>
#include <format>
#include <mutex>
#include <ranges>
#include <span>
#include <string_view>
#include <vector>

Runtime* g_pluginRuntime {};

namespace
{
    constexpr void copy_to(std::span<char> dest, std::string_view src) noexcept
    {
        const auto count = std::min(src.size(), dest.size() - 1);
        std::ranges::copy_n(src.data(), count, dest.data());
        dest[count] = '\0';
    }

    std::once_flag g_flag {};

    constexpr auto PANEL_ID         = "deci3_config";
    constexpr auto FIELD_TARGET     = "target_select";
    constexpr auto FIELD_INIT_TMAPI = "target_init_tmapi";
    constexpr auto FIELD_CONNECT    = "target_connect";
    constexpr auto FIELD_DISCONNECT = "target_disconnect";

    constexpr std::uint32_t MAX_DISCOVERED_TARGETS = 64;

    struct TargetEntry final
    {
        HTARGET handle {};
        std::array<char, VERTEX_UI_MAX_OPTION_LABEL_LENGTH> name {};
    };

    std::vector<TargetEntry> g_discoveredTargets {};

    int __stdcall enumerate_targets_callback(const HTARGET hTarget, void*)
    {
        if (g_discoveredTargets.size() >= MAX_DISCOVERED_TARGETS)
        {
            return 1;
        }

        SNPS3TargetInfo info {};
        info.nFlags  = SN_TI_TARGETID;
        info.hTarget = hTarget;

        if (SN_SUCCEEDED(SNPS3GetTargetInfo(&info)))
        {
            TargetEntry entry {};
            entry.handle = hTarget;

            if ((info.nFlags & SN_TI_NAME) && info.pszName)
            {
                copy_to(entry.name, info.pszName);
            }
            else
            {
                auto [out, _] = std::format_to_n(entry.name.data(), entry.name.size() - 1, "Target {}", static_cast<int>(hTarget));
                *out = '\0';
            }

            g_discoveredTargets.push_back(std::move(entry));
        }

        return 0;
    }

    void VERTEX_API on_target_apply(const char* fieldId, const UIValue* value, [[maybe_unused]] void* userData)
    {
        if (!fieldId || !g_pluginRuntime)
        {
            return;
        }

        auto* ctx = DECI3::context();
        if (!ctx)
        {
            g_pluginRuntime->vertex_log_error("DECI3 context not initialized.");
            return;
        }

        const std::string_view field { fieldId };

        if (field == FIELD_INIT_TMAPI)
        {
            const SNRESULT result = SNPS3InitTargetComms();
            if (SN_FAILED(result))
            {
                g_pluginRuntime->vertex_log_error("Failed to initialize TMAPI target comms.");
                return;
            }
            g_pluginRuntime->vertex_log_info("TMAPI target comms initialized.");
            return;
        }

        if (field == FIELD_CONNECT)
        {
            const SNRESULT result = SNPS3Connect(ctx->module.targetNumber, nullptr);
            if (SN_FAILED(result))
            {
                g_pluginRuntime->vertex_log_error("Failed to connect to target (handle %d).", static_cast<int>(ctx->module.targetNumber));
                return;
            }
            g_pluginRuntime->vertex_log_info("Connected to target (handle %d).", static_cast<int>(ctx->module.targetNumber));
            return;
        }

        if (field == FIELD_DISCONNECT)
        {
            const SNRESULT result = SNPS3Disconnect(ctx->module.targetNumber);
            if (SN_FAILED(result))
            {
                g_pluginRuntime->vertex_log_error("Failed to disconnect from target (handle %d).", static_cast<int>(ctx->module.targetNumber));
                return;
            }
            g_pluginRuntime->vertex_log_info("Disconnected from target (handle %d).", static_cast<int>(ctx->module.targetNumber));
            return;
        }

        if (field == FIELD_TARGET && value)
        {
            const std::string_view selectedLabel { value->stringValue };

            for (const auto& [handle, name] : g_discoveredTargets)
            {
                if (selectedLabel == std::string_view { name.data() })
                {
                    ctx->module.targetNumber = handle;
                    g_pluginRuntime->vertex_log_info("Selected target: %s (handle %d)", name.data(), static_cast<int>(handle));
                    return;
                }
            }

            g_pluginRuntime->vertex_log_warn("Target not found: %s", value->stringValue);
        }
    }

    void VERTEX_API on_target_reset([[maybe_unused]] void* userData)
    {
        if (g_pluginRuntime)
        {
            g_pluginRuntime->vertex_log_info("DECI3 target configuration reset.");
        }
    }

    StatusCode register_target_ui_panel()
    {
        g_discoveredTargets.clear();

        const SNRESULT enumResult = SNPS3EnumerateTargetsEx(enumerate_targets_callback, nullptr);
        if (SN_FAILED(enumResult))
        {
            g_pluginRuntime->vertex_log_error("Failed to enumerate targets.");
            return StatusCode::STATUS_ERROR_GENERAL;
        }

        g_pluginRuntime->vertex_log_info("Discovered %d target(s).", static_cast<int>(g_discoveredTargets.size()));

        auto options = g_discoveredTargets | std::views::transform([](const TargetEntry& entry) {
            UIOption opt {};
            copy_to(opt.label, std::string_view { entry.name.data() });
            copy_to(opt.value.stringValue, std::string_view { entry.name.data() });
            return opt;
        }) | std::ranges::to<std::vector>();

        UIField targetField {};
        copy_to(targetField.fieldId, FIELD_TARGET);
        copy_to(targetField.label, "PS3 Target");
        copy_to(targetField.tooltip, "Select a PS3 development kit target discovered via SNPS3EnumerateTargetsEx");
        targetField.type        = VERTEX_UI_FIELD_DROPDOWN;
        targetField.required    = 1;
        targetField.options     = options.data();
        targetField.optionCount = static_cast<uint32_t>(options.size());

        if (!g_discoveredTargets.empty())
        {
            copy_to(targetField.defaultValue.stringValue, std::string_view { g_discoveredTargets[0].name.data() });
        }

        UIField initTmapiField {};
        copy_to(initTmapiField.fieldId, FIELD_INIT_TMAPI);
        copy_to(initTmapiField.label, "Initialize TMAPI");
        copy_to(initTmapiField.tooltip, "Initialize TMAPI target communications via SNPS3InitTargetComms");
        initTmapiField.type              = VERTEX_UI_FIELD_BUTTON;
        initTmapiField.layoutOrientation = VERTEX_UI_LAYOUT_HORIZONTAL;

        UIField connectField {};
        copy_to(connectField.fieldId, FIELD_CONNECT);
        copy_to(connectField.label, "Connect");
        copy_to(connectField.tooltip, "Connect to the selected PS3 target");
        connectField.type              = VERTEX_UI_FIELD_BUTTON;
        connectField.layoutOrientation = VERTEX_UI_LAYOUT_HORIZONTAL;

        UIField disconnectField {};
        copy_to(disconnectField.fieldId, FIELD_DISCONNECT);
        copy_to(disconnectField.label, "Disconnect");
        copy_to(disconnectField.tooltip, "Disconnect from the selected PS3 target");
        disconnectField.type              = VERTEX_UI_FIELD_BUTTON;
        disconnectField.layoutOrientation = VERTEX_UI_LAYOUT_HORIZONTAL;

        std::array<UIField, 4> fields { targetField, initTmapiField, connectField, disconnectField };

        UISection section {};
        copy_to(section.title, "Target Selection");
        section.fields     = fields.data();
        section.fieldCount = static_cast<uint32_t>(fields.size());

        UIPanel panel {};
        copy_to(panel.panelId, PANEL_ID);
        copy_to(panel.title, "DECI3 Configuration");
        panel.onApply      = on_target_apply;
        panel.onReset      = on_target_reset;
        panel.userData     = nullptr;
        panel.sections     = &section;
        panel.sectionCount = 1;

        if (!g_pluginRuntime->vertex_register_ui_panel)
        {
            g_pluginRuntime->vertex_log_warn("UI panel registration not available in this runtime version.");
            return StatusCode::STATUS_OK;
        }

        return g_pluginRuntime->vertex_register_ui_panel(&panel);
    }
}

namespace
{
    constexpr PluginInformation g_pluginInformation = {
        .pluginName        = "Vertex DECI3 Runtime",
        .pluginVersion     = "0.1",
        .pluginDescription = "Implements functionality to communicate with Playstation 3 console through the DECI3 protocol",
        .pluginAuthor      = "PHTNC<>",
        .apiVersion        = VERTEX_TARGET_API_VERSION(VERTEX_MAJOR_API_VERSION, VERTEX_MINOR_API_VERSION, VERTEX_PATCH_API_VERSION),
        .featureCapability = VERTEX_FEATURE_RUN_MODE_SINGLE_THREADED | VERTEX_FEATURE_DEBUGGER_DEPENDENT
    };
}

extern "C" VERTEX_EXPORT StatusCode VERTEX_API vertex_init(PluginInformation* pluginInfo, Runtime* runtime, const bool singleThreadModeInit)
{
    std::call_once(g_flag, [&]()
    {
        *pluginInfo      = g_pluginInformation;
        g_pluginRuntime  = runtime;

        g_pluginRuntime->vertex_log_info("Initializing Vertex Deci3 Runtime.");
    });

    if (singleThreadModeInit)
    {
        g_pluginRuntime->vertex_log_info("Deci3 runtime running in single-threaded mode.");

        if (SN_FAILED(DECI3::initialize_communications()))
        {
            return StatusCode::STATUS_ERROR_GENERAL;
        }

        if (auto* ctx = DECI3::context())
        {
            const auto uiStatus = register_target_ui_panel();
            if (uiStatus != StatusCode::STATUS_OK)
            {
                g_pluginRuntime->vertex_log_warn("UI panel registration returned: %d", static_cast<int>(uiStatus));
            }

            if (!g_discoveredTargets.empty())
            {
                ctx->module.targetNumber = g_discoveredTargets[0].handle;
                g_pluginRuntime->vertex_log_info("Default target set: %s (handle %d)",
                    g_discoveredTargets[0].name.data(), static_cast<int>(ctx->module.targetNumber));
            }
            else
            {
                g_pluginRuntime->vertex_log_warn("No targets discovered. Use SNPS3PickTarget fallback.");
                const SNRESULT result = SNPS3PickTarget(nullptr, &ctx->module.targetNumber);
                if (SN_FAILED(result))
                {
                    g_pluginRuntime->vertex_log_error("Failed to pick target.");
                    return StatusCode::STATUS_ERROR_GENERAL;
                }
            }
        }
    }

    return StatusCode::STATUS_OK;
}

extern "C" VERTEX_EXPORT StatusCode VERTEX_API vertex_exit()
{
    g_discoveredTargets.clear();
    DECI3::destroy_context();
    return StatusCode::STATUS_OK;
}

extern "C" VERTEX_EXPORT StatusCode VERTEX_API vertex_event([[maybe_unused]] const Event event, [[maybe_unused]] const void* data)
{
    return StatusCode::STATUS_OK;
}
