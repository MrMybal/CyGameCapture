// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "SpoutSender.hpp"
#include "Addon/Log.hpp"

#include <SpoutSenderNames.h>
#include <SpoutFrameCount.h>

#include <cstring>

namespace cygc
{
	SpoutSender::SpoutSender() :
		names_(std::make_unique<spoutSenderNames>()),
		frame_(std::make_unique<spoutFrameCount>())
	{
	}

	SpoutSender::~SpoutSender()
	{
		Release();
	}

	std::string SpoutSender::MakeUniqueName(const std::string &base_name, unsigned int first_index)
	{
		spoutSenderNames names;
		unsigned int index = first_index < 1 ? 1 : first_index;
		std::string candidate = index == 1 ? base_name : base_name + "::" + std::to_string(index);
		while (names.FindSenderName(candidate.c_str()) && index < 64)
		{
			++index;
			candidate = base_name + "::" + std::to_string(index);
		}
		return candidate;
	}

	int SpoutSender::MaxSenders()
	{
		spoutSenderNames names;
		return names.GetMaxSenders();
	}

	int SpoutSender::ActiveSenderCount()
	{
		spoutSenderNames names;
		return names.GetSenderCount();
	}

	bool SpoutSender::CanRegisterNewSender(std::string *reason)
	{
		spoutSenderNames names;
		const int max_senders = names.GetMaxSenders();
		const int count = names.GetSenderCount();
		if (count < max_senders)
			return true;
		if (reason != nullptr)
			*reason = "Spout's sender table is full (" + std::to_string(count) + " of " + std::to_string(max_senders) +
				" senders on this machine, all applications included). Raise HKCU\Software\Leading Edge\Spout\MaxSenders or stop a sender.";
		return false;
	}

	bool SpoutSender::Create(const std::string &name, uint32_t width, uint32_t height, HANDLE share_handle, uint32_t dxgi_format)
	{
		Release();

		// Spout refuses names silently once its machine-wide table is full: check explicitly
		std::string capacity_reason;
		if (!CanRegisterNewSender(&capacity_reason))
		{
			log::Error("%s", capacity_reason.c_str());
			return false;
		}

		char name_buffer[256] = {};
		strncpy_s(name_buffer, name.c_str(), _TRUNCATE);

		if (!names_->CreateSender(name_buffer, width, height, share_handle, dxgi_format))
		{
			log::Error("Spout sender registration failed for '%s'", name.c_str());
			return false;
		}

		frame_->CreateAccessMutex(name_buffer);
		frame_->EnableFrameCount(name_buffer);

		name_ = name;
		width_ = width;
		height_ = height;
		format_ = dxgi_format;
		created_ = true;

		log::Info("Spout sender started: '%s' %ux%u format=%u handle=0x%p", name_.c_str(), width, height, dxgi_format, share_handle);
		return true;
	}

	bool SpoutSender::Update(uint32_t width, uint32_t height, HANDLE share_handle, uint32_t dxgi_format)
	{
		if (!created_)
			return false;
		if (!names_->UpdateSender(name_.c_str(), width, height, share_handle, dxgi_format))
		{
			log::Error("Spout sender update failed for '%s'", name_.c_str());
			return false;
		}
		width_ = width;
		height_ = height;
		format_ = dxgi_format;
		log::Info("Spout sender updated: '%s' %ux%u format=%u", name_.c_str(), width, height, dxgi_format);
		return true;
	}

	void SpoutSender::Release()
	{
		if (!created_)
			return;
		frame_->CleanupFrameCount();
		names_->ReleaseSenderName(name_.c_str());
		log::Info("Spout sender released: '%s'", name_.c_str());
		created_ = false;
		name_.clear();
		width_ = height_ = format_ = 0;
	}

	bool SpoutSender::BeginFrame()
	{
		if (!created_)
			return false;
		return frame_->CheckAccess();
	}

	void SpoutSender::EndFrame()
	{
		if (!created_)
			return;
		frame_->SetNewFrame();
		frame_->AllowAccess();
	}
}
