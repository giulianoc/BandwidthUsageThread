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

#include "BandwidthPercentileThread.h"

#include "System.h"

#include "ThreadLogger.h"

BandwidthPercentileThread::BandwidthPercentileThread(const std::optional<std::string> &interfaceNameToMonitor,
	const int16_t windowInSeconds, const double percentile, const std::shared_ptr<spdlog::logger>& logger):
	_windowInSeconds(windowInSeconds), _percentile(percentile), _running(false), _stopSignal(false), _logger(logger)
{
	try
	{
		std::vector<std::tuple<std::string, std::string, bool, std::string>> nativeNetworkInterfaces = System::getActiveNetworkInterface();
		for (const auto &[interfaceName, interfaceType, privateIp, ipAddress] : nativeNetworkInterfaces)
		{
			LOG_INFO(
				"getActiveNetworkInterface"
				", interface name: {}"
				", interface type: {}"
				", private ip: {}"
				", ip address: {}",
				interfaceName, interfaceType, privateIp, ipAddress
			);
			if (_networkInterfaceToMonitor.empty())
			{
				if (interfaceNameToMonitor)
				{
					if (interfaceNameToMonitor && *interfaceNameToMonitor == interfaceName)
						_networkInterfaceToMonitor = interfaceName;
				}
				else
				{
					// by default, monitor the first public IPv4 interface
					if (interfaceType == "IPv4" && !privateIp)
						_networkInterfaceToMonitor = interfaceName;
				}
			}
		}
		LOG_INFO(
			"getActiveNetworkInterface"
			", _networkInterfaceToMonitor: {}",
			_networkInterfaceToMonitor
		);
	}
	catch (std::exception &e)
	{
		LOG_ERROR(
			"System::getActiveNetworkInterface failed"
			", exception: {}",
			e.what()
		);
	}
}

BandwidthPercentileThread::~BandwidthPercentileThread()
{
	if (isRunning())
		stop();
}

void BandwidthPercentileThread::start()
{
	if (_running)
	{
		const std::string errorMessage = "BandwidthPercentileThread already running";
		LOG_ERROR(errorMessage);
		throw std::runtime_error(errorMessage);
	}

	_stopSignal = false;
	_thread = std::thread(&BandwidthPercentileThread::run, this);
	_running = true;
}

void BandwidthPercentileThread::stop()
{
	if (_running)
	{
		_stopSignal = true;
		if (_thread.joinable())
			_thread.join();
	}
	_running = false;
}

bool BandwidthPercentileThread::isRunning() const
{
	return _running;
}

void BandwidthPercentileThread::run()
{
	std::optional<ThreadLogger> threadLogger;
	if (_logger)
		threadLogger.emplace(_logger);

	std::size_t capacity = 2880; // ad es. 24h a finestre da 30s -> 2880 campioni
	std::deque<double> rxSamplesBps;
	std::deque<double> txSamplesBps;

	using clock = std::chrono::steady_clock;
	constexpr auto percentilePeriod = std::chrono::minutes(5);
	auto nextPercentileAt = clock::now() + percentilePeriod;

	auto before = System::getNetworkUsage();
	auto tBefore = std::chrono::steady_clock::now();

	while (!_stopSignal)
	{
		try
		{
			std::this_thread::sleep_for(std::chrono::seconds(_windowInSeconds));

			auto after = System::getNetworkUsage();
			auto tAfter = std::chrono::steady_clock::now();
			std::chrono::duration<double> elapsed = tAfter - tBefore;

			// ritorna bytes/sec per iface tra due letture distanziate di elapsedSeconds
			auto rates = System::bandwidthBetween(before, after, elapsed.count());

			auto it = rates.find(_networkInterfaceToMonitor);
			if (it != rates.end())
			{
				const auto& [rxAvgBandwidthUsage, txAvgBandwidthUsage] = it->second;

				LOG_INFO(
					"BandwidthPercentileThread, bandwidthUsageInMbps"
					", networkInterfaceToMonitor: {}"
					", windowInSeconds: {}"
					", rxAvgBandwidthUsage: @{}@Mbps"
					", txAvgBandwidthUsage: @{}@Mbps",
					_networkInterfaceToMonitor, _windowInSeconds,
					static_cast<uint32_t>((rxAvgBandwidthUsage * 8) / 1000000), static_cast<uint32_t>((txAvgBandwidthUsage * 8) / 1000000)
				);

				if (rxSamplesBps.size() == capacity)
					rxSamplesBps.pop_front(); // rimuove il più vecchio
				rxSamplesBps.push_back(rxAvgBandwidthUsage);

				if (txSamplesBps.size() == capacity)
					txSamplesBps.pop_front(); // rimuove il più vecchio
				txSamplesBps.push_back(txAvgBandwidthUsage);

				// calcolo il percentile solo ogni XXX minutes
				if (const auto now = clock::now(); now >= nextPercentileAt)
				{
					nextPercentileAt += percentilePeriod;

					double rxPercentileBandwidthUsage = percentileNearestRank(rxSamplesBps, _percentile);
					double txPercentileBandwidthUsage = percentileNearestRank(txSamplesBps, _percentile);

					_rxPercentileBandwidthUsage.store(rxPercentileBandwidthUsage, std::memory_order_relaxed);
					_txPercentileBandwidthUsage.store(txPercentileBandwidthUsage, std::memory_order_relaxed);

					// messaggio usato da servicesStatusLibrary::mms_delivery_check_bandwidth_usage
					LOG_INFO(
						"BandwidthPercentileThread, bandwidthInMbps"
						", percentile: {}"
						", rxPercentileBandwidthUsage: @{}@Mbps"
						", txPercentileBandwidthUsage: @{}@Mbps", _percentile,
						static_cast<uint32_t>((rxPercentileBandwidthUsage * 8) / 1000000),
						static_cast<uint32_t>((txPercentileBandwidthUsage * 8) / 1000000)
					);

					try
					{
						newPercentileBandwidthAvailable(rxPercentileBandwidthUsage, txPercentileBandwidthUsage);
					}
					catch (std::exception &e)
					{
						LOG_ERROR("BandwidthPercentileThread, newPercentileBandwidthAvailable failed"
							", exception: {}",
							e.what()
						);
					}
				}
			}
			else
				LOG_WARN(
					"BandwidthPercentileThread, networkInterfaceToMonitor not found"
					", _networkInterfaceToMonitor: {}",
					_networkInterfaceToMonitor
				);

			before = std::move(after);
			tBefore = tAfter;
		}
		catch (std::exception &e)
		{
			LOG_ERROR(
				"BandwidthPercentileThread failed"
				", exception: {}",
				e.what()
			);
		}
	}
}

double BandwidthPercentileThread::percentileNearestRank(std::deque<double> samples, double p)
{
	if (samples.empty())
		return 0.0;
	if (p <= 0.0)
		return *std::ranges::min_element(samples);
	if (p >= 1.0)
		return *std::ranges::max_element(samples);

	const std::size_t N = samples.size();
	// nearest-rank: k = ceil(p*N) (1-based), index = k-1 (0-based)
	std::size_t idx = static_cast<std::size_t>(std::ceil(p * N)) - 1;
	if (idx >= N)
		idx = N - 1;

	std::ranges::nth_element(samples, samples.begin() + idx);
	return samples[idx];
}

std::pair<double, double> BandwidthPercentileThread::getPercentileBandwidthUsage() const {
	return std::make_pair(_rxPercentileBandwidthUsage.load(std::memory_order_relaxed), _txPercentileBandwidthUsage.load(std::memory_order_relaxed));
};

void BandwidthPercentileThread::newPercentileBandwidthAvailable(double& rxPercentileBandwidthUsage, double& txPercentileBandwidthUsage) const
{
	// default implementation does nothing
}
