#pragma once

#include <cstdint>
#include <vector>

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>

#include <openvic-simulation/utility/Containers.hpp>

namespace OpenVic::efx {
	enum struct Version : int32_t { ONE = 1, TWO, THREE };
	enum struct Type : int32_t {
		// XSM
		XSM_METADATA = 0xC9,
		XSM_SUBMOTION = 0xC8,
		XSM_BONE_ANIMATION = 0xCA, //
		// XAC
		XAC_METADATA = 0x7,
		XAC_NODE_HIERARCHY = 0xB,
		XAC_NODE_CHUNK = 0x0,
		XAC_MESH = 0x1,
		XAC_SKINNING = 0x2,
		XAC_MATERIAL_DEFINITION = 0x3,
		XAC_MATERIAL_LAYER = 0x4,
		XAC_MATERIAL_TOTALS = 0xD,
		XAC_JUNK_ONE = 0x8,
		XAC_JUNK_TWO = 0xA
	};

	template<typename T>
	_FORCE_INLINE_ void read_struct(godot::Ref<godot::FileAccess> const& file, T& t, godot::Error* error) {
		if (error) {
			*error = godot::ERR_FILE_CANT_READ;
		}

		ERR_FAIL_COND_MSG(
			file->get_length() - file->get_position() < sizeof(T),
			godot::vformat("Cannot read struct size of %d, reads past end of file", sizeof(T))
		);

		uint64_t res = file->get_buffer(reinterpret_cast<uint8_t*>(&t), sizeof(t));

		if (error) {
			*error = godot::ERR_INVALID_DATA;
		}
		ERR_FAIL_COND(res != sizeof(t));

		godot::Error err = file->get_error();
		if (error) {
			*error = err;
		}
		ERR_FAIL_COND_MSG(err != godot::OK, "Failed to read file buffer");
	}

	template<typename T>
	_FORCE_INLINE_ T read_struct(godot::Ref<godot::FileAccess> const& file, godot::Error* error) {
		T result;
		read_struct(file, result, error);
		return result;
	}

	// Warning: works on the assumption of it being a packed struct being loaded into the array
	template<typename T, typename Alloc>
	_FORCE_INLINE_ void read_struct_array( //
		godot::Ref<godot::FileAccess> const& file, std::vector<T, Alloc>& t, uint32_t size, godot::Error* error
	) {
		if (error) {
			*error = godot::ERR_FILE_CANT_READ;
		}

		ERR_FAIL_COND_MSG(
			file->get_length() - file->get_position() < size * sizeof(T),
			godot::vformat("Cannot read struct array length of %d, reads past end of file", size * sizeof(T))
		);

		t.resize(size * sizeof(T));
		uint64_t res = file->get_buffer(reinterpret_cast<uint8_t*>(t.data()), sizeof(T) * size);

		if (error) {
			*error = godot::ERR_INVALID_DATA;
		}
		ERR_FAIL_COND(res != sizeof(T) * size);

		godot::Error err = file->get_error();
		if (error) {
			*error = err;
		}
		ERR_FAIL_COND_MSG(err != godot::OK, "Failed to read file buffer");
	}

	static void	read_string( //
		godot::Ref<godot::FileAccess> const& file, godot::String& str, godot::Error* error, bool replace_chars = true
	) {
		if (error) {
			*error = godot::ERR_FILE_CANT_READ;
		}

		// string = uint32 len, char[len]
		uint32_t length = file->get_32();
		ERR_FAIL_COND_MSG(
			file->get_length() - file->get_position() < length,
			godot::vformat("Cannot read string length of %d, reads past end of file", length)
		);

		godot::PackedByteArray array;
		array.resize(length);
		uint64_t res = file->get_buffer(array.ptrw(), length);

		if (error) {
			*error = godot::ERR_INVALID_DATA;
		}
		ERR_FAIL_COND(res != array.size());

		godot::Error err = file->get_error();
		if (error) {
			*error = err;
		}
		ERR_FAIL_COND_MSG(err != godot::OK, "Failed to read file buffer");

		str = array.get_string_from_ascii();
		if (replace_chars) {
			str = str.replace(":", "_");
			str = str.replace("\\", "_");
			str = str.replace("/", "_");
		}
	}

#pragma pack(push, 1)
	struct chunk_header_t {
		Type type;
		int32_t length;
		Version version;
	};

	_FORCE_INLINE_ chunk_header_t read_chunk_header(godot::Ref<godot::FileAccess> const& file, godot::Error* error) {
		chunk_header_t header; // NOLINT(cppcoreguidelines-pro-type-member-init)

		if (error) {
			*error = godot::ERR_FILE_CANT_READ;
		}

		if (file->get_length() - file->get_position() < sizeof(chunk_header_t)) {
			return header;
		}

		uint64_t res = file->get_buffer(reinterpret_cast<uint8_t*>(&header), sizeof(header));

		if (error) {
			*error = godot::ERR_INVALID_DATA;
		}
		ERR_FAIL_COND_V(res != sizeof(header), header);

		godot::Error err = file->get_error();
		if (error) {
			*error = err;
		}
		ERR_FAIL_COND_V_MSG(err != godot::OK, header, "Failed to read file buffer");

		return header;
	}

	struct vec2d_t {
		float x;
		float y;
	};

	struct vec3d_t {
		float x;
		float y;
		float z;
	};

	struct vec3d_inpos_t : vec3d_t {};

	struct vec4d_t {
		float x;
		float y;
		float z;
		float w;
	};

	struct quat_v1_t {
		float x;
		float y;
		float z;
		float w;
	};

	struct quat_v2_t { // divide by 32767 to get proper quat
		int16_t x;
		int16_t y;
		int16_t z;
		int16_t w;
	};

	struct matrix44_t {
		vec4d_t col1;
		vec4d_t col2;
		vec4d_t col3;
		vec4d_t col4;
	};

	struct color_32_t {
		int8_t r;
		int8_t g;
		int8_t b;
		int8_t a;
	};

	struct color_128_t {
		int32_t r;
		int32_t g;
		int32_t b;
		int32_t a;
	};
#pragma pack(pop)
}
