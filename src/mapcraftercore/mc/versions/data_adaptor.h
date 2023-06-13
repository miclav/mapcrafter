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

#ifndef DATA_ADAPTOR_H_
#define DATA_ADAPTOR_H_

#include <stdint.h>

#include <unordered_map>

#include "../../compat/thread.h"
#include "chunk_status.h"

namespace mapcrafter {
namespace mc {

/**
 * This class provide access to data version specific logic.
 * It implies supported version, chunk data formnat, or any other logic that needs to be adapted depending of
 * the data version.
 * To save memory and speed up, built adaptors are cached and reused.
 */
class DataAdaptor {
  private:
	DataAdaptor(int version);

  public:
	// This accessor returns the version adaptor
	static const DataAdaptor& GetVersionAdaptor(int version);

  private:
	// Holds already created adaptors to not rebuild them everytime
	static std::unordered_map<int, std::shared_ptr<const DataAdaptor>> builtAdaptors;
	static thread_ns::mutex                                               mutex;

	bool isSupported;

  public:
	// Returns true if the version is handled
	bool IsSupported() const;

	// Chunk status
	ChunkStatus chunkStatus;
};

}  // namespace mc
}  // namespace mapcrafter

#endif /* DATA_ADAPTOR_H_ */
