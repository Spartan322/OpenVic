#pragma once

#include <cstdint>

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/resource_format_loader.hpp>
#include <godot_cpp/classes/wrapped.hpp>
#include <godot_cpp/templates/hash_map.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <godot_cpp/variant/typed_array.hpp>

#include "openvic-extension/scene/resources/XACMesh.hpp"

namespace godot {
	class Node3D;
	class Shader;
	class StringName;
	class ShaderMaterial;
	class String;
	class FileAccess;
	enum Error;
	class Variant;
	class PackedStringArray;
}

namespace OpenVic {
	extern void startup_scene_loop();

	class ResourceFormatLoaderUnitModel;

	class XACModel : public godot::Resource {
		GDCLASS(XACModel, godot::Resource);

		friend class ResourceFormatLoaderXAC;

		godot::Vector<godot::Ref<XACMesh>> mesh_list;

		bool has_specular_material = false;

	protected:
		static void _bind_methods();

	public:
		godot::Error load_model_from(
			godot::Ref<godot::Shader> const& flag_shader, godot::Ref<godot::Shader> const& scrolling_shader,
			godot::Ref<godot::Shader> const& unit_colour_shader, godot::Ref<godot::FileAccess> const& file
		);

		void set_mesh_list(godot::Vector<godot::Ref<XACMesh>> const& mesh_list);
		void _set_mesh_list(godot::TypedArray<godot::Ref<XACMesh>> const& mesh_list);
		uint32_t push_mesh(godot::Ref<XACMesh> const& mesh);
		void set_mesh(uint32_t index, godot::Ref<XACMesh> const& mesh);
		godot::Ref<XACMesh> get_mesh(uint32_t index) const;
		uint32_t get_mesh_count() const;

		virtual godot::Node3D* generate_node() const;

		virtual ~XACModel();
	};

	class ResourceFormatLoaderXAC : public godot::ResourceFormatLoader {
		GDCLASS(ResourceFormatLoaderXAC, godot::ResourceFormatLoader);

		friend OpenVic::ResourceFormatLoaderUnitModel;

		inline static ResourceFormatLoaderXAC* singleton = nullptr;

		static constexpr uint32_t MAX_UNIT_TEXTURES = 64;

		godot::Ref<godot::Shader> flag_shader;
		godot::Ref<godot::Shader> scrolling_shader;
		godot::Ref<godot::Shader> unit_colour_shader;

		godot::HashMap<godot::StringName, int32_t> scroll_index_map;

		friend void OpenVic::startup_scene_loop();
		static void _on_startup_mainloop();

	protected:
		inline static void _bind_methods() {}

	public:
		virtual godot::Variant _load(
			const godot::String& p_path, const godot::String& p_original_path, bool p_use_sub_threads, int32_t p_cache_mode
		) const override;
		virtual godot::PackedStringArray _get_recognized_extensions() const override;
		virtual bool _handles_type(const godot::StringName& p_type) const override;
		virtual godot::String _get_resource_type(const godot::String& p_path) const override;

		ResourceFormatLoaderXAC();
		virtual ~ResourceFormatLoaderXAC();
	};
}
