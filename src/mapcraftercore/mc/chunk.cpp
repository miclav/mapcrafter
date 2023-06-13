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

#include "chunk.h"
#include "blockstate.h"
#include "versions/data_adaptor.h"
#include "../renderer/biomes.h"
#include "../renderer/blockimages.h"

#include <cmath>
#include <iostream>
#include <boost/range.hpp>

namespace mapcrafter {
namespace mc {

namespace {

void readPackedShorts_v116(const std::vector<int64_t>& data, uint16_t* palette, uint16_t* palette_end) {
	uint32_t palette_size = palette_end - palette;
	uint32_t shorts_per_long = (palette_size + data.size() - 1) / data.size();
	uint32_t bits_per_value = 64 / shorts_per_long;
	std::fill(palette, &palette[palette_size], 0);
	uint16_t mask = (1 << bits_per_value) - 1;

	for (uint32_t i=0; i<shorts_per_long; i++) {
		uint32_t j = 0;
		for( uint32_t k=i; k<palette_size; k+=shorts_per_long) {
			assert(j < data.size());
			palette[k] = (uint16_t)(data[j] >> (bits_per_value * i)) & mask;
			j++;
		}
		assert(j <= data.size());
	}
}

} // namespace

uint16_t Chunk::nop_id = 0;

Chunk::Chunk()
	: chunkpos(42, 42) {
	clear();
}

Chunk::~Chunk() {
}

void Chunk::setWorldCrop(const WorldCrop& world_crop) {
	this->world_crop = world_crop;
}

int Chunk::positionToKey(int x, int z, int y) const {
	return y + 256 * (x + 16 * z);
}

bool Chunk::readNBT(mc::BlockStateRegistry& block_registry, const char* data, size_t len,
		nbt::Compression compression) {
	clear();

	// In case it wasn't set before
	if (nop_id == 0) {
		// The no operation block is the air block
		nop_id = block_registry.getBlockID(mc::BlockState("minecraft:air"));
	}

	nbt::NBTFile nbt;
	nbt.readNBT(data, len, compression);

	// Make sure we know which data format this chunk is built of
	if (!nbt.hasTag<nbt::TagInt>("DataVersion")) {
		LOG(ERROR) << "Chunk error: No version tag found!";
		return false;
	}

	int data_version = nbt.findTag<nbt::TagInt>("DataVersion").payload;
	const DataAdaptor& dadap = DataAdaptor::GetVersionAdaptor(data_version);
	if (!dadap.IsSupported()){
		LOG(ERROR) << "Chunk error: Unsupported chunk version, please upgrade.";
		return false;
	}

	// then find x/z pos of the chunk
	if (!nbt.hasTag<nbt::TagInt>("xPos") || !nbt.hasTag<nbt::TagInt>("yPos") || !nbt.hasTag<nbt::TagInt>("zPos")) {
		LOG(ERROR) << "Corrupt chunk: No x/z position found!";
		return false;
	}

	chunkpos = ChunkPos(nbt.findTag<nbt::TagInt>("xPos").payload,
	                             nbt.findTag<nbt::TagInt>("zPos").payload);
	int chunk_lowest = nbt.findTag<nbt::TagInt>("yPos").payload;

	// now we have the original chunk position:
	// check whether this chunk is completely contained within the cropped world
	chunk_completely_contained = world_crop.isChunkCompletelyContained(chunkpos);

	if (nbt.hasTag<nbt::TagString>("Status")) {
		const nbt::TagString& tag = nbt.findTag<nbt::TagString>("Status");
		if(! dadap.chunkStatus.isFull(tag.payload) ){
			return true;
		}
	}

	// find sections list
	// ignore it if section list does not exist, can happen sometimes with the empty
	// chunks of the end
	if (!nbt.hasList<nbt::TagCompound>("sections"))
		return true;

	const nbt::TagList& sections_tag = nbt.findTag<nbt::TagList>("sections");
	if (sections_tag.tag_type != nbt::TagCompound::TAG_TYPE)
		return true;

	// go through all sections
	for (auto it = sections_tag.payload.begin(); it != sections_tag.payload.end(); ++it) {
		const nbt::TagCompound& section_tag = (*it)->cast<nbt::TagCompound>();

		// make sure section is valid
		if (!section_tag.hasTag<nbt::TagByte>("Y")
			|| !section_tag.hasTag<nbt::TagCompound>("block_states")
			|| !section_tag.hasTag<nbt::TagCompound>("biomes"))
			continue;

		// Check the Y
		const nbt::TagByte& y = section_tag.findTag<nbt::TagByte>("Y");
		if (y.payload < chunk_lowest || y.payload >= chunk_lowest+Y_CHUNKS_PER_REGION_FILE )
			continue;

		const nbt::TagCompound& blockstates = section_tag.findTag<nbt::TagCompound>("block_states");
		if (!blockstates.hasTag<nbt::TagList>("palette"))
			continue;

		const nbt::TagCompound& biomes = section_tag.findTag<nbt::TagCompound>("biomes");
		if (!biomes.hasTag<nbt::TagList>("palette"))
			continue;

		// create a ChunkSection-object
		ChunkSection section;
		section.y = y.payload;

		/**
		 * Get the block states palette
		 */

		// Depalettize block_states palette
		const nbt::TagList& palettebs = blockstates.findTag<nbt::TagList>("palette");
		std::vector<mc::BlockState> palette_blockstates(palettebs.payload.size());
		std::vector<uint16_t> palette_blockstates_idx(palettebs.payload.size());
		int i = 0;
		for (auto pbsit = palettebs.payload.begin(); pbsit != palettebs.payload.end(); ++pbsit, ++i) {
			nbt::TagCompound& entry = (*pbsit)->cast<nbt::TagCompound>();
			const nbt::TagString& name = entry.findTag<nbt::TagString>("Name");

			mc::BlockState block(name.payload);
			if (entry.hasTag<nbt::TagCompound>("Properties")) {
				const nbt::TagCompound& properties = entry.findTag<nbt::TagCompound>("Properties");
				for (auto it3 = properties.payload.begin(); it3 != properties.payload.end(); ++it3) {
					std::string key = it3->first;
					std::string value = it3->second->cast<nbt::TagString>().payload;
					if (block_registry.isKnownProperty(block.getName(), key)) {
						block.setProperty(key, value);
					}
				}
			}
			palette_blockstates[i] = block;
			palette_blockstates_idx[i] = block_registry.getBlockID(block);
		}

		/**
		 * Get the block states data
		 */
		if (palettebs.payload.size()>1) {
			const nbt::TagLongArray& databs = blockstates.findTag<nbt::TagLongArray>("data");
			readPackedShorts_v116(databs.payload, section.block_ids, &section.block_ids[boost::size(section.block_ids)]);

			bool ok = true;
			for (size_t i = 0; i < 16*16*16; i++) {
				if (section.block_ids[i] >= palette_blockstates.size()) {
					int bits_per_entry = databs.payload.size() * 64 / (16*16*16);
					LOG(ERROR) << "Incorrectly parsed palette ID " << section.block_ids[i]
						<< " at index " << i << " (max is " << palette_blockstates.size()-1
						<< " with " << bits_per_entry << " bits per entry)";
					ok = false;
					break;
				}
				section.block_ids[i] = palette_blockstates_idx[section.block_ids[i]];
			}
			if (!ok) {
				continue;
			}
		} else if (palettebs.payload.size()==1) {
			// Check if air is the only block in this section, if so, ignore it completly, it will speed up the rest
			// of the rendering as we won't have to verify every single block in this section.
			if (palette_blockstates_idx[0] == nop_id) continue;
			// Only 1 in palette: There's only block in this chunk
			std::fill(section.block_ids, section.block_ids+boost::size(section.block_ids), palette_blockstates_idx[0]);
		} else {
			// No palette, this shouldn't happen, anyway let's use the default one
			std::fill(section.block_ids, section.block_ids+boost::size(section.block_ids), 0);
		}


		/**
		 * Get the biome data
		 */
		const nbt::TagList& paletteb = biomes.findTag<nbt::TagList>("palette");
		if (paletteb.payload.size()>1) {
			// More than one biome: there must be data and palette size > 1
			if (!biomes.hasTag<nbt::TagLongArray>("data"))
				continue;
			const nbt::TagLongArray& databiomes = biomes.findTag<nbt::TagLongArray>("data");
			readPackedShorts_v116(databiomes.payload, section.biomes, &section.biomes[boost::size(section.biomes)]);

			std::vector<uint16_t> palette_biomes(paletteb.payload.size());
			size_t i = 0;
			for (auto pbit = paletteb.payload.begin(); pbit != paletteb.payload.end(); ++pbit, ++i) {
				nbt::TagString& biome = (*pbit)->cast<nbt::TagString>();
				palette_biomes[i] = mapcrafter::renderer::Biome::getBiomeId(biome.payload);
			}
			// Convert chunk local index into the global biome index
			for (i = 0; i < boost::size(section.biomes); ++i) {
				uint16_t idx = section.biomes[i];
				// Make sure we stay in the array, if it happens, use the default biome
				if (idx>=palette_biomes.size()) idx = 0;
				section.biomes[i] = palette_biomes[idx];
			}
		} else if (paletteb.payload.size()==1) {
			// Only 1 in palette: It's only this biome in this chunk
			nbt::TagString& biome = (*paletteb.payload.begin())->cast<nbt::TagString>();
			std::fill(section.biomes, section.biomes+boost::size(section.biomes), mapcrafter::renderer::Biome::getBiomeId(biome.payload));
		} else {
			// No palette, this shouldn't happen, anyway let's use the default one
			std::fill(section.biomes, section.biomes+boost::size(section.biomes), 0);
		}

		if (section_tag.hasArray<nbt::TagByteArray>("BlockLight")) {
			const nbt::TagByteArray& block_light = section_tag.findTag<nbt::TagByteArray>("BlockLight");
			std::copy(block_light.payload.begin(), block_light.payload.end(), section.block_light);
		} else {
			std::fill(&section.block_light[0], &section.block_light[2048], 0);
		}

		if (section_tag.hasArray<nbt::TagByteArray>("SkyLight", 2048)) {
			const nbt::TagByteArray& sky_light = section_tag.findTag<nbt::TagByteArray>("SkyLight");
			std::copy(sky_light.payload.begin(), sky_light.payload.end(), section.sky_light);
		} else {
			std::fill(&section.sky_light[0], &section.sky_light[2048], 0);
		}

		// add this section to the section list
		section_offsets[section.y-CHUNK_LOWEST] = sections.size();
		sections.push_back(section);
	}

	return true;
}

void Chunk::clear() {
	sections.clear();
	for (size_t i = 0; i < boost::size(section_offsets); i++)
		section_offsets[i] = -1;
}

bool Chunk::hasSection(int y) const {
	const ChunkSection* cs = this->getSection(y);
	return cs != NULL;
}

const ChunkSection* Chunk::getSection(int y) const {
	int chunk_idx = y >> 4;
	if( chunk_idx < CHUNK_LOWEST || chunk_idx >= CHUNK_HIGHEST) {
		return NULL;
	}
	// Convert index to index into the data array
	size_t section_idx = section_offsets[chunk_idx - CHUNK_LOWEST];
	if( section_idx < 0 || section_idx >= sections.size()) {
		return NULL;
	}
	return &sections[section_idx];
}

uint16_t Chunk::getBlockID(const LocalBlockPos& pos, bool force) const {
	const ChunkSection* cs = getSection(pos.y);
	if (!cs)
		return nop_id;

	// check whether this block is really rendered
	if (checkBlockWorldCrop(pos.x, pos.z, pos.y)!=0)
		return nop_id;

	// calculate the offset and get the block ID
	// and don't forget the add data
	int offset = ((pos.y & 15) * 256) + (pos.z * 16) + pos.x;
	uint16_t id = cs->block_ids[offset];
	if (!force && world_crop.hasBlockMask()) {
		const BlockMask* mask = world_crop.getBlockMask();
		BlockMask::BlockState block_state = mask->getBlockState(id);
		if (block_state == BlockMask::BlockState::COMPLETELY_HIDDEN)
			return nop_id;
		else if (block_state == BlockMask::BlockState::COMPLETELY_SHOWN)
			return id;
		if (mask->isHidden(id, 0))
			return nop_id;
	}
	return id;
}

int Chunk::checkBlockWorldCrop(int x, int z, int y) const {
	// now about the actual world cropping:
	// get the global position of the block
	BlockPos global_pos = LocalBlockPos(x, z, y).toGlobalPos(chunkpos);
	// check whether the block is contained in the y-bounds.
	if (!world_crop.isBlockContainedY(global_pos))
		return 1;
	// only check x/z-bounds if the chunk is not completely contained
	if (!chunk_completely_contained && !world_crop.isBlockContainedXZ(global_pos))
		return 2;
	return 0;
}

uint8_t Chunk::getData(const LocalBlockPos& pos, int array, bool force) const {
	const ChunkSection* cs = getSection(pos.y);
	if (!cs) {
		 // not existing sections top sections should always have skylight
		 return array == 1 ? (this->sections.size() ? 15 : mc::OUT_OF_WORLD_LIGHT) : 0;
	}

	// check whether this block is really rendered
	int crop = checkBlockWorldCrop(pos.x, pos.z, pos.y);
	switch (crop) {
		case 0: break;
		case 1: return array == 1 ? 15 : 0;
		case 2: return array == 1 ? mc::OUT_OF_WORLD_LIGHT : 0;
	}

	uint8_t data = 0;
	// calculate the offset and get the block data
	int offset = ((pos.y & 15) * 256) + (pos.z * 16) + pos.x;
	// handle bottom/top nibble
	if ((offset % 2) == 0)
		 data = cs->getArray(array)[offset / 2] & 0xf;
	else
		 data = (cs->getArray(array)[offset / 2] >> 4) & 0x0f;
	return data;
}

uint8_t Chunk::getBlockLight(const LocalBlockPos& pos) const {
	return getData(pos, 0);
}

uint8_t Chunk::getSkyLight(const LocalBlockPos& pos) const {
	return getData(pos, 1);
}

uint16_t Chunk::getBiomeAt(const LocalBlockPos& pos) const {
	const ChunkSection* cs = getSection(pos.y);
	if (!cs)
		return renderer::DEFAULT_BIOME_ID;

	int x = pos.x >> 2;
	int z = pos.z >> 2;
	int y = (pos.y & 15) >> 2;

	return cs->biomes[(y << 4) + (z << 2) + x];
}

const ChunkPos& Chunk::getPos() const {
	return chunkpos;
}

}
}
