// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureAI — reading the pictures the add-on shared, and writing them as PNG files.
#pragma once

#include <cstdint>
#include <string>
#include <vector>

namespace cygc
{
	struct AtlasLayout
	{
		uint32_t width = 0;
		uint32_t height = 0;
		uint32_t tile_width = 0;
		uint32_t tile_height = 0;
		uint32_t columns = 1;
	};

	struct TileRequest
	{
		uint32_t index = 0;       // position in the atlas
		uint32_t buffer = 0;      // resource id in the game
		uint32_t moment = 0;      // 0 = end of the frame
		std::string label;
	};

	/** Opens the atlas by its share handle and writes `picture_NN.png` for every tile, in `folder`. */
	bool ExportTiles(uint64_t share_handle, const AtlasLayout &layout, const std::vector<TileRequest> &tiles,
		const std::wstring &folder, std::vector<std::wstring> &out_paths, std::string &error);
}
