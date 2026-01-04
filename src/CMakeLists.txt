
# Copyright (C) Giuliano Catrambone (giulianocatrambone@gmail.com)

# This program is free software; you can redistribute it and/or 
# modify it under the terms of the GNU General Public License 
# as published by the Free Software Foundation; either 
# version 2 of the License, or (at your option) any later 
# version.

# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.

# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.

# Commercial use other than under the terms of the GNU General Public
# License is allowed only after express negotiation of conditions
# with the authors.

SET (SOURCES
		BandwidthStats.cpp
		BandwidthUsageThread.cpp
		HostsBandwidthTracker.cpp
)

SET (HEADERS
		BandwidthStats.h
		BandwidthUsageThread.h
		HostsBandwidthTracker.h
)

include_directories("${SPDLOG_INCLUDE_DIR}")
include_directories("${DATETIME_INCLUDE_DIR}")
include_directories("${SYSTEM_INCLUDE_DIR}")
include_directories("${NLOHMANN_INCLUDE_DIR}")
include_directories("${JSONUTILS_INCLUDE_DIR}")

add_library (BandwidthUsageThread SHARED ${SOURCES} ${HEADERS})

if(APPLE)
	target_link_libraries(BandwidthUsageThread System)
	target_link_libraries(BandwidthUsageThread Datetime)
endif()

install (TARGETS BandwidthUsageThread DESTINATION lib)
install (FILES ${HEADERS} DESTINATION include)
