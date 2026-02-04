#pragma once

#include <cstdint>

#include <godot_cpp/classes/animation.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/resource_format_loader.hpp>
#include <godot_cpp/classes/wrapped.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_vector4_array.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <openvic-simulation/utility/Containers.hpp>

namespace OpenVic {
	class ResourceFormatLoaderXSM : public godot::ResourceFormatLoader {
		GDCLASS(ResourceFormatLoaderXSM, godot::ResourceFormatLoader);

	protected:
		inline static void _bind_methods() {}

	public:
		virtual godot::Variant _load(
			const godot::String& p_path, const godot::String& p_original_path, bool p_use_sub_threads, int32_t p_cache_mode
		) const override;
		virtual godot::PackedStringArray _get_recognized_extensions() const override;
		virtual bool _handles_type(const godot::StringName& p_type) const override;
		virtual godot::String _get_resource_type(const godot::String& p_path) const override;

		ResourceFormatLoaderXSM();
		virtual ~ResourceFormatLoaderXSM();
	};
}
