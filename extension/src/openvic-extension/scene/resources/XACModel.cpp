#include "XACModel.hpp"

#include <cstdint>
#include <ranges>
#include <span>

#include <godot_cpp/classes/array_mesh.hpp>
#include <godot_cpp/classes/display_server.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/gradient.hpp>
#include <godot_cpp/classes/gradient_texture2d.hpp>
#include <godot_cpp/classes/image.hpp>
#include <godot_cpp/classes/image_texture.hpp>
#include <godot_cpp/classes/material.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/rendering_server.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/classes/resource_preloader.hpp>
#include <godot_cpp/classes/script.hpp>
#include <godot_cpp/classes/shader.hpp>
#include <godot_cpp/classes/shader_material.hpp>
#include <godot_cpp/classes/skeleton3d.hpp>
#include <godot_cpp/classes/surface_tool.hpp>
#include <godot_cpp/core/defs.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <godot_cpp/variant/array.hpp>
#include <godot_cpp/variant/basis.hpp>
#include <godot_cpp/variant/dictionary.hpp>
#include <godot_cpp/variant/node_path.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_float32_array.hpp>
#include <godot_cpp/variant/packed_int32_array.hpp>
#include <godot_cpp/variant/packed_int64_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/packed_vector2_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_vector4_array.hpp>
#include <godot_cpp/variant/plane.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/typed_array.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <openvic-simulation/utility/Containers.hpp>
#include <openvic-simulation/utility/Logger.hpp>

#include "openvic-extension/core/Bind.hpp"
#include "openvic-extension/core/Convert.hpp"
#include "openvic-extension/core/io/EfxPackedStructs.hpp"
#include "openvic-extension/scene/resources/UnitModel.hpp"
#include "openvic-extension/scene/resources/XACMesh.hpp"
#include "openvic-extension/scene/resources/XACSkeleton.hpp"
#include "openvic-extension/singletons/AssetManager.hpp"
#include "openvic-extension/singletons/GameSingleton.hpp"
// #include "openvic-extension/singletons/ModelSingleton.hpp"

using namespace OpenVic;
using namespace godot;

/*enum struct MAP_TYPE {
	DIFFUSE = 2,
	SPECULAR,
	SHADOW,
	NORMAL
};*/
/*struct MAP_TYPE {
	enum Values : int32_t {
		DIFFUSE = 2,
		SPECULAR,
		SHADOW,
		NORMAL
	};
};*/

static constexpr uint32_t FORMAT_SPECIFIER = ' CAX'; // Order reversed due to little endian
static constexpr uint8_t VERSION_MAJOR = 1;
static constexpr uint8_t VERSION_MINOR = 0;

// TODO: How do we get this enum to work both here and in modelSingleton?
// public:
enum class MAP_TYPE { DIFFUSE = 2, SPECULAR, SHADOW, NORMAL };
/*struct MAP_TYPE {
	enum Values : int32_t {
		DIFFUSE = 2,
		SPECULAR,
		SHADOW,
		NORMAL
	};
};
private:*/

#pragma pack(push, 1)
struct xac_header_t {
	uint32_t format_identifier;
	uint8_t version_major;
	uint8_t version_minor;
	uint8_t big_endian;
	uint8_t multiply_order;
};

struct xac_metadata_v2_pack_t {
	uint32_t reposition_mask; // 1=position, 2=rotation, 4=scale
	int32_t repositioning_node;
	uint8_t exporter_major_version;
	uint8_t exporter_minor_version;
	uint16_t pad;
	float retarget_root_offset;
};

struct node_hierarchy_pack_t { // v1
	int32_t node_count;
	int32_t root_node_count; // nodes with parent_id == -1
};

struct node_data_pack_t { // v1
	efx::quat_v1_t rotation;
	efx::quat_v1_t scale_rotation;
	efx::vec3d_inpos_t position;
	efx::vec3d_t scale;
	float unused[3];
	int32_t unknown[2];
	int32_t parent_node_id;
	int32_t child_nodes_count;
	int32_t include_in_bounds_calculation; // bool
	efx::matrix44_t transform;
	float importance_factor;
};

struct material_totals_t { // v1
	int32_t total_materials_count;
	int32_t standard_materials_count;
	int32_t fx_materials_count;
};

struct material_definition_pack_t {
	efx::vec4d_t ambient_color;
	efx::vec4d_t diffuse_color;
	efx::vec4d_t specular_color;
	efx::vec4d_t emissive_color;
	float shine;
	float shine_strength;
	float opacity;
	float ior; // index of refraction
	uint8_t double_sided; // bool
	uint8_t wireframe; // bool
	uint8_t unused; // in v1, unknown, but used
	uint8_t layers_count; // in v1, unknown, but used
};

struct material_layer_pack_t { // also chunk 0x4, v2
	float amount;
	efx::vec2d_t uv_offset;
	efx::vec2d_t uv_tiling;
	float rotation_in_radians;
	int16_t material_id;
	uint8_t map_type; // (1-5) enum MAP_TYPE
	uint8_t unused;
};

struct mesh_pack_t {
	int32_t node_id;
	int32_t influence_ranges_count;
	int32_t vertices_count;
	int32_t indices_count;
	int32_t submeshes_count;
	int32_t attribute_layers_count;
	uint8_t is_collision_mesh; // bool
	uint8_t pad[3];
};

enum struct Attribute : int32_t { POSITION, NORMAL, TANGENT, UV, COL_32, INFLUENCE_RANGE, COL_128 };

struct vertices_attribute_pack_t {
	Attribute type; // 0-6 (enum ATTRIBUTE)
	int32_t attribute_size;
	uint8_t keep_originals; // bool
	uint8_t is_scale_factor; // bool
	uint16_t pad;
};

struct submesh_pack_t {
	int32_t indices_count;
	int32_t vertices_count;
	int32_t material_id;
	int32_t bones_count;
};

struct skinning_pack_t {
	int32_t influences_count;
	uint8_t is_for_collision; // bool
	uint8_t pad[3];
};

struct influence_data_t {
	float weight;
	int16_t bone_id;
	int16_t pad;
};

struct influence_range_t {
	int32_t first_influence_index;
	int32_t influences_count;
};

// 0x4, 0x6, 0xA chunk types appear in vic2, but what they do
// is unknown
// 0x8 is junk data
// 0x0 is the older node/bone chunk

// 0x0, v3
struct node_chunk_pack_t {
	efx::quat_v1_t rotation;
	efx::quat_v1_t scale_rotation;
	efx::vec3d_inpos_t position;
	efx::vec3d_t scale;
	efx::vec3d_t unused; //-1, -1, -1
	int32_t unknown; //-1 (0xFFFF)
	int32_t parent_node_id;
	float uncertain[17]; // likely a matrix44 + fimportancefactor
};

/*
0x6, v? unknown
	12x int32?
	09x float?
*/
#pragma pack(pop)

struct xac_metadata_v2_t { // NOLINT(cppcoreguidelines-pro-type-member-init)
	xac_metadata_v2_pack_t packed; // NOLINT(cppcoreguidelines-pro-type-member-init)
	godot::String source_app;
	godot::String original_file_name;
	godot::String export_date;
	godot::String actor_name;
};

struct node_data_t { // NOLINT(cppcoreguidelines-pro-type-member-init) // v1
	node_data_pack_t packed; // NOLINT(cppcoreguidelines-pro-type-member-init)
	godot::String name;
};

struct node_hierarchy_t { // NOLINT(cppcoreguidelines-pro-type-member-init) // v1
	node_hierarchy_pack_t packed; // NOLINT(cppcoreguidelines-pro-type-member-init)
	memory::vector<node_data_t> node_data;
};

struct material_layer_t { // NOLINT(cppcoreguidelines-pro-type-member-init)
	material_layer_pack_t packed; // NOLINT(cppcoreguidelines-pro-type-member-init)
	godot::String texture;
};

struct material_definition_t { // NOLINT(cppcoreguidelines-pro-type-member-init)
	material_definition_pack_t packed; // NOLINT(cppcoreguidelines-pro-type-member-init)
	godot::String name;
	memory::vector<material_layer_t> layers;
};

struct vertices_attribute_t { // NOLINT(cppcoreguidelines-pro-type-member-init)
	vertices_attribute_pack_t packed; // NOLINT(cppcoreguidelines-pro-type-member-init)
	memory::vector<uint8_t> data;
};

struct submesh_t { // NOLINT(cppcoreguidelines-pro-type-member-init)
	submesh_pack_t packed; // NOLINT(cppcoreguidelines-pro-type-member-init)
	memory::vector<int32_t> relative_indices;
	memory::vector<int32_t> bone_ids;
};

struct mesh_t { // NOLINT(cppcoreguidelines-pro-type-member-init)
	mesh_pack_t packed; // NOLINT(cppcoreguidelines-pro-type-member-init)
	memory::vector<vertices_attribute_t> vertices_attributes;
	memory::vector<submesh_t> submeshes;
};

struct skinning_t { // NOLINT(cppcoreguidelines-pro-type-member-init)
	int32_t node_id = 0;
	int32_t local_bones_count = -1; // v3 only
	skinning_pack_t packed; // NOLINT(cppcoreguidelines-pro-type-member-init)
	memory::vector<influence_data_t> influence_data;
	memory::vector<influence_range_t> influence_ranges;
};

struct node_chunk_t { // NOLINT(cppcoreguidelines-pro-type-member-init)
	node_chunk_pack_t packed; // NOLINT(cppcoreguidelines-pro-type-member-init)
	godot::String name;
};

// Xac -> godot conversion functions
struct material_mapping {
	//-1 means unused
	godot::Ref<godot::Material> godot_material;
	int32_t diffuse_texture_index = -1;
	int32_t specular_texture_index = -1;
	// int32_t shadow_texture_index = -1;
	int32_t scroll_index = -1;
};

struct model_texture_set {
	godot::String diffuse_name;
	godot::String specular_name;
	godot::String normal_name;
	// godot::String shadow_name;
};

void XACModel::_bind_methods() {
	OpenVic::detail::bind_method<"set_mesh_list">(&XACModel::_set_mesh_list, { "mesh_list" });

	OV_BIND_METHOD(XACModel::push_mesh, { "mesh" });
	OV_BIND_METHOD(XACModel::set_mesh, { "index", "mesh" });
	OV_BIND_METHOD(XACModel::get_mesh, { "index" });
	OV_BIND_METHOD(XACModel::get_mesh_count);
	OV_BIND_METHOD(XACModel::generate_node);
}

void XACModel::set_mesh_list(Vector<Ref<XACMesh>> const& mesh_list) {
	this->mesh_list = mesh_list;
}

void XACModel::_set_mesh_list(TypedArray<Ref<XACMesh>> const& mesh_list) {
	this->mesh_list = {};
	this->mesh_list.resize(mesh_list.size());
	for (size_t i = 0; i < mesh_list.size(); i++) {
		Ref<XACMesh> const& mesh = mesh_list[i];
		ERR_CONTINUE(mesh.is_null());
		this->mesh_list.ptrw()[i] = mesh;
	}
}

uint32_t XACModel::push_mesh(Ref<XACMesh> const& mesh) {
	uint32_t index = mesh_list.size();
	mesh_list.push_back(mesh);
	return index;
}

void XACModel::set_mesh(uint32_t index, Ref<XACMesh> const& mesh) {
	ERR_FAIL_INDEX(index, mesh_list.size());
	mesh_list.set(index, mesh);
}

Ref<XACMesh> XACModel::get_mesh(uint32_t index) const {
	ERR_FAIL_INDEX_V(index, mesh_list.size(), Ref<XACMesh>());
	return mesh_list[index];
}

uint32_t XACModel::get_mesh_count() const {
	return mesh_list.size();
}

Node3D* XACModel::generate_node() const {
	Node3D* node = memnew(Node3D);
	node->set_name(get_name());

	HashMap<Ref<XACSkeleton>, Skeleton3D*> skeleton_map;
	for (Ref<XACMesh> const& mesh : mesh_list) {
		MeshInstance3D* mesh_instance = mesh->generate_mesh_instance();
		node->add_child(mesh_instance);

		Ref<XACSkeleton> skeleton = mesh->get_skeleton();
		if (skeleton.is_null()) {
			continue;
		} else if (Skeleton3D** node = skeleton_map.getptr(skeleton); node != nullptr) {
			mesh_instance->set_skeleton_path(mesh_instance->get_path_to(*node));
			continue;
		}

		Skeleton3D* skeleton_node = skeleton->generate_skeleton();
		node->add_child(skeleton_node);

		NodePath path = mesh_instance->get_path_to(skeleton_node);
		skeleton_map.insert(skeleton, skeleton_node);
		mesh_instance->set_skeleton_path(path);
	}
	return node;
}

XACModel::~XACModel() {}

static Ref<ImageTexture> _get_model_texture(godot::String name) {
	AssetManager* asset_manager = AssetManager::get_singleton();
	return asset_manager->get_texture(vformat("gfx/anims/%s.dds", name));
}

static StringName _skeleton_name() {
	static const StringName result = { "skeleton", true };
	return result;
}

static Error _read_xac_header(Ref<FileAccess> const& file) {
	Error err = OK;
	xac_header_t header = efx::read_struct<xac_header_t>(file, &err);
	ERR_FAIL_COND_V(err != OK, err);

	ERR_FAIL_COND_V_MSG(
		header.format_identifier != FORMAT_SPECIFIER, ERR_INVALID_DATA,
		vformat("Invalid XAC format identifier: %x (should be %x)", header.format_identifier, FORMAT_SPECIFIER)
	);

	ERR_FAIL_COND_V_MSG(
		header.version_major != VERSION_MAJOR || header.version_minor != VERSION_MINOR, ERR_INVALID_DATA,
		vformat(
			"Invalid XAC version: %d.%d (should be %d.%d)", header.version_major, header.version_minor, VERSION_MAJOR,
			VERSION_MINOR
		)
	);

	ERR_FAIL_COND_V_MSG(
		header.big_endian != 0, ERR_INVALID_DATA, "Invalid XAC endianness: big endian (only little endian is supported)"
	);

	return err;
}

static xac_metadata_v2_t _read_xac_metadata(Ref<FileAccess> const& file, Error& error_return) {
	xac_metadata_v2_t metadata;

	efx::read_struct(file, metadata.packed, &error_return);
	if (error_return != OK) {
		return metadata;
	}

	efx::read_string(file, metadata.source_app, &error_return, false);
	if (error_return != OK) {
		return metadata;
	}

	efx::read_string(file, metadata.original_file_name, &error_return, false);
	if (error_return != OK) {
		return metadata;
	}

	efx::read_string(file, metadata.export_date, &error_return, false);
	if (error_return != OK) {
		return metadata;
	}

	efx::read_string(file, metadata.actor_name, &error_return);

	return metadata;
}

static node_data_t _read_node_data(Ref<FileAccess> const& file, Error& error_return) {
	node_data_t node_data;

	efx::read_struct(file, node_data.packed, &error_return);
	if (error_return != OK) {
		return node_data;
	}

	efx::read_string(file, node_data.name, &error_return);

	return node_data;
}

static node_hierarchy_t _read_node_hierarchy(Ref<FileAccess> const& file, Error& error_return) {
	node_hierarchy_t hierarchy;

	efx::read_struct(file, hierarchy.packed, &error_return);
	if (error_return != OK) {
		return hierarchy;
	}

	hierarchy.node_data.reserve(hierarchy.packed.node_count);
	for (int32_t i = 0; i < hierarchy.packed.node_count; i++) {
		node_data_t node = _read_node_data(file, error_return);
		ERR_CONTINUE(error_return != OK);
		hierarchy.node_data.push_back(std::move(node));
	}
	return hierarchy;
}

static material_totals_t _read_material_totals(Ref<FileAccess> const& file, Error& error_return) {
	material_totals_t totals = efx::read_struct<material_totals_t>(file, &error_return);
	return totals;
}

static material_layer_t _read_layer(Ref<FileAccess> const& file, Error& error_return) {
	material_layer_t layer;

	efx::read_struct(file, layer.packed, &error_return);
	if (error_return != OK) {
		return layer;
	}

	efx::read_string(file, layer.texture, &error_return, false);

	return layer;
}

static material_definition_t _read_material_definition(Ref<FileAccess> const& file, efx::Version version, Error& error_return) {
	material_definition_t def;

	efx::read_struct(file, def.packed, &error_return);
	if (error_return != OK) {
		return def;
	}

	efx::read_string(file, def.name, &error_return, false);
	if (error_return != OK) {
		return def;
	}

	if (version != efx::Version::ONE) { // in version 1, the layers are defined in separate chunks of type 0x4
		def.layers.reserve(def.packed.layers_count);
		for (size_t i = 0; i < def.packed.layers_count; i++) {
			material_layer_t layer = _read_layer(file, error_return);
			ERR_CONTINUE(error_return != OK);
			def.layers.push_back(std::move(layer));
		}
	}
	return def;
}

static vertices_attribute_t _read_vertices_attribute(Ref<FileAccess> const& file, int32_t vertices_count, Error& error_return) {
	vertices_attribute_t attribute;

	efx::read_struct(file, attribute.packed, &error_return);
	if (error_return != OK) {
		return attribute;
	}

	efx::read_struct_array(file, attribute.data, vertices_count * attribute.packed.attribute_size, &error_return);

	return attribute;
}

static submesh_t _read_submesh(Ref<FileAccess> const& file, Error& error_return) {
	submesh_t submesh;

	efx::read_struct(file, submesh.packed, &error_return);
	if (error_return != OK) {
		return submesh;
	}

	efx::read_struct_array(file, submesh.relative_indices, submesh.packed.indices_count, &error_return);
	if (error_return != OK) {
		return submesh;
	}

	efx::read_struct_array(file, submesh.bone_ids, submesh.packed.bones_count, &error_return);

	return submesh;
}

static mesh_t _read_mesh(Ref<FileAccess> const& file, Error& error_return) {
	mesh_t mesh;

	efx::read_struct(file, mesh.packed, &error_return);
	if (error_return != OK) {
		return mesh;
	}

	mesh.vertices_attributes.reserve(mesh.packed.attribute_layers_count);
	for (size_t i = 0; i < mesh.packed.attribute_layers_count; i++) {
		vertices_attribute_t attribute = _read_vertices_attribute(file, mesh.packed.vertices_count, error_return);
		ERR_CONTINUE(error_return != OK);
		mesh.vertices_attributes.push_back(std::move(attribute));
	}

	mesh.submeshes.reserve(mesh.packed.submeshes_count);
	for (size_t i = 0; i < mesh.packed.submeshes_count; i++) {
		submesh_t submesh = _read_submesh(file, error_return);
		ERR_CONTINUE(error_return != OK);
		mesh.submeshes.push_back(std::move(submesh));
	}

	return mesh;
}

static skinning_t _read_skinning( //
	Ref<FileAccess> const& file, std::span<const mesh_t> meshes, efx::Version version, Error& error_return
) {
	skinning_t skin;

	efx::read_struct(file, skin.node_id, &error_return);
	if (error_return != OK) {
		return skin;
	}

	if (version == efx::Version::THREE) {
		efx::read_struct(file, skin.local_bones_count, &error_return);
		if (error_return != OK) {
			return skin;
		}
	}

	efx::read_struct(file, skin.packed, &error_return);
	if (error_return != OK) {
		return skin;
	}

	efx::read_struct_array(file, skin.influence_data, skin.packed.influences_count, &error_return);
	if (error_return != OK) {
		return skin;
	}

	bool found = false;
	for (mesh_t const& mesh : meshes) {
		if (mesh.packed.is_collision_mesh == skin.packed.is_for_collision && mesh.packed.node_id == skin.node_id) {
			efx::read_struct_array(file, skin.influence_ranges, mesh.packed.influence_ranges_count, &error_return);
			ERR_FAIL_COND_V(error_return != OK, skin);
			found = true;
			break;
		}
	}

	if (!found) {
		error_return = ERR_INVALID_DATA;
	}
	return skin;
}

static node_chunk_t _read_node_chunk(Ref<FileAccess> const& file, Error& error_return) {
	node_chunk_t node;

	efx::read_struct(file, node.packed, &error_return);
	if (error_return != OK) {
		return node;
	}

	efx::read_string(file, node.name, &error_return);

	return node;
}

static Transform3D _make_transform( //
	efx::vec3d_inpos_t const& position, efx::quat_v1_t const& quaternion, efx::vec3d_t const& scale
) {
	Basis basis = Basis();
	basis.set_quaternion(convert_to<Quaternion>(quaternion));
	basis.scale(convert_to<Vector3>(scale));

	return Transform3D(basis, convert_to<Vector3>(position));
}

// TODO: do we want to use node's bone id instead of current_id?
static XACSkeleton* _build_armature_hierarchy(node_hierarchy_t const& hierarchy) {
	XACSkeleton* skeleton = memnew(XACSkeleton);
	skeleton->set_name(_skeleton_name());

	for (uint32_t current_id = 0; node_data_t const& node : hierarchy.node_data) {
		skeleton->add_bone(node.name);
		skeleton->set_bone_parent(current_id, node.packed.parent_node_id);

		Transform3D transform = _make_transform(node.packed.position, node.packed.rotation, node.packed.scale);
		skeleton->set_bone_pose(current_id, transform);

		current_id += 1;
	}

	return skeleton;
}

static XACSkeleton* _build_armature_nodes(std::span<const node_chunk_t> nodes) {
	XACSkeleton* skeleton = memnew(XACSkeleton);
	skeleton->set_name(_skeleton_name());

	for (uint32_t current_id = 0; node_chunk_t const& node : nodes) {
		skeleton->add_bone(node.name);
		skeleton->set_bone_parent(current_id, node.packed.parent_node_id);

		Transform3D transform = _make_transform(node.packed.position, node.packed.rotation, node.packed.scale);
		skeleton->set_bone_pose(current_id, transform);

		current_id += 1;
	}

	return skeleton;
}

static model_texture_set _get_model_textures(material_definition_t const& material) {
	model_texture_set texture_set;

	for (material_layer_t const& layer : material.layers) {
		if (layer.texture == "test256texture" || layer.texture == "unionjacksquare") { //|| layer.texture == "nospec"
			continue;
		}
		// Get the texture names
		switch (static_cast<MAP_TYPE>(layer.packed.map_type)) {
		case MAP_TYPE::DIFFUSE:	 texture_set.diffuse_name = layer.texture; break;
		case MAP_TYPE::SPECULAR: texture_set.specular_name = layer.texture; break;
		case MAP_TYPE::NORMAL:	 texture_set.normal_name = layer.texture; break;
		case MAP_TYPE::SHADOW:	 break;
		default:
			UtilityFunctions::print(vformat("Unknown layer type: %x, texture: %s", layer.packed.map_type, layer.texture));
			break;
		}
	}
	if (texture_set.specular_name.is_empty()) {
		texture_set.specular_name = "nospec";
	}
	return texture_set;
}

inline static StringName _texture_flag_sheet_diffuse_name() {
	static const StringName name = { "texture_flag_sheet_diffuse", true };
	return name;
}

// unit colours not working, likely due to improper setup of the unit script
static memory::vector<material_mapping> _build_materials( //
	Ref<Shader> const& flag_shader, Ref<Shader> const& scrolling_shader, Ref<Shader> const& unit_colour_shader, std::span<const material_definition_t> materials
) {
	// General
	memory::vector<material_mapping> mappings;

	for (material_definition_t const& mat : materials) {
		model_texture_set texture_names = _get_model_textures(mat);
		material_mapping mapping;

		// TODO: consolidate same ShaderMaterials for parents and simulation-related units to a single ShaderMaterial

		// *** Determine the correct material to use, and set it up ***
		// Note all 3 of these materials correspond to different compile targets of avatar.fx
		//  it has techniques for TexAnim (tracks), Smoke, Shadow, Flag, Standard (we combine TexAnim+smoke, shadow+standard),
		// in the original shader, shadow = just sample the color, smoke and flag share speed properties

		// TODO: perhaps flag should be determined by hard-coding so that other models can have a normal texture?
		// There shouldn't be a specular texture
		if (!texture_names.normal_name.is_empty() && texture_names.diffuse_name.is_empty()) {
			Ref<ShaderMaterial> flag_shader_material;
			flag_shader_material.instantiate();
			flag_shader_material->set_shader(flag_shader);

			static const StringName Param_texture_normal = { "texture_normal", true };
			flag_shader_material->set_shader_parameter(Param_texture_normal, _get_model_texture(texture_names.normal_name));

			UtilityFunctions::print("mat name: ", mat.name, ", texture normal name: ", texture_names.normal_name);

			// static const StringName Param_flag_dimensions = { "flag_dims", true };
			// flag_shader_material->set_shader_parameter(Param_flag_dimensions, game_singleton->get_flag_dims());

			GameSingleton const* game_singleton = GameSingleton::get_singleton();
			flag_shader_material->set_shader_parameter(
				_texture_flag_sheet_diffuse_name(), game_singleton->get_flag_sheet_texture()
			);

			// if (static bool flag_sheet_set = false; unlikely(!flag_sheet_set)) {
			// 	RenderingServer::get_singleton()->global_shader_parameter_set(
			// 		_texture_flag_sheet_diffuse_name(), game_singleton->get_flag_sheet_texture()
			// 	);
			// 	flag_sheet_set = true;
			// }

			mapping.godot_material = flag_shader_material;
		}
		// Scrolling texture
		else if (!texture_names.diffuse_name.is_empty() && (mat.name == "TexAnim" || mat.name == "Smoke")) {
			Ref<ShaderMaterial> scrolling_shader_material;
			scrolling_shader_material.instantiate();
			scrolling_shader_material->set_shader(scrolling_shader);

			static const StringName Param_Scroll_texture_diffuse = { "scroll_texture_diffuse", true };
			scrolling_shader_material->set_shader_parameter(
				Param_Scroll_texture_diffuse, _get_model_texture(texture_names.diffuse_name)
			);

			float scroll_factor = 0.0;
			if (texture_names.diffuse_name == "TexAnim") {
				scroll_factor = 2.5;
			} else if (texture_names.diffuse_name == "Smoke") {
				scroll_factor = 0.3;
			}

			static const StringName Param_Scroll_factor = { "scroll_factor", true };
			scrolling_shader_material->set_shader_parameter(Param_Scroll_factor, scroll_factor);

			mapping.godot_material = scrolling_shader_material;
		}
		// standard material (diffuse optionally with a specular/unit colours)
		else if (!texture_names.diffuse_name.is_empty()) {
			Ref<ShaderMaterial> unit_colour_shader_material;
			unit_colour_shader_material.instantiate();
			unit_colour_shader_material->set_shader(unit_colour_shader);

			static const StringName Param_texture_diffuse = { "texture_diffuse", true };
			unit_colour_shader_material->set_shader_parameter(
				Param_texture_diffuse, _get_model_texture(texture_names.diffuse_name)
			);

			// red channel is specular, green and blue are nation colours
			static const StringName Param_texture_nation_colors_mask = { "texture_nation_colors_mask", true };
			unit_colour_shader_material->set_shader_parameter(
				Param_texture_nation_colors_mask, _get_model_texture(texture_names.specular_name)
			);

			// static const StringName Param_texture_shadow = "texture_shadow";

			mapping.godot_material = unit_colour_shader_material;
		}

		else {
			WARN_PRINT_ED(vformat("Material %s did not have a diffuse texture! Skipping", mat.name));
		}

		mappings.push_back(mapping);
	}


	return mappings;
}

template<typename T>
struct byte_array_wrapper : std::span<const T> {
	using std::span<const T>::span;
	template<class R>
	byte_array_wrapper(R&& source)
		: std::span<const T>(
			  reinterpret_cast<T const* const>(std::ranges::data(source)), std::ranges::size(source) / sizeof(T)
		  ) {}
};

static memory::vector<Ref<XACMesh>> _build_mesh( //
	mesh_t const& mesh_chunk, skinning_t* skin, std::span<const material_mapping> materials
) {
	static constexpr uint32_t EXTRA_CULL_MARGIN = 2;

	memory::vector<Ref<XACMesh>> meshes; // return value

	byte_array_wrapper<efx::vec3d_inpos_t> verts;
	byte_array_wrapper<efx::vec3d_inpos_t> normals;
	byte_array_wrapper<efx::vec4d_t> tangents;
	uint32_t uvs_read = 0;
	byte_array_wrapper<efx::vec2d_t> uv1;
	byte_array_wrapper<efx::vec2d_t> uv2;
	byte_array_wrapper<uint32_t> influence_range_indices;

	for (vertices_attribute_t const& attribute : mesh_chunk.vertices_attributes) {
		switch (attribute.packed.type) {
			using enum Attribute;
		case POSITION: verts = attribute.data; break;
		case NORMAL:   normals = attribute.data; break;
		case TANGENT:  tangents = attribute.data; break;
		case UV:
			if (uvs_read == 0) {
				uv1 = attribute.data;
			} else if (uvs_read == 1) {
				uv2 = attribute.data;
			}
			uvs_read += 1;
			break;
		case INFLUENCE_RANGE:
			if (skin == nullptr) {
				spdlog::warn_s("Mesh chunk has influence attribute but no corresponding skinning chunk");
				break;
			}
			influence_range_indices = attribute.data;
			break;
		// for now, ignore color data
		case COL_32:
		case COL_128:
		default:	  break;
		}
	}

	uint32_t vert_total = 0;
	// static const StringName key_diffuse = { "tex_index_diffuse", true };
	// static const StringName key_specular = { "tex_index_specular", true };
	// static const StringName key_scroll = { "scroll_tex_index_diffuse", true };

	Ref<SurfaceTool> st = Ref<SurfaceTool>();
	st.instantiate();

	// for now we treat a submesh as a godot mesh surface
	for (submesh_t const& submesh : mesh_chunk.submeshes) {
		// one mesh per submesh so that they can use different instance uniform parameters on the same material
		Ref<ArrayMesh> mesh;
		mesh.instantiate();

		Ref<XACMesh> mesh_inst;
		mesh_inst.instantiate();
		mesh_inst->set_extra_cull_margin(EXTRA_CULL_MARGIN);
		mesh_inst->set_mesh(mesh);
		// MeshInstance3D* mesh_inst = memnew(MeshInstance3D);
		// mesh_inst->set_extra_cull_margin(EXTRA_CULL_MARGIN);
		// mesh_inst->set_mesh(mesh);

		st->begin(Mesh::PRIMITIVE_TRIANGLES);

		for (int j = 0; j < submesh.relative_indices.size(); j++) {
			int32_t rel_index = submesh.relative_indices[j] + vert_total;
			if (!normals.empty()) {
				st->set_normal(convert_to<Vector3>(normals[rel_index]));
			}
			if (!tangents.empty()) {
				efx::vec4d_t const& v4 = tangents[rel_index];
				st->set_tangent(Plane(-v4.x, v4.y, v4.z, v4.w));
			}
			if (!uv1.empty()) {
				st->set_uv(convert_to<Vector2>(uv1[rel_index]));
			}
			if (!uv2.empty()) {
				st->set_uv2(convert_to<Vector2>(uv2[rel_index]));
			}
			if (skin != nullptr && !influence_range_indices.empty()) {
				PackedInt32Array boneIds = { 0, 0, 0, 0 };
				PackedFloat32Array weights = { 0, 0, 0, 0 };

				influence_range_t const& infl_range = skin->influence_ranges[influence_range_indices[rel_index]];
				int32_t first = infl_range.first_influence_index;
				int32_t count = Math::min(infl_range.influences_count, 4);

				for (int32_t i = 0; i < count; i++) {
					influence_data_t const& infl_data = skin->influence_data[first + i];
					boneIds[i] = infl_data.bone_id;
					weights[i] = infl_data.weight;
				}

				st->set_bones(boneIds);
				st->set_weights(weights);
			}
			st->add_vertex(convert_to<Vector3>(verts[rel_index]));
		}

		material_mapping const& material_mapping = materials[submesh.packed.material_id];
		st->set_material(material_mapping.godot_material);

		mesh = st->commit(mesh); // add a new surface to the mesh
		st->clear();

		vert_total += submesh.packed.vertices_count;

		// setup materials for the surface
		// if (material_mapping.diffuse_texture_index != -1) {
		// 	mesh_inst->set_instance_shader_parameter(key_diffuse, material_mapping.diffuse_texture_index);
		// }
		// if (material_mapping.specular_texture_index != -1) {
		// 	mesh_inst->set_instance_shader_parameter(key_specular, material_mapping.specular_texture_index);
		// }
		// if (material_mapping.scroll_index != -1) {
		// 	mesh_inst->set_instance_shader_parameter(key_scroll, material_mapping.scroll_index);
		// }

		meshes.push_back(mesh_inst);
	}

	return meshes;
}

Error XACModel::load_model_from(
	Ref<Shader> const& flag_shader, Ref<Shader> const& scrolling_shader, Ref<Shader> const& unit_colour_shader,
	Ref<FileAccess> const& file
) {
	Error error_return = _read_xac_header(file);
	ERR_FAIL_COND_V(error_return != OK, error_return);

	ResourceLoader* loader = ResourceLoader::get_singleton();

	xac_metadata_v2_t metadata;

	bool hierarchy_read = false;
	node_hierarchy_t hierarchy;
	memory::vector<node_chunk_t> nodes; // other way of making a bone hierarchy

	material_totals_t material_totals; // NOLINT(cppcoreguidelines-pro-type-member-init)
	memory::vector<material_definition_t> materials;

	memory::vector<mesh_t> meshes;
	memory::vector<skinning_t> skinnings;

	bool log_all = false;

	// 0xA unknown (ex. Spanish_Helmet1.xac), only 2 bytes long...
	// 0x6 unknown (ex. ...)
	// 0x8 junk data (ie. old versions of the file duplicated in append mode)
	//	(ex. Port_Empty.xac)
	// 0xC morphtargets (not used in vic2)

	while (!file->eof_reached()) {
		efx::chunk_header_t header = efx::read_chunk_header(file, &error_return);
		if (error_return == ERR_FILE_CANT_READ) {
			error_return = OK;
			break;
		}
		ERR_BREAK(error_return != OK);

		if (log_all) {
			UtilityFunctions::print(vformat(
				"XAC chunk: type = %x, length = %x, version = %d at %x", static_cast<int32_t>(header.type), header.length,
				static_cast<int32_t>(header.version), file->get_position()
			));
		}

		switch (header.type) {
			using enum efx::Type;
			using enum efx::Version;
		case XAC_METADATA:
			if (header.version == TWO) {
				metadata = _read_xac_metadata(file, error_return);
			}
			break;
		case XAC_NODE_HIERARCHY:
			if (header.version == ONE) {
				hierarchy = _read_node_hierarchy(file, error_return);
				hierarchy_read = error_return == OK;
			}
			break;
		case XAC_NODE_CHUNK:
			if (header.version == THREE) {
				node_chunk_t node = _read_node_chunk(file, error_return);
				if (error_return != OK) {
					goto EXIT_LOOP;
				}
				nodes.push_back(std::move(node));
			}
			break;
		case XAC_MESH:
			if (header.version == ONE) {
				mesh_t mesh = _read_mesh(file, error_return);
				if (error_return != OK) {
					goto EXIT_LOOP;
				}
				meshes.push_back(std::move(mesh));
			}
			break;
		case XAC_SKINNING: {
			skinning_t skin = _read_skinning(file, meshes, header.version, error_return);
			if (error_return != OK) {
				goto EXIT_LOOP;
			}
			skinnings.push_back(std::move(skin));
		} break;
		case XAC_MATERIAL_DEFINITION: {
			material_definition_t mat = _read_material_definition(file, header.version, error_return);
			if (error_return != OK) {
				goto EXIT_LOOP;
			}
			materials.push_back(std::move(mat));
		} break;
		case XAC_MATERIAL_LAYER:
			if (header.version == TWO) {
				material_layer_t layer = _read_layer(file, error_return);
				if (error_return != OK) {
					goto EXIT_LOOP;
				}

				if (layer.packed.material_id < materials.size()) {
					materials[layer.packed.material_id].layers.push_back(std::move(layer));
				} else {
					spdlog::error_s("No material of id ", layer.packed.material_id, " to attach layer to");
				}
			}
			break;
		case XAC_MATERIAL_TOTALS:
			if (header.version == ONE) {
				material_totals = _read_material_totals(file, error_return);
			}
			break;
			break;
		default:
			UtilityFunctions::print(vformat(
				"Unsupported XAC chunk: type = %x, length = %x, version = %d at %x", static_cast<int32_t>(header.type),
				header.length, static_cast<int32_t>(header.version), file->get_position()
			));
			log_all = true;
			[[fallthrough]];
		case XAC_JUNK_ONE:
		case XAC_JUNK_TWO:
			// Skip unsupported chunks by using seek, make sure this doesn't break anything, since chunk length can be
			// wrong
			error_return = ERR_INVALID_DATA;
			ERR_FAIL_COND_V_MSG(
				header.length + file->get_position() > file->get_length(), ERR_INVALID_DATA,
				vformat("XAC header length %d is too long", header.length)
			);

			file->seek(file->get_position() + header.length);
			error_return = file->get_error();
			if (error_return != OK) {
				goto EXIT_LOOP;
			}
			break;
		}
	}
EXIT_LOOP:
	if (error_return != OK) {
		return error_return;
	}

	set_name(metadata.original_file_name.get_file().get_basename());
	UtilityFunctions::print("==========\nmodel file name: ", metadata.original_file_name);

	// Setup skeleton
	XACSkeleton* skeleton = nullptr;
	if (!skinnings.empty()) {
		if (hierarchy_read) {
			skeleton = _build_armature_hierarchy(hierarchy);
		} else if (!nodes.empty()) {
			skeleton = _build_armature_nodes(nodes);
		}
	}

	// Setup materials
	memory::vector<material_mapping> const& material_mappings = _build_materials( //
		flag_shader, scrolling_shader, unit_colour_shader, materials
	);

	UtilityFunctions::print("==========");

	has_specular_material = false;
	for (material_mapping const& material : material_mappings) {
		if (material.specular_texture_index != -1) {
			has_specular_material = true;
			break;
		}
	}

	// don't set the unit script if we don't have one of:
	//  -specular (unit colours) texture
	//  -skeleton with animation
	// static const Ref<godot::Script> unit_script = loader->load("res://src/Game/Model/UnitModel.gd");
	// if (is_unit || has_specular) {
	// 	base->set_script(unit_script);
	// }

	// setup mesh
	for (mesh_t const& mesh : meshes) {
		if (mesh.packed.is_collision_mesh) {
			continue;
		} // we'll use our own collision meshes where needed

		skinning_t* mesh_skin = nullptr;
		for (uint32_t i = 0; i < skinnings.size(); i++) {
			skinning_t& skin = skinnings[i];
			if (skin.node_id == mesh.packed.node_id && skin.packed.is_for_collision == mesh.packed.is_collision_mesh) {
				mesh_skin = &skin;
				break;
			}
		}

		if (skeleton != nullptr && mesh_skin == nullptr) {
			continue;
		}

		memory::vector<Ref<XACMesh>> mesh_instances = _build_mesh(mesh, mesh_skin, material_mappings);
		for (Ref<XACMesh> mesh_inst : mesh_instances) {
			if (skeleton != nullptr) {
				mesh_inst->set_skeleton(skeleton);
				int32_t node_id = mesh.packed.node_id;
				if (hierarchy_read && node_id < hierarchy.node_data.size()) {
					mesh_inst->set_name(hierarchy.node_data[node_id].name);
				} else if (!nodes.empty() && node_id < nodes.size()) {
					mesh_inst->set_name(nodes[node_id].name);
				}

				push_mesh(mesh_inst);
			}
		}
	}

	return OK;
}

Variant ResourceFormatLoaderXAC::_load(
	const String& p_path, const String& p_original_path, bool p_use_sub_threads, int32_t p_cache_mode
) const {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	Error err = file->get_open_error();
	ERR_FAIL_COND_V_MSG(err != OK, Ref<XACModel>(), vformat("Could not open file %s", p_path));

	Ref<XACModel> result;
	result.instantiate();
	err = result->load_model_from(flag_shader, scrolling_shader, unit_colour_shader, file);

	ERR_FAIL_COND_V(err != OK, result);

	if (result->has_specular_material) {
		Ref<UnitModel> unit_result;
		unit_result.instantiate();
		unit_result->set_mesh_list(result->mesh_list);
		return unit_result;
	}

	return result;
}

PackedStringArray ResourceFormatLoaderXAC::_get_recognized_extensions() const {
	return { "xac" };
}

bool ResourceFormatLoaderXAC::_handles_type(const StringName& p_type) const {
	return ClassDB::is_parent_class(p_type, "XACModel");
}

String ResourceFormatLoaderXAC::_get_resource_type(const String& p_path) const {
	if (p_path.get_extension() == "xac") {
		return "XACModel";
	}
	return "";
}

void ResourceFormatLoaderXAC::_on_startup_mainloop() {
	// Ref<GradientTexture2D> tex;
	// tex.instantiate();
	// tex->set_width(3456);
	// tex->set_height(3264);

	// Ref<Gradient> gradient;
	// gradient.instantiate();
	// gradient->add_point(0, Color::named("pink"));
	// gradient->remove_point(1);
	// tex->set_gradient(gradient);

	// RenderingServer::get_singleton()->global_shader_parameter_add(
	// 	_texture_flag_sheet_diffuse_name(), RenderingServer::GLOBAL_VAR_TYPE_SAMPLER2D, tex
	// );

	// singleton->flag_shader->set_code(singleton->flag_shader->get_code());
}

ResourceFormatLoaderXAC::ResourceFormatLoaderXAC() {
	singleton = this;

	GameSingleton const* singleton = GameSingleton::get_singleton();
	Vector2i const& flag_dims = singleton->get_flag_dims();

	flag_shader.instantiate();
	flag_shader->set_code(vformat(
		R"(
shader_type spatial;

render_mode cull_disabled;

// Both vanilla flags use the same normal texture
const uvec2 flag_dims = uvec2(%d, %d);
uniform sampler2D texture_flag_sheet_diffuse : source_color;
uniform sampler2D texture_normal : hint_normal;

instance uniform uint flag_index;

const float normal_scroll_speed = 0.3;

// Scroll the Normal map, but leave the albedo alone
void fragment() {
	uvec2 flag_sheet_dims = uvec2(textureSize(texture_flag_sheet_diffuse, 0));
	uint scaled_index = flag_index * flag_dims.x;

	uvec2 flag_pos = uvec2(scaled_index % flag_sheet_dims.x, scaled_index / flag_sheet_dims.x * flag_dims.y);

	vec2 flag_uv = (vec2(flag_pos) + UV * vec2(flag_dims)) / vec2(flag_sheet_dims);

	ALBEDO = texture(texture_flag_sheet_diffuse, flag_uv).rgb;

	vec2 normal_uv = UV;
	normal_uv.x -= TIME * normal_scroll_speed;

	NORMAL_MAP = texture(texture_normal, normal_uv).rgb;
}
)",
		flag_dims.x, flag_dims.y
	));

	scrolling_shader.instantiate();
	scrolling_shader->set_code(R"(
shader_type spatial;

// depth_prepass_alpha is to ensure opaque scrolling textures
// (e.g. tank tracks) are rendered correctly
render_mode cull_disabled, depth_prepass_alpha;

// uniform sampler2D scroll_texture_diffuse[32] : source_color, filter_linear_mipmap, repeat_enable;
// uniform float scroll_factor[32];

uniform sampler2D scroll_texture_diffuse : source_color, filter_linear_mipmap, repeat_enable;
uniform float scroll_factor;

instance uniform float scroll_speed;

void fragment() {
	vec2 uv_scrolled = UV;
	uv_scrolled.y += TIME * scroll_speed * scroll_factor;

	ALBEDO = texture(scroll_texture_diffuse, uv_scrolled).rgb;
	ALPHA = texture(scroll_texture_diffuse, UV).a;
}
)");

	unit_colour_shader.instantiate();
	unit_colour_shader->set_code(R"(
shader_type spatial;

render_mode cull_disabled, depth_prepass_alpha;

//hold all the textures for the units that need this shader to mix in their
//nation colours (mostly generic infantry units)
//TODO: decrease back to 32
// uniform sampler2D texture_diffuse[64] : source_color, filter_linear_mipmap, repeat_enable;
// uniform sampler2D texture_nation_colors_mask[64] : source_color, filter_linear_mipmap, repeat_enable;
//uniform sampler2D texture_shadow[32] : source_color, filter_linear_mipmap, repeat_enable;

uniform sampler2D texture_diffuse : source_color, filter_linear_mipmap, repeat_enable;
uniform sampler2D texture_nation_colors_mask : source_color, filter_linear_mipmap, repeat_enable;

instance uniform vec3 colour_primary : source_color;
instance uniform vec3 colour_secondary : source_color;
instance uniform vec3 colour_tertiary : source_color;

//used to access the right textures since different units (with different textures)
//will use this same shader
//instance uniform int tex_index_shadow = -1; //0 = none

void fragment() {
	vec2 base_uv = UV;
	vec4 diffuse_tex = texture(texture_diffuse, base_uv);
	vec4 nation_colours_tex = texture(texture_nation_colors_mask, base_uv);

	//set colours to either be white (1,1,1) or the nation colour based on the mask
	vec3 primary_col = mix(vec3(1.0, 1.0, 1.0), colour_primary, nation_colours_tex.g);
	vec3 secondary_col = mix(vec3(1.0, 1.0, 1.0), colour_secondary, nation_colours_tex.b);
	vec3 tertiary_col = mix(vec3(1.0, 1.0, 1.0), colour_tertiary, nation_colours_tex.r);

	ALBEDO = diffuse_tex.rgb * primary_col * secondary_col * tertiary_col;
	ALPHA = diffuse_tex.a;
	/*if(tex_index_shadow != -1){
		vec4 shadow_tex = texture(texture_shadow[tex_index_shadow], base_uv);
		ALBEDO = shadow_tex.rgb;
	}*/
}
)");
}

ResourceFormatLoaderXAC::~ResourceFormatLoaderXAC() {
	singleton = nullptr;

	flag_shader.unref();
	scrolling_shader.unref();
	unit_colour_shader.unref();
}
