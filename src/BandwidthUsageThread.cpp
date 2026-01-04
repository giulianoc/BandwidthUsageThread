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

#include <spdlog/spdlog.h>

BandwidthUsageThread::BandwidthUsageThread(): _running(false), _stopSignal(false)
{
	try
	{
		std::vector<std::tuple<std::string, std::string, bool, std::string>> nativeNetworkInterfaces = System::getActiveNetworkInterface();
		for (const auto &[interfaceName, interfaceType, privateIp, ipAddress] : nativeNetworkInterfaces)
		{
			SPDLOG_INFO(
				"getActiveNetworkInterface"
				", interface name: {}"
				", interface type: {}"
				", private ip: {}"
				", ip address: {}",
				interfaceName, interfaceType, privateIp, ipAddress
			);
			if (interfaceType != "IPv4" || privateIp)
				continue; // rete interna
			_networkInterfaceToMonitor = interfaceName;
		}
		SPDLOG_INFO(
			"getActiveNetworkInterface"
			", _networkInterfaceToMonitor: {}",
			_networkInterfaceToMonitor
		);
	}
	catch (std::exception &e)
	{
		SPDLOG_ERROR(
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
		SPDLOG_ERROR(errorMessage);
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
	while (!_stopSignal)
	{
		// non serve lo sleep perchè lo sleep è già all'interno di System::getBandwidthInBytes
		// this_thread::sleep_for(chrono::seconds(_bandwidthUsagePeriodInSeconds));

		// aggiorniamo la banda usata da questo server. Ci server per rispondere alla API .../bandwidthUsage
		uint64_t avgBandwidthUsage = 0;
		try
		{
			// impieghera' 15 secs
			// Ritorna la banda media secondo i parametri specificati ed anche i picchi
			std::map<std::string, std::pair<uint64_t, uint64_t>> peakInBytes;
			std::map<std::string, std::pair<uint64_t, uint64_t>> avgBandwidth =
				System::getAvgAndPeakBandwidthInBytes(peakInBytes, 2, 5);

			bool networkInterfaceToMonitorFound = false;
			for (const auto &[iface, stats] : avgBandwidth)
			{
				auto [rx, tx] = stats;
				SPDLOG_INFO(
					"bandwidthUsageThread, avgBandwidthInMbps"
					", iface: {}"
					", rx: {} ({}Mbps)"
					", tx: {} ({}Mbps)",
					iface, rx, static_cast<uint32_t>((rx * 8) / 1000000), tx, static_cast<uint32_t>((tx * 8) / 1000000)
				);
				if (_networkInterfaceToMonitor == iface)
				{
					avgBandwidthUsage = tx;
					networkInterfaceToMonitorFound = true;
					// break; commentato in modo da avere sempre il log della banda usata da tutte le reti (public e internal)
				}
			}
			if (!networkInterfaceToMonitorFound)
				SPDLOG_WARN(
					"bandwidthUsageThread, getAvgAndPeakBandwidthInBytes"
					", networkInterfaceToMonitor not found"
					", _networkInterfaceToMonitor: {}",
					_networkInterfaceToMonitor
				);
			else
				_avgBandwidthUsage.store(avgBandwidthUsage, std::memory_order_relaxed);
			SPDLOG_INFO(
				"bandwidthUsageThread, avgBandwidthInMbps"
				", avgBandwidthUsage: @{}@Mbps",
				static_cast<uint32_t>((avgBandwidthUsage * 8) / 1000000)
			);

			// loggo il picco
			for (const auto &[iface, stats] : peakInBytes)
			{
				if (_networkInterfaceToMonitor == iface)
				{
					auto [peakRx, peakTx] = stats;
					// messaggio usato da servicesStatusLibrary::mms_delivery_check_bandwidth_usage
					SPDLOG_INFO(
						"bandwidthUsageThread, peakBandwidthInMbps"
						", iface: {}"
						", peakTx: @{}@Mbps",
						iface, static_cast<uint32_t>((peakTx * 8) / 1000000)
					);
					break;
				}
			}
		}
		catch (std::exception &e)
		{
			SPDLOG_ERROR(
				"System::getBandwidthInMbps failed"
				", exception: {}",
				e.what()
			);
		}

		// inizializziamo la struttura BandwidthStats
		try
		{
			// addSample logs when a new day is started
			_bandwidthStats.addSample(avgBandwidthUsage, std::chrono::system_clock::now());
		}
		catch (std::exception &e)
		{
			SPDLOG_ERROR(
				"_bandwidthStats.addSample failed"
				", exception: {}",
				e.what()
			);
		}
	}
}

uint64_t BandwidthUsageThread::getAvgBandwidthUsage() const {
	return _avgBandwidthUsage.load(std::memory_order_relaxed);
};
