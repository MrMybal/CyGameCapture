// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
#include "Automation.hpp"
#include "Addon/Config.hpp"
#include "Addon/DeviceContext.hpp"
#include "Addon/Log.hpp"
#include "ResourceTracker/FormatUtils.hpp"
#include <nlohmann/json.hpp>
using Json = nlohmann::json;
namespace cygc
{
namespace
{
Json Schema(Json fields = {}, Json required = Json::array())
{
    return {{"type", "object"},
            {"properties", fields.is_null() ? Json::object() : fields},
            {"required", required},
            {"additionalProperties", false}};
}
Json Id()
{
    return {{"type", "integer"}, {"minimum", 1}, {"maximum", 4294967295ULL}};
}
Json Number(int low, int high)
{
    return {{"type", "integer"}, {"minimum", low}, {"maximum", high}};
}
Json Buffer(const TrackedResource &item)
{
    return {{"id", item.id},
            {"width", item.Width()},
            {"height", item.Height()},
            {"format", FormatName(item.Format())},
            {"writes", item.last.writes},
            {"backbuffer", item.is_backbuffer}};
}
} // namespace
Automation::Automation(DeviceContext &device) : device_(device)
{
    Json options = {
        {"component", "reshade"},
        {"allow_mutations", config::GetBool("AutomationAllowMutations", false)},
        {"compact", config::GetBool("AutomationCompact", false)},
        {"notifications", config::GetBool("AutomationNotifications", true)},
        {"cache_ttl_ms", std::max(0, config::GetInt("AutomationCacheTTL", 1000))},
        {"performance", config::GetBool("AutomationPerformance", false)},
        {"result_memory_bytes", std::max(16384, config::GetInt("AutomationResultMemoryBytes", 8 * 1024 * 1024))},
        {"result_ttl_ms", std::max(1000, config::GetInt("AutomationResultTTL", 300000))}};
    host_ = cygc_automation_create(options.dump().c_str());
    if (!host_)
    {
        log::Error("CyMCPSDK: %s", cygc_automation_error());
        return;
    }
    auto add = [&](const char *name, const char *description, bool read, Json input) {
        Json spec = {{"name", name}, {"description", description}, {"readOnly", read}, {"inputSchema", input}};
        if (!cygc_automation_add(host_, spec.dump().c_str(), Invoke, this))
            log::Error("CyMCPSDK operation: %s", cygc_automation_error());
    };
    const Json page = {{"offset", Number(0, 1000000)}, {"limit", Number(1, 128)}};
    add("app.status", "Read the live renderer, frame, selection and capture state before choosing a buffer.", true,
        Schema());
    add("buffers.list",
        "List live render targets with pagination and optional size/format filters; IDs belong to this instance only.",
        true,
        Schema({{"offset", Number(0, 1000000)},
                {"limit", Number(1, 128)},
                {"width", Number(1, 32768)},
                {"height", Number(1, 32768)},
                {"format", {{"type", "string"}, {"maxLength", 100}}}}));
    add("buffers.inspect", "Read one live buffer's metadata; use buffers.list to discover its ID.", true,
        Schema({{"id", Id()}}, {"id"}));
    add("buffers.select", "Select a live buffer in the inspector. Requires remote control enabled in the application.",
        false, Schema({{"id", Id()}}, {"id"}));
    add("buffers.moment",
        "Select the intermediate write moment for inspection; zero means end of frame. D3D11 only for nonzero moments.",
        false, Schema({{"moment", Number(0, 1000000)}}, {"moment"}));
    add("captures.list", "Read active streams, exact sender names, frame counters and output formats.", true, Schema());
    add("captures.start",
        "Capture an existing live buffer as colour, depth or HUD, in the primary or an extra slot. Does not choose a "
        "buffer automatically.",
        false,
        Schema({{"id", Id()},
                {"extra", {{"type", "boolean"}}},
                {"transform", {{"type", "string"}, {"enum", {"colour", "depth", "hud"}}}},
                {"secondary_id", Number(0, 1000000)},
                {"moment", Number(0, 1000000)},
                {"secondary_moment", Number(0, 1000000)}},
               {"id"}));
    add("captures.stop", "Stop the specified zero-based stream slot, or all streams when all=true.", false,
        Schema({{"slot", Number(0, 255)}, {"all", {{"type", "boolean"}}}}));
    add("captures.alpha",
        "Choose whether one stream preserves source alpha; HUD masks are always retained by the application.", false,
        Schema({{"slot", Number(0, 255)}, {"preserve", {{"type", "boolean"}}}}, {"slot", "preserve"}));
    add("view.set", "Choose the original game view or the selected-buffer in-game view (the same action as F10).",
        false, Schema({{"selected", {{"type", "boolean"}}}}, {"selected"}));
    add("timeline.list", "Read paginated write events for the last presented frame; refresh after a scene change.",
        true, Schema(page));
    add("inspector.filters",
        "Change inspector filters for colour/depth targets, small resources and persistent buffers.", false,
        Schema({{"depth", {{"type", "boolean"}}},
                {"colour", {{"type", "boolean"}}},
                {"small", {{"type", "boolean"}}},
                {"persistent", {{"type", "boolean"}}}}));
    add("assistant.cancel", "Cancel the current assistant generation without applying any pending proposal.", false,
        Schema());
    log::Info("CyMCPSDK 0.7.0 connection: %s", Connection().c_str());
}
Automation::~Automation()
{
    cygc_automation_destroy(host_);
}
void Automation::Pump()
{
    cygc_automation_pump(host_);
}
void Automation::AllowMutations(bool allow)
{
    cygc_automation_allow_mutations(host_, allow ? 1 : 0);
}
std::string Automation::Connection() const
{
    return cygc_automation_connection(host_);
}
const char *__cdecl Automation::Invoke(void *owner, const char *operation, const char *input)
{
    auto &self = *static_cast<Automation *>(owner);
    auto &d = self.device_;
    try
    {
        const auto args = Json::parse(input);
        const std::string name = operation;
        Json result = Json::object();
        if (name == "app.status")
            result = {{"renderer", d.backend.api_name},
                      {"frame", d.present_frame_index},
                      {"selected_id", d.inspector.selected_id},
                      {"moment", d.inspector.moment},
                      {"selected_view", d.view_mode == ViewMode::SelectedBuffer},
                      {"sdk", "0.7.0"},
                      {"remote_control", config::GetBool("AutomationAllowMutations", false)}};
        else if (name == "buffers.list")
        {
            auto list = Json::array();
            std::vector<TrackedResource> matches;
            for (const auto &item : d.tracker->Snapshot())
            {
                if (args.contains("width") && item.Width() != args["width"].get<uint32_t>())
                    continue;
                if (args.contains("height") && item.Height() != args["height"].get<uint32_t>())
                    continue;
                if (args.contains("format") && args["format"] != FormatName(item.Format()))
                    continue;
                matches.push_back(item);
            }
            size_t offset = args.value("offset", 0U), limit = args.value("limit", 32U);
            for (size_t i = offset; i < matches.size() && i < offset + limit; ++i)
                list.push_back(Buffer(matches[i]));
            result = {
                {"buffers", list},
                {"total", matches.size()},
                {"offset", offset},
                {"next_offset", offset + list.size() < matches.size() ? Json(offset + list.size()) : Json(nullptr)}};
        }
        else if (name == "buffers.inspect" || name == "buffers.select")
        {
            TrackedResource item;
            if (!d.tracker->Get(args.at("id"), item))
                throw std::runtime_error("Buffer no longer exists");
            if (name == "buffers.select")
            {
                d.inspector.selected_id = item.id;
                d.inspector.moment = 0;
            }
            result = Buffer(item);
        }
        else if (name == "buffers.moment")
        {
            auto moment = args.at("moment").get<uint32_t>();
            if (moment && d.gpu_device->get_api() != reshade::api::device_api::d3d11)
                throw std::runtime_error("Intermediate moments require D3D11");
            d.inspector.moment = moment;
            result = {{"moment", moment}};
        }
        else if (name == "captures.list")
        {
            auto streams = Json::array();
            for (size_t slot = 0; slot < d.captures.size(); ++slot)
            {
                auto s = d.captures[slot]->GetStatus();
                streams.push_back({{"slot", slot},
                                   {"state", CaptureStateName(s.state)},
                                   {"source_id", s.source_id},
                                   {"sender", s.sender_name},
                                   {"width", s.width},
                                   {"height", s.height},
                                   {"frames_sent", s.frames_sent},
                                   {"frames_skipped", s.frames_skipped},
                                   {"message", s.message}});
            }
            result = {{"streams", streams}};
        }
        else if (name == "captures.start")
        {
            std::string error;
            const auto transform = args.value("transform", std::string("colour"));
            const auto kind = transform == "depth" ? StreamTransform::DepthLinear
                              : transform == "hud" ? StreamTransform::HudDifference
                                                   : StreamTransform::Copy;
            const auto id = args.at("id").get<uint32_t>(), secondary = args.value("secondary_id", 0U),
                       moment = args.value("moment", 0U), secondary_moment = args.value("secondary_moment", 0U);
            const bool success = args.value("extra", false)
                                     ? d.StartCaptureInNewSlot(id, &error, kind, secondary, moment, secondary_moment)
                                     : d.StartCapture(id, &error, kind, secondary, moment, secondary_moment);
            if (!success)
                throw std::runtime_error(error);
            result = {{"started", true}, {"source_id", id}};
        }
        else if (name == "captures.stop")
        {
            if (args.value("all", false))
                d.StopAllCaptures();
            else
            {
                const auto slot = args.value("slot", 0U);
                if (slot >= d.captures.size())
                    throw std::runtime_error("Unknown stream slot");
                d.StopCapture(slot);
            }
            result = {{"stopped", true}};
        }
        else if (name == "captures.alpha")
        {
            auto slot = args.at("slot").get<size_t>();
            if (slot >= d.captures.size())
                throw std::runtime_error("Unknown stream slot");
            d.captures[slot]->SetPreserveSourceAlpha(args.at("preserve"));
            result = {{"slot", slot}, {"preserve", d.captures[slot]->PreserveSourceAlpha()}};
        }
        else if (name == "view.set")
        {
            d.view_mode = args.at("selected").get<bool>() ? ViewMode::SelectedBuffer : ViewMode::Game;
            result = {{"selected", d.view_mode == ViewMode::SelectedBuffer}};
        }
        else if (name == "timeline.list")
        {
            auto all = d.tracker->TimelineSnapshot();
            auto entries = Json::array();
            size_t offset = args.value("offset", 0U), limit = args.value("limit", 64U);
            for (size_t i = offset; i < all.size() && i < offset + limit; ++i)
                entries.push_back({{"event", all[i].event_index},
                                   {"buffer", all[i].resource_id},
                                   {"count", all[i].count},
                                   {"write", all[i].is_write},
                                   {"kind", static_cast<int>(all[i].kind)}});
            result = {
                {"events", entries},
                {"total", all.size()},
                {"next_offset", offset + entries.size() < all.size() ? Json(offset + entries.size()) : Json(nullptr)}};
        }
        else if (name == "inspector.filters")
        {
            auto &f = d.inspector.filter;
            if (args.contains("depth"))
                f.depth = args["depth"];
            if (args.contains("colour"))
                f.color_targets = args["colour"];
            if (args.contains("small"))
                f.small_resources = args["small"];
            if (args.contains("persistent"))
                f.persistent_only = args["persistent"];
            d.inspector.Refresh(*d.tracker);
            result = {{"depth", f.depth},
                      {"colour", f.color_targets},
                      {"small", f.small_resources},
                      {"persistent", f.persistent_only}};
        }
        else if (name == "assistant.cancel")
        {
            d.ai->Cancel();
            result = {{"cancelled", true}};
        }
        else
            throw std::runtime_error("Unknown operation");
        self.reply_ = result.dump();
    }
    catch (const std::exception &error)
    {
        self.reply_ = Json({{"error", {{"code", "OperationFailed"}, {"message", error.what()}}}}).dump();
    }
    return self.reply_.c_str();
}
} // namespace cygc
