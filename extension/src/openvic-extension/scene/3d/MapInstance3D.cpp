#include "MapInstance3D.hpp"

#include <godot_cpp/classes/animation_library.hpp>
#include <godot_cpp/classes/animation_player.hpp>
#include <godot_cpp/classes/bone_attachment3d.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/object.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/skeleton3d.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/core/object.hpp>
#include <godot_cpp/core/property_info.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>

#include <openvic-simulation/core/Typedefs.hpp>

#include "openvic-extension/core/Bind.hpp"
#include "openvic-extension/scene/resources/XACModel.hpp"

using namespace OpenVic;
using namespace godot;

inline static StringName _animation_player_name() {
	static const StringName animation_player_name = { "anim_player", true };
	return animation_player_name;
}

MapInstance3D::MapInstance3D() {
	Ref<AnimationLibrary> animation_library;
	animation_library.instantiate();
	animation_library->set_name("default_lib");

	animation_player = memnew(AnimationPlayer);
	animation_player->set_name(_animation_player_name());
	animation_player->add_animation_library(_animation_player_name(), animation_library);
	add_child(animation_player, false, INTERNAL_MODE_FRONT);
}

MapInstance3D::~MapInstance3D() {}

Skeleton3D* MapInstance3D::get_skeleton() const {
	if (skeleton != nullptr) {
		return skeleton;
	}

	const int32_t count = get_child_count();
	for (int32_t index = 0; index < count; index++) {
		skeleton = Object::cast_to<Skeleton3D>(get_child(index));
		if (skeleton != nullptr) {
			return skeleton;
		}
	}

	ERR_FAIL_V_MSG(nullptr, vformat("Could not find Skeleton3D for %s", get_path()));
}

AnimationPlayer* MapInstance3D::get_animation_player() const {
	return animation_player;
}

void MapInstance3D::add_animation(StringName const& animation_name, Ref<Animation> const& animation) {
	Ref<AnimationLibrary> library = animation_player->get_animation_library(_animation_player_name());
	ERR_FAIL_COND(library.is_null());
	ERR_FAIL_COND(library->has_animation(animation_name));

	library->add_animation(animation_name, animation);
}

void MapInstance3D::remove_animation(StringName const& animation_name) {
	Ref<AnimationLibrary> library = animation_player->get_animation_library(_animation_player_name());
	ERR_FAIL_COND(library.is_null());

	library->remove_animation(animation_name);
}

Ref<Animation> MapInstance3D::get_animation(StringName const& animation_name) const {
	Ref<AnimationLibrary> library = animation_player->get_animation_library(_animation_player_name());
	ERR_FAIL_COND_V(library.is_null(), Ref<Animation>());

	Ref<Animation> anim = library->get_animation(animation_name);
	ERR_FAIL_COND_V(anim.is_null(), anim);

	return anim;
}

void MapInstance3D::set_current_animation(StringName const& animation) {
	Ref<AnimationLibrary> library = animation_player->get_animation_library(_animation_player_name());
	ERR_FAIL_COND(library.is_null());

	Ref<Animation> anim = library->get_animation(animation);
	ERR_FAIL_COND(anim.is_null());

	StringName animation_path = animation_player->find_animation(anim);
	animation_player->set_current_animation(animation_path);
	for (MapInstance3D* child : child_instances) {
		child->animation_player->set_current_animation(animation_path);
	}
	is_dirty = true;
}

StringName MapInstance3D::get_current_animation() const {
	return animation_player->get_current_animation();
}

Error MapInstance3D::attach_model(StringName const& bone_name, Node3D* instance) {
	ERR_FAIL_NULL_V(instance, ERR_INVALID_PARAMETER);

	Skeleton3D* skeleton = get_skeleton();
	ERR_FAIL_NULL_V(skeleton, ERR_UNCONFIGURED);

	int32_t bone_index = skeleton->find_bone(bone_name);
	ERR_FAIL_COND_V(bone_index == -1, ERR_INVALID_PARAMETER);

	BoneAttachment3D* attachment = memnew(BoneAttachment3D);
	skeleton->add_child(attachment);
	attachment->add_child(instance);
	attachment->set_name(bone_name);
	attachment->set_bone_idx(bone_index);

	MapInstance3D* child_instance = Object::cast_to<MapInstance3D>(instance);
	if (child_instance == nullptr) {
		return OK;
	}

	ERR_FAIL_COND_V(child_instance->parent_instance == this, ERR_INVALID_PARAMETER);

	child_instance->parent_instance = this;
	child_instances.push_back(child_instance);
	child_instance->is_dirty = true;
	child_instance->update_shader_data();
	return OK;
}

void MapInstance3D::set_colour_index_override(uint32_t index, Color color) {
	if (index >= colour_overrides.size()) {
		colour_overrides.resize(index + 1);
	}

	colour_overrides[index] = color;
	is_dirty = true;
}

Color MapInstance3D::get_colour_from_index(uint32_t index) const {
	if (colour_overrides.is_empty() && parent_instance != nullptr) {
		return parent_instance->get_colour_from_index(index);
	}

	ERR_FAIL_INDEX_V(index, colour_overrides.size(), Color());
	return colour_overrides[index];
}

void MapInstance3D::set_texture_animation_scroll_override(StringName const& animation, float scroll) {
	Ref<AnimationLibrary> library = animation_player->get_animation_library(_animation_player_name());
	ERR_FAIL_COND(library.is_null());

	Ref<Animation> anim = library->get_animation(animation);
	ERR_FAIL_COND(anim.is_null());

	animation_to_texture_scroll_override[animation] = scroll;
	is_dirty = true;
}

float MapInstance3D::get_texture_animation_scroll(StringName const& animation) const {
	Ref<AnimationLibrary> library = animation_player->get_animation_library(_animation_player_name());
	ERR_FAIL_COND_V(library.is_null(), 0);

	Ref<Animation> anim = library->get_animation(animation);
	ERR_FAIL_COND_V(anim.is_null(), 0);

	float const* texture_scroll = animation_to_texture_scroll_override.getptr(animation);
	if (texture_scroll == nullptr && parent_instance != nullptr) {
		return parent_instance->get_texture_animation_scroll(animation);
	}

	return *texture_scroll;
}

void MapInstance3D::set_flag_index_override(uint16_t index) {
	flag_index_override = index;
	is_dirty = true;
}

void MapInstance3D::reset_flag_index() {
	flag_index_override = -1;
	is_dirty = true;
}

uint16_t MapInstance3D::get_flag_index() const {
	if (flag_index_override == -1 && parent_instance != nullptr) {
		return parent_instance->get_flag_index();
	}

	return flag_index_override;
}

inline void MapInstance3D::update_shader_data() {
	if (!is_dirty) {
		return;
	}

	for (MapInstance3D* child_instance : child_instances) {
		child_instance->is_dirty = true;
		child_instance->update_shader_data();
	}

	const float texture_animation_scroll = get_texture_animation_scroll(get_current_animation());
	const int32_t flag_index = get_flag_index();
	const Color colour_list[] {
		get_colour_from_index(0), //
		get_colour_from_index(1), //
		get_colour_from_index(2) //
	};

	static const StringName scroll_speed_name = { "scroll_speed", true };
	static const StringName flag_index_name = { "flag_index", true };
	static const StringName colour_primary_name = { "colour_primary", true };
	static const StringName colour_secondary_name = { "colour_secondary", true };
	static const StringName colour_tertiary_name = { "colour_tertiary", true };

	const int32_t count = get_child_count();
	for (int32_t index = 0; index < count; index++) {
		MeshInstance3D* mesh_instance = Object::cast_to<MeshInstance3D>(get_child(index));
		if (mesh_instance == nullptr) {
			continue;
		}
		// TODO: consolidate same ShaderMaterials for parents and simulation-related units to a single ShaderMaterial
		// TODO: don't hardcode shader parameter names
		mesh_instance->set_instance_shader_parameter(scroll_speed_name, texture_animation_scroll);
		mesh_instance->set_instance_shader_parameter(flag_index_name, flag_index);
		mesh_instance->set_instance_shader_parameter(colour_primary_name, colour_list[0]);
		mesh_instance->set_instance_shader_parameter(colour_secondary_name, colour_list[1]);
		mesh_instance->set_instance_shader_parameter(colour_tertiary_name, colour_list[2]);
	}
	is_dirty = false;
}

void MapInstance3D::_notification(int what) {
	if (what != NOTIFICATION_PROCESS) {
		return;
	}
	if (parent_instance != nullptr) {
		return;
	}
	update_shader_data();
}

void MapInstance3D::_bind_methods() {
	OV_BIND_METHOD(MapInstance3D::get_skeleton);
	OV_BIND_METHOD(MapInstance3D::get_animation_player);

	OV_BIND_METHOD(MapInstance3D::add_animation, { "name", "animation" });
	OV_BIND_METHOD(MapInstance3D::remove_animation, { "name" });
	OV_BIND_METHOD(MapInstance3D::get_animation, { "name" });

	OV_BIND_METHOD(MapInstance3D::set_current_animation, { "animation" });
	OV_BIND_METHOD(MapInstance3D::get_current_animation);
	ADD_PROPERTY(PropertyInfo(Variant::STRING_NAME, "current_animation"), "set_current_animation", "get_current_animation");

	OV_BIND_METHOD(MapInstance3D::attach_model, { "bone_name", "instance" });

	OV_BIND_METHOD(MapInstance3D::set_colour_index_override, { "index", "color" });
	OV_BIND_METHOD(MapInstance3D::get_colour_from_index, { "index" });

	OV_BIND_METHOD(MapInstance3D::set_texture_animation_scroll_override, { "animation", "scroll" });
	OV_BIND_METHOD(MapInstance3D::get_texture_animation_scroll, { "animation" });

	OV_BIND_METHOD(MapInstance3D::set_flag_index_override, { "index" });
	OV_BIND_METHOD(MapInstance3D::reset_flag_index);
	OV_BIND_METHOD(MapInstance3D::get_flag_index);

	OV_BIND_METHOD(MapInstance3D::update_shader_data);
}
