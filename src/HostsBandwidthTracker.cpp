#include "HostsBandwidthTracker.h"
#include "JsonPath.h"
#include "spdlog/spdlog.h"
#include <optional>
#include <set>
#include <unordered_set>
#include <utility>

using namespace std;
using json = nlohmann::json;
using ordered_json = nlohmann::ordered_json;

// It updates the internal hosts and the running flag according to the given json array.
// The json array must contain objects with the following fields:
// - host: string
// - running: bool
// - bandwidthCorrection: int64_t (optional, default is 0)
void HostsBandwidthTracker::updateHosts(const json& hostAndRunningRoot)
{
	lock_guard locker(_trackerMutex);

	unordered_set<string> updatedHosts;

	// inizializza updatedHosts con gli hosts attuali
	// if host is not present: add to _bandwidthMap
	// if host is present: update running of _bandwidthMap
	for (const auto& hostRoot : hostAndRunningRoot)
	{
		auto host = JsonPath(&hostRoot)["host"].as<string>();
		auto running = JsonPath(&hostRoot)["running"].as<bool>();
		auto bandwidthCorrection = JsonPath(&hostRoot)["bandwidthCorrection"].as<int64_t>(0);

		updatedHosts.insert(host);

		if (auto it = _bandwidthMap.find(host); it == _bandwidthMap.end())
			_bandwidthMap[host] = make_tuple(running, 0, bandwidthCorrection); // insert
		else
			get<0>(it->second) = running; // update
	}

	// remove from _bandwidthMap if host is not present anymore
	for (auto it = _bandwidthMap.begin(); it != _bandwidthMap.end();)
	{

		// erase(it) restituisce l’iteratore al prossimo elemento valido → non serve fare ++it in quel caso.
		if (const string &host = it->first; updatedHosts.find(host) == updatedHosts.end())
			it = _bandwidthMap.erase(it); // remove
		else
			++it; // Avanza solo se non si cancella
	}
}

// it adds bandwidth to the given host
void HostsBandwidthTracker::addBandwidth(const string &host, const uint64_t bandwidth)
{
	lock_guard locker(_trackerMutex);
	auto it = _bandwidthMap.find(host);
	if (it != _bandwidthMap.end())
		get<1>(it->second) += bandwidth;
}

// it set the bandwidth for a given host
void HostsBandwidthTracker::setBandwidth(const string &host, const uint64_t bandwidth)
{
	lock_guard locker(_trackerMutex);

	auto it = _bandwidthMap.find(host);
	if (it != _bandwidthMap.end())
		get<1>(it->second) = bandwidth;
}

// it fills the given set with the running hosts
void HostsBandwidthTracker::fillWithRunningHosts(unordered_set<string> &hosts)
{
	lock_guard locker(_trackerMutex);

	for (const auto &[host, bandwidthDetails] : _bandwidthMap)
	{
		bool running;
		tie(running, ignore, ignore) = bandwidthDetails;

		if (running)
			hosts.insert(host);
	}
}

// it returns the host with the minimum bandwidth usage
optional<string> HostsBandwidthTracker::getMinBandwidthHost()
{
	lock_guard locker(_trackerMutex);

	if (_bandwidthMap.empty())
		return nullopt;

	uint64_t minBandwidth = numeric_limits<uint64_t>::max();
	string minHost;

	for (const auto &[host, bandwidthDetails] : _bandwidthMap)
	{
		auto [running, bandwidth, bandwidthCorrection] = bandwidthDetails;

		SPDLOG_INFO(
			"getMinBandwidthHost"
			", host: {}"
			", running: {}"
			", bandwidthCorrection: {}"
			", bandwidth: {} ({} Mbps)",
			host, running, bandwidthCorrection, bandwidth, (bandwidth * 8) / 1000000
		);

		bandwidth += bandwidthCorrection;

		if (running && bandwidth < minBandwidth)
		{
			minBandwidth = bandwidth;
			minHost = host;

			if (bandwidth == 0)
				break; // inutile cercare un host con meno banda
		}
	}

	if (minHost.empty())
		return nullopt;

	SPDLOG_INFO(
		"getMinBandwidthHost"
		", minHost: {}"
		", minBandwidth: {} ({} Mbps)",
		minHost, minBandwidth, (minBandwidth * 8) / 1000000
	);
	return minHost;
}

/*
uint64_t HostBandwidthTracker::getBandwidth(const string &hostname)
{
	lock_guard<mutex> locker(_trackerMutex);
	auto it = _bandwidthMap.find(hostname);
	return (it != _bandwidthMap.end()) ? it->second.second : 0;
}

// Stampa tutti gli host
void HostBandwidthTracker::logDetails() const
{
	for (const auto &[host, bandwidth] : _bandwidthMap)
		SPDLOG_INFO(
			"HostBandwidthTracker"
			", host: {}"
			", bandwidth (bytes): {}",
			host, bandwidth.second
		);
}
*/
