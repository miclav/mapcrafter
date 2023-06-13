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

#include "chunk_status.h"

#include <string>

#include "data_versions.h"

namespace mapcrafter {
namespace mc {

ChunkStatus::ChunkStatus(int version) {
	if (version >= DataVersion::v1_20_pre6) {
		this->isFull = [](std::string status) {
			return status == "minecraft:fullchunk" || status == "minecraft:full" ||
				   status == "minecraft:postprocessed" || status == "minecraft:mobs_spawned";
		};

	} else if (version > DataVersion::v1_16) {
		// version 1.10
		this->isFull = [](std::string status) {
			return status == "fullchunk" || status == "full" || status == "postprocessed" ||
				   status == "mobs_spawned";
		};

	} else {
		// completely generated chunks in fresh 1.13 worlds usually have status 'fullchunk' or 'postprocessed'
		// however, chunks of converted <1.13 worlds don't use these, but the state 'mobs_spawned'
		// version default
		this->isFull = [](std::string status) {
			return false;
		};
	}
}

}  // namespace mc
}  // namespace mapcrafter
