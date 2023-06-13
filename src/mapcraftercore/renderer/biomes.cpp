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

#include "biomes.h"

#include "blockimages.h" // ColorMapType
#include "image.h"
#include "../mc/pos.h"
#include "../util.h"

#include <cmath>
#include <iostream>
#include <boost/unordered_map.hpp>
#include <boost/array.hpp>


namespace mapcrafter {
namespace renderer {

const mc::JavaSimplexGenerator Biome::SWAMP_GRASS_NOISE;

Biome::Biome(std::string name, double temperature, double rainfall, RGBAPixel grass_tint, RGBAPixel foliage_tint, RGBAPixel water_tint, bool swamp_mod, bool forest_mod)
	:name(name), temperature(temperature), rainfall(rainfall),
		grass_tint(grass_tint), foliage_tint(foliage_tint), water_tint(water_tint)
	//   ,swamp_mod(swamp_mod), forest_mod(forest_mod)
{
}

/**
 * Returns the biome ID.
 */
std::string Biome::getName() const {
	return name;
}

/**
 * Calculates the color of the biome with a biome color image.
 */
RGBAPixel Biome::getColor(const mc::BlockPos& pos, const ColorMapType& color_type,
		const ColorMap& color_map) const {

	if (color_type == ColorMapType::WATER) {
		// return default_water;
		return water_tint;
	}

	// RGBAPixel tint = default_foliage;
	RGBAPixel tint = foliage_tint;
	if (color_type == ColorMapType::GRASS) {
		// tint = default_grass;
		tint = grass_tint;
	}
	// return tint;

	// bandland grass colors
	// if (id >= 165 && id <= 167) {
	// 	if (color_type == ColorMapType::GRASS) {
	// 		return rgba(0x90, 0x91, 0x4d, 0xff);
	// 	} else if (color_type == ColorMapType::FOLIAGE) {
	// 		return rgba(0x9e, 0x81, 0x4d, 0xff);
	// 	}
	// }
	// // swamp grass colors
	// if ((id == 6 || id == 134) && color_type == ColorMapType::GRASS) {
	// 	double v = SWAMP_GRASS_NOISE.getValue(pos.x * 0.0225, pos.z * 0.0225);
	// 	return v < -0.1 ? rgba(0x4C, 0x76, 0x3C) : rgba(0x6A, 0x70, 0x39);
	// }

	float elevation = std::max(pos.y - 64, 0);
	// x is temperature
	float x = std::min(1.0, std::max(0.0, temperature - elevation*0.00166667f));
	// y is temperature * rainfall
	float y = std::min(1.0, std::max(0.0, rainfall)) * x;

	RGBAPixel color = color_map.getColor(x, y);
	color = rgba_average(color, tint);

	return color;
}

// array with all possible biomes
// empty/unknown biomes in this array have the ID 0, Name mapcrafter:unknown
static bool biomes_initialized = false;
static boost::unordered_map<std::string, uint16_t> biome_names;
static std::set<std::string> unknown_biomes;

void Biome::initializeBiomes() {
	if (biomes_initialized) return;
	biomes_initialized = true;
	// put all biomes with their IDs into the array with all possible biomes
	for (size_t i = 0; i < BIOMES_SIZE; i++) {
		const Biome* biome = &BIOMES[i];
		biome_names[biome->getName()] = i;
	}
}

const Biome& Biome::getBiome(uint16_t id) {
	// check if this biome exists and return the default biome otherwise
	if (id < BIOMES_SIZE)
		return BIOMES[id];
	return BIOMES[DEFAULT_BIOME_ID];
}

uint16_t Biome::getBiomeId(std::string name) {
	auto bit = biome_names.find(name);
	if ( bit != biome_names.end())
		return biome_names[name];
	if (unknown_biomes.find(name) == unknown_biomes.end()) {
		LOG(WARNING) << "Unknown biome " << name;
		unknown_biomes.insert(name);
	}
	return DEFAULT_BIOME_ID;
}

} /* namespace render */
} /* namespace mapcrafter */
