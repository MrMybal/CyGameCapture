// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: MIT
#pragma once
#include <cymcp/core.hpp>
// Transport-neutral result contracts, shared by every frontend through the host registry.
inline cymcp::Json OutputSchema(const std::string &component, const std::string &name)
{
    using J = cymcp::Json;
    auto type = [](const char *t) { return J{{"type", t}}; };
    const auto s = type("string"), n = type("integer"), b = type("boolean");
    auto object = [](J fields) {
        J required = J::array();
        for (auto it = fields.begin(); it != fields.end(); ++it)
            required.push_back(it.key());
        return J{{"type", "object"}, {"properties", fields}, {"required", required}};
    };
    auto array = [](J item) { return J{{"type", "array"}, {"items", item}}; };
    auto page = [&](const char *key, J row) {
        return object({{key, array(row)}, {"total", n}, {"next_offset", {{"type", {"integer", "null"}}}}});
    };
    auto buffer = object({{"id", n}, {"width", n}, {"height", n}, {"format", s}, {"writes", n}, {"backbuffer", b}});
    if (component == "reshade")
    {
        if (name == "app.status")
            return object({{"renderer", s},
                           {"frame", n},
                           {"selected_id", n},
                           {"moment", n},
                           {"selected_view", b},
                           {"sdk", s},
                           {"remote_control", b}});
        if (name == "buffers.list")
            return page("buffers", buffer);
        if (name == "buffers.select" || name == "buffers.inspect")
            return buffer;
        if (name == "buffers.moment")
            return object({{"moment", n}});
        if (name == "captures.list")
            return object({{"streams", array(object({{"slot", n},
                                                     {"state", s},
                                                     {"source_id", n},
                                                     {"sender", s},
                                                     {"width", n},
                                                     {"height", n},
                                                     {"frames_sent", n},
                                                     {"frames_skipped", n},
                                                     {"message", s}}))}});
        if (name == "captures.start")
            return object({{"started", b}, {"source_id", n}});
        if (name == "captures.stop")
            return object({{"stopped", b}});
        if (name == "captures.alpha")
            return object({{"slot", n}, {"preserve", b}});
        if (name == "view.set")
            return object({{"selected", b}});
        if (name == "timeline.list")
            return page("events", object({{"event", n}, {"buffer", n}, {"count", n}, {"write", b}, {"kind", n}}));
        if (name == "inspector.filters")
            return object({{"depth", b}, {"colour", b}, {"small", b}, {"persistent", b}});
        if (name == "assistant.cancel")
            return object({{"cancelled", b}});
    }
    if (component == "obs-source")
    {
        if (name == "source.status")
            return object({{"name", s},
                           {"connected", b},
                           {"requested_sender", s},
                           {"sender", s},
                           {"width", n},
                           {"height", n},
                           {"sync_frame", n},
                           {"error", s}});
        if (name == "senders.list")
            return page("senders", object({{"name", s}, {"width", n}, {"height", n}, {"format", n}}));
        if (name == "source.sender")
            return object({{"sender", s}});
        if (name == "recording.start")
            return object({{"started_sources", n}});
        if (name == "recording.stop")
            return object({{"stopped_sources", n}});
    }
    if (component == "unreal")
    {
        if (name == "app.status")
            return object(
                {{"component", s}, {"project", s}, {"renderer", s}, {"spout_supported", b}, {"active_streams", n}});
        if (name == "senders.list")
            return page("senders", object({{"name", s}, {"width", n}, {"height", n}, {"format", s}}));
        if (name == "components.list")
            return page("components", object({{"path", s}, {"type", s}}));
        // Stream fields are the plugin's reflected public FCyGameCaptureStreamStats structure.
        if (name == "streams.list")
            return page("streams", object({{"direction", s}}));
        if (name == "streams.stop_all" || name == "sender.stop" || name == "receiver.stop")
            return object({{"stopped", b}});
        if (name == "sender.start" || name == "receiver.start")
            return object({{"started", b}});
        if (name == "sender.name")
            return object({{"name", s}});
        if (name == "receiver.sender")
            return object({{"sender", s}});
    }
    return type("object");
}
