// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later
//
// CyGameCaptureRS — ReShade overlay.
// imgui.h must be included before reshade.hpp so that reshade_overlay.hpp redirects the
// ImGui namespace to the function table exported by ReShade.
#include <imgui.h>
#include <reshade.hpp>

#include "Overlay.hpp"
#include "Addon/Config.hpp"
#include "Addon/DeviceContext.hpp"
#include "Addon/Log.hpp"
#include "ResourceTracker/FormatUtils.hpp"
#include "Capture/SpoutSender.hpp"
#include "UI/Localization.hpp"

#include <CyGameCaptureCore/Version.hpp>

#include <Windows.h>
#include <algorithm>
#include <cstdio>
#include <string>

using namespace reshade::api;

namespace cygc
{
	namespace
	{
		constexpr float kListHeight = 160.0f;

		std::string ResourceLabel(const TrackedResource &res)
		{
			char buffer[256];
			snprintf(buffer, sizeof(buffer), "#%03u  %ux%u  %-10s  W:%-4u %s%s%s%s",
				res.id, res.Width(), res.Height(), FormatShortName(res.Format()), res.last.writes,
				res.is_backbuffer ? "[BB] " : "",
				(res.IsDepthStencil() || IsDepthStencilFormat(res.Format())) ? "[Depth] " : "",
				res.IsMultisampled() ? "[MSAA] " : "",
				res.IsUnorderedAccess() && !res.IsColorTarget() ? "[UAV] " : "");
			return buffer;
		}

		void DrawStatusLine(const char *label, const char *value)
		{
			ImGui::TextUnformatted(label);
			ImGui::SameLine(160.0f);
			ImGui::TextUnformatted(value);
		}

		void DrawPreviewImage(DeviceContext &ctx, float max_height)
		{
			PreviewManager &preview = *ctx.preview;
			if (!preview.HasImage() || preview.Width() == 0 || preview.Height() == 0)
			{
				ImGui::TextDisabled(preview.LastError().empty() ? TR("No preview yet") : preview.LastError().c_str());
				return;
			}

			const float aspect = static_cast<float>(preview.Width()) / static_cast<float>(preview.Height());
			ImVec2 size = ImGui::GetContentRegionAvail();
			size.x = std::max(size.x, 64.0f);
			size.y = size.x / aspect;
			if (size.y > max_height)
			{
				size.y = max_height;
				size.x = size.y * aspect;
			}
			ImGui::Image(static_cast<ImTextureID>(preview.View().handle), size);
		}

		const char *KeyName(uint32_t vk)
		{
			static char buffer[32];
			if (vk >= VK_F1 && vk <= VK_F24)
			{
				snprintf(buffer, sizeof(buffer), "F%u", vk - VK_F1 + 1);
				return buffer;
			}
			if ((vk >= '0' && vk <= '9') || (vk >= 'A' && vk <= 'Z'))
			{
				snprintf(buffer, sizeof(buffer), "%c", static_cast<char>(vk));
				return buffer;
			}
			switch (vk)
			{
			case VK_HOME: return "Home";
			case VK_END: return "End";
			case VK_INSERT: return "Insert";
			case VK_DELETE: return "Delete";
			case VK_PAUSE: return "Pause";
			case VK_SCROLL: return "Scroll Lock";
			case VK_TAB: return "Tab";
			default:
				snprintf(buffer, sizeof(buffer), TR("VK 0x%02X"), vk);
				return buffer;
			}
		}

		void DrawOutputImage(reshade::api::resource_view view, uint32_t width, uint32_t height, float max_height)
		{
			if (view == 0 || width == 0 || height == 0)
				return;
			const ImVec2 avail = ImGui::GetContentRegionAvail();
			const float aspect = static_cast<float>(width) / static_cast<float>(height);
			ImVec2 size(std::max(avail.x, 64.0f), 0.0f);
			size.y = size.x / aspect;
			if (size.y > max_height)
			{
				size.y = max_height;
				size.x = size.y * aspect;
			}
			ImGui::Image(static_cast<ImTextureID>(view.handle), size);
		}

		// ------------------------------------------------------------------ Capture tab
		void DrawViewModeControls(DeviceContext &ctx)
		{
			bool show_selected = ctx.view_mode == ViewMode::SelectedBuffer;
			if (ImGui::Checkbox(TR("Show selected buffer in game (play without HUD / UI)"), &show_selected))
				ctx.view_mode = show_selected ? ViewMode::SelectedBuffer : ViewMode::Game;
			if (ctx.view_toggle_key != 0)
			{
				ImGui::SameLine();
				ImGui::TextDisabled(TR("(hotkey: %s, [CYGAMECAPTURE] ViewToggleKey in ReShade.ini)"), KeyName(ctx.view_toggle_key));
			}
			if (show_selected && !ctx.view_message.empty())
				ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "%s", TR(ctx.view_message.c_str()));
		}

		void DrawCaptureTab(DeviceContext &ctx)
		{
			char buffer[160];

			DrawStatusLine(TR("Renderer:"), ctx.backend.api_name);
			if (!ctx.backend.spout_capture)
				ImGui::TextWrapped("%s", ctx.backend.reason.c_str());
			DrawViewModeControls(ctx);
			ImGui::Separator();

			int stop_slot = -1;
			for (size_t slot = 0; slot < ctx.captures.size(); ++slot)
			{
				CaptureManager &capture = *ctx.captures[slot];
				const CaptureStatus status = capture.GetStatus();

				ImGui::PushID(static_cast<int>(slot));
				snprintf(buffer, sizeof(buffer), TR("Stream %u"), capture.SlotIndex());
				ImGui::TextColored(ImVec4(0.6f, 0.85f, 1.0f, 1.0f), "%s", buffer);
				if (status.state != CaptureState::Idle)
				{
					ImGui::SameLine(160.0f);
					if (ImGui::Button(slot == 0 ? TR("Stop Capture") : TR("Stop Stream")))
						stop_slot = static_cast<int>(slot);
				}

				DrawStatusLine(TR("Capture Status:"), TR(CaptureStateName(status.state)));
				if (!status.message.empty())
					ImGui::TextWrapped("%s", status.message.c_str());
				DrawStatusLine(TR("Sender:"), status.sender_name.empty() ? "-" : status.sender_name.c_str());
				if (status.source_id != 0)
				{
					snprintf(buffer, sizeof(buffer), "#%03u", status.source_id);
					DrawStatusLine(TR("Source:"), buffer);
				}
				if (status.width != 0)
				{
					snprintf(buffer, sizeof(buffer), "%u x %u", status.width, status.height);
					DrawStatusLine(TR("Resolution:"), buffer);
					DrawStatusLine(TR("Format:"), FormatName(status.format));

					// Dynamic resolution: say what the game is really rendering, and that the stream is
					// scaled rather than cropped, so a varying number here is not a bug.
					if (status.active_width != 0 && (status.active_width != status.width || status.active_height != status.height))
					{
						snprintf(buffer, sizeof(buffer), "%u x %u  (%.0f%%)", status.active_width, status.active_height,
							100.0f * static_cast<float>(status.active_width) / static_cast<float>(status.width));
						DrawStatusLine(TR("Rendered area:"), buffer);
						if (ImGui::IsItemHovered())
							ImGui::SetTooltip(TR("The game renders into part of a fixed-size buffer (dynamic resolution).\n"
								"That area is scaled into the stream, so the stream keeps a stable size."));
					}
				}
				if (status.state == CaptureState::Active)
				{
					snprintf(buffer, sizeof(buffer), "%.0f", status.fps);
					DrawStatusLine(TR("FPS:"), buffer);
					snprintf(buffer, sizeof(buffer), TR("%llu sent, %llu skipped"), static_cast<unsigned long long>(status.frames_sent), static_cast<unsigned long long>(status.frames_skipped));
					DrawStatusLine(TR("Frames:"), buffer);
					if (status.uses_bridge)
						snprintf(buffer, sizeof(buffer), TR("%.1f us CPU (mutex wait %.1f, D3D11On12 bridge %.1f)"), status.last_copy_cpu_us, status.last_mutex_wait_us, status.last_bridge_us);
					else
						snprintf(buffer, sizeof(buffer), TR("%.1f us CPU (of which mutex wait %.1f us)"), status.last_copy_cpu_us, status.last_mutex_wait_us);
					DrawStatusLine(TR("Copy cost:"), buffer);
				}
				if (status.state == CaptureState::Idle)
					ImGui::TextDisabled(TR("Select a buffer in the Buffer Inspector tab and click \"Use As Capture Source\"."));

				// Depth streams: the numbers that cannot be guessed from outside the game. They are
				// the same ones ReShade's own depth shaders use, so settings already worked out for a
				// game carry over unchanged.
				if (capture.Transform() == StreamTransform::DepthLinear)
				{
					DepthSettings depth = capture.GetDepthSettings();
					const DepthSettings before = depth;

					ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.75f, 1.0f), TR("Depth linearisation"));
					ImGui::SetNextItemWidth(160.0f);
					ImGui::DragFloat(TR("Far plane"), &depth.far_plane, 1.0f, 1.0f, 100000.0f, "%.0f");
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip(TR("Distance mapped to white. Too large a value makes the whole image look black,\n"
							"too small makes it saturate. Same meaning as RESHADE_DEPTH_LINEARIZATION_FAR_PLANE."));

					ImGui::Checkbox(TR("Reversed Z (1 at the near plane)"), &depth.reversed);
					ImGui::Checkbox(TR("Logarithmic"), &depth.logarithmic);
					ImGui::Checkbox(TR("Upside down"), &depth.upside_down);

					if (depth != before)
					{
						capture.SetDepthSettings(depth);
						config::SetFloat("DepthFarPlane", depth.far_plane);
						config::SetBool("DepthReversed", depth.reversed);
						config::SetBool("DepthLogarithmic", depth.logarithmic);
						config::SetBool("DepthUpsideDown", depth.upside_down);
					}
				}

				if (capture.Transform() == StreamTransform::HudDifference)
				{
					HudSettings hud = capture.GetHudSettings();
					const HudSettings before = hud;

					snprintf(buffer, sizeof(buffer), TR("HUD isolation (against #%03u)"), capture.SecondarySourceId());
					ImGui::TextColored(ImVec4(0.75f, 0.75f, 0.75f, 1.0f), "%s", buffer);

					ImGui::SetNextItemWidth(160.0f);
					ImGui::SliderFloat(TR("Threshold"), &hud.threshold, 0.0f, 0.5f, "%.3f");
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip(TR("Below this much difference between the two buffers the pixel counts as pure scene.\n"
							"Raise it when the scene itself shows through the mask, lower it when faint interface elements are lost."));

					ImGui::SetNextItemWidth(160.0f);
					ImGui::SliderFloat(TR("Softness"), &hud.softness, 0.0f, 0.5f, "%.3f");
					if (ImGui::IsItemHovered())
						ImGui::SetTooltip(TR("Difference at which the mask is fully opaque. The gap with the threshold is what keeps edges soft."));

					ImGui::Checkbox(TR("Keep the finished colours (instead of the raw difference)"), &hud.keep_final_colour);

					if (hud != before)
					{
						capture.SetHudSettings(hud);
						config::SetFloat("HudThreshold", hud.threshold);
						config::SetFloat("HudSoftness", hud.softness);
						config::SetBool("HudKeepFinalColour", hud.keep_final_colour);
					}
				}

				if (status.state == CaptureState::Active && capture.OutputView() != 0)
				{
					if (ImGui::TreeNodeEx(TR("Output preview (what OBS receives)"), slot == 0 ? ImGuiTreeNodeFlags_DefaultOpen : 0))
					{
						DrawOutputImage(capture.OutputView(), status.width, status.height, 300.0f);
						ImGui::TreePop();
					}
				}
				ImGui::PopID();
				ImGui::Separator();
			}
			if (stop_slot >= 0)
				ctx.StopCapture(static_cast<size_t>(stop_slot));

			if (ImGui::Checkbox(TR("Flush after copy (submit immediately, like Spout's own sender)"), &ctx.flush_after_copy))
				config::SetBool("FlushAfterCopy", ctx.flush_after_copy);
			if (ctx.flush_after_copy)
			{
				ImGui::SameLine();
				ImGui::TextDisabled(TR("%.1f us CPU per frame"), ctx.last_flush_us);
			}
			ImGui::TextDisabled(TR("Each active stream costs one GPU copy per frame (two on D3D12). No stream limit: Spout allows %d senders machine wide (%d in use)."),
				SpoutSender::MaxSenders(), SpoutSender::ActiveSenderCount());
		}

		// ------------------------------------------------------------------ Buffer Inspector tab
		void DrawFilters(InspectorModel &model)
		{
			InspectorFilter &f = model.filter;

			// A table rather than fixed SameLine offsets: the overlay font is whatever ReShade is
			// configured with, and a label wider than the offset used to run into the next column.
			if (ImGui::BeginTable("##filters", 3, ImGuiTableFlags_SizingStretchSame))
			{
				auto cell = [](int column) { ImGui::TableSetColumnIndex(column); };

				ImGui::TableNextRow();
				cell(0); ImGui::Checkbox(TR("Steady buffers only"), &f.persistent_only);
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip(TR("Hides the transient targets an engine takes from a pool and gives back a few\n"
						"frames later. Those are what makes the list flicker; a buffer worth capturing\n"
						"is written nearly every frame. Back buffers are always kept."));
				cell(1); ImGui::Checkbox(TR("Written recently"), &f.written_recently);
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip(TR("Keeps a buffer listed for %u frames after its last write, so the ones an\n"
						"engine fills every other frame do not blink in and out."), f.recent_frames);
				cell(2); ImGui::Checkbox(TR("Back buffers"), &f.backbuffers);

				ImGui::TableNextRow();
				cell(0); ImGui::Checkbox(TR("Colour render targets"), &f.color_targets);
				cell(1); ImGui::Checkbox(TR("Depth"), &f.depth);
				cell(2); ImGui::Checkbox(TR("Small resources"), &f.small_resources);

				ImGui::TableNextRow();
				cell(0); ImGui::Checkbox(TR("MSAA"), &f.multisampled);
				cell(1); ImGui::Checkbox(TR("Mip / array resources"), &f.mips_or_arrays);
				ImGui::EndTable();
			}

			const char *const resolution_names[] = { TR("Any"), TR("Backbuffer Resolution"), TR(">= 75% Backbuffer"), TR(">= 50% Backbuffer"), TR("Custom") };
			int resolution = static_cast<int>(f.resolution);
			ImGui::SetNextItemWidth(220.0f);
			if (ImGui::Combo(TR("Resolution"), &resolution, resolution_names, static_cast<int>(std::size(resolution_names))))
				f.resolution = static_cast<ResolutionFilter>(resolution);
			if (f.resolution == ResolutionFilter::Custom)
			{
				int custom[2] = { static_cast<int>(f.custom_width), static_cast<int>(f.custom_height) };
				ImGui::SameLine();
				ImGui::SetNextItemWidth(160.0f);
				if (ImGui::InputInt2(TR("Min size"), custom))
				{
					f.custom_width = static_cast<uint32_t>(std::max(custom[0], 0));
					f.custom_height = static_cast<uint32_t>(std::max(custom[1], 0));
				}
			}

			const char *const sort_names[] = { TR("Id"), TR("Steadiness"), TR("Size"), TR("Writes"), TR("Format") };
			int sort = static_cast<int>(f.sort);
			ImGui::SetNextItemWidth(220.0f);
			if (ImGui::Combo(TR("Sort by"), &sort, sort_names, static_cast<int>(std::size(sort_names))))
				f.sort = static_cast<SortKey>(sort);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip(TR("Ordering only, nothing is hidden. \"Steadiness\" puts the buffers written the\n"
					"most regularly first, which is usually where the scene targets are."));

			ImGui::SetNextItemWidth(220.0f);
			const char *current_format = f.format == format::unknown ? TR("Any") : FormatShortName(f.format);
			if (ImGui::BeginCombo(TR("Format"), current_format))
			{
				if (ImGui::Selectable(TR("Any"), f.format == format::unknown))
					f.format = format::unknown;
				for (const format fmt : model.AvailableFormats())
				{
					char label[64];
					snprintf(label, sizeof(label), "%s (%s)", FormatShortName(fmt), FormatName(fmt));
					if (ImGui::Selectable(label, fmt == f.format))
						f.format = fmt;
				}
				ImGui::EndCombo();
			}
		}

		void DrawSelectedDetails(DeviceContext &ctx, const TrackedResource &res)
		{
			char buffer[160];
			ImGui::Text(TR("Resource #%03u"), res.id);
			if (!res.debug_name.empty())
				ImGui::Text(TR("Name: %s"), res.debug_name.c_str());

			snprintf(buffer, sizeof(buffer), "%u x %u", res.Width(), res.Height());
			DrawStatusLine(TR("Resolution:"), buffer);
			DrawStatusLine(TR("Format:"), FormatName(res.Format()));
			snprintf(buffer, sizeof(buffer), TR("mips %u, layers %u, samples %u"), res.desc.texture.levels, res.desc.texture.depth_or_layers, res.desc.texture.samples);
			DrawStatusLine(TR("Layout:"), buffer);

			std::string usage;
			if (res.IsColorTarget()) usage += "RenderTarget ";
			if (res.IsDepthStencil()) usage += "DepthStencil ";
			if (res.IsUnorderedAccess()) usage += "UAV ";
			if (res.IsShaderResource()) usage += "ShaderResource ";
			if ((res.desc.usage & resource_usage::copy_dest) != 0) usage += "CopyDest ";
			if (usage.empty()) usage = "-";
			DrawStatusLine(TR("Usage:"), usage.c_str());

			DrawStatusLine(TR("Written This Frame:"), res.last.writes > 0 ? TR("Yes") : TR("No"));
			snprintf(buffer, sizeof(buffer), TR("%u (%u draw calls%s)"), res.last.writes, res.last.draw_calls, res.last.cleared ? ", cleared" : "");
			DrawStatusLine(TR("Writes:"), buffer);
			if (res.last.writes > 0)
			{
				snprintf(buffer, sizeof(buffer), TR("Event %u (%s)   first: %u"), res.last.last_write_event, WriteKindName(res.last.last_write_kind), res.last.first_write_event);
				DrawStatusLine(TR("Last Write:"), buffer);
			}
			snprintf(buffer, sizeof(buffer), "%u", res.last.reads);
			DrawStatusLine(TR("Reads:"), buffer);
			DrawStatusLine(TR("Backbuffer:"), res.is_backbuffer ? TR("Yes") : TR("No"));
			snprintf(buffer, sizeof(buffer), TR("%llu   (total writes %llu)"), static_cast<unsigned long long>(res.created_frame), static_cast<unsigned long long>(res.total_writes));
			DrawStatusLine(TR("Created Frame:"), buffer);

			const resource_desc bb = ctx.inspector.BackbufferDesc();
			if (bb.texture.width != 0 && bb.texture.height != 0)
			{
				snprintf(buffer, sizeof(buffer), "%.0f%% x %.0f%%", 100.0f * res.Width() / bb.texture.width, 100.0f * res.Height() / bb.texture.height);
				DrawStatusLine(TR("Relative to BB:"), buffer);
			}
		}

		/**
		* The contact sheet: every listed buffer drawn small, all at once.
		*
		* Clicking through a list of forty render targets to find out which one holds the scene is the
		* slow way; seeing them side by side is the fast one. The images come from ThumbnailGrid, which
		* renders them on the GPU at present time, so this only lays them out and reports the misses.
		*/
		void DrawThumbnails(DeviceContext &ctx, InspectorModel &model)
		{
			ctx.thumbnails_wanted = true;

			const std::vector<TrackedResource> &entries = model.Entries();
			const size_t page_count = entries.empty() ? 1 : (entries.size() + kThumbnailBudget - 1) / kThumbnailBudget;
			if (ctx.thumbnail_page >= page_count * kThumbnailBudget)
				ctx.thumbnail_page = 0;

			if (page_count > 1)
			{
				const size_t page = ctx.thumbnail_page / kThumbnailBudget;
				if (ImGui::Button(TR("<##thumb_prev")) && page > 0)
					ctx.thumbnail_page -= kThumbnailBudget;
				ImGui::SameLine();
				if (ImGui::Button(TR(">##thumb_next")) && page + 1 < page_count)
					ctx.thumbnail_page += kThumbnailBudget;
				ImGui::SameLine();
				ImGui::Text(TR("Page %zu / %zu"), page + 1, page_count);
			}

			const float thumb_width = 176.0f;
			const float thumb_height = thumb_width * kThumbnailHeight / kThumbnailWidth;
			const float available = ImGui::GetContentRegionAvail().x;
			const int columns = std::max(1, static_cast<int>(available / (thumb_width + ImGui::GetStyle().ItemSpacing.x * 2.0f)));

			// A fixed height that scrolls: the sheet is for finding a buffer, and the moment slider, the
			// capture buttons and the preview below must stay in view while it is open.
			const size_t remaining = entries.size() > ctx.thumbnail_page ? entries.size() - ctx.thumbnail_page : 0;
			const size_t shown = std::min(remaining, kThumbnailBudget);
			const int rows = static_cast<int>((shown + static_cast<size_t>(columns) - 1) / static_cast<size_t>(columns));
			const float row_height = thumb_height + ImGui::GetStyle().FramePadding.y * 2.0f + ImGui::GetTextLineHeightWithSpacing() + ImGui::GetStyle().ItemSpacing.y;
			const float sheet_height = std::min(static_cast<float>(std::max(rows, 1)), 2.4f) * row_height;
			ImGui::BeginChild("##contact_sheet", ImVec2(0.0f, sheet_height), ImGuiChildFlags_Borders);
			if (ImGui::BeginTable("##thumbnails", columns, ImGuiTableFlags_SizingStretchSame))
			{
				const size_t last = std::min(entries.size(), ctx.thumbnail_page + kThumbnailBudget);
				for (size_t i = ctx.thumbnail_page; i < last; ++i)
				{
					const TrackedResource &res = entries[i];
					ImGui::TableNextColumn();
					ImGui::PushID(static_cast<int>(res.id));

					const resource_view view = ctx.thumbnails->ViewFor(res.id);
					const bool selected = res.id == model.selected_id;
					if (selected)
						ImGui::PushStyleColor(ImGuiCol_Button, ImGui::GetStyleColorVec4(ImGuiCol_ButtonActive));

					if (view != 0)
					{
						if (ImGui::ImageButton("##thumb", static_cast<ImTextureID>(view.handle), ImVec2(thumb_width, thumb_height)))
							model.Select(res.id);
					}
					else
					{
						const char *const reason = ctx.thumbnails->ReasonFor(res.id);
						if (ImGui::Button(reason != nullptr ? reason : TR("no image"), ImVec2(thumb_width, thumb_height)))
							model.Select(res.id);
					}
					if (selected)
						ImGui::PopStyleColor();

					if (ImGui::IsItemHovered())
						ImGui::SetTooltip(TR("#%03u  %ux%u  %s\nwritten during %.0f%% of the frames"),
							res.id, res.Width(), res.Height(), FormatName(res.Format()),
							res.WriteRatio(model.FrameIndex()) * 100.0f);

					const bool is_capture_source = ctx.CaptureOf(res.id) != nullptr;
					if (is_capture_source)
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));
					ImGui::Text("#%03u %s", res.id, FormatShortName(res.Format()));
					if (is_capture_source)
						ImGui::PopStyleColor();

					ImGui::PopID();
				}
				ImGui::EndTable();
			}
			ImGui::EndChild();
			if (entries.empty())
				ImGui::TextDisabled(TR("No resource matches the current filters."));
		}

		/**
		* When in the frame the selected buffer is looked at, captured and shown in game.
		*
		* Some games never keep their image without the interface in a buffer of its own: Unreal Engine 4
		* tonemaps straight into the back buffer and draws its interface into it right after. The clean
		* picture then only exists for a moment of the frame, between two writes of the same buffer. This
		* slider picks that moment; the preview shows it, and "Use As Capture Source" captures it.
		*/
		void DrawMomentControls(DeviceContext &ctx, InspectorModel &model, const TrackedResource &res)
		{
			std::string why;
			if (!ctx.snapshots->IsSupported(&why))
			{
				ImGui::TextDisabled(TR("Moment of the frame: end (%s)"), TR(why.c_str()));
				return;
			}

			// Writes of the last frame, without the present that closes a back buffer's frame
			const uint32_t writes = res.last.writes - ((res.is_backbuffer && res.last.writes > 0) ? 1u : 0u);
			if (writes <= 1)
			{
				ImGui::TextDisabled(TR("Moment of the frame: end (written %s per frame, so there is no earlier moment)"),
					writes == 1 ? TR("once") : TR("zero times"));
				model.moment = 0;
				return;
			}

			int position = model.moment == 0 || model.moment >= writes ? static_cast<int>(writes) : static_cast<int>(model.moment);
			char format[96];
			if (position >= static_cast<int>(writes))
				snprintf(format, sizeof(format), TR("end of frame (after all %u writes)"), writes);
			else
				snprintf(format, sizeof(format), TR("after write %%d of %u"), writes);
			ImGui::SetNextItemWidth(320.0f);
			if (ImGui::SliderInt(TR("Moment of the frame"), &position, 1, static_cast<int>(writes), format, ImGuiSliderFlags_AlwaysClamp))
				model.moment = position >= static_cast<int>(writes) ? 0u : static_cast<uint32_t>(position);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip(TR("When in the frame this buffer is read, instead of the end of the frame.\n"
					"A game that draws its interface straight into the final image (Unreal Engine 4 does)\n"
					"has no buffer without it, only a moment: move back until the interface disappears from\n"
					"the preview. Capture, preview and \"show selected buffer in game\" all use this moment."));

			// The order of those writes, from the timeline, so the moment can be picked knowingly:
			// "1 clear, 2 draw, 3-4 draw" reads as "cleared, one pass, then two more draws".
			std::string order;
			uint32_t position_in_frame = 0;
			for (const TimelineEntry &entry : ctx.tracker->TimelineSnapshot())
			{
				if (entry.resource_id != res.id || !entry.is_write || entry.kind == WriteKind::Present)
					continue;
				const uint32_t first = position_in_frame + 1;
				position_in_frame += entry.count;
				char part[48];
				if (entry.count == 1)
					snprintf(part, sizeof(part), "%s%u %s", order.empty() ? "" : ", ", first, WriteKindName(entry.kind));
				else
					snprintf(part, sizeof(part), "%s%u-%u %s", order.empty() ? "" : ", ", first, position_in_frame, WriteKindName(entry.kind));
				order += part;
				if (order.size() > 160)
				{
					order += ", ...";
					break;
				}
			}
			if (!order.empty())
				ImGui::TextDisabled(TR("Writes of this buffer in the last frame: %s"), order.c_str());
			if (model.moment != 0 && !ctx.selection_snapshot_error.empty())
				ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), "%s", TR(ctx.selection_snapshot_error.c_str()));
		}

		// The discussion window's state; it belongs to the overlay, not to a device
		bool s_chat_open = false;
		bool s_chat_attach = true;
		char s_chat_input[4096] = {};
		uint32_t s_chat_seen_revision = 0;

		void DrawAiClientChoice()
		{
			static const char *const client_keys[] = { "auto", "claude", "codex", "opencode" };
			const char *const client_names[] = { TR("Auto (first one installed)"), "Claude Code", "Codex", "OpenCode" };
			static int client = -1;
			if (client < 0)
			{
				const std::string saved = config::GetString("AIClient", "auto");
				client = 0;
				for (int i = 0; i < static_cast<int>(std::size(client_keys)); ++i)
					if (saved == client_keys[i])
						client = i;
			}
			ImGui::SetNextItemWidth(220.0f);
			if (ImGui::Combo(TR("AI client"), &client, client_names, static_cast<int>(std::size(client_names))))
				config::SetString("AIClient", client_keys[client]);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip(TR("The command line client that answers, with its own account: nothing is stored here.\n"
					"Optional in ReShade.ini [CYGAMECAPTURE]: AIModel=<model>, AIClientPath=<client .exe>, AILanguage=fr|en."));
		}

		/** "Show" and "Apply" for a proposal; nothing is applied any other way. */
		void DrawProposal(DeviceContext &ctx, const AiMessage &message)
		{
			const AiProposal &proposal = message.proposal;
			ImGui::PushID(&message);
			if (proposal.moment == 0)
				ImGui::TextColored(ImVec4(0.45f, 0.85f, 1.0f, 1.0f), TR("Proposal: #%03u, at the end of the frame"), proposal.buffer);
			else
				ImGui::TextColored(ImVec4(0.45f, 0.85f, 1.0f, 1.0f), TR("Proposal: #%03u, right after its write %u of the frame"), proposal.buffer, proposal.moment);

			// Showing is always safe: it only selects the buffer and the moment, for the preview
			auto show = [&]() {
				ctx.inspector.Select(proposal.buffer);
				ctx.inspector.moment = proposal.moment;
			};
			if (ImGui::Button(TR("Show")))
				show();
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip(TR("Selects it in the Buffer Inspector, at that moment, so the preview shows it."));
			ImGui::SameLine();
			if (ImGui::Button(TR("Apply")))
			{
				show();
				std::string error;
				if (!ctx.StartCapture(proposal.buffer, &error, StreamTransform::Copy, 0, proposal.moment))
					log::Warning("Applying the AI proposal failed: %s", error.c_str());
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip(TR("Stream 1 sends this buffer at this moment of the frame."));
			if (proposal.hud_stream && proposal.back_buffer != 0)
			{
				ImGui::SameLine();
				if (ImGui::Button(TR("Apply, and send the interface alone too")))
				{
					show();
					std::string error;
					if (!ctx.StartCapture(proposal.buffer, &error, StreamTransform::Copy, 0, proposal.moment) ||
						!ctx.StartCaptureInNewSlot(proposal.back_buffer, &error, StreamTransform::HudDifference, proposal.buffer, 0, proposal.moment))
						log::Warning("Applying the AI proposal failed: %s", error.c_str());
				}
				if (ImGui::IsItemHovered())
					ImGui::SetTooltip(TR("Stream 1: the picture without the interface. Stream 2: the finished image minus stream 1,\n"
						"which leaves the interface alone, with an alpha channel."));
			}
			ImGui::PopID();
		}

		/**
		* The AI assistant section of the inspector: the button (an automatic first message), the way to
		* the discussion, and the latest proposal at hand. See Assistant/AiAssistant for what is sent.
		*/
		void DrawAiPanel(DeviceContext &ctx)
		{
			AiAssistant &ai = *ctx.ai;
			ai.Poll();

			std::string why;
			if (!ai.IsSupported(&why))
			{
				ImGui::TextDisabled("%s", TR(why.c_str()));
				return;
			}

			DrawAiClientChoice();
			ImGui::SameLine();
			ImGui::BeginDisabled(ai.IsBusy());
			if (ImGui::Button(TR("Find the setting that removes the game's interface")))
			{
				ai.FindSettingWithoutInterface();
				s_chat_open = true;
			}
			ImGui::EndDisabled();
			if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
				ImGui::SetTooltip(TR("Show a scene where the game's interface is visible first.\n"
					"Pictures of the frame and the list of its buffers are sent to the chosen AI client,\n"
					"which proposes where the picture without the interface is. Nothing changes until you apply it.\n"
					"Then tell it in the discussion what is still wrong."));
			ImGui::SameLine();
			if (ImGui::Button(s_chat_open ? TR("Hide the discussion") : TR("Discussion")))
				s_chat_open = !s_chat_open;

			if (ai.IsBusy())
				ImGui::TextDisabled("%s", TR(ai.Progress().c_str()));
			else if (const AiMessage *latest = ai.LatestProposal())
				DrawProposal(ctx, *latest);
		}

		/**
		* The discussion: every message and reply, the proposals with their buttons, and a field to answer.
		*
		* The clients keep no session, so the whole conversation goes with each message. "Attach the current
		* frame" adds fresh pictures, for "look again, there is a subtitle now"; without it the AI works from
		* the buffer list, the write order and what was said before.
		*/
		void DrawChatWindow(effect_runtime *runtime)
		{
			DeviceContext *const ctx = DeviceContext::From(runtime->get_device());
			if (ctx == nullptr)
				return;
			AiAssistant &ai = *ctx->ai;
			ai.Poll();
			// A new message or reply brings the discussion up, even if it was closed meanwhile
			if (ai.Revision() != s_chat_seen_revision)
				s_chat_open = true;
			if (!s_chat_open)
				return;

			const ImVec2 display = ImGui::GetIO().DisplaySize;
			// First time: next to the inspector window (right side of the screen), clear of ReShade's own
			// window on the left when the screen is wide enough for the three
			const float width = std::min(620.0f, display.x * 0.4f);
			const float inspector_width = std::min(1000.0f, display.x * 0.6f);
			const float x = std::max(20.0f, display.x - inspector_width - 20.0f - width - 10.0f);
			ImGui::SetNextWindowSize(ImVec2(width, std::min(700.0f, display.y - 60.0f)), ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowPos(ImVec2(x, 40.0f), ImGuiCond_FirstUseEver);
			char title[128];
			snprintf(title, sizeof(title), "%s###cygc_ai_assistant", TR("CyGameCapture - AI Assistant"));
			if (!ImGui::Begin(title, &s_chat_open))
			{
				ImGui::End();
				return;
			}

			std::string why;
			if (!ai.IsSupported(&why))
			{
				ImGui::TextWrapped("%s", TR(why.c_str()));
				ImGui::End();
				return;
			}

			DrawAiClientChoice();
			ImGui::SameLine();
			ImGui::BeginDisabled(ai.IsBusy() || ai.Conversation().empty());
			if (ImGui::Button(TR("New discussion")))
				ai.NewConversation();
			ImGui::EndDisabled();
			ImGui::Separator();

			// The messages, keeping the room for the answer field below
			const float input_height = ImGui::GetTextLineHeight() * 4.0f + ImGui::GetFrameHeightWithSpacing() * 2.0f;
			if (ImGui::BeginChild("##messages", ImVec2(0.0f, -input_height), ImGuiChildFlags_Borders))
			{
				if (ai.Conversation().empty())
					ImGui::TextWrapped(TR("Show a scene where the game's interface is visible, then press \"Find the setting that "
						"removes the game's interface\", or ask a question below. Every setting proposed here is only "
						"applied when you click Apply."));
				for (const AiMessage &message : ai.Conversation())
				{
					if (message.from_user)
						ImGui::TextColored(ImVec4(0.6f, 1.0f, 0.6f, 1.0f), TR("You%s"), message.pictures ? TR("  (with pictures of the frame)") : "");
					else if (message.failed)
						ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), TR("Error"));
					else
						ImGui::TextColored(ImVec4(0.45f, 0.85f, 1.0f, 1.0f), "%s", message.client.empty() ? TR("AI") : message.client.c_str());
					ImGui::PushTextWrapPos(0.0f);
					ImGui::TextUnformatted(message.failed ? TR(message.text.c_str()) : message.text.c_str());
					ImGui::PopTextWrapPos();
					if (message.has_proposal)
						DrawProposal(*ctx, message);
					ImGui::Spacing();
					ImGui::Separator();
				}
				if (ai.IsBusy())
					ImGui::TextDisabled("%s", TR(ai.Progress().c_str()));
				// Follow the conversation as it grows
				if (s_chat_seen_revision != ai.Revision())
				{
					s_chat_seen_revision = ai.Revision();
					ImGui::SetScrollHereY(1.0f);
				}
			}
			ImGui::EndChild();

			ImGui::InputTextMultiline("##message", s_chat_input, sizeof(s_chat_input),
				ImVec2(-1.0f, ImGui::GetTextLineHeight() * 4.0f));
			const bool send_by_keyboard = ImGui::IsItemFocused() && ImGui::GetIO().KeyCtrl && ImGui::IsKeyPressed(ImGuiKey_Enter);
			ImGui::Checkbox(TR("Attach the current frame"), &s_chat_attach);
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip(TR("Sends fresh pictures of the frame as it is now with this message:\n"
					"the back buffer at each of its writes, and the full-size buffers."));
			ImGui::SameLine();
			ImGui::BeginDisabled(ai.IsBusy());
			if ((ImGui::Button(TR("Send (Ctrl+Enter)")) || send_by_keyboard) && !ai.IsBusy() && s_chat_input[0] != '\0')
			{
				ai.Send(s_chat_input, s_chat_attach);
				s_chat_input[0] = '\0';
			}
			ImGui::SameLine();
			if (ImGui::Button(TR("Find the setting")))
				ai.FindSettingWithoutInterface();
			ImGui::EndDisabled();
			ImGui::End();
		}

		void DrawInspectorTab(effect_runtime *runtime, DeviceContext &ctx)
		{
			InspectorModel &model = ctx.inspector;
			model.Refresh(*ctx.tracker);

			if (ImGui::CollapsingHeader(TR("AI Assistant"), ImGuiTreeNodeFlags_DefaultOpen))
				DrawAiPanel(ctx);

			if (ImGui::CollapsingHeader(TR("Filters"), ImGuiTreeNodeFlags_DefaultOpen))
				DrawFilters(model);

			ImGui::Text(TR("Detected Resources: %u   (%u shown)"), static_cast<unsigned int>(model.TotalCount()), static_cast<unsigned int>(model.Entries().size()));

			if (ImGui::CollapsingHeader(TR("Contact Sheet"), ImGuiTreeNodeFlags_DefaultOpen))
				DrawThumbnails(ctx, model);

			// Keyboard navigation: Left/Right (or PageUp/PageDown) while the inspector is visible and no text field is being edited
			if (!ImGui::GetIO().WantTextInput)
			{
				if (runtime->is_key_pressed(VK_LEFT) || runtime->is_key_pressed(VK_PRIOR))
					model.SelectPrevious();
				if (runtime->is_key_pressed(VK_RIGHT) || runtime->is_key_pressed(VK_NEXT))
					model.SelectNext();
			}

			// ---- resource list
			const uint64_t frame_index = model.FrameIndex();
			constexpr ImGuiTableFlags list_flags = ImGuiTableFlags_ScrollY | ImGuiTableFlags_RowBg |
				ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_SizingFixedFit;
			if (ImGui::BeginTable("##resource_list", 6, list_flags, ImVec2(0.0f, kListHeight)))
			{
				ImGui::TableSetupScrollFreeze(0, 1);
				ImGui::TableSetupColumn(TR("Id"));
				ImGui::TableSetupColumn(TR("Resolution"));
				ImGui::TableSetupColumn(TR("Format"));
				ImGui::TableSetupColumn("W");
				ImGui::TableSetupColumn(TR("Steady"));
				ImGui::TableSetupColumn(TR("Flags"), ImGuiTableColumnFlags_WidthStretch);
				ImGui::TableHeadersRow();

				const int selected_index = model.SelectedIndex();
				int index = 0;
				for (const TrackedResource &res : model.Entries())
				{
					ImGui::TableNextRow();
					ImGui::PushID(static_cast<int>(res.id));

					const bool is_capture_source = ctx.CaptureOf(res.id) != nullptr;
					if (is_capture_source)
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(0.4f, 1.0f, 0.4f, 1.0f));

					char id_text[16];
					snprintf(id_text, sizeof(id_text), "#%03u", res.id);
					ImGui::TableSetColumnIndex(0);
					if (ImGui::Selectable(id_text, index == selected_index, ImGuiSelectableFlags_SpanAllColumns))
						model.Select(res.id);
					if (index == selected_index && ImGui::IsWindowAppearing())
						ImGui::SetScrollHereY();

					ImGui::TableSetColumnIndex(1);
					if (res.IsDownscaled())
					{
						const ActiveRect active = res.ActiveArea();
						ImGui::Text("%ux%u (%ux%u)", res.Width(), res.Height(), active.width, active.height);
					}
					else
						ImGui::Text("%ux%u", res.Width(), res.Height());

					ImGui::TableSetColumnIndex(2);
					ImGui::TextUnformatted(FormatShortName(res.Format()));
					ImGui::TableSetColumnIndex(3);
					ImGui::Text("%u", res.last.writes);

					// How regularly the engine writes it: the single most useful number for telling a
					// scene target apart from a one-off, and the reason the list used to churn.
					ImGui::TableSetColumnIndex(4);
					const float ratio = res.WriteRatio(frame_index);
					if (ratio < 0.5f)
						ImGui::TextDisabled("%3.0f%%", ratio * 100.0f);
					else
						ImGui::Text("%3.0f%%", ratio * 100.0f);

					ImGui::TableSetColumnIndex(5);
					ImGui::Text("%s%s%s%s", res.is_backbuffer ? TR("[BB] ") : "",
						(res.IsDepthStencil() || IsDepthStencilFormat(res.Format())) ? TR("[Depth] ") : "",
						res.IsMultisampled() ? TR("[MSAA] ") : "",
						res.IsUnorderedAccess() && !res.IsColorTarget() ? TR("[UAV] ") : "");

					if (is_capture_source)
						ImGui::PopStyleColor();
					ImGui::PopID();
					++index;
				}
				ImGui::EndTable();
			}
			if (model.Entries().empty())
				ImGui::TextDisabled(TR("No resource matches the current filters."));

			// ---- navigation + actions
			if (ImGui::Button(TR("Previous")))
				model.SelectPrevious();
			ImGui::SameLine();
			if (ImGui::Button(TR("Next")))
				model.SelectNext();
			ImGui::SameLine();
			bool frozen = ctx.preview->IsFrozen();
			if (ImGui::Checkbox(TR("Freeze Preview"), &frozen))
				ctx.preview->SetFrozen(frozen);

			const TrackedResource *selected = model.Selected();
			TrackedResource selected_copy;
			if (selected == nullptr && model.selected_id != 0 && ctx.tracker->Get(model.selected_id, selected_copy))
				selected = &selected_copy; // selected but hidden by the filters

			if (selected != nullptr)
			{
				DrawMomentControls(ctx, model, *selected);
				const uint32_t moment = model.moment;

				if (CaptureManager *const capture = ctx.CaptureAt(selected->id, moment))
				{
					char label[64];
					snprintf(label, sizeof(label), TR("Stop Stream %u"), capture->SlotIndex());
					if (ImGui::Button(label))
					{
						for (size_t slot = 0; slot < ctx.captures.size(); ++slot)
							if (ctx.captures[slot].get() == capture)
							{
								ctx.StopCapture(slot);
								break;
							}
					}
				}
				else
				{
					// A depth buffer cannot be copied out as-is: it goes through the linearisation pass,
					// so it gets its own buttons rather than silently behaving differently.
					const bool is_depth = selected->IsDepthStencil() || IsDepthStencilFormat(selected->Format());
					const StreamTransform transform = is_depth ? StreamTransform::DepthLinear : StreamTransform::Copy;
					const char *const primary_label = is_depth ? TR("Use As Depth Stream") : TR("Use As Capture Source");
					const char *const extra_label = is_depth ? TR("Add Depth As Extra Stream") : TR("Add As Extra Stream");

					if (ImGui::Button(primary_label))
					{
						std::string error;
						if (!ctx.StartCapture(selected->id, &error, transform, 0, moment))
							log::Warning("%s failed: %s", primary_label, error.c_str());
					}
					if (is_depth && ImGui::IsItemHovered())
						ImGui::SetTooltip(TR("Linearises the depth buffer and sends it as 16 bits per channel.\n"
							"Set the far plane and the reversed-Z flag in the Capture tab: they cannot be detected from outside the game."));

					if (ctx.PrimaryCapture().IsActive() && ctx.captures.size() < DeviceContext::kSanityStreamLimit)
					{
						ImGui::SameLine();
						if (ImGui::Button(extra_label))
						{
							std::string error;
							if (!ctx.StartCaptureInNewSlot(selected->id, &error, transform, 0, moment))
								log::Warning("%s failed: %s", extra_label, error.c_str());
						}
						if (ImGui::IsItemHovered())
							ImGui::SetTooltip(TR("Keeps the current stream and sends this buffer as an additional Spout sender (\"%s::N\")"), SenderBaseName().c_str());

						// The interface on its own is what stream 1 is capturing subtracted from this
						// buffer, so it only makes sense once stream 1 holds the scene without the HUD.
						// Stream 1 may also be this very buffer at an earlier moment: the back buffer right after
						// the tonemapper against the same back buffer at the end of the frame.
						const uint32_t clean_id = ctx.PrimaryCapture().SourceId();
						const uint32_t clean_moment = ctx.PrimaryCapture().Moment();
						if (!is_depth && clean_id != 0 && (clean_id != selected->id || clean_moment != moment) &&
							ctx.PrimaryCapture().Transform() == StreamTransform::Copy)
						{
							if (ImGui::Button(TR("Isolate HUD Against Stream 1")))
							{
								std::string error;
								if (!ctx.StartCaptureInNewSlot(selected->id, &error, StreamTransform::HudDifference, clean_id, moment, clean_moment))
									log::Warning("Isolate HUD failed: %s", error.c_str());
							}
							if (ImGui::IsItemHovered())
								ImGui::SetTooltip(TR("Sends this buffer minus #%03u as a stream with an alpha channel:\n"
									"select the finished image here and keep the scene before the HUD on stream 1.\n"
									"Exact where the interface is opaque, approximate in its soft edges."), clean_id);
						}
					}
				}

				for (const std::unique_ptr<CaptureManager> &capture : ctx.captures)
				{
					const CaptureStatus status = capture->GetStatus();
					if (status.state != CaptureState::Active && status.state != CaptureState::Idle && !status.message.empty())
						ImGui::TextColored(ImVec4(1.0f, 0.6f, 0.2f, 1.0f), TR("Stream %u: %s"), capture->SlotIndex(), status.message.c_str());
				}
				DrawViewModeControls(ctx);

				ImGui::Separator();

				// Details on the left, live GPU preview on the right (visible without scrolling)
				const float details_width = 330.0f;
				if (ImGui::BeginChild("##details", ImVec2(details_width, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_None))
					DrawSelectedDetails(ctx, *selected);
				ImGui::EndChild();
				ImGui::SameLine();
				if (ImGui::BeginChild("##preview", ImVec2(0.0f, 0.0f), ImGuiChildFlags_None, ImGuiWindowFlags_None))
				{
					const uint32_t preview_source = ctx.preview->SourceId();
					if (ctx.preview->IsFrozen())
						ImGui::TextColored(ImVec4(0.4f, 0.8f, 1.0f, 1.0f), TR("Preview frozen%s"), preview_source != selected->id ? TR(" (showing another resource)") : "");
					else if (preview_source != selected->id)
						ImGui::TextDisabled(TR("Preview updating..."));
					else if (ctx.CaptureOf(selected->id) != nullptr)
						ImGui::TextDisabled(TR("Preview = capture output texture (no extra copy)"));
					else
						ImGui::TextDisabled(TR("Preview (GPU copy, %ux%u)"), ctx.preview->Width(), ctx.preview->Height());
					DrawPreviewImage(ctx, ImGui::GetContentRegionAvail().y);
				}
				ImGui::EndChild();
			}
			else
			{
				ImGui::Separator();
				ImGui::TextDisabled(TR("Select a resource above (Previous / Next buttons, Left / Right keys, or click) to preview it."));
			}
		}

		// ------------------------------------------------------------------ Timeline tab
		void DrawTimelineTab(DeviceContext &ctx)
		{
			static bool only_selected = false;
			static bool only_writes = true;
			ImGui::Checkbox(TR("Only selected resource"), &only_selected);
			ImGui::SameLine();
			ImGui::Checkbox(TR("Only writes"), &only_writes);

			const std::vector<TimelineEntry> timeline = ctx.tracker->TimelineSnapshot();
			ImGui::Text(TR("Frame %llu: %u events"), static_cast<unsigned long long>(ctx.tracker->FrameIndex() - 1), static_cast<unsigned int>(timeline.size()));

			if (ImGui::BeginChild("##timeline", ImVec2(0.0f, 400.0f), ImGuiChildFlags_Borders))
			{
				for (const TimelineEntry &entry : timeline)
				{
					if (only_selected && entry.resource_id != ctx.inspector.selected_id)
						continue;
					if (only_writes && !entry.is_write)
						continue;
					const bool highlight = entry.resource_id == ctx.inspector.selected_id;
					if (highlight)
						ImGui::PushStyleColor(ImGuiCol_Text, ImVec4(1.0f, 0.9f, 0.4f, 1.0f));
					ImGui::Text(TR("Event %5u   Resource #%03u   %s%s x%u"), entry.event_index, entry.resource_id, WriteKindName(entry.kind), entry.is_write ? TR(" Write") : TR(" Read"), entry.count);
					if (highlight)
						ImGui::PopStyleColor();
					if (ImGui::IsItemClicked())
						ctx.inspector.Select(entry.resource_id);
				}
			}
			ImGui::EndChild();
		}

		// ------------------------------------------------------------------ Debug tab
		void DrawDebugTab(DeviceContext &ctx)
		{
			bool verbose = log::IsVerbose();
			if (ImGui::Checkbox(TR("Verbose Tracking (log every resource creation, default off)"), &verbose))
			{
				log::SetVerbose(verbose);
				config::SetBool("Verbose", verbose);
			}

			const ResourceTracker::Counters counters = ctx.tracker->GetCounters();
			ImGui::Text(TR("Backend: %s"), ctx.backend.api_name);
			ImGui::Text(TR("Frame index: %llu"), static_cast<unsigned long long>(ctx.tracker->FrameIndex()));
			ImGui::Text(TR("Tracked resources alive: %llu (created %llu, destroyed %llu)"), static_cast<unsigned long long>(counters.alive), static_cast<unsigned long long>(counters.created), static_cast<unsigned long long>(counters.destroyed));
			ImGui::Text(TR("Resource views mapped: %llu"), static_cast<unsigned long long>(counters.views));
			ImGui::Text(TR("Command list merges: %llu, events last frame: %llu"), static_cast<unsigned long long>(counters.merges), static_cast<unsigned long long>(counters.events_last_frame));
			ImGui::Text(TR("Command queues: %u"), static_cast<unsigned int>(ctx.tracker->Queues().size()));
			const resource_desc bb = ctx.inspector.BackbufferDesc();
			ImGui::Text(TR("Back buffer: %ux%u %s"), bb.texture.width, bb.texture.height, FormatName(bb.texture.format));
			ImGui::Text(TR("Sender base name: %s"), SenderBaseName().c_str());
			ImGui::Text(TR("CyGameCaptureRS %s, ReShade API %u"), kVersionString, static_cast<unsigned int>(RESHADE_API_VERSION));
		}

		/**
		* Logo, product name, version and sender, on one line at the top of the window.
		*
		* Scaled on the font rather than in pixels, so it follows ReShade's font size setting the same
		* way the text beside it does.
		*/
		void DrawBrandHeader(DeviceContext &ctx)
		{
			const float size = ImGui::GetFontSize() * 2.4f;
			const resource_view view = ctx.logo->View();
			if (view != 0)
			{
				ImGui::Image(static_cast<ImTextureID>(view.handle), ImVec2(size, size));
				ImGui::SameLine();
			}
			ImGui::BeginGroup();
			ImGui::Text(TR("CyGameCapture  %s"), kVersionString);
			ImGui::TextDisabled(TR("Sender: %s"), SenderBaseName().c_str());
			ImGui::EndGroup();

			// Each language named in itself, so the choice can be found whatever the current one is
			const bool french = i18n::Current() == i18n::Language::French;
			ImGui::SameLine();
			ImGui::SetNextItemWidth(ImGui::CalcTextSize("Francais").x + ImGui::GetFrameHeight() * 2.0f);
			if (ImGui::BeginCombo("##language", french ? "Fran\xc3\xa7" "ais" : "English"))
			{
				if (ImGui::Selectable("English", !french))
					i18n::SetCurrent(i18n::Language::English);
				if (ImGui::Selectable("Fran\xc3\xa7" "ais", french))
					i18n::SetCurrent(i18n::Language::French);
				ImGui::EndCombo();
			}
			if (ImGui::IsItemHovered())
				ImGui::SetTooltip("%s", TR("Language"));

			// Copyright, licence, absence of warranty and where the source is: the legal notices the AGPL
			// expects an interactive program to make available
			ImGui::SameLine();
			if (ImGui::SmallButton(TR("About")))
				ImGui::OpenPopup("cygc_about", 0);
			if (ImGui::BeginPopup("cygc_about", 0))
			{
				ImGui::Text("CyGameCapture %s - %s", kVersionString, kCopyrightString);
				ImGui::TextUnformatted(TR("Free software under the GNU Affero General Public License, version 3 or later."));
				ImGui::TextUnformatted(TR("Provided WITHOUT ANY WARRANTY; see the licence for details."));
				ImGui::Spacing();
				ImGui::TextUnformatted(TR("Source code and licence:"));
				ImGui::TextUnformatted(kProjectUrl);
				if (ImGui::SmallButton(TR("Copy the link")))
					ImGui::SetClipboardText(kProjectUrl);
				ImGui::EndPopup();
			}
		}

		void DrawOverlay(effect_runtime *runtime)
		{
			DeviceContext *const ctx = DeviceContext::From(runtime->get_device());
			if (ctx == nullptr)
			{
				ImGui::TextUnformatted(TR("CyGameCaptureRS: no device context"));
				return;
			}

			DrawBrandHeader(*ctx);
			ImGui::Separator();

			// Tell the present-time code that somebody is looking at the preview
			ctx->preview->MarkWanted(ctx->tracker->FrameIndex());

			// Ctrl+1..4 selects a tab (keyboard only workflow)
			int requested_tab = -1;
			if (ImGui::IsWindowFocused(ImGuiFocusedFlags_RootAndChildWindows) && runtime->is_key_down(VK_CONTROL))
				for (int i = 0; i < 4; ++i)
					if (runtime->is_key_pressed('1' + i) || runtime->is_key_pressed(VK_NUMPAD1 + i))
						requested_tab = i;
			const auto tab_flags = [requested_tab](int index) { return requested_tab == index ? ImGuiTabItemFlags_SetSelected : ImGuiTabItemFlags_None; };

			if (ImGui::BeginTabBar("##cygc_tabs"))
			{
				if (ImGui::BeginTabItem(TR("Buffer Inspector"), nullptr, tab_flags(0)))
				{
					DrawInspectorTab(runtime, *ctx);
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(TR("Capture"), nullptr, tab_flags(1)))
				{
					DrawCaptureTab(*ctx);
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(TR("Timeline"), nullptr, tab_flags(2)))
				{
					DrawTimelineTab(*ctx);
					ImGui::EndTabItem();
				}
				if (ImGui::BeginTabItem(TR("Debug"), nullptr, tab_flags(3)))
				{
					DrawDebugTab(*ctx);
					ImGui::EndTabItem();
				}
				ImGui::EndTabBar();
			}
		}

		// Standalone window: visible as soon as the ReShade overlay opens, no need to dig into the Add-ons tab.
		bool s_standalone_window = true;
		// ReShade keeps calling the reshade_overlay event even while its overlay is closed (as soon as an
		// add-on registered that event), so the open state has to be tracked through reshade_open_overlay.
		bool s_overlay_open = false;

		bool OnReShadeOpenOverlay(effect_runtime *, bool open, input_source)
		{
			s_overlay_open = open;
			return false; // never block the request
		}

		void OnReShadeOverlay(effect_runtime *runtime)
		{
			if (!s_overlay_open)
				return;
			// The discussion has its own window, whether or not the inspector is shown as one
			DrawChatWindow(runtime);
			if (!s_standalone_window)
				return;

			const ImVec2 display = ImGui::GetIO().DisplaySize;
			// Wide enough that the filter columns and the resource table are not cut off; the labels
			// scale with ReShade's font, so the old 700 px turned into overlapping text on a 1440p screen.
			const ImVec2 size(std::min(1000.0f, display.x * 0.6f), std::min(900.0f, display.y - 40.0f));
			ImGui::SetNextWindowSize(size, ImGuiCond_FirstUseEver);
			ImGui::SetNextWindowPos(ImVec2(display.x - size.x - 20.0f, 20.0f), ImGuiCond_FirstUseEver);

			bool open = true;
			char title[128];
			snprintf(title, sizeof(title), "%s###cygc_buffer_inspector", TR("CyGameCaptureRS - Buffer Inspector"));
			if (ImGui::Begin(title, &open))
				DrawOverlay(runtime);
			ImGui::End();

			if (!open)
			{
				s_standalone_window = false;
				config::SetBool("StandaloneWindow", false);
			}
		}

		// Settings section shown in ReShade's add-on list
		void DrawSettings(effect_runtime *runtime)
		{
			if (DeviceContext *const ctx = DeviceContext::From(runtime->get_device()))
			{
				DrawBrandHeader(*ctx);
				ImGui::Separator();
			}

			if (ImGui::Checkbox(TR("Show CyGameCaptureRS window when the overlay is open"), &s_standalone_window))
				config::SetBool("StandaloneWindow", s_standalone_window);

			bool verbose = log::IsVerbose();
			if (ImGui::Checkbox(TR("Verbose tracking log"), &verbose))
			{
				log::SetVerbose(verbose);
				config::SetBool("Verbose", verbose);
			}
		}
	}

	void RegisterOverlays()
	{
		s_standalone_window = config::GetBool("StandaloneWindow", true);
		reshade::register_event<reshade::addon_event::reshade_open_overlay>(OnReShadeOpenOverlay);
		reshade::register_event<reshade::addon_event::reshade_overlay>(OnReShadeOverlay);
		reshade::register_overlay("CyGameCaptureRS", DrawOverlay);
		reshade::register_overlay(nullptr, DrawSettings);
	}

	void UnregisterOverlays()
	{
		reshade::unregister_event<reshade::addon_event::reshade_open_overlay>(OnReShadeOpenOverlay);
		reshade::unregister_event<reshade::addon_event::reshade_overlay>(OnReShadeOverlay);
		reshade::unregister_overlay("CyGameCaptureRS", DrawOverlay);
		reshade::unregister_overlay(nullptr, DrawSettings);
	}
}
