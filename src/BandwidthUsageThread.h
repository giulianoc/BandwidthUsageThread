/*
Copyright (C) Giuliano Catrambone (giulianocatrambone@gmail.com)

 This program is free software; you can redistribute it and/or
 modify it under the terms of the GNU General Public License
 as published by the Free Software Foundation; either
 version 2 of the License, or (at your option) any later
 version.

 This program is distributed in the hope that it will be useful,
 but WITHOUT ANY WARRANTY; without even the implied warranty of
 MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 GNU General Public License for more details.

 You should have received a copy of the GNU General Public License
 along with this program; if not, write to the Free Software
 Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

 Commercial use other than under the terms of the GNU General Public
 License is allowed only after express negotiation of conditions
 with the authors.
*/

#pragma once
#include "BandwidthStats.h"

#include <spdlog/logger.h>
#include <thread>

class BandwidthUsageThread
{
public:
	BandwidthUsageThread(const std::optional<std::string> &interfaceNameToMonitor = std::nullopt,
		const std::shared_ptr<spdlog::logger>& logger = nullptr);
	virtual ~BandwidthUsageThread();

	void start();
	void stop();
	bool isRunning() const;

	std::pair<uint64_t, uint64_t> getAvgBandwidthUsage() const;

private:
	std::thread _thread;
	std::atomic<bool> _running;
	std::atomic<bool> _stopSignal;
	std::string _networkInterfaceToMonitor;
	std::shared_ptr<spdlog::logger> _logger;

	std::atomic<uint64_t> _txAvgBandwidthUsage;
	std::atomic<uint64_t> _rxAvgBandwidthUsage;
	std::atomic<uint64_t> _txPeakBandwidthUsage;
	std::atomic<uint64_t> _rxPeakBandwidthUsage;
	BandwidthStats _txBandwidthStats{false};
	BandwidthStats _rxBandwidthStats{true};

	void run();

	virtual void newBandwidthUsageAvailable(uint64_t& rxAvgBandwidthUsage, uint64_t& txAvgBandwidthUsage,
		uint64_t& rxPeakBandwidthUsage, uint64_t& txPeakBandwidthUsage) const;

};

