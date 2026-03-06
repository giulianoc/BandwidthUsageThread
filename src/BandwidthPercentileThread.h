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
#include <spdlog/logger.h>
#include <deque>
#include <thread>

class BandwidthPercentileThread
{
public:
	BandwidthPercentileThread(const std::optional<std::string> &interfaceNameToMonitor = std::nullopt,
		const int16_t windowInSeconds = 30, const double percentile = 0.95, const int16_t percentilePeriodInSeconds = 60,
		const std::shared_ptr<spdlog::logger>& logger = nullptr);
	virtual ~BandwidthPercentileThread();

	void start();
	void stop();
	bool isRunning() const;

	std::pair<double, double> getPercentileBandwidthUsage() const;

private:
	std::thread _thread;
	int16_t _windowInSeconds;
	double _percentile;
	int16_t _percentilePeriodInSeconds;
	std::atomic<bool> _running;
	std::atomic<bool> _stopSignal;
	std::string _networkInterfaceToMonitor;
	std::shared_ptr<spdlog::logger> _logger;

	std::atomic<double> _rxPercentileBandwidthUsage;
	std::atomic<double> _txPercentileBandwidthUsage;
	std::atomic<double> _rxPeakBandwidthUsage;
	std::atomic<double> _txPeakBandwidthUsage;

	void run();

	static std::pair<double, double> percentileNearestRank(std::deque<double> samples, double p);

	virtual void newBandwidthStatsAvailable(double &rxPercentileBandwidthUsage, double &txPercentileBandwidthUsage,
		double &rxPeakBandwidthUsage, double &txPeakBandwidthUsage);
};

