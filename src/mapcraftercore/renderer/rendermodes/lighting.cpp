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

#include "lighting.h"

#include "../blockimages.h"
#include "../image.h"
#include "../../mc/chunk.h"
#include "../../mc/pos.h"
#include "../../util.h"

#include <cmath>

namespace mapcrafter {
namespace renderer {

CornerNeighbors::CornerNeighbors() {
}

CornerNeighbors::CornerNeighbors(const mc::BlockDir& pos1, const mc::BlockDir& dir1,
		const mc::BlockDir& dir2)
	: pos1(pos1),
	  pos2(pos1 + dir1),
	  pos3(pos1 + dir2),
	  pos4(pos1 + dir1 + dir2),

	  dir1(dir1), dir2(dir2) {
}

CornerNeighbors CornerNeighbors::addDir(const mc::BlockDir& dir) const {
	return CornerNeighbors(this->pos1 + dir, dir1, dir2);
}

FaceCorners::FaceCorners() {
}

FaceCorners::FaceCorners(const CornerNeighbors& corner1)
	: corner1(corner1),
	  corner2(corner1.addDir(corner1.dir1)),
	  corner3(corner1.addDir(corner1.dir2)),
	  corner4(corner1.addDir(corner1.dir1 + corner1.dir2)) {
}

LightingData::LightingData(uint8_t block_light, uint8_t sky_light)
	: block_light(block_light), sky_light(sky_light) {
}

LightingData::~LightingData() {
}

uint8_t LightingData::getBlockLight() const {
	return block_light;
}

uint8_t LightingData::getSkyLight() const {
	return sky_light;
}

uint8_t LightingData::getLightLevel(bool day) const {
	if (day)
		return std::max(block_light, sky_light);
	return std::max(block_light + 0, sky_light - 11);
}

LightingData LightingData::estimate(const mc::Block& block,
		RenderedBlockImages* block_images, mc::WorldCache* world, mc::Chunk* current_chunk) {
	// estimate the light if this is a special block
	if (!block_images->getBlockImage(block.id).has_faulty_lighting) {
		return LightingData(block.block_light, block.sky_light);
	}

	uint8_t block_light = 0, sky_light = 0;

	// get the sky light from the block above
	mc::BlockDir off(0, 0, 0);
	mc::Block above;
	while (++off.y) {
		above = world->getBlock(block.pos + off, current_chunk,
				mc::GET_ID | mc::GET_SKY_LIGHT);
		const BlockImage& above_block = block_images->getBlockImage(above.id);
		if (above_block.has_faulty_lighting) {
			continue;
		}
		//if (above.id == 0 || images->isBlockTransparent(above.id, above.data))
		if (above_block.is_empty || above_block.is_transparent)
			sky_light = above.sky_light;
		else
			sky_light = 15;
		break;
	}

	// get the block light from the neighbor blocks
	int block_lights = 0;
	int block_lights_count = 0;
	for (int dx = -1; dx <= 1; dx++)
		for (int dz = -1; dz <= 1; dz++)
			for (int dy = -1; dy <= 1; dy++) {
				mc::Block other = world->getBlock(block.pos + mc::BlockDir(dx, dz, dy),
						current_chunk, mc::GET_ID | mc::GET_BLOCK_LIGHT);
				const BlockImage& other_block = block_images->getBlockImage(other.id);
				if ((other_block.is_empty || other_block.is_transparent)
						&& !other_block.has_faulty_lighting) {
					block_lights += other.block_light;
					block_lights_count++;
				}
			}

	if (block_lights_count > 0)
		block_light = block_lights / block_lights_count;
	return LightingData(block_light, sky_light);
}

LightingRenderMode::LightingRenderMode(bool day, double lighting_intensity,
		double lighting_water_intensity, bool simulate_sun_light)
	: day(day), lighting_intensity(lighting_intensity),
	  lighting_water_intensity(lighting_water_intensity),
	  simulate_sun_light(simulate_sun_light) {
}


LightingRenderMode::~LightingRenderMode() {
}

void LightingRenderMode::draw(RGBAImage& image, const BlockImage& block_image,
		const mc::BlockPos& pos, uint16_t id, const RenderRotation& rotation) {

	CORNERS_LEFT = FaceCorners(CornerNeighbors(
		/*mc::DIR_WEST + mc::DIR_NORTH + mc::DIR_TOP*/ rotation.rotate(mc::BlockDir(-1, -1, 1)),
		/*mc::DIR_SOUTH*/ rotation.rotate(mc::BlockDir(0, 1, 0)),
		/*mc::DIR_BOTTOM*/ rotation.rotate(mc::BlockDir(0, 0, -1))));
	CORNERS_RIGHT = FaceCorners(CornerNeighbors(
		/*mc::DIR_SOUTH + mc::DIR_WEST + mc::DIR_TOP*/ rotation.rotate(mc::BlockDir(-1, 1, 1)),
		/*mc::DIR_EAST*/ rotation.rotate(mc::BlockDir(1, 0, 0)),
		/*mc::DIR_BOTTOM*/ rotation.rotate(mc::BlockDir(0, 0, -1))));
	CORNERS_TOP = FaceCorners(CornerNeighbors(
		/*mc::DIR_TOP + mc::DIR_NORTH + mc::DIR_WEST*/ rotation.rotate(mc::BlockDir(-1, -1, 1)),
		/*mc::DIR_EAST*/ rotation.rotate(mc::BlockDir(1, 0, 0)),
		/*mc::DIR_SOUTH*/ rotation.rotate(mc::BlockDir(0, 1, 0))));
	CORNERS_BOTTOM = FaceCorners(CornerNeighbors(
		/*mc::DIR_NORTH + mc::DIR_WEST*/ rotation.rotate(mc::BlockDir(-1, -1, 0)),
		/*mc::DIR_EAST*/ rotation.rotate(mc::BlockDir(1, 0, 0)),
		/*mc::DIR_SOUTH*/ rotation.rotate(mc::BlockDir(0, 1, 0))));

	//void blockImageMultiply(RGBAImage& block, const RGBAImage& uv_mask,
	//		const CornerValues& factors_left, const CornerValues& factors_right, const CornerValues& factors_up);

	// top-left, top-right, bottom-right, bottom-left
	/*
	CornerValues left = {1.0, 1.0, 1.0, 0.0};
	CornerValues right = {1.0, 1.0, 1.0, 1.0};
	CornerValues up = {1.0, 1.0, 1.0, 1.0};
	blockImageMultiply(image, block_image.uv_image, left, right, up);
	*/

	// flat snow and grass paths: smooth (but bottom corners) (aka. lighting type smooth_bottom ?)
	// waterlogged blocks: simple but water surface smooth (aka. lighting type smooth_top_simple_rest ?)
	// slabs: smooth
	// transparent, except full water, except ice
	// smooth

	// so what we need:
	// - blockstate property: lighting type (smooth, simple, ...)
	// - block image: side mask (3x bool)
	// - ice needs to be treated a bit like water after all

	float intensity = block_image.is_waterlogged ? lighting_water_intensity : lighting_intensity;

	if (block_image.lighting_type == LightingType::SMOOTH) {
		doSmoothLight(image, block_image, pos, id, false, rotation);
	} else if (block_image.lighting_type == LightingType::SIMPLE) {
		// doSmoothLight(image, block_image, pos, id, false, rotation);
		doSimpleLight(image, block_image, pos, id, rotation);
	} else if (block_image.lighting_type == LightingType::SMOOTH_TOP_REMAINING_SIMPLE) {
		CornerValues id = {1.0, 1.0, 1.0, 1.0};
		CornerValues up = getCornerColors(pos, CORNERS_TOP, intensity);
		blockImageMultiply(image, block_image.uv_image(0), id, id, up);

		float factor = getLightingColor(pos, intensity);
		blockImageMultiplyExcept(image, block_image.uv_image(0), FACE_UP_INDEX, factor);
	} else if (block_image.lighting_type == LightingType::SMOOTH_BOTTOM) {
		CornerValues left = getCornerColors(pos, CORNERS_LEFT, intensity);
		CornerValues right = getCornerColors(pos, CORNERS_RIGHT, intensity);
		CornerValues up = getCornerColors(pos, CORNERS_BOTTOM, intensity);
		blockImageMultiply(image, block_image.uv_image(0), left, right, up);
	}
}

LightingColor LightingRenderMode::calculateLightingColor(const LightingData& light) const {
	return pow(0.8, 15 - light.getLightLevel(day));
}

LightingData LightingRenderMode::getBlockLight(const mc::BlockPos& pos) {
	mc::Block block = getBlock(pos, mc::GET_ID | mc::GET_LIGHT);
	LightingData light = LightingData::estimate(block, block_images, world, *current_chunk);

	// TODO also move this to LightingData class?
	// lighting fix for The End
	// The End has no sun light set -> lighting looks ugly
	// just emulate the sun light for transparent blocks
	if (simulate_sun_light) {
		uint8_t sky = 15;
		/*
		if (block.id != 0 && !images->isBlockTransparent(block.id, block.data))
			sky = 0;
		*/
		const BlockImage& block_image = block_images->getBlockImage(block.id);
		if (!block_image.is_empty && !block_image.is_transparent) {
			sky = 0;
		}
		return LightingData(light.getBlockLight(), sky);
	}
	return light;
}

LightingColor LightingRenderMode::getLightingColor(const mc::BlockPos& pos, double intensity) {
	LightingData lighting = getBlockLight(pos);
	LightingColor color = calculateLightingColor(lighting);
	return color + (1-color)*(1-intensity);
}

LightingColor LightingRenderMode::getCornerColor(const mc::BlockPos& pos,
		const CornerNeighbors& corner, double intensity) {
	LightingColor color = 0;
	color += getLightingColor(pos + corner.pos1, intensity) * 0.25;
	color += getLightingColor(pos + corner.pos2, intensity) * 0.25;
	color += getLightingColor(pos + corner.pos3, intensity) * 0.25;
	color += getLightingColor(pos + corner.pos4, intensity) * 0.25;
	return color;
}

CornerColors LightingRenderMode::getCornerColors(const mc::BlockPos& pos,
		const FaceCorners& corners, double intensity) {
	if (intensity < 0)
		intensity = lighting_intensity;
	CornerColors colors = {{
		getCornerColor(pos, corners.corner1, intensity),
		getCornerColor(pos, corners.corner2, intensity),
		getCornerColor(pos, corners.corner3, intensity),
		getCornerColor(pos, corners.corner4, intensity),
	}};
	return colors;
}

void LightingRenderMode::doSmoothLight(RGBAImage& image, const BlockImage& block_image,
		const mc::BlockPos& pos, uint16_t id, bool use_bottom_corners, const RenderRotation& rotation) {

	// TODO adapt
	// - light only visible faces
	// - underwater

	std::array<bool, 3> side_mask = block_image.side_mask;
	bool under_water[3] = {false, false, false};

	mc::BlockDir dirs[3] = {rotation.getWest(), rotation.getSouth(), rotation.getTop()};
	for (int i = 0; i < 3; i++) {
		if (side_mask[i]) {
			const BlockImage& block = block_images->getBlockImage(getBlock(pos + dirs[i]).id);
			under_water[i] = block.is_waterlogged;
			side_mask[i] = block.is_empty || block.is_transparent;
		}
	}

	CornerValues left = {1.0, 1.0, 1.0, 1.0};
	CornerValues right = {1.0, 1.0, 1.0, 1.0};
	CornerValues up = {1.0, 1.0, 1.0, 1.0};

	if (side_mask[0]) {
		left = getCornerColors(pos, CORNERS_LEFT,
				under_water[0] ? lighting_water_intensity : lighting_intensity);
	}
	if (side_mask[1]) {
		right = getCornerColors(pos, CORNERS_RIGHT,
				under_water[1] ? lighting_water_intensity : lighting_intensity);
	}
	if (side_mask[2]) {
		up = getCornerColors(pos, use_bottom_corners ? CORNERS_BOTTOM : CORNERS_TOP,
				under_water[2] ? lighting_water_intensity : lighting_intensity);
	}
	blockImageMultiply(image, block_image.uv_image(0), left, right, up);
}

void LightingRenderMode::doSimpleLight(RGBAImage& image, const BlockImage& block_image,
		const mc::BlockPos& pos, uint16_t id, const RenderRotation& rotation) {
	// TODO adapt how to consider underwater with waterlogged blocks?
	// (some waterlogged blocks are rendered as if they weren't)

	float intensity = lighting_intensity;
	if (block_image.is_waterlogged) {
		// TODO adapt
		// all waterloggable blocks are assumed to be under water for now
		intensity = lighting_water_intensity;
	}
	uint8_t factor = getLightingColor(pos, intensity) * 255;
	if (factor == 255) {
		//blockImageTint(image, rgba(0xff, 0x00, 0x00));
		return;
	}

	/*
	int size = image.getWidth();
	for (int x = 0; x < size; x++) {
		for (int y = 0; y < size; y++) {
			uint32_t& pixel = image.pixel(x, y);
			if (pixel != 0)
				pixel = rgba_multiply(pixel, factor, factor, factor, 255);
		}
	}
	*/

	/*
	size_t n = image.getWidth() * image.getHeight();
	for (size_t i = 0; i < n; i++) {
		image.data[i] = rgba_multiply_scalar(image.data[i], factor);
	}
	*/

	blockImageMultiply(image, factor);
}

} /* namespace render */
} /* namespace mapcrafter */
