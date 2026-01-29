#pragma once

#include <cstdint>
#include <vector>

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/core/defs.hpp>

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
	_FORCE_INLINE_ godot::Error read_struct(godot::Ref<godot::FileAccess> const& file, T& t) {
		if (file->get_length() - file->get_position() < sizeof(T)) {
			return godot::ERR_FILE_CANT_READ;
		}
		uint64_t res = file->get_buffer(reinterpret_cast<uint8_t*>(&t), sizeof(t)) == sizeof(t);
		return res != 0 ? godot::OK : godot::FAILED;
	}

	// Warning: works on the assumption of it being a packed struct being loaded into the array
	template<typename T, typename Alloc>
	_FORCE_INLINE_ godot::Error	read_struct_array( //
		godot::Ref<godot::FileAccess> const& file, std::vector<T, Alloc>& t, uint32_t size
	) {
		if (file->get_length() - file->get_position() < size * sizeof(T)) {
			return godot::ERR_FILE_CANT_READ;
		}
		t.resize(size * sizeof(T));
		uint64_t res = file->get_buffer(reinterpret_cast<uint8_t*>(t.data()), sizeof(T) * size) == sizeof(t);
		return res != 0 ? godot::OK : godot::FAILED;
	}

	static bool read_string(godot::Ref<godot::FileAccess> const& file, godot::String& str, bool replace_chars = true) {
		// string = uint32 len, char[len]
		uint32_t length = file->get_32();
		if (file->get_length() - file->get_position() < length) {
			return false;
		}

		str = file->get_buffer(length).get_string_from_ascii();
		if (replace_chars) {
			str = str.replace(":", "_");
			str = str.replace("\\", "_");
			str = str.replace("/", "_");
		}
		return true;
	}

#pragma pack(push, 1)
	template<typename SelfT>
	struct readable_struct_t {
		_FORCE_INLINE_ godot::Error read_from(godot::Ref<godot::FileAccess> const& file) {
			return read_struct<SelfT>(file, *this);
		}
	};

	struct chunk_header_t : readable_struct_t<chunk_header_t> {
		Type type;
		int32_t length;
		Version version;
	};

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
