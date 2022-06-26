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

#ifndef BIOMES_H_
#define BIOMES_H_

#include "image.h"
#include "../mc/java.h"

#include <cstdint>
#include <cstdlib>

namespace mapcrafter {
namespace mc {
class BlockPos;
}

namespace renderer {

class RGBAImage;
enum class ColorMapType;
struct ColorMap;

// static const uint32_t one = rgba(0xff, 0xff, 0xff, 0xff);
static const uint32_t default_grass = rgba(0x7F, 0xB2, 0x38, 0xff);	// 7FB238
static const uint32_t default_foliage = rgba(0x00, 0x7C, 0x00, 0xff); // 007C00
static const uint32_t default_water = rgba(0x3F, 0x76, 0xE4, 0xFF);	// 3F76E4

/**
 * A Minecraft Biome with data to tint the biome-depend blocks.
 */
class Biome {
private:
	std::string name;

	// temperature and rainfall
	// used to calculate the position of the tinting color in the color image
	double temperature;
	double rainfall;

	RGBAPixel grass_tint, foliage_tint, water_tint;
	bool swamp_mod, forest_mod;

	static const mc::JavaSimplexGenerator SWAMP_GRASS_NOISE;

public:
	static void initializeBiomes();

	Biome(std::string name = "mapcrafter:unknown", double temperature = 0.5, double rainfall = 0.5,
			RGBAPixel grass_tint = default_grass, RGBAPixel foliage_tint = default_foliage, RGBAPixel water_tint = default_water,
			bool swamp_mod = false, bool forest_mod = false);

	std::string getName() const;
	RGBAPixel getColor(const mc::BlockPos& pos, const ColorMapType& color_type, const ColorMap& color_map) const;

	static uint16_t getBiomeId(std::string name);
	static const Biome& getBiome(uint16_t id);
};

// different Minecraft Biomes
// first few biomes from Minecraft Overviewer
// temperature/rainfall data from Minecraft source code (via MCP)
// DO NOT directly access this array, use the getBiome function
static const Biome BIOMES[] = {
	{},	// unknown biome, all default values

	{"minecraft:the_void", 0.5, 0.5},

	{"minecraft:plains", 0.0, 0.5},
	{"minecraft:sunflower_plains", 0.0, 0.5},
	{"minecraft:snowy_plains", 0.8, 0.4},
	{"minecraft:ice_spikes", 0.8, 0.4},
	{"minecraft:desert", 2.0, 0.0},
	{"minecraft:swamp", 0.8, 0.9, default_grass, rgba(0x6a, 0x70, 0x39, 0xff), rgba(0x61, 0x7B, 0x64, 0xff), true},
	{"minecraft:forest", 0.6, 0.6},
	{"minecraft:flower_forest", 0.6, 0.6},
	{"minecraft:birch_forest", 0.7, 0.8},
	{"minecraft:dark_forest", 0.7, 0.8, default_grass, default_foliage, default_water, false, true},
	{"minecraft:old_growth_birch_forest", 0.7, 0.8},
	{"minecraft:old_growth_pine_taiga", 0.3, 0.8},
	{"minecraft:old_growth_spruce_taiga", 0.25, 0.8},
	{"minecraft:taiga", 0.25, 0.8},
	{"minecraft:snowy_taiga", -0.5, 0.4, default_grass, default_foliage, rgba(0x3D, 0x57, 0xD6, 0xff)},
	{"minecraft:savanna", 2.0, 0.0},
	{"minecraft:savanna_plateau", 2.0, 0.0},
	{"minecraft:windswept_hills", 0.2, 0.3},
	{"minecraft:windswept_gravelly_hills", 0.2, 0.3},
	{"minecraft:windswept_forest", 0.2, 0.3},
	{"minecraft:windswept_savanna", 2.0, 0.0},
	{"minecraft:jungle", 0.95, 0.9},
	{"minecraft:sparse_jungle", 0.95, 0.8},
	{"minecraft:bamboo_jungle", 0.95, 0.9},
	{"minecraft:badlands", 2.0, 0, rgba(0x90, 0x81, 0x4D, 0xff), rgba(0x9E, 0x81, 0x4D, 0xff)},
	{"minecraft:eroded_badlands", 2.0, 0, rgba(0x90, 0x81, 0x4D, 0xff), rgba(0x9E, 0x81, 0x4D, 0xff)},
	{"minecraft:wooded_badlands", 2.0, 0, rgba(0x90, 0x81, 0x4D, 0xff), rgba(0x9E, 0x81, 0x4D, 0xff)},
	{"minecraft:meadow", 0.5, 0.8, default_grass, default_foliage, rgba(0x0E, 0x4E, 0xCF, 0xff)},
	{"minecraft:grove", -0.2, 0.8},
	{"minecraft:snowy_slopes", -0.3, 0.9},
	{"minecraft:frozen_peaks", -0.7, 0.9},
	{"minecraft:jagged_peaks", -0.7, 0.9},
	{"minecraft:stony_peaks", 1.0, 0.3},
	{"minecraft:river", 0.5, 0.5},
	{"minecraft:frozen_river", 0.0, 0.5, default_grass, default_foliage, rgba(0x39, 0x38, 0xC9, 0xff)},
	{"minecraft:beach", 0.8, 0.4},
	{"minecraft:snowy_beach", 0.05, 0.3, default_grass, default_foliage, rgba(0x3D, 0x57, 0xD6, 0xff)},
	{"minecraft:stony_shore", 0.2, 0.3},
	{"minecraft:warm_ocean", 0.5, 0.5, default_grass, default_foliage, rgba(0x43, 0xD5, 0xEE, 0xff)},
	{"minecraft:lukewarm_ocean", 0.5, 0.5, default_grass, default_foliage, rgba(0x45, 0xAD, 0xF2, 0xff)},
	{"minecraft:deep_lukewarm_ocean", 0.5, 0.5, default_grass, default_foliage, rgba(0x45, 0xAD, 0xF2, 0xff)},
	{"minecraft:ocean", 0.5, 0.5},
	{"minecraft:deep_ocean", 0.5, 0.5},
	{"minecraft:cold_ocean", 0.5, 0.5, default_grass, default_foliage, rgba(0x3D, 0x57, 0xD6, 0xff)},
	{"minecraft:deep_cold_ocean", 0.5, 0.5, default_grass, default_foliage, rgba(0x3D, 0x57, 0xD6, 0xff)},
	{"minecraft:frozen_ocean", 0.0, 0.5, default_grass, default_foliage, rgba(0x39, 0x38, 0xC9, 0xff)},
	{"minecraft:deep_frozen_ocean", 0.5, 0.5, default_grass, default_foliage, rgba(0x39, 0x38, 0xC9, 0xff)},
	{"minecraft:mushroom_fields", 0.9, 1.0},
	{"minecraft:dripstone_caves", 0.8, 0.4},
	{"minecraft:lush_caves", 0.5, 0.5},

	{"minecraft:nether_wastes", 2.0, 0.0},
	{"minecraft:warped_forest", 2.0, 0.0},
	{"minecraft:crimson_forest", 2.0, 0.0},
	{"minecraft:soul_sand_valley", 2.0, 0.0},
	{"minecraft:basalt_deltas", 2.0, 0.0},

	{"minecraft:the_end", 0.5, 0.5},
	{"minecraft:end_highlands", 0.5, 0.5},
	{"minecraft:end_midlands", 0.5, 0.5},
	{"minecraft:small_end_islands", 0.5, 0.5},
	{"minecraft:end_barrens", 0.5, 0.5},

	{"minecraft:deep_dark", 0.8, 0.4},
	{"minecraft:mangrove_swamp", 0.8, 0.9, default_grass, rgba(0x8D, 0xB1, 0x27, 0xff), rgba(0x3A, 0x7A, 0x6A, 0xff), true},
};

static const std::size_t BIOMES_SIZE = sizeof(BIOMES) / sizeof(Biome);
static const int DEFAULT_BIOME_ID = 0;

} /* namespace render */
} /* namespace mapcrafter */
#endif /* BIOMES_H_ */
