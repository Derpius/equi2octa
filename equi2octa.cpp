#define STB_IMAGE_IMPLEMENTATION
#define STB_IMAGE_WRITE_IMPLEMENTATION
#define STBI_WINDOWS_UTF8
#include "stb_image.h"
#include "stb_image_write.h"

#include "glm/glm.hpp"

#include <filesystem>
#include <string>
#include <iostream>

using namespace glm;

static vec2 invAtan{ .1591f, .3183f };
vec2 EquirectangularDirToTexel(vec3 direction)
{
	vec2 uv = vec2(atan(direction.y, direction.x), asin(-direction.z));
	uv *= invAtan;
	uv += .5f;

	return uv;
}

// https://gamedev.stackexchange.com/questions/169508/octahedral-impostors-octahedral-mapping
vec3 OctahedralTexelToDir(vec2 uv)
{
	uv.y = 1.f - uv.y;

	vec3 position = vec3(2.f * (uv - .5f), 0.f);

	vec2 absolute = abs(vec2(position));
	position.z = 1.f - absolute.x - absolute.y;

	if (position.z < 0.f) {
		position = vec3(
			sign(vec2(position)) * vec2(1.f - absolute.y, 1.f - absolute.x),
			position.z
		);
	}

	return normalize(position);
}

int main(const int argc, const char* argv[])
{
	std::filesystem::path input;
	std::filesystem::path output;

	switch (argc) {
	case 2: // input path only (output should be same as input)
		input = argv[1];
		break;
	case 3: // input and output path
		input = argv[1];
		output = argv[2];
		break;
	default:
		printf("Expected 1 or 2 arguments, got %i", argc - 1); // subtract the program name
		return -1;
	}

	// Canonicalise input
	std::error_code err;
	input = std::filesystem::canonical(input, err);

	if (err) {
		printf("Couldn't find input file:\n%s\n", err.message().c_str());
		return -1;
	}

	if (!std::filesystem::is_regular_file(input)) {
		printf("Input is not a regular file\n");
		return -1;
	}

	if (argc == 2) { // Implicit output
		std::filesystem::path filename = input.stem();
		filename += "_octahedral.hdr";
		output = input.parent_path() / filename;
	} else { // Explicit output
		if (!output.has_filename()) {
			printf("Invalid output path\n");
			return -1;
		}

		output = std::filesystem::canonical(output.parent_path(), err);
		if (err) {
			printf("Couldn't find output path:\n%s\n", err.message().c_str());
			return -1;
		}

		output /= output.filename();
	}

	if (std::filesystem::exists(output)) {
		if (std::filesystem::is_directory(output)) {
			printf("Cannot overwrite directory\n");
			return -1;
		}

		if (!std::filesystem::is_regular_file(output)) {
			printf("Cannot overwrite special file\n");
			return -1;
		}

		printf("Do you want to overwrite %s?\n[y/N]: ", output.string().c_str());

		std::string answer;
		std::cin >> answer;
		std::transform(answer.begin(), answer.end(), answer.begin(), ::tolower);
		if (answer != "y") return 0;
	}

#ifdef _WIN32
	size_t size = sizeof(wchar_t) * input.native().size();
	char* buf = new char[size];
	stbi_convert_wchar_to_utf8(buf, size, input.native().c_str());
#else
	char* buf = input.native().c_str();
#endif

	int resX, resY, channels;
	float* pEquiData = stbi_loadf(buf, &resX, &resY, &channels, 0);
#ifdef _WIN32
	delete[] buf;
#endif

	if (pEquiData == nullptr) {
		printf("Failed to load input image\n");
		return -1;
	};

	float* pConverted = new float[resY * resY * channels];

	#pragma omp parallel for collapse(2)
	for (int y = 0; y < resY; y++) {
		for (int x = 0; x < resY; x++) {
			vec3 dir = OctahedralTexelToDir(vec2(static_cast<float>(x) / resY, static_cast<float>(y) / resY));
			vec2 equiUV = EquirectangularDirToTexel(dir);
			int xEqui = static_cast<int>(floorf(equiUV.x * resX));
			int yEqui = static_cast<int>(floorf(equiUV.y * resY));

			for (int channel = 0; channel < channels; channel++) {
				pConverted[y * resY * channels + x * channels + channel] = pEquiData[yEqui * resX * channels + xEqui * channels + channel];
			}
		}
	}
	stbi_image_free(pEquiData);

#ifdef _WIN32
	size = sizeof(wchar_t) * output.native().size();
	buf = new char[size];
	stbi_convert_wchar_to_utf8(buf, size, output.native().c_str());
#else
	buf = output.native().c_str();
#endif

	int success = stbi_write_hdr(buf, resY, resY, channels, pConverted);
	delete[] pConverted;
#ifdef _WIN32
	delete[] buf;
#endif

	if (success == 0) {
		printf("Failed to save output image\n");
		return -1;
	}

	return 0;
}
