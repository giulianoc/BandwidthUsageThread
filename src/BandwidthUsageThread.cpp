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

#include "BandwidthUsageThread.h"

#include "System.h"

#include <nlohmann/detail/exceptions.hpp>
#include "ThreadLogger.h"

BandwidthUsageThread::BandwidthUsageThread(const std::optional<std::string> &interfaceNameToMonitor,
	const std::shared_ptr<spdlog::logger>& logger):
	_running(false), _stopSignal(false), _logger(logger)
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

BandwidthUsageThread::~BandwidthUsageThread()
{
	if (isRunning())
		stop();
}

void BandwidthUsageThread::start()
{
	if (_running)
	{
		const std::string errorMessage = "BandwidthUsageThread already running";
		LOG_ERROR(errorMessage);
		throw std::runtime_error(errorMessage);
	}

	_stopSignal = false;
	_thread = std::thread(&BandwidthUsageThread::run, this);
	_running = true;
}

void BandwidthUsageThread::stop()
{
	if (_running)
	{
		_stopSignal = true;
		if (_thread.joinable())
			_thread.join();
	}
	_running = false;
}

bool BandwidthUsageThread::isRunning() const
{
	return _running;
}

void BandwidthUsageThread::run()
{
	std::optional<ThreadLogger> threadLogger;
	if (_logger)
		threadLogger.emplace(_logger);

	while (!_stopSignal)
	{
		// non serve lo sleep perchè lo sleep è già all'interno di System::getBandwidthInBytes
		// this_thread::sleep_for(chrono::seconds(_bandwidthUsagePeriodInSeconds));

		// aggiorniamo la banda usata da questo server. Ci server per rispondere alla API .../bandwidthUsage
		uint64_t txAvgBandwidthUsage = 0; // bytes
		uint64_t rxAvgBandwidthUsage = 0;
		uint64_t txPeakBandwidthUsage = 0;
		uint64_t rxPeakBandwidthUsage = 0;
		try
		{
			// impieghera' 15 secs
			// Ritorna la banda media secondo i parametri specificati ed anche i picchi
			std::map<std::string, std::tuple<uint64_t, uint64_t, uint64_t, uint64_t>> avgAndPeakBandwidthInBytes =
				System::getAvgAndPeakBandwidthInBytes(2, 5);

			bool networkInterfaceToMonitorFound = false;
			for (const auto &[iface, stats] : avgAndPeakBandwidthInBytes)
			{
				auto [rxAvg, txAvg, rxPeak, txPeak] = stats;
				LOG_INFO(
					"bandwidthUsageThread, avgBandwidthInMbps"
					", iface: {}"
					", rxAvg: {} ({}Mbps)"
					", txAvg: {} ({}Mbps)"
					", peakRx: {} ({}Mbps)"
					", peakTx: {} ({}Mbps)",
					iface, rxAvg, static_cast<uint32_t>((rxAvg * 8) / 1000000), txAvg, static_cast<uint32_t>((txAvg * 8) / 1000000),
					rxPeak, static_cast<uint32_t>((rxPeak * 8) / 1000000), txPeak, static_cast<uint32_t>((txPeak * 8) / 1000000)
				);
				if (_networkInterfaceToMonitor == iface)
				{
					rxAvgBandwidthUsage = rxAvg;
					txAvgBandwidthUsage = txAvg;
					rxPeakBandwidthUsage = rxPeak;
					txPeakBandwidthUsage = txPeak;
					networkInterfaceToMonitorFound = true;

					// messaggio usato da servicesStatusLibrary::mms_delivery_check_bandwidth_usage
					LOG_INFO(
						"bandwidthUsageThread, peakBandwidthInMbps"
						", iface: {}"
						", rxPeak: @{}@Mbps"
						", txPeak: @{}@Mbps",
						iface, static_cast<uint32_t>((rxPeak * 8) / 1000000), static_cast<uint32_t>((txPeak * 8) / 1000000)
					);
					// break; commentato in modo da avere sempre il log della banda usata da tutte le reti (public e internal)
				}
			}
			if (!networkInterfaceToMonitorFound)
				LOG_WARN(
					"bandwidthUsageThread, getAvgAndPeakBandwidthInBytes"
					", networkInterfaceToMonitor not found"
					", _networkInterfaceToMonitor: {}",
					_networkInterfaceToMonitor
				);
			else
			{
				_rxAvgBandwidthUsage.store(rxAvgBandwidthUsage, std::memory_order_relaxed);
				_txAvgBandwidthUsage.store(txAvgBandwidthUsage, std::memory_order_relaxed);
				_rxPeakBandwidthUsage.store(rxPeakBandwidthUsage, std::memory_order_relaxed);
				_txPeakBandwidthUsage.store(txPeakBandwidthUsage, std::memory_order_relaxed);
				LOG_INFO(
					"bandwidthUsageThread, bandwidthInMbps"
					", rxAvgBandwidthUsage: @{}@Mbps"
					", txAvgBandwidthUsage: @{}@Mbps"
					", rxPeakBandwidthUsage: @{}@Mbps"
					", txPeakBandwidthUsage: @{}@Mbps",
					static_cast<uint32_t>((rxAvgBandwidthUsage * 8) / 1000000),
					static_cast<uint32_t>((txAvgBandwidthUsage * 8) / 1000000),
					static_cast<uint32_t>((rxPeakBandwidthUsage * 8) / 1000000),
					static_cast<uint32_t>((txPeakBandwidthUsage * 8) / 1000000)
				);

				try
				{
					newBandwidthUsageAvailable(rxAvgBandwidthUsage, txAvgBandwidthUsage, rxPeakBandwidthUsage, txPeakBandwidthUsage);
				}
				catch (std::exception &e)
				{
					LOG_ERROR("newBandwidthUsageAvailable failed"
						", exception: {}",
						e.what()
					);
				}
			}
		}
		catch (std::exception &e)
		{
			LOG_ERROR(
				"System::getBandwidthInMbps failed"
				", exception: {}",
				e.what()
			);
		}

		// inizializziamo la struttura BandwidthStats
		try
		{
			// addSample logs when a new day is started
			_txBandwidthStats.addSample(txAvgBandwidthUsage, std::chrono::system_clock::now());
			_rxBandwidthStats.addSample(rxAvgBandwidthUsage, std::chrono::system_clock::now());
		}
		catch (std::exception &e)
		{
			LOG_ERROR(
				"_bandwidthStats.addSample failed"
				", exception: {}",
				e.what()
			);
		}
	}
}

std::pair<uint64_t, uint64_t> BandwidthUsageThread::getAvgBandwidthUsage() const {
	return std::make_pair(_txAvgBandwidthUsage.load(std::memory_order_relaxed), _rxAvgBandwidthUsage.load(std::memory_order_relaxed));
};

void BandwidthUsageThread::newBandwidthUsageAvailable(uint64_t& rxAvgBandwidthUsage, uint64_t& txAvgBandwidthUsage,
	uint64_t& rxPeakBandwidthUsage, uint64_t& txPeakBandwidthUsage) const
{
	// default implementation does nothing
}
