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

#include "tilerenderer.h"

#include <boost/range/algorithm/sort.hpp>

#include "blockimages.h"
#include "rendermode.h"
#include "renderview.h"
#include "tileset.h"
#include "mcrandom.h"
#include "../mc/blockstate.h"
#include "../mc/pos.h"
#include "../util.h"

namespace mapcrafter {
namespace renderer {

TileRenderer::TileRenderer(const RenderView* render_view, mc::BlockStateRegistry& block_registry,
		BlockImages* images, int tile_width, mc::WorldCache* world, RenderMode* render_mode) :
		block_registry(block_registry), images(images), block_images(dynamic_cast<RenderedBlockImages*>(images)),
		tile_width(tile_width), world(world), current_chunk(nullptr),
		render_mode(render_mode), render_view(render_view),
		render_biomes(true), shadow_edges({0, 0, 0, 0, 0}),
		waterlog_full_image(
			block_images->getBlockImage(
				block_registry.getBlockID(
					mc::BlockState::parse("minecraft:water_mask", "level=0")))),
		waterlog_shore_image(
			block_images->getBlockImage(
				block_registry.getBlockID(
					mc::BlockState::parse("minecraft:water_mask", "level=2" )))),
		tile_image(waterlog_full_image.image(0).width, waterlog_full_image.image(0).height),
		waterLogTinted(tile_image.image.width, tile_image.image.height) {
	assert(block_images);
	render_mode->initialize(render_view, images, world, &current_chunk);
	// Pre-allocate rendering buffers
}

TileRenderer::~TileRenderer() {
}

void TileRenderer::setRenderBiomes(bool render_biomes) {
	this->render_biomes = render_biomes;
}

void TileRenderer::setShadowEdges(std::array<uint8_t, 5> shadow_edges) {
	this->shadow_edges = shadow_edges;
}

TileRenderer::cmpBlockPos* TileRenderer::getTileComparator() const {
	switch ((RenderRotation::Direction)render_view->getRotation()){
	default:
	case RenderRotation::TOP_LEFT:
		return [](const TileImage& a, const TileImage& b) -> bool {
			return
				(a.pos.y != b.pos.y) ? (a.pos.y < b.pos.y) : (
				(a.pos.z != b.pos.z) ? (a.pos.z < b.pos.z) : (
				(a.pos.x != b.pos.x) ? (a.pos.x > b.pos.x) : (
					false
				)));
		};
		break;
	case RenderRotation::TOP_RIGHT:
		return [](const TileImage& a, const TileImage& b) -> bool {
			return
				(a.pos.y != b.pos.y) ? (a.pos.y < b.pos.y) : (
				(a.pos.x != b.pos.x) ? (a.pos.x < b.pos.x) : (
				(a.pos.z != b.pos.z) ? (a.pos.z < b.pos.z) : (
					false
				)));
		};
		break;
	case RenderRotation::BOTTOM_RIGHT:
		return [](const TileImage& a, const TileImage& b) -> bool {
			return
				(a.pos.y != b.pos.y) ? (a.pos.y < b.pos.y) : (
				(a.pos.z != b.pos.z) ? (a.pos.z > b.pos.z) : (
				(a.pos.x != b.pos.x) ? (a.pos.x < b.pos.x) : (
					false
				)));
		};
		break;
	case RenderRotation::BOTTOM_LEFT:
		return [](const TileImage& a, const TileImage& b) -> bool {
			return
				(a.pos.y != b.pos.y) ? (a.pos.y < b.pos.y) : (
				(a.pos.x != b.pos.x) ? (a.pos.x > b.pos.x) : (
				(a.pos.z != b.pos.z) ? (a.pos.z > b.pos.z) : (
					false
				)));
		};
		break;
	}
}

void TileRenderer::renderTile(const TilePos& tile_pos, RGBAImage& tile) {
	tile.setSize(getTileWidth(), getTileHeight());

	boost::container::vector<TileImage> tile_images;
	renderTopBlocks(tile_pos, tile_images);

	// Sort them in order depending of the rotation
	boost::range::sort(tile_images, getTileComparator());

	for (auto it = tile_images.begin(); it != tile_images.end(); ++it) {
		tile.alphaBlit(it->image, it->x, it->y);
	}
}

int TileRenderer::getTileWidth() const {
	return getTileSize();
}

int TileRenderer::getTileHeight() const {
	return getTileSize();
}

void TileRenderer::renderBlocks(int x, int y, mc::BlockPos top, const mc::BlockDir& dir, boost::container::vector<TileImage>& tile_images) {

	for (; top.y >= mc::CHUNK_LOWEST*16 ; top += dir) {
		// get current chunk position
		mc::ChunkPos current_chunk_pos(top);

		// check if current chunk is not null
		// and if the chunk wasn't replaced in the cache (i.e. position changed)
		if (current_chunk == nullptr || current_chunk->getPos() != current_chunk_pos) {
			//if (!state.world->hasChunkSection(current_chunk, top.current.y))
			//	continue;
			current_chunk = world->getChunk(current_chunk_pos);
		}
		if (current_chunk == nullptr) {
			continue;
		}

		// get local block position
		mc::LocalBlockPos local(top);

		uint16_t id = current_chunk->getBlockID(local, false);
		if (id == mc::Chunk::nop_id) continue;
		// const mc::BlockState& bs = block_registry.getBlockState(id);
		const BlockImage* block_image = &block_images->getBlockImage(id);

		// Early rejection if nothing to draw
		if ((block_image->is_empty && !block_image->is_waterlogged)
			|| render_mode->isHidden(top, *block_image)) {
			continue;
		}

		// What's on each side ?
		uint16_t id_top   = current_chunk->getBlockID(mc::LocalBlockPos(local.x,local.z,local.y+1), true);
		uint16_t id_south = getBlock(top + render_view->getRotation().getSouth()).id;
		uint16_t id_west  = getBlock(top + render_view->getRotation().getWest()).id;

		// Try an early rejection if full_water with waterloged neighbours
		bool solid_top = false;
		bool water_top = false;
		bool water_south = false;
		bool water_west = false;
		if (block_image->is_waterlogged) {
			const BlockImage* block_image_top = &block_images->getBlockImage(id_top);
			const BlockImage* block_image_south = &block_images->getBlockImage(id_south);
			const BlockImage* block_image_west = &block_images->getBlockImage(id_west);

			auto is_full_water = [](const BlockImage* b) -> bool {
				return b->is_empty &&
					b->is_waterlogged;
			};
			auto is_waterlogged = [](const BlockImage* b) -> bool {
				return b->is_waterlogged;
			};

			water_top = is_waterlogged(block_image_top);
			water_south = is_waterlogged(block_image_south);
			water_west = is_waterlogged(block_image_west);

			if (is_full_water(block_image)
				&& water_top
				&& water_south
				&& water_west)
				continue;

			solid_top = !block_image_top->is_transparent;
		}

		// Retrieve the image to print. Now exactly how it's rendered in Minecraft
		int32_t alt = 0;
		if (block_image->images_idx.size() > 1) {
			MCRandom rnd(top.x,top.y,top.z);
			alt = abs((int32_t)rnd.nextLong());
			alt = block_image->variant_2_index(alt);
		}
		const RGBAImage& image = block_image->image(alt);
		const RGBAImage& uv_image = block_image->uv_image(alt);

		// Prep the tile
		tile_image.x = x;
		tile_image.y = y;
		tile_image.pos = top;
		tile_image.image.setSize(image.width,image.height);

		// Only display if there's something to print
		// This applies for water blocks, where we print
		// the water on the next step
		if (!block_image->is_empty) {

			bool strip_up = false;
			bool strip_left = false;
			bool strip_right = false;
			if (block_image->can_partial) {
				strip_up    = id == id_top;
				strip_right = id == id_south;
				strip_left  = id == id_west;
			}

			if (strip_up || strip_left || strip_right) {
				for (int i=0; i<tile_image.image.width*tile_image.image.height; i++) {
					RGBAPixel puv = uv_image.data[i];
					RGBAPixel p = image.data[i];
					switch(rgba_blue(puv)) {
						case FACE_UP_INDEX:
							if (strip_up) {
								p = 0;
							}
							break;
						case FACE_LEFT_INDEX:
							if (strip_left) {
								p = 0;
							}
							break;
						case FACE_RIGHT_INDEX:
							if (strip_right) {
								p = 0;
							}
							break;
					}
					tile_image.image.data[i] = p;
				}
			} else {
				std::copy(image.data.begin(), image.data.end(), tile_image.image.data.begin());
			}

			if (block_image->is_biome) {
				block_images->prepareBiomeBlockImage(tile_image.image, *block_image, getBiomeColor(top, *block_image, current_chunk));
			}

			if (block_image->shadow_edges > 0) {
				auto shadow_edge = [this, top](const mc::BlockDir& dir) {
					const BlockImage& b = block_images->getBlockImage(getBlock(top + dir).id);
					return b.shadow_edges == 0;
				};
				uint8_t diff_top = (id != id_top);
				uint8_t north = shadow_edge(render_view->getRotation().getNorth()) && diff_top;
				uint8_t south = shadow_edge(render_view->getRotation().getSouth()) && diff_top;
				uint8_t east = shadow_edge(render_view->getRotation().getEast()) && diff_top;
				uint8_t west = shadow_edge(render_view->getRotation().getWest()) && diff_top;
				uint8_t bottom = shadow_edge(render_view->getRotation().getBottom());
				uint8_t bottomleft = bottom && (id != id_west);
				uint8_t bottomright = bottom && (id != id_south);

				if (north + south + east + west + bottomleft + bottomright != 0) {
					int f = block_image->shadow_edges;
					north *= shadow_edges[0] * f;
					south *= shadow_edges[1] * f;
					east *= shadow_edges[2] * f;
					west *= shadow_edges[3] * f;
					bottomleft *= shadow_edges[4] * f;
					bottomright *= shadow_edges[4] * f;
					blockImageShadowEdges(tile_image.image, uv_image,
						north, south, east, west, bottomleft, bottomright);
				}
			}

			// let the render mode do their magic with the block image
			render_mode->draw(tile_image.image, *block_image, tile_image.pos, id, render_view->getRotation());

		} else {
			// Clear out the tile from previous rendering
			std::fill(tile_image.image.data.begin(), tile_image.image.data.end(), 0);
		}


		if (block_image->is_waterlogged) {
			// assert( !(water_top && water_south && water_west) );

			const RGBAImage* waterlog;
			const RGBAImage* waterlog_uv;
			if (water_top || solid_top) {
				// This will be displayed as full water
				waterlog = &waterlog_full_image.image(0);
				waterlog_uv = &waterlog_full_image.uv_image(0);
			} else {
				// That one will be displayed a bit lower to look like a shore line
				waterlog = &waterlog_shore_image.image(0);
				waterlog_uv = &waterlog_shore_image.uv_image(0);
			}

			uint32_t biome_color = getBiomeColor(top, waterlog_full_image, current_chunk);
			biome_color = rgba(rgba_red(biome_color), rgba_green(biome_color), rgba_blue(biome_color), (render_view->getWaterOpacity() * 255));

			std::vector<RGBAPixel>::const_iterator pit      = waterlog->data.begin();
			std::vector<RGBAPixel>::const_iterator pitend   = waterlog->data.end();
			std::vector<RGBAPixel>::const_iterator puvit    = waterlog_uv->data.begin();
			std::vector<RGBAPixel>::iterator pdestit        = waterLogTinted.data.begin();

			if ((water_top || water_south || water_west) == false) {
				// fast lane
				// Nothing to clip, just render the whole water block with biome color
				while (pit != pitend)
				{
					RGBAPixel p = *pit;
					if (p) {
						p = rgba_multiply_with_alpha(p, biome_color);
					}
					*pdestit = p;
					pit ++;
					puvit ++;
					pdestit ++;
				}
			} else {
				// Clip the some faces, and multiply by biome color
				while (pit != pitend)
				{
					RGBAPixel p = *pit;
					if (p) {
						RGBAPixel puv = *puvit;
						switch(rgba_blue(puv)){
							case FACE_UP_INDEX:
								if(water_top) {
									p = 0;
								}
								break;
							case FACE_LEFT_INDEX:
								if(water_west) {
									p = 0;
								}
								break;
							case FACE_RIGHT_INDEX:
								if(water_south) {
									p = 0;
								}
								break;
						}
						if (p) {
							p = rgba_multiply_with_alpha(p, biome_color);
						}
					}
					*pdestit = p;
					pit ++;
					puvit ++;
					pdestit ++;
				}
			}

			blockImageBlendZBuffered(tile_image.image, uv_image, waterLogTinted, *waterlog_uv);
		}

		tile_images.push_back(tile_image);

		// if this block is not transparent, then stop looking for more blocks
		if (!block_image->is_transparent) {
			break;
		}
	}
}

mc::Block TileRenderer::getBlock(const mc::BlockPos& pos, int get) {
	return world->getBlock(pos, current_chunk, get);
}

uint32_t TileRenderer::getBiomeColor(const mc::BlockPos& pos, const BlockImage& block, const mc::Chunk* chunk) {
	const int radius = 2;
	float f = ((2*radius+1)*(2*radius+1));
	float r = 0.0, g = 0.0, b = 0.0;

	for (int dx = -radius; dx <= radius; dx++) {
		for (int dz = -radius; dz <= radius; dz++) {
			mc::BlockPos other = pos + mc::BlockDir(dx, dz, 0);
			mc::ChunkPos chunk_pos(other);

			uint16_t biome_id;
			mc::LocalBlockPos local(other);
			if (chunk_pos != chunk->getPos()) {
				mc::Chunk* other_chunk = world->getChunk(chunk_pos);
				if (other_chunk == nullptr) {
					f -= 1.0f;
					continue;
				}
				biome_id = other_chunk->getBiomeAt(local);
			} else {
				biome_id = chunk->getBiomeAt(local);
			}
			const Biome& biome = Biome::getBiome(biome_id);
			uint32_t c = biome.getColor(other, block.biome_color, block.biome_colormap);
			r += (float) rgba_red(c);
			g += (float) rgba_green(c);
			b += (float) rgba_blue(c);
		}
	}

	f = 1.0 / f;
	return rgba(r * f, g * f, b * f, 255);
}

}
}
