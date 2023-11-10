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

#include "../../biomes.h"
#include "../../blockimages.h"
#include "../../image.h"
#include "../../rendermode.h"
#include "../../tileset.h"
#include "../../../mc/blockstate.h"
#include "../../../mc/pos.h"
#include "../../../mc/worldcache.h"
#include "../../../util.h"

#include <fstream>
#include <iostream>
#include <map>
#include <set>
#include <sstream>
#include <vector>

namespace mapcrafter {
namespace renderer {

namespace old {

mc::ChunkPos TileTopBlockIterator::tile2Pos(int r, int c) const {
	return tile2Pos(r, c, render_view->getRotation());
}

mc::ChunkPos TileTopBlockIterator::tile2Pos(int r, int c, const RenderRotation& rotation) {
	switch (rotation.getRotation()) {
		default:
		case RenderRotation::TOP_LEFT:
			return mc::ChunkPos((+c - r) / 2, (+c + r) / 2);
		case RenderRotation::TOP_RIGHT:
			return mc::ChunkPos((+c + r) / 2, (-c + r) / 2 + 31);
		case RenderRotation::BOTTOM_RIGHT:
			return mc::ChunkPos((-c + r) / 2 + 31, (-c - r) / 2 + 31);
		case RenderRotation::BOTTOM_LEFT:
			return mc::ChunkPos((-c - r) / 2 + 31, (+c - r) / 2);
	}
}

int TileTopBlockIterator::pos2Row(const mc::BlockPos& pos) const {
	switch(render_view->getRotation().getRotation()) {
		default:
		case RenderRotation::TOP_LEFT:
			return - pos.x + pos.z;
		case RenderRotation::TOP_RIGHT:
			return + pos.x + pos.z;
		case RenderRotation::BOTTOM_RIGHT:
			return + pos.x - pos.z;
		case RenderRotation::BOTTOM_LEFT:
			return - pos.x - pos.z;
	}
}

int TileTopBlockIterator::pos2Col(const mc::BlockPos& pos) const {
	switch(render_view->getRotation().getRotation()) {
		default:
		case RenderRotation::TOP_LEFT:
			return + pos.x + pos.z;
		case RenderRotation::TOP_RIGHT:
			return + pos.x - pos.z;
		case RenderRotation::BOTTOM_RIGHT:
			return - pos.x - pos.z;
		case RenderRotation::BOTTOM_LEFT:
			return - pos.x + pos.z;
	}
}

TileTopBlockIterator::TileTopBlockIterator(const TilePos& tile, int block_size,
		int tile_width, const RenderView* render_view)
		: block_size(block_size), is_end(false), render_view(render_view) {
	// row/col 0,0 are the top left chunk of the tile 0,0
	// each tile is four rows high, two columns wide

	tile_dir = render_view->getRotation().rotate(mc::DIR_SOUTH);
	tile_rewind = render_view->getRotation().rotate(mc::DIR_NORTH + mc::DIR_WEST);

	// at first get the chunk, whose row and column is at the top right of the tile
	// top right chunk of a tile is the top left chunk of the tile x+1,y
	mc::ChunkPos topright_chunk = tile2Pos(
			4 * tile_width * tile.getY(),
			2 * tile_width * (tile.getX() + 1));

	// now get the first visible block from this chunk in this tile, in the middle of chunk
	top = mc::LocalBlockPos(8, 8, mc::CHUNK_HIGHEST * 16 - 1).toGlobalPos(topright_chunk);
	// and set this as start
	current = top;

	// Render a bit earlier on top right to have the bottom left of tiles rendered
	int relcol = 2 * (16 * tile_width - 1);
	int relrow = -1;

	// calculate bounds of the tile
	max_col = pos2Col(top) + (2 * 16) - relcol;
	min_col = max_col - (2 * 16 * tile_width);
	min_row = pos2Row(top) - relrow; // Included
	max_row = min_row + (4 * 16 * tile_width) + 4; // Excluded

	// every column is a 1/2 block and every row is a 1/4 block
	draw_x = relcol * block_size / 2;
	// -1/2 blocksize, because we would see the top side of the blocks in the tile if not
	draw_y = relrow * block_size / 4 - block_size / 2;
}

TileTopBlockIterator::~TileTopBlockIterator() {
}

void TileTopBlockIterator::next() {
	if (is_end)
		return;

	// go one block to bottom right (z+1)
	current += tile_dir;

	int absrow = pos2Row(current);
	int abscol = pos2Col(current);

	// check if row/col is too big
	if (abscol >= max_col || absrow >= max_row) {
		// move the top one block to the left
		top += tile_rewind;
		// and set the current block to the top block
		current = top;

		// check if the current top block is out of the tile
		if (pos2Col(current) < min_col) {
			// then move it by a few blocks to bottom right
			current += render_view->getRotation().rotate(mc::BlockDir(0, min_col - pos2Col(current) - 1, 0));
		}

		// Recalculate the row and col
		absrow = pos2Row(current);
		abscol = pos2Col(current);
	}

	// now calculate the block position like in the constructor
	draw_x = (abscol - min_col) * block_size / 2;
	draw_y = (absrow - min_row) * block_size / 4 - block_size / 2; // -16

	// and set end if reached
	if (absrow >= max_row && abscol <= min_col+1)
		is_end = true;
}

bool TileTopBlockIterator::end() const {
	return is_end;
}

}

NewIsometricTileRenderer::NewIsometricTileRenderer(const RenderView* render_view,
		mc::BlockStateRegistry& block_registry,
		BlockImages* images, int tile_width, mc::WorldCache* world,
		RenderMode* render_mode)
	: TileRenderer(render_view, block_registry, images, tile_width, world, render_mode) {
}

NewIsometricTileRenderer::~NewIsometricTileRenderer() {
}

int NewIsometricTileRenderer::getTileSize() const {
	return images->getBlockSize() * 16 * tile_width;
}

void NewIsometricTileRenderer::renderTopBlocks(const TilePos& tile_pos, boost::container::vector<TileImage>& tile_images) {
	int block_size = images->getBlockSize();
	mc::BlockDir dir = render_view->getRotation().rotate(mc::DIR_NORTH + mc::DIR_EAST + mc::DIR_BOTTOM);
	for (old::TileTopBlockIterator it(tile_pos, block_size, tile_width, render_view); !it.end(); it.next()) {
		renderBlocks(it.getDrawX(), it.getDrawY(), it.getCurrentPos(), dir, tile_images);
	}
}

}
}
