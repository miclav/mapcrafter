/*
 * Copyright 2012-2016 Moritz Hilscher
 *
 * This file is part of Mapcrafter.
 *
 * Mapcrafter is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * Mapcrafter is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with Mapcrafter.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "data_adaptor.h"
#include "data_versions.h"

namespace mapcrafter {
namespace mc {

DataAdaptor::DataAdaptor(int version) : chunkStatus(version) {
	this->isSupported = version >= DataVersion::v1_18;
}

std::unordered_map<int, std::shared_ptr<const DataAdaptor>> DataAdaptor::builtAdaptors;
thread_ns::mutex                                               DataAdaptor::mutex;

const DataAdaptor& DataAdaptor::GetVersionAdaptor(int version) {
	thread_ns::unique_lock<thread_ns::mutex> lock(mutex);

	// Return the adaptor from the cache if present
	auto it = DataAdaptor::builtAdaptors.find(version);
	if (it != DataAdaptor::builtAdaptors.end()) {
		return *(it->second).get();
	}

	// Build an adaptor depending of the version
	DataAdaptor* adapt = new DataAdaptor(version);

	// Store and return it
	DataAdaptor::builtAdaptors[version] = std::shared_ptr<const DataAdaptor>(adapt);
	return *adapt;
}

bool DataAdaptor::IsSupported() const {
	return this->isSupported;
}

}  // namespace mc
}  // namespace mapcrafter
