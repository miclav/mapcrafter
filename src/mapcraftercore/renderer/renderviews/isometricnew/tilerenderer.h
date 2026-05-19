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

#ifndef ISOMETRICNEW_TILERENDERER_H_
#define ISOMETRICNEW_TILERENDERER_H_

#include "../../image.h"
#include "../../tilerenderer.h"
#include "../../renderview.h"

#include <boost/filesystem.hpp>

namespace fs = boost::filesystem;

namespace mapcrafter {

namespace mc {
class BlockStateRegistry;
}

namespace renderer {

namespace old {

/**
 * Iterates over the top blocks of a tile.
 */
class TileTopBlockIterator {
private:
	int block_size;

	bool is_end;
	int min_row, max_row;
	int min_col, max_col;
	mc::BlockPos top;
	mc::BlockPos current;

	const RenderView* render_view;
	int draw_x, draw_y;

	mc::BlockDir tile_dir;
	mc::BlockDir tile_rewind;

	static mc::ChunkPos tile2Pos(int r, int c, const RenderRotation& rotation);
	mc::ChunkPos tile2Pos(int r, int c) const;
	int pos2Row(const mc::BlockPos& pos) const;
	int pos2Col(const mc::BlockPos& pos) const;

public:
	TileTopBlockIterator(const TilePos& tile, int block_size, int tile_width, const RenderView* render_view);
	~TileTopBlockIterator();

	mc::BlockPos getCurrentPos() const { return current; };
	int getDrawX() const { return draw_x; };
	int getDrawY() const { return draw_y; };

	void next();
	bool end() const;
};

}

/**
 * Renders tiles from world data.
 */
class NewIsometricTileRenderer : public TileRenderer {
public:
	NewIsometricTileRenderer(const RenderView* render_view, mc::BlockStateRegistry& block_registry,
			BlockImages* images, int tile_width, mc::WorldCache* world, RenderMode* render_mode);
	virtual ~NewIsometricTileRenderer();

	//virtual void renderTile(const TilePos& tile_pos, RGBAImage& tile);

	virtual int getTileSize() const;

protected:
	virtual void renderTopBlocks(const TilePos& tile_pos, std::vector<TileImage>& tile_images);
};

}
}

#endif /* ISOMETRICNEW_TILERENDERER_H_ */
