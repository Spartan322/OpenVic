#include "UnitModel.hpp"

#include <cstdint>

#include <godot_cpp/classes/animation_library.hpp>
#include <godot_cpp/classes/animation_player.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/resource_format_loader.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/core/memory.hpp>
#include <godot_cpp/templates/local_vector.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/variant.hpp>

#include "openvic-extension/scene/3d/MapInstance3D.hpp"
#include "openvic-extension/scene/resources/XACModel.hpp"

using namespace OpenVic;
using namespace godot;

UnitModel::~UnitModel() {}

Node3D* UnitModel::generate_node() const {
	MapInstance3D* map_instance = memnew(MapInstance3D);

	Node3D* node = XACModel::generate_node();
	map_instance->set_name(node->get_name());

	LocalVector<Node*> children;
	const int32_t count = node->get_child_count();
	children.reserve(count);

	for (int32_t index = 0; index < count; index++) {
		children.push_back(node->get_child(index));
	}

	for (Node* child : children) {
		node->remove_child(child);
		map_instance->add_child(child);
	}
	node->queue_free();

	return map_instance;
}

void UnitModel::_bind_methods() {}

ResourceFormatLoaderUnitModel::ResourceFormatLoaderUnitModel() {}

ResourceFormatLoaderUnitModel::~ResourceFormatLoaderUnitModel() {}

Variant ResourceFormatLoaderUnitModel::_load(
	const String& p_path, const String& p_original_path, bool p_use_sub_threads, int32_t p_cache_mode
) const {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	Error err = file->get_open_error();
	ERR_FAIL_COND_V_MSG(err != OK, Ref<UnitModel>(), vformat("Could not open file %s", p_path));

	Ref<UnitModel> result;
	result.instantiate();

	ResourceFormatLoaderXAC* xac_loader = ResourceFormatLoaderXAC::singleton;
	ERR_FAIL_NULL_V(xac_loader, Ref<UnitModel>());

	result->load_model_from(xac_loader->flag_shader, xac_loader->scrolling_shader, xac_loader->unit_colour_shader, file);
	ERR_FAIL_COND_V(err != OK, result);
	return result;
}

PackedStringArray ResourceFormatLoaderUnitModel::_get_recognized_extensions() const {
	return { "xac" };
}

bool ResourceFormatLoaderUnitModel::_handles_type(const StringName& p_type) const {
	return ClassDB::is_parent_class(p_type, "UnitModel");
}

String ResourceFormatLoaderUnitModel::_get_resource_type(const String& p_path) const {
	if (p_path.get_extension() == "xac") {
		return "UnitModel";
	}
	return "";
}
