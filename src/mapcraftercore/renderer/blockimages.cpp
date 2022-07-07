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

#include "blockimages.h"

#include "biomes.h"
#include "../util.h"
#include "../mc/blockstate.h"
#include "../mc/chunk.h"

#include <chrono>
#include <map>
#include <vector>

namespace mapcrafter {
namespace util {

template <>
renderer::ColorMapType as<renderer::ColorMapType>(const std::string& str) {
	if (str == "foliage") {
		return renderer::ColorMapType::FOLIAGE;
	} else if (str == "foliage_flipped") {
		return renderer::ColorMapType::FOLIAGE_FLIPPED;
	} else if (str == "grass") {
		return renderer::ColorMapType::GRASS;
	} else if (str == "water") {
		return renderer::ColorMapType::WATER;
	} else {
		throw std::invalid_argument("Must be 'foliage', 'foliage_flipped', 'grass' or 'water'!");
	}
}

template <>
renderer::LightingType as<renderer::LightingType>(const std::string& str) {
	if (str == "none") {
		return renderer::LightingType::NONE;
	} else if (str == "simple") {
		return renderer::LightingType::SIMPLE;
	} else if (str == "smooth") {
		return renderer::LightingType::SMOOTH;
	} else if (str == "smooth_bottom") {
		return renderer::LightingType::SMOOTH_BOTTOM;
	} else {
		throw std::invalid_argument("Must be 'none', 'simple' or 'smooth'!");
	}
}

}
}

namespace mapcrafter {
namespace renderer {

bool ColorMap::parse(const std::string& str) {
	std::vector<std::string> parts = util::split(str, '|');
	if (parts.size() != 3) {
		return false;
	}

	for (size_t i = 0; i < 3; i++) {
		std::string part = parts[i];
		if (part.size() != 9 || part[0] != '#' || !util::isHexNumber(part.substr(1))) {
			return false;
		}
		colors[i] = util::parseHexNumber(part.substr(1));
	}

	return true;
}

uint32_t ColorMap::getColor(float x, float y) const {
	float r = 0, g = 0, b = 0, a = 0;
	// factors are barycentric coordinates
	// colors are colors of the colormap triangle points
	float factors[] = {
		x - y,
		1.0f - x,
		y
	};

	for (size_t i = 0; i < 3; i++) {
		r += (float) rgba_red(colors[i]) * factors[i];
		g += (float) rgba_green(colors[i]) * factors[i];
		b += (float) rgba_blue(colors[i]) * factors[i];
		a += (float) rgba_alpha(colors[i]) * factors[i];
	}

	return rgba(r, g, b, a);
}

BlockImages::~BlockImages() {
}

void blockImageTest(RGBAImage& block, const RGBAImage& uv_mask) {
	assert(block.getWidth() == uv_mask.getWidth());
	assert(block.getHeight() == uv_mask.getHeight());

	for (int x = 0; x < block.getWidth(); x++) {
		for (int y = 0; y < block.getHeight(); y++) {
			uint32_t& pixel = block.pixel(x, y);
			uint32_t uv_pixel = uv_mask.pixel(x, y);
			if (rgba_alpha(uv_pixel) == 0) {
				continue;
			}

			uint8_t side = rgba_blue(uv_pixel);
			if (side == FACE_LEFT_INDEX) {
				pixel = rgba(255, 0, 0);
			}
			if (side == FACE_RIGHT_INDEX) {
				pixel = rgba(0, 255, 0);
			}
			if (side == FACE_UP_INDEX) {
				pixel = rgba(0, 0, 255);;
			}
		}
	}
}

void blockImageMultiplyExcept(RGBAImage& block, const RGBAImage& uv_mask,
		uint8_t except_face, float factor) {
	assert(block.getWidth() == uv_mask.getWidth());
	assert(block.getHeight() == uv_mask.getHeight());

	for (int x = 0; x < block.getWidth(); x++) {
		for (int y = 0; y < block.getHeight(); y++) {
			uint32_t& pixel = block.pixel(x, y);
			uint32_t uv_pixel = uv_mask.pixel(x, y);
			if (rgba_alpha(uv_pixel) == 0) {
				continue;
			}

			uint8_t side = rgba_blue(uv_pixel);
			if (side != except_face) {
				pixel = rgba_multiply(pixel, factor, factor, factor);
			}
		}
	}
}

namespace {

inline uint32_t mix(uint32_t x, uint32_t y, uint32_t a) {
	// >> 8 = / 256, serves as approximation for division by 255
	return ((x * (255-a)) + (y * a)) >> 8;
}

}

void blockImageMultiply(RGBAImage& block, const RGBAImage& uv_mask,
		const CornerValues& factors_left, const CornerValues& factors_right, const CornerValues& factors_up) {
	assert(block.getWidth() == uv_mask.getWidth());
	assert(block.getHeight() == uv_mask.getHeight());

	uint32_t fl[4], fr[4], fu[4];
	for (int i = 0; i < 4; i++) {
		fl[i] = std::min(255u, (uint32_t)(factors_left[i] * 255u));
		fr[i] = std::min(255u, (uint32_t)(factors_right[i] * 255u));
		fu[i] = std::min(255u, (uint32_t)(factors_up[i] * 255u));
	}


	int n = block.getWidth() * block.getHeight();
	for (int i = 0; i < n; i++) {
		uint32_t& pixel = block.data[i];
		uint32_t uv_pixel = uv_mask.data[i];
		if (rgba_alpha(uv_pixel) == 0) {
			continue;
		}

		//const CornerValues* vptr = nullptr;
		uint32_t* f = nullptr;
		uint8_t side = rgba_blue(uv_pixel);
		if (side == FACE_LEFT_INDEX) {
			//vptr = &factors_left;
			f = fl;
		} else if (side == FACE_RIGHT_INDEX) {
			//vptr = &factors_right;
			f = fr;
		} else if (side == FACE_UP_INDEX) {
			//vptr = &factors_up;
			f = fu;
		} else {
			continue;
		}

		/*
		const CornerValues& values = *vptr;
		float u = (float) rgba_red(uv_pixel) / 255.0;
		float v = (float) rgba_green(uv_pixel) / 255.0;
		float ab = (1-u) * values[0] + u * values[1];
		float cd = (1-u) * values[2] + u * values[3];
		float x = (1-v) * ab + v * cd;
		*/

		uint32_t u = rgba_red(uv_pixel);
		uint32_t v = rgba_green(uv_pixel);

		//uint32_t ab = divide255((255-u), f[0]) + divide255(u, f[1]);
		//uint32_t cd = divide255((255-u), f[2]) + divide255(u, f[3]);
		//uint32_t x = divide255((255-v), ab) + divide255(v,  cd);

		// jetzt sogar 34.17
		// und mit noch mehr rgba_multiply sogar 35.28
		uint32_t ab = mix(f[0], f[1], u); // divide255((255-u) * f[0], u * f[1]);
		uint32_t cd = mix(f[2], f[3], u); // divide255((255-u) * f[2], u * f[3]);
		uint32_t x = mix(ab, cd, v); // divide255((255-v) * ab, v * cd);

		// OHNE BASIS
		// 45.68
		//pixel = rgba_multiply(pixel, 0.5);

		// 34.44
		//float x = 0.5;
		//pixel = rgba_multiply(pixel, x, x, x);

		// FLOAT ALS BASIS
		// 25.68
		//pixel = rgba_multiply(pixel, x, x, x);

		// geht. aber vielleicht auch nicht mega viel schneller
		//uint8_t factor = x * 255;
		//pixel = rgba_multiply(pixel, factor, factor, factor);

		// geht, 29.13
		//int factor = x * 255;
		//assert(factor >= 0 && factor <= 255);
		//pixel = rgba_multiply(pixel, factor);

		// INTEGER ALS BASIS
		//assert(x >= 0 && x <= 255);

		// geht, 28.93
		//double factor = (double) x / 255;
		//pixel = rgba_multiply(pixel, factor, factor, factor);

		// geht, 28.00
		//uint8_t factor = x;
		//pixel = rgba_multiply(pixel, factor, factor, factor);

		// geht, 29.83
		// ohne uv-alpha check sogar 32.15
		// ohne uv-alpha check und uint8_t 32.39
		// ... div255 32.60
		// ... anderes div255 32.88
		// rgba_ inline: 33.64
		pixel = rgba_multiply_scalar(pixel, x);
	}
}

void blockImageMultiply(RGBAImage& block, uint8_t factor) {
	int n = block.getWidth() * block.getHeight();
	for (int i = 0; i < n; i++) {
		block.data[i] = rgba_multiply_scalar(block.data[i], factor);
	}
}

void blockImageTint(RGBAImage& block, const RGBAImage& mask, uint32_t color) {
	assert(block.getWidth() == mask.getWidth());
	assert(block.getHeight() == mask.getHeight());

	int n = block.getWidth() * block.getHeight();
	for (int i = 0; i < n; i++) {
		uint32_t mask_pixel = mask.data[i];
		if (rgba_alpha(mask_pixel)) {
			uint32_t& pixel = block.data[i];
			// The mask is not supposed to be transfered directly
			// but to be blend in with block pixel
			// This will avoid white pixels on edges of the mask
			RGBAPixel colored_mask_pixel = rgba_multiply(mask_pixel, color);
			blend(pixel, colored_mask_pixel);
		}
	}
}

void blockImageTint(RGBAImage& block, uint32_t color) {
	int n = block.getWidth() * block.getHeight();
	for (int i = 0; i < n; i++) {
		uint32_t pixel = block.data[i];
		if(rgba_alpha(pixel)) {
			block.data[i] = rgba_multiply(pixel, color);
		}
	}
}

void blockImageTintHighContrast(RGBAImage& block, uint32_t color) {
	// get luminance of recolor:
	// "10*r + 3*g + b" should actually be "3*r + 10*g + b"
	// it was a typo, but doesn't look bad either
	int luminance = (10 * rgba_red(color) + 3 * rgba_green(color) + rgba_blue(color)) / 14;

	float alpha_factor = 3; // 3 is similar to alpha=85
	// something like that would be possible too, but overlays won't look exactly like
	// overlays with that alpha value, so don't use it for now
	// alpha_factor = (float) 255.0 / rgba_alpha(color);

	// try to do luminance-neutral additive/subtractive color
	// instead of alpha blending (for better contrast)
	// so first subtract luminance from each component
	int nr = (rgba_red(color) - luminance) / alpha_factor;
	int ng = (rgba_green(color) - luminance) / alpha_factor;
	int nb = (rgba_blue(color) - luminance) / alpha_factor;

	size_t n = block.getWidth() * block.getHeight();
	for (size_t i = 0; i < n; i++) {
		RGBAPixel& pixel = block.data[i];
		if ((pixel & 0xff000000) > 0) {
			pixel = rgba_add_clamp(pixel, nr, ng, nb, 0);
		}
	}
}

void blockImageTintHighContrast(RGBAImage& block, const RGBAImage& mask, int face, uint32_t color) {
	assert(block.getWidth() == mask.getWidth());
	assert(block.getHeight() == mask.getHeight());

	// same as above
	int luminance = (10 * rgba_red(color) + 3 * rgba_green(color) + rgba_blue(color)) / 14;
	float alpha_factor = 3;
	int nr = (rgba_red(color) - luminance) / alpha_factor;
	int ng = (rgba_green(color) - luminance) / alpha_factor;
	int nb = (rgba_blue(color) - luminance) / alpha_factor;

	size_t n = block.getWidth() * block.getHeight();
	for (size_t i = 0; i < n; i++) {
		RGBAPixel& pixel = block.data[i];
		RGBAPixel mask_pixel = mask.data[i];
		if (rgba_blue(mask_pixel) == face) {
			pixel = rgba_add_clamp(pixel, nr, ng, nb, 0);
		}
	}
}

void blockImageBlendZBuffered(RGBAImage& block, const RGBAImage& uv_mask,
		const RGBAImage& top, const RGBAImage& top_uv_mask) {
	assert(block.getWidth() == uv_mask.getWidth());
	assert(block.getHeight() == uv_mask.getHeight());
	assert(top.getWidth() == top_uv_mask.getWidth());
	assert(top.getHeight() == top_uv_mask.getHeight());
	assert(block.getWidth() == top.getWidth());
	assert(block.getHeight() == top.getHeight());

	size_t n = block.getWidth() * block.getHeight();
	for (size_t i = 0; i < n; i++) {
		RGBAPixel& pixel = block.data[i];
		const RGBAPixel& uv_pixel = uv_mask.data[i];
		const RGBAPixel& top_pixel = top.data[i];
		const RGBAPixel& top_uv_pixel = top_uv_mask.data[i];

		// basically what we want to do is:
		// compare uv-coords of block vs. waterlog pixels
		// if the uv-coords are the same and both textures pointing up, don't show water here

		// use the Z value of each pixels to blend or not the top pixel
		if (rgba_alpha(uv_pixel) < rgba_alpha(top_uv_pixel)) {
			blend(pixel, top_pixel);
		} else {
			// The top pixel is behind the block one, so use the alpha of
			// the destination pixel to blend the top pixel behind
			RGBAPixel tmp_pix = pixel;
			pixel = top_pixel;
			blend(pixel, tmp_pix);
		}
	}
}

void blockImageShadowEdges(RGBAImage& block, const RGBAImage& uv_mask,
		uint8_t north, uint8_t south, uint8_t east, uint8_t west, uint8_t bottomleft, uint8_t bottomright) {
	assert(block.getWidth() == uv_mask.getWidth());
	assert(block.getHeight() == uv_mask.getHeight());

	size_t n = block.getWidth() * block.getHeight();
	for (size_t i = 0; i < n; i++) {
		RGBAPixel& pixel = block.data[i];
		const RGBAPixel& uv_pixel = uv_mask.data[i];

		// TODO
		// not really optimized yet, and quite dirty code
		float u = (float) rgba_red(uv_pixel) / 255;
		float v = (float) rgba_green(uv_pixel) / 255;
		uint8_t face = rgba_blue(uv_pixel);

		uint8_t alpha = 0;
		#define setalpha(x) (alpha = std::max(alpha, (uint8_t) (x)))
		auto genalpha = [&alpha, &face](int mask_face, int edge, float uv) {
			// explanation of edge influence:
			// edge=0: no edge
			// edge=1: edge with threshold 2px
			// edge=2: edge with threshold 3px
			// edge=3: edge with threshold 3px, a bit darker (for stronger visual on leaves etc.)
			float t = (float) (1 + std::min(2, edge)) / 16.0;
			float strong = 64;
			float weak = 32;
			if (edge > 2) {
				strong = 128;
				weak = 64;
			}
			if (edge && face == mask_face && uv < t) {
				if (uv < t / 2.0) {
					setalpha(strong);
				} else {
					float a = (uv-t/2.0) / (t/2.0);
					setalpha((float) (1-a) * weak + a*16.0);
				}
			}
		};

		genalpha(FACE_UP_INDEX, north, v);
		genalpha(FACE_UP_INDEX, south, 1.0 - v);
		genalpha(FACE_UP_INDEX, east, 1.0 - u);
		genalpha(FACE_UP_INDEX, west, u);

		genalpha(FACE_LEFT_INDEX, bottomleft, 1.0 - v);
		genalpha(FACE_RIGHT_INDEX, bottomright, 1.0 - v);

		#undef setalpha

		pixel = rgba_multiply_scalar(pixel, 255 - alpha);
	}
}

bool blockImageIsTransparent(const RGBAImage& block, const RGBAImage& uv_mask) {
	assert(block.getWidth() == uv_mask.getWidth());
	assert(block.getHeight() == uv_mask.getHeight());

	for (int x = 0; x < block.getWidth(); x++) {
		for (int y = 0; y < block.getHeight(); y++) {
			uint32_t pixel = block.pixel(x, y);
			uint32_t uv_pixel = uv_mask.pixel(x, y);
			if (rgba_alpha(uv_pixel) == 0) {
				continue;
			}

			if (rgba_alpha(pixel) != 255) {
				return true;
			}
		}
	}

	return false;
}

std::array<bool, 3> blockImageGetSideMask(const RGBAImage& uv) {
	std::array<bool, 3> side_mask = {false, false, false};
	uint8_t mask_indices[3] = {FACE_LEFT_INDEX, FACE_RIGHT_INDEX, FACE_UP_INDEX};
	for (int x = 0; x < uv.getWidth(); x++) {
		for (int y = 0; y < uv.getHeight(); y++) {
			uint32_t pixel = uv.pixel(x, y);
			if (rgba_alpha(pixel) == 0) {
				continue;
			}

			uint8_t face = rgba_blue(pixel);
			for (uint8_t i = 0; i < 3; i++) {
				if (face == mask_indices[i]) {
					side_mask[i] = true;
				}
			}
		}
	}
	return side_mask;
}

RenderedBlockImages::RenderedBlockImages(mc::BlockStateRegistry& block_registry)
	: block_registry(block_registry), darken_left(1.0), darken_right(1.0) {
}

RenderedBlockImages::~RenderedBlockImages() {
	for (auto it = block_images.begin(); it != block_images.end(); ++it) {
		if (*it != nullptr) {
			delete *it;
		}
	}
}

void RenderedBlockImages::setBlockSideDarkening(float darken_left, float darken_right) {
	this->darken_left = darken_left;
	this->darken_right = darken_right;
}

bool RenderedBlockImages::loadBlockImages(fs::path path, std::string view, int rotation, int texture_size) {
	LOG(INFO) << "I will load block images from " << path << " now";

	if (!fs::is_directory(path)) {
		LOG(ERROR) << "Unable to load block images: " << path << " is not a directory!";
		return false;
	}

	std::string name = view + "_" + util::str(rotation) + "_" + util::str(texture_size);

	BlockAtlas::instance().OpenDictionnary(path,name);

	fs::path info_file = path / (name + ".txt");

	if (!fs::is_regular_file(info_file)) {
		LOG(ERROR) << "Unable to load block images: Block info file " << info_file
			<< " does not exist!";
		return false;
	}

	block_width = BlockAtlas::instance().GetBlockWidth();
	block_height = BlockAtlas::instance().GetBlockHeight();;
	block_images.reserve(BlockAtlas::instance().GetCount() * 2);

	std::ifstream in(info_file.string());
	// Skip the first line
	{
		std::string first_line;
		std::getline(in, first_line);
	}
	int lineno = 2;
	for (std::string line; std::getline(in, line); lineno++) {
		line = util::trim(line);
		if (line.size() == 0) {
			continue;
		}

		std::vector<std::string> parts = util::split(line, ' ');
		if (parts.size() != 3) {
			LOG(ERROR) << "Invalid line in block info file '" << info_file << "'!";
			LOG(ERROR) << "Line " << lineno << ": '" << line << "'";
			return false;
		}

		std::string block_name = parts[0];
		std::string variant = parts[1];
		std::map<std::string, std::string> block_info = util::parseProperties(parts[2]);

		std::vector<std::string> colors = util::split(block_info["color"],':');
		std::vector<std::string> uvs = util::split(block_info["uv"],':');
		std::vector<std::string> weights = util::split(block_info["weight"],':');
		std::size_t variantCnt = colors.size();
		if (weights.size()==0) weights = std::vector<std::string>(variantCnt,"1");
		assert(uvs.size() == variantCnt && weights.size() == variantCnt && "Block info file corrupted");
		std::vector<uint32_t> image_index(variantCnt), image_uv_index(variantCnt), image_weight(variantCnt);
		uint32_t total_weight = 0;
		double_t weight_factor = 1.0;
		for (std::size_t cnt=0; cnt<variantCnt; cnt++) {
			image_index[cnt] = util::as<int>(colors[cnt]);
			image_uv_index[cnt] = util::as<int>(uvs[cnt]);
			int weight = util::as<int>(weights[cnt]);
			image_weight[cnt] = weight;
			total_weight += weight;
		}
		weight_factor = double_t(1.0) / double_t(total_weight);

		mc::BlockState block_state = mc::BlockState::parse(block_name, variant);
		BlockImage& block = *new BlockImage();;
		block.image(image_index);
		block.uv_image(image_uv_index);
		block.weight_image(image_weight, weight_factor);

		block.is_biome = block_info.count("biome_type");
		if (block.is_biome) {
			block.is_masked_biome = block_info["biome_type"] == "masked";
			block.biome_color = util::as<ColorMapType>(block_info["biome_colors"]);
			if (block_info.count("biome_colormap")) {
				if (!block.biome_colormap.parse(block_info["biome_colormap"])) {
					LOG(WARNING) << "Unable to parse colormap '" << block_info["biome_colormap"] << "'.";
				}
			}
		}
		if (block_info.count("lighting_type")) {
			block.lighting_specified = true;
			block.lighting_type = util::as<LightingType>(block_info["lighting_type"]);
		}
		block.has_faulty_lighting = block_info.count("faulty_lighting");

		block.can_partial = block_info.count("partial") ? block_info["partial"] == "true" : false;

		block.shadow_edges = -1;
		if (block_info.count("shadow_edges")) {
			block.shadow_edges = util::as<int>(block_info["shadow_edges"]);
		}

		auto set_image_idx = [this](int id, BlockImage* block ) -> void {
			uint16_t id_next = id + 1;
			if (block_images.size() < id_next) {
				block_images.resize(id + 1, nullptr);
			}
			block_images[id] = block;
		};

		block.is_waterlogged = block_info.count("inherently_waterlogged") != 0;
		if (!block.is_waterlogged) {
			if (block_info.count("is_waterloggable") != 0) {
				int water_id;
				mc::BlockState waterlogged = block_state;
				BlockImage& water_block = *new BlockImage();
				water_block = block;
				water_block.is_waterlogged = true;
				waterlogged.setProperty("waterlogged", "true");
				water_id = block_registry.getBlockID(waterlogged);
				set_image_idx(water_id, &water_block);
				block_state.setProperty("waterlogged", "false");
			}
		}

		// Save the blockimage
		uint16_t id = block_registry.getBlockID(block_state);
		set_image_idx(id, &block);

		const std::map<std::string, std::string>& properties = block_state.getProperties();
		for (std::map<std::string, std::string>::const_iterator it = properties.begin(); it != properties.end(); ++it) {
			block_registry.addKnownProperty(block_state.getName(), it->first);
		}

		//std::cout << block_name << " " << variant << std::endl;
	}
	in.close();

	prepareBlockImages();
	//runBenchmark();

	return true;
}

RGBAImage RenderedBlockImages::exportBlocks() const {
	/*
	std::vector<RGBAImage> blocks;

	for (auto it = block_images.begin(); it != block_images.end(); ++it) {
		blocks.push_back(it->second.image);
	}

	if (blocks.size() == 0) {
		return RGBAImage(0, 0);
	}

	int width = 16;
	int height = std::ceil((double) blocks.size() / width);
	int block_size = blocks[0].getWidth();
	RGBAImage image(width * block_size, height * block_size);

	for (int y = 0; y < height; y++) {
		for (int x = 0; x < width; x++) {
			int offset = y * width + x;
			if ((size_t) offset >= blocks.size())
				break;
			image.alphaBlit(blocks.at(offset), x * block_size, y * block_size);
		}
	}

	return image;
	*/
	return RGBAImage(1, 1);
}

const BlockImage& RenderedBlockImages::getBlockImage(uint16_t id) const {
	if (block_images.size() <= id) {
		const mc::BlockState& block_state = block_registry.getBlockState(id);

		if (!block_state.hasProperty("waterlogged")) {
			mc::BlockState test = mc::BlockState::parse(block_state.getName(), block_state.getVariantDescription());
			test.setProperty("waterlogged", "false");
			return getBlockImage(block_registry.getBlockID(test));
		}
		LOG(INFO) << "Unknown block " << block_state.getName() << " " << block_state.getVariantDescription();

		return unknown_block;
	}
	return *block_images[id];
}

void RenderedBlockImages::prepareBiomeBlockImage(RGBAImage& image, const BlockImage& block, uint32_t color) {

	if (block.is_masked_biome) {
		blockImageTint(image, *block.biome_mask, color);
	} else {
		blockImageTint(image, color);
	}
}

int RenderedBlockImages::getTextureSize() const {
	return texture_size;
}

int RenderedBlockImages::getBlockSize() const {
	return block_width;
}

int RenderedBlockImages::getBlockWidth() const {
	return block_width;
}

int RenderedBlockImages::getBlockHeight() const {
	return block_height;
}

void RenderedBlockImages::prepareBlockImages() {
	const uint16_t solid_id = block_registry.getBlockID(mc::BlockState("minecraft:unknown_block"));
	assert(block_images.size() > solid_id && block_images[solid_id] != nullptr);
	const BlockImage& solid = *block_images[solid_id];

	const uint16_t air_id = block_registry.getBlockID(mc::BlockState("minecraft:air"));
	assert(block_images.size() > air_id && block_images[air_id] != nullptr);
	const BlockImage& air = *block_images[air_id];
	const uint16_t air_image_id = air.images_idx[0];

	std::unordered_set<uint16_t> shaded_blocks;
	shaded_blocks.reserve(BlockAtlas::instance().GetCount());

	// Go through all images to clarify few flags, and
	// prepare compute the shading per direction
	for (uint16_t id = 0; id < block_images.size(); ++id) {
		if (block_images[id] == nullptr) {
			continue;
		}

		BlockImage& block = *block_images[id];
		const mc::BlockState& block_state = block_registry.getBlockState(id);

		// Check if display is empty
		block.is_empty = true;
		for (auto &&i : block.images_idx) {
			if (i != air_image_id) {
				block.is_empty = false;
				break;
			}
		}

		// Biome mask ?
		// The detection by name is not the best
		// TODO: Make a flag in block file to disable the shading
		std::string name = block_state.getName();
		if (!util::endswith(name, "_biome_mask")) {
			for (int16_t i = block.images_idx.size()-1; i >= 0 ; --i) {
				uint32_t bid = block.images_idx[i];
				uint32_t uv_bid = block.uv_images_idx[i];
				BlockAtlas::instance().ShadeBlock(bid, uv_bid, darken_left, darken_right, 1.0);
			}
		}

		block.side_mask = blockImageGetSideMask(block.uv_image(0));
		block.is_transparent = blockImageIsTransparent(block.image(0), solid.uv_image(0));

		if (block.is_biome && block.is_masked_biome) {
			std::string mask_name = name + "_biome_mask";
			uint16_t mask_id = block_registry.getBlockID(mc::BlockState::parse(mask_name, block_state.getVariantDescription()));
			assert(block_images.size() > mask_id && block_images[mask_id] != nullptr);
			block.biome_mask = &block_images[mask_id]->image(0);
		}

		if (!block.lighting_specified) {
			if (!block.is_transparent) {
				block.lighting_type = LightingType::SMOOTH;
			} else {
				if (block.is_waterlogged) {
					block.lighting_type = LightingType::SMOOTH_TOP_REMAINING_SIMPLE;
				} else {
					block.lighting_type = LightingType::SIMPLE;
				}
			}
		}

		if (block.shadow_edges == -1) {
			block.shadow_edges = !block.is_transparent;
		}
	}

	unknown_block = solid;
}

void RenderedBlockImages::runBenchmark() {
	LOG(INFO) << "Running benchmark";

	typedef std::chrono::high_resolution_clock clock_;
	typedef std::chrono::duration<double, std::ratio<1> > second_;

	uint16_t id = block_registry.getBlockID(mc::BlockState::parse("minecraft:full_water", "up=false,south=false,west=false"));
	assert(block_images.size() > id && block_images[id] != nullptr);
	const BlockImage& solid = *block_images[id];

	// uint32_t color = rgba(0x30, 0x59, 0xad, 0xff);

	CornerValues left = {1.0, 0.8, 0.5, 1.0};
	CornerValues right = {1.0, 0.6, 0.3, 0.8};
	CornerValues up = {0.5, 1.0, 0.6, 0.8};

	std::chrono::time_point<clock_> begin = clock_::now();
	const RGBAImage& image = solid.image(0);
	RGBAImage solid_image(image.getHeight(), image.getWidth());
	solid_image.simpleBlit(image,0,0);

	for (size_t i = 0; i < 1000000; i++) {

		// 5.841s
		//blockImageTint(image, image, 0x30, 0x59, 0xad, 0xff);

		// 3.441s
		// 3.072s mit nicem rgba_multiply
		// 2.876s mit alpha check als bitmaske und vergleich > 0
		//blockImageTint(image, image, color);

		// 1.597s (wenn alpha check weg!)
		// 1.590s (sonst auch!)
		// 1.105s mit nicem rgba_multiply
		//blockImageTint(image, color);

		// 9.534s mit rgb_multiply_scalar
		// 6.345s mit rgb_multiply_scalar inline
		// 6.377s mit rgba_multiply_scalar ohne f+1
		// 6.126s doch wenn der alpha check drin ist
		blockImageMultiply(solid_image, solid.uv_image(0), left, right, up);
	}

	double elapsed = std::chrono::duration_cast<second_>(clock_::now() - begin).count();
	LOG(INFO) << "took " << elapsed << "s";
	exit(0);
}

}
}
