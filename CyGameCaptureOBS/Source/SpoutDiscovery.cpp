// Copyright (C) 2026 Cyberalien
// SPDX-License-Identifier: AGPL-3.0-or-later

#include "SpoutDiscovery.hpp"

#include <SpoutSenderNames.h>
#include <SpoutFrameCount.h>

#include <algorithm>
#include <mutex>
#include <set>

namespace cygc::obs
{
	namespace
	{
		// One shared-memory view of the sender table for the whole plugin: opening it is not free and
		// several sources enumerate on the same (OBS UI) thread.
		std::mutex &EnumMutex()
		{
			static std::mutex mutex;
			return mutex;
		}

		spoutSenderNames &EnumNames()
		{
			static spoutSenderNames names;
			return names;
		}

		bool ReadDescription(spoutSenderNames &names, const std::string &name, SenderDescription &out)
		{
			unsigned int width = 0;
			unsigned int height = 0;
			HANDLE handle = nullptr;
			DWORD format = 0;
			if (!names.GetSenderInfo(name.c_str(), width, height, handle, format))
				return false;

			out.name = name;
			out.width = width;
			out.height = height;
			out.dxgi_format = static_cast<uint32_t>(format);
			out.share_handle = handle;
			return true;
		}
	}

	std::vector<SenderDescription> EnumerateSenders()
	{
		std::vector<SenderDescription> result;

		std::lock_guard<std::mutex> lock(EnumMutex());
		spoutSenderNames &names = EnumNames();

		std::set<std::string> name_set;
		if (!names.GetSenderNames(&name_set))
			return result;

		result.reserve(name_set.size());
		for (const std::string &name : name_set)
		{
			SenderDescription description;
			if (ReadDescription(names, name, description))
				result.push_back(std::move(description));
		}

		std::sort(result.begin(), result.end(), [](const SenderDescription &a, const SenderDescription &b) { return a.name < b.name; });
		return result;
	}

	bool GetSenderDescription(const std::string &name, SenderDescription &out)
	{
		if (name.empty())
			return false;

		std::lock_guard<std::mutex> lock(EnumMutex());
		return ReadDescription(EnumNames(), name, out);
	}

	bool GetActiveSenderName(std::string &out)
	{
		std::lock_guard<std::mutex> lock(EnumMutex());
		char buffer[256] = {};
		if (!EnumNames().GetActiveSender(buffer))
			return false;
		out = buffer;
		return !out.empty();
	}

	int MaxSenders()
	{
		std::lock_guard<std::mutex> lock(EnumMutex());
		return EnumNames().GetMaxSenders();
	}

	int ActiveSenderCount()
	{
		std::lock_guard<std::mutex> lock(EnumMutex());
		return EnumNames().GetSenderCount();
	}

	// -------------------------------------------------------------------------------------------
	struct SenderAccess::Impl
	{
		spoutFrameCount frame;
	};

	SenderAccess::SenderAccess()
		: impl_(new Impl())
	{
	}

	SenderAccess::~SenderAccess()
	{
		Close();
		delete impl_;
	}

	bool SenderAccess::Open(const std::string &name)
	{
		Close();
		if (name.empty())
			return false;

		if (!impl_->frame.CreateAccessMutex(name.c_str()))
			return false;
		impl_->frame.EnableFrameCount(name.c_str());

		name_ = name;
		open_ = true;
		return true;
	}

	void SenderAccess::Close()
	{
		if (!open_)
			return;
		impl_->frame.CloseAccessMutex();
		impl_->frame.CleanupFrameCount();
		name_.clear();
		open_ = false;
	}

	bool SenderAccess::BeginAccess()
	{
		return open_ && impl_->frame.CheckAccess();
	}

	void SenderAccess::EndAccess()
	{
		if (open_)
			impl_->frame.AllowAccess();
	}

	bool SenderAccess::HasNewFrame()
	{
		return open_ && impl_->frame.GetNewFrame();
	}

	double SenderAccess::SenderFps() const
	{
		return open_ ? impl_->frame.GetSenderFps() : 0.0;
	}
}
