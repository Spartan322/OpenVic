#pragma once

#include <cstdint>

#include <godot_cpp/classes/resource_format_loader.hpp>
#include <godot_cpp/classes/wrapped.hpp>

#include "openvic-extension/scene/resources/XACModel.hpp"

namespace godot {
	class Node3D;
	class Variant;
	class String;
	class PackedStringArray;
	class StringName;
}

namespace OpenVic {
	class UnitModel : public XACModel {
		GDCLASS(UnitModel, XACModel);

	protected:
		static void _bind_methods();

	public:
		godot::Node3D* generate_node() const override;

		virtual ~UnitModel();
	};

	class ResourceFormatLoaderUnitModel : public godot::ResourceFormatLoader {
		GDCLASS(ResourceFormatLoaderUnitModel, godot::ResourceFormatLoader);

	protected:
		inline static void _bind_methods() {}

	public:
		virtual godot::Variant _load(
			const godot::String& p_path, const godot::String& p_original_path, bool p_use_sub_threads, int32_t p_cache_mode
		) const override;
		virtual godot::PackedStringArray _get_recognized_extensions() const override;
		virtual bool _handles_type(const godot::StringName& p_type) const override;
		virtual godot::String _get_resource_type(const godot::String& p_path) const override;

		ResourceFormatLoaderUnitModel();
		virtual ~ResourceFormatLoaderUnitModel();
	};
}
