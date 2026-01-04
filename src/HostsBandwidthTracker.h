
#pragma once

#include "nlohmann/json.hpp"
#include <optional>
#include <string>
#include <thread>
#include <unordered_map>
#include <unordered_set>

// Manage a group of hosts.
// Each host can be marked as running or not, has a bandwidth usage associated to it and a correction field.
// The correction is useful to artificially increase or decrease the bandwidth of a host, this is useful
// when the host is also used for other purposes, for example as a storage and we want to decrease its usage as delivery server.
// The class allows to update the hosts, their bandwidth and to get the host with the minimum bandwidth usage.
// Thread safe.
class HostsBandwidthTracker
{
  public:
	HostsBandwidthTracker() = default;
	~HostsBandwidthTracker() = default;

	void updateHosts(const nlohmann::json& hostAndRunningRoot);

	// Aggiorna (sovrascrive) la banda per un dato host
	void setBandwidth(const std::string &host, uint64_t bandwidth);

	// Aggiunge banda (cumulativamente)
	void addBandwidth(const std::string &host, uint64_t bandwidth);

	std::optional<std::string> getMinBandwidthHost();

	void fillWithRunningHosts(std::unordered_set<std::string> &hosts);

  private:
	std::mutex _trackerMutex;
	// associa ad ogni host le seguenti informazioni: running, bandwidthUsage
	// e bandwidthCorrection (se il server di delivery fa anche da storage potrebbe essere utile banda fittizia perchè sia servito di meno)
	std::unordered_map<std::string, std::tuple<bool, uint64_t, int64_t>> _bandwidthMap;
};
