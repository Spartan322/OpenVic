#include "ResourceFormatLoaderXSM.hpp"

#include <cstdint>
#include <utility>

#include <godot_cpp/classes/animation.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/global_constants.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/core/class_db.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_vector4_array.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/string_name.hpp>
#include <godot_cpp/variant/utility_functions.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <openvic-simulation/utility/Containers.hpp>

#include "openvic-extension/core/Convert.hpp"
#include "openvic-extension/core/io/EfxPackedStructs.hpp"

using namespace OpenVic;
using namespace godot;

static constexpr uint32_t FORMAT_SPECIFIER = ' MSX'; /* Order reversed due to little endian */
static constexpr uint8_t VERSION_MAJOR = 1, XSM_VERSION_MINOR = 0;

#pragma pack(push, 1)
struct xsm_header_t {
	uint32_t format_identifier;
	uint8_t version_major;
	uint8_t version_minor;
	uint8_t big_endian;
	uint8_t pad;
};

// v2 of the header adds these 2 properties at the start
struct xsm_metadata_v2_pack_extra_t {
	float unused;
	float max_acceptable_error;
};

struct xsm_metadata_pack_t {
	int32_t fps;
	uint8_t exporter_version_major; // these 3 properties are uncertain
	uint8_t exporter_version_minor;
	uint16_t pad;
};

struct position_key_t {
	efx::vec3d_inpos_t position;
	float time;
};

struct rotation_key_v2_t {
	efx::quat_v2_t rotation;
	float time;
};

struct rotation_key_v1_t {
	efx::quat_v1_t rotation;
	float time;
};

struct scale_key_t {
	efx::vec3d_t scale;
	float time;
};

// the quaternions come at the start of the skeletal submotion
struct submotion_rot_v2_pack_t {
	efx::quat_v2_t pose_rotation;
	efx::quat_v2_t bind_pose_rotation;
	efx::quat_v2_t pose_scale_rotation;
	efx::quat_v2_t bind_pose_scale_rotation;
};

struct submotion_rot_v1_pack_t {
	efx::quat_v1_t pose_rotation;
	efx::quat_v1_t bind_pose_rotation;
	efx::quat_v1_t pose_scale_rotation;
	efx::quat_v1_t bind_pose_scale_rotation;
};

struct skeletal_submotion_pack_t {
	efx::vec3d_inpos_t pose_position;
	efx::vec3d_t pose_scale;
	efx::vec3d_t bind_pose_position;
	efx::vec3d_t bind_pose_scale;
	int32_t position_key_count;
	int32_t rotation_key_count;
	int32_t scale_key_count;
	int32_t scale_rotation_key_count;
	float max_error;
};

struct node_submotion_pack_t {
	efx::vec3d_inpos_t pose_position;
	efx::vec3d_t pose_scale;
	efx::vec3d_t bind_pose_position;
	efx::vec3d_t bind_pose_scale;
	int32_t position_key_count;
	int32_t rotation_key_count;
	int32_t scale_key_count;
	int32_t scale_rotation_key_count;
};
#pragma pack(pop)

struct xsm_metadata_t { // NOLINT(cppcoreguidelines-pro-type-member-init)
	bool has_v2_data = false;
	xsm_metadata_v2_pack_extra_t v2_data; // NOLINT(cppcoreguidelines-pro-type-member-init)
	xsm_metadata_pack_t packed; // NOLINT(cppcoreguidelines-pro-type-member-init)
	godot::String source_app;
	godot::String original_file_name;
	godot::String export_date;
	godot::String motion_name;
};

struct rotation_key_t {
	godot::Quaternion rotation;
	float time;
};

struct skeletal_submotion_t { // NOLINT(cppcoreguidelines-pro-type-member-init)
	godot::Quaternion pose_rotation;
	godot::Quaternion bind_pose_rotation;
	godot::Quaternion pose_scale_rotation;
	godot::Quaternion bind_pose_scale_rotation;
	skeletal_submotion_pack_t packed; // NOLINT(cppcoreguidelines-pro-type-member-init)
	godot::String node_name;
	memory::vector<position_key_t> position_keys;
	memory::vector<rotation_key_t> rotation_keys;
	memory::vector<scale_key_t> scale_keys;
	memory::vector<rotation_key_t> scale_rotation_keys;
};

struct bone_animation_t {
	int32_t submotion_count = 0;
	memory::vector<skeletal_submotion_t> submotions;
};

/*
0xC8, v1 chunk documentation: (Let's call it node_submotion)
	4*8x 32 bit sections = 32
		4x quat32 = 16
		4x vec3   = 12
		4x keycount 04
					32
	string node_name
	data...

	so this chunk is the same as a skeletal submotion v1
	but without the fMaxError
*/
struct node_submotion_t { // NOLINT(cppcoreguidelines-pro-type-member-init)
	godot::Quaternion pose_rotation;
	godot::Quaternion bind_pose_rotation;
	godot::Quaternion pose_scale_rotation;
	godot::Quaternion bind_pose_scale_rotation;
	node_submotion_pack_t packed; // NOLINT(cppcoreguidelines-pro-type-member-init)
	godot::String node_name;
	memory::vector<position_key_t> position_keys;
	memory::vector<rotation_key_t> rotation_keys;
	memory::vector<scale_key_t> scale_keys;
	memory::vector<rotation_key_t> scale_rotation_keys;
};

static Error _read_xsm_header(Ref<FileAccess> const& file) {
	Error err = OK;
	xsm_header_t header = efx::read_struct<xsm_header_t>(file, &err);

	ERR_FAIL_COND_V(err != OK, err);

	ERR_FAIL_COND_V_MSG(
		header.format_identifier != FORMAT_SPECIFIER, ERR_INVALID_DATA,
		vformat("Invalid XSM format identifier: %x (should be %x)", header.format_identifier, FORMAT_SPECIFIER)
	);

	ERR_FAIL_COND_V_MSG(
		header.version_major != VERSION_MAJOR || header.version_minor != XSM_VERSION_MINOR, ERR_INVALID_DATA,
		vformat(
			"Invalid XSM version: %d.%d (should be %d.%d)", header.version_major, header.version_minor, VERSION_MAJOR,
			XSM_VERSION_MINOR
		)
	);

	ERR_FAIL_COND_V_MSG(
		header.big_endian != 0, ERR_INVALID_DATA, "Invalid XSM endianness: big endian (only little endian is supported)"
	);

	return err;
}

static xsm_metadata_t _read_xsm_metadata(Ref<FileAccess> const& file, efx::Version version, Error& error_return) {
	xsm_metadata_t metadata;

	if (version == efx::Version::TWO) {
		efx::read_struct(file, metadata.v2_data, &error_return);
		if (error_return != OK) {
			return metadata;
		}

		metadata.has_v2_data = true;
	}

	efx::read_struct(file, metadata.packed, &error_return);
	if (error_return != OK) {
		return metadata;
	}

	efx::read_string(file, metadata.source_app, &error_return);
	if (error_return != OK) {
		return metadata;
	}

	efx::read_string(file, metadata.original_file_name, &error_return);
	if (error_return != OK) {
		return metadata;
	}

	efx::read_string(file, metadata.export_date, &error_return);
	if (error_return != OK) {
		return metadata;
	}

	efx::read_string(file, metadata.motion_name, &error_return);

	return metadata;
}

static memory::vector<rotation_key_t> _read_rot_keys( //
	Ref<FileAccess> const& file, int32_t count, efx::Version version, Error& error_return
) {
	memory::vector<rotation_key_t> result;

	switch (version) {
		using enum efx::Version;
	case ONE: {
		memory::vector<rotation_key_v1_t> rot_keys_v1;
		efx::read_struct_array(file, rot_keys_v1, count, &error_return);
		if (error_return != OK) {
			return result;
		}

		result.reserve(result.size() + rot_keys_v1.size());
		for (rotation_key_v1_t const& key : rot_keys_v1) {
			result.emplace_back(convert_to<Quaternion>(key.rotation), key.time);
		}
	} break;
	case TWO: {
		memory::vector<rotation_key_v2_t> rot_keys_v2;
		efx::read_struct_array(file, rot_keys_v2, count, &error_return);
		if (error_return != OK) {
			return result;
		}

		result.reserve(result.size() + rot_keys_v2.size());
		for (rotation_key_v2_t const& key : rot_keys_v2) {
			result.emplace_back(convert_to<Quaternion>(key.rotation), key.time);
		}
	} break;
	default: break;
	}

	return result;
}

static skeletal_submotion_t _read_skeletal_submotion(Ref<FileAccess> const& file, efx::Version version, Error& error_return) {
	skeletal_submotion_t submotion;

	switch (version) {
		using enum efx::Version;
	case ONE: { // float component quats (v1)
		submotion_rot_v1_pack_t rot_comps = efx::read_struct<submotion_rot_v1_pack_t>(file, &error_return);
		if (error_return != OK) {
			return submotion;
		}

		submotion.pose_rotation = convert_to<Quaternion>(rot_comps.pose_rotation);
		submotion.bind_pose_rotation = convert_to<Quaternion>(rot_comps.bind_pose_rotation);
		submotion.pose_scale_rotation = convert_to<Quaternion>(rot_comps.pose_scale_rotation);
		submotion.bind_pose_scale_rotation = convert_to<Quaternion>(rot_comps.bind_pose_scale_rotation);
	} break;
	case TWO: { // int16 component quats (v2)
		submotion_rot_v2_pack_t rot_comps = efx::read_struct<submotion_rot_v2_pack_t>(file, &error_return);
		if (error_return != OK) {
			return submotion;
		}

		submotion.pose_rotation = convert_to<Quaternion>(rot_comps.pose_rotation);
		submotion.bind_pose_rotation = convert_to<Quaternion>(rot_comps.bind_pose_rotation);
		submotion.pose_scale_rotation = convert_to<Quaternion>(rot_comps.pose_scale_rotation);
		submotion.bind_pose_scale_rotation = convert_to<Quaternion>(rot_comps.bind_pose_scale_rotation);
	} break;
	default: break;
	}

	efx::read_struct(file, submotion.packed, &error_return);
	if (error_return != OK) {
		return submotion;
	}

	efx::read_string(file, submotion.node_name, &error_return, true);
	if (error_return != OK) {
		return submotion;
	}

	efx::read_struct_array(file, submotion.position_keys, submotion.packed.position_key_count, &error_return);
	if (error_return != OK) {
		return submotion;
	}

	submotion.rotation_keys = _read_rot_keys(file, submotion.packed.rotation_key_count, version, error_return);
	if (error_return != OK) {
		return submotion;
	}

	efx::read_struct_array(file, submotion.scale_keys, submotion.packed.scale_key_count, &error_return);
	if (error_return != OK) {
		return submotion;
	}

	submotion.scale_rotation_keys = _read_rot_keys(file, submotion.packed.scale_rotation_key_count, version, error_return);

	return submotion;
}

static memory::vector<skeletal_submotion_t> _read_xsm_bone_animation( //
	Ref<FileAccess> const& file, efx::Version version, Error& error_return
) {
	error_return = OK;

	memory::vector<skeletal_submotion_t> submotions;

	submotions.reserve(file->get_32());
	for (size_t i = 0; i < submotions.capacity(); i++) {
		skeletal_submotion_t submotion = _read_skeletal_submotion(file, version, error_return);
		ERR_CONTINUE(error_return != OK);
		submotions.emplace_back(std::move(submotion));
	}

	return submotions;
}

static node_submotion_t _read_node_submotion(Ref<FileAccess> const& file, Error& error_return) {
	node_submotion_t submotion;

	submotion_rot_v1_pack_t rot_comps = efx::read_struct<submotion_rot_v1_pack_t>(file, &error_return);

	if (error_return != OK) {
		return submotion;
	}

	submotion.pose_rotation = convert_to<Quaternion>(rot_comps.pose_rotation);
	submotion.bind_pose_rotation = convert_to<Quaternion>(rot_comps.bind_pose_rotation);
	submotion.pose_scale_rotation = convert_to<Quaternion>(rot_comps.pose_scale_rotation);
	submotion.bind_pose_scale_rotation = convert_to<Quaternion>(rot_comps.bind_pose_scale_rotation);

	efx::read_struct(file, submotion.packed, &error_return);
	if (error_return != OK) {
		return submotion;
	}

	efx::read_string(file, submotion.node_name, &error_return);
	if (error_return != OK) {
		return submotion;
	}

	efx::read_struct_array(file, submotion.position_keys, submotion.packed.position_key_count, &error_return);
	if (error_return != OK) {
		return submotion;
	}

	submotion.rotation_keys = _read_rot_keys(file, submotion.packed.rotation_key_count, efx::Version::ONE, error_return);
	if (error_return != OK) {
		return submotion;
	}

	efx::read_struct_array(file, submotion.scale_keys, submotion.packed.scale_key_count, &error_return);
	if (error_return != OK) {
		return submotion;
	}

	submotion.scale_rotation_keys =
		_read_rot_keys(file, submotion.packed.scale_rotation_key_count, efx::Version::ONE, error_return);

	return submotion;
}

// returns highest time in the submotion
template<typename T>
static float _add_submotion(Ref<Animation>& anim, T const& submotion) {
	// a variable string or stringname with %s makes godot fail silently if placed outside a function
	const String SKELETON_NODE_PATH = "./skeleton:%s";
	float max_time = 0;

	// NOTE: godot uses ':' to specify properties, so we replaced such characters with '_' when we read them in
	String skeleton_path = vformat(SKELETON_NODE_PATH, submotion.node_name);

	int32_t pos_id = anim->add_track(Animation::TYPE_POSITION_3D);
	int32_t rot_id = anim->add_track(Animation::TYPE_ROTATION_3D);
	int32_t scale_id = anim->add_track(Animation::TYPE_SCALE_3D);

	anim->track_set_path(pos_id, skeleton_path);
	anim->track_set_path(rot_id, skeleton_path);
	anim->track_set_path(scale_id, skeleton_path);

	for (position_key_t const& key : submotion.position_keys) {
		anim->position_track_insert_key(pos_id, key.time, convert_to<Vector3>(key.position));
		if (key.time > max_time) {
			max_time = key.time;
		}
	}

	for (rotation_key_t const& key : submotion.rotation_keys) {
		anim->rotation_track_insert_key(rot_id, key.time, key.rotation);
		if (key.time > max_time) {
			max_time = key.time;
		}
	}

	// not needed for vic2 animations, but we can still support it
	for (scale_key_t const& key : submotion.scale_keys) {
		anim->scale_track_insert_key(scale_id, key.time, convert_to<Vector3>(key.scale));
		if (key.time > max_time) {
			max_time = key.time;
		}
	}

	// TODO: SCALEROTATION (unused in vic2, so low priority)
	for (rotation_key_t const& key : submotion.scale_rotation_keys) {
		if (key.time > max_time) {
			max_time = key.time;
		}
	}

	// If you pay close attention, there's still a small jump along the loop point because of this hack
	//  this hack is in place because otherwise animations completely broken.
	//  The small animation jump is visible in vic2 aswell, so its likely something to do with bad data, rather than a problem
	//  with the loader here.
	if (anim->track_find_key(pos_id, 0) != -1) {
		anim->track_remove_key_at_time(pos_id, 0);
	}
	if (anim->track_find_key(rot_id, 0) != -1) {
		anim->track_remove_key_at_time(rot_id, 0);
	}
	if (anim->track_find_key(scale_id, 0) != -1) {
		anim->track_remove_key_at_time(scale_id, 0);
	}

	// add the "default" positions/rotation/scale/... as keys at time 0
	anim->position_track_insert_key(pos_id, 0, convert_to<Vector3>(submotion.packed.pose_position));
	anim->rotation_track_insert_key(rot_id, 0, submotion.pose_rotation);
	anim->scale_track_insert_key(scale_id, 0, convert_to<Vector3>(submotion.packed.pose_scale));


	return max_time;
}

static Ref<Animation> _load_xsm_animation(Ref<FileAccess> const& file, Error& error_return) {
	error_return = _read_xsm_header(file);
	ERR_FAIL_COND_V(error_return != OK, Ref<Animation>());

	xsm_metadata_t metadata;
	memory::vector<skeletal_submotion_t> skeletal_submotions;
	memory::vector<node_submotion_t> node_submotions;

	while (!file->eof_reached()) {
		efx::chunk_header_t header = efx::read_chunk_header(file, &error_return);
		if (error_return == ERR_FILE_CANT_READ) {
			error_return = OK;
			break;
		}
		ERR_BREAK(error_return != OK);

		switch (header.type) {
			using enum efx::Type;
		case XSM_METADATA:
			metadata = _read_xsm_metadata(file, header.version, error_return); // in zulu_moving.xsm, this is v1
			if (error_return != OK) {
				goto EXIT_LOOP;
			}
			break;
		case XSM_SUBMOTION: {
			node_submotion_t node = _read_node_submotion(file, error_return);
			if (error_return != OK) {
				goto EXIT_LOOP;
			}
			node_submotions.push_back(std::move(node));
		} break;
		case XSM_BONE_ANIMATION:
			skeletal_submotions = _read_xsm_bone_animation(file, header.version, error_return);
			if (error_return != OK) {
				goto EXIT_LOOP;
			}
			break;
		default: //
		{
			UtilityFunctions::push_warning(vformat(
				"Unsupported XSM chunk: file: %s, type = %x, length = %x, version = %d at %x", file->get_path(),
				static_cast<int32_t>(header.type), header.length, static_cast<int32_t>(header.version), file->get_position()
			));

			// Skip unsupported chunks
			error_return = ERR_INVALID_DATA;
			ERR_FAIL_COND_V_MSG(
				header.length + file->get_position() > file->get_length(), Ref<Animation>(),
				vformat("XSM header length %d is too long", header.length)
			);

			file->seek(file->get_position() + header.length);
			error_return = file->get_error();
			if (error_return != OK) {
				goto EXIT_LOOP;
			}
		} break;
		}
	}
EXIT_LOOP:

	Ref<Animation> anim = Ref<Animation>();
	if (error_return != OK) {
		return anim;
	}

	float animation_length = 0.0;
	anim.instantiate();

	anim->set_step(1.0 / metadata.packed.fps);
	anim->set_loop_mode(Animation::LOOP_LINEAR);

	for (skeletal_submotion_t const& submotion : skeletal_submotions) {
		float submotion_len = _add_submotion(anim, submotion);
		if (submotion_len > animation_length) {
			animation_length = submotion_len;
		}
	}

	for (node_submotion_t const& submotion : node_submotions) {
		float submotion_len = _add_submotion(anim, submotion);
		if (submotion_len > animation_length) {
			animation_length = submotion_len;
		}
	}

	anim->set_length(animation_length);

	return anim;
}

Variant ResourceFormatLoaderXSM::_load(
	const String& p_path, const String& p_original_path, bool p_use_sub_threads, int32_t p_cache_mode
) const {
	Ref<FileAccess> file = FileAccess::open(p_path, FileAccess::READ);
	Error err = file->get_open_error();
	ERR_FAIL_COND_V_MSG(err != OK, Ref<Animation>(), vformat("Could not open file %s", p_path));

	Ref<Animation> result = _load_xsm_animation(file, err);
	ERR_FAIL_COND_V(err != OK, result);
	return result;
}

PackedStringArray ResourceFormatLoaderXSM::_get_recognized_extensions() const {
	return { "xsm" };
}

bool ResourceFormatLoaderXSM::_handles_type(const StringName& p_type) const {
	return ClassDB::is_parent_class(p_type, "Animation");
}

String ResourceFormatLoaderXSM::_get_resource_type(const String& p_path) const {
	if (p_path.get_extension() == "xsm") {
		return "Animation";
	}
	return "";
}

ResourceFormatLoaderXSM::ResourceFormatLoaderXSM() {}

ResourceFormatLoaderXSM::~ResourceFormatLoaderXSM() {}
