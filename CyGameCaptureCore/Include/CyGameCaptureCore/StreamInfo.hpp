// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureCore — description of a CyGameCapture video stream.
// Today only one stream exists ("Color": the manually selected clean colour buffer).
// The enumeration lives here so sender naming, profiles and the future OBS plugin
// agree on stream identifiers without inventing them twice.
#pragma once

#include <cstdint>

namespace cygc
{
	enum class StreamKind : uint32_t
	{
		Color = 0,   // Selected colour render target (V1: the only stream)
		Final,       // Final back buffer (with HUD)  — future
		Depth,       // Depth buffer                  — future
		Normal,      // Normals                       — future
		UI,          // UI only                       — future
		Mask,        // Mask                          — future
	};

	inline const char *StreamKindName(StreamKind kind)
	{
		switch (kind)
		{
		case StreamKind::Color:  return "Color";
		case StreamKind::Final:  return "Final";
		case StreamKind::Depth:  return "Depth";
		case StreamKind::Normal: return "Normal";
		case StreamKind::UI:     return "UI";
		case StreamKind::Mask:   return "Mask";
		}
		return "Unknown";
	}

	// Metadata the sender knows about the stream it produces. Not transported over
	// Spout today (Spout only carries width/height/format/handle); reserved for the
	// future small IPC metadata channel between CyGameCaptureRS and CyGameCaptureOBS.
	struct StreamInfo
	{
		StreamKind kind = StreamKind::Color;
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t dxgi_format = 0;      // DXGI_FORMAT value of the shared texture
		uint32_t source_resource_id = 0;
		float fps = 0.0f;
	};
}
