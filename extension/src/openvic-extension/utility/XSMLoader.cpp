#include "XSMLoader.hpp"

#include <cstdint>

#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/variant.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include <openvic-simulation/utility/Containers.hpp>

#include "openvic-extension/core/Convert.hpp"
#include "openvic-extension/core/io/EfxPackedStructs.hpp"

using namespace OpenVic;
using namespace godot;

bool XsmLoader::_read_xsm_header(Ref<FileAccess> const& file) {
	xsm_header_t header; // NOLINT(cppcoreguidelines-pro-type-member-init)
	ERR_FAIL_COND_V(header.read_from(file) != OK, false);

	ERR_FAIL_COND_V_MSG(
		header.format_identifier != FORMAT_SPECIFIER, false,
		vformat("Invalid XSM format identifier: %x (should be %x)", header.format_identifier, FORMAT_SPECIFIER)
	);

	ERR_FAIL_COND_V_MSG(
		header.version_major != VERSION_MAJOR || header.version_minor != XSM_VERSION_MINOR, false,
		vformat(
			"Invalid XSM version: %d.%d (should be %d.%d)", header.version_major, header.version_minor, VERSION_MAJOR,
			XSM_VERSION_MINOR
		)
	);

	ERR_FAIL_COND_V_MSG(header.big_endian != 0, false, "Invalid XSM endianness: big endian (only little endian is supported)");

	return true;
}

bool XsmLoader::_read_xsm_metadata(Ref<FileAccess> const& file, xsm_metadata_t& metadata, efx::Version version) {
	bool res = true;
	if (version == efx::Version::TWO) {
		res &= metadata.v2_data.read_from(file);
		metadata.has_v2_data = true;
	}
	res &= metadata.packed.read_from(file);
	res &= efx::read_string(file, metadata.source_app);
	res &= efx::read_string(file, metadata.original_file_name);
	res &= efx::read_string(file, metadata.export_date);
	res &= efx::read_string(file, metadata.motion_name);

	return res;
}

bool XsmLoader::_read_rot_keys(
	Ref<FileAccess> const& file, memory::vector<rotation_key_t>& keys_out, int32_t count, efx::Version version
) {
	bool res = true;

	switch (version) {
		using enum efx::Version;
	case ONE: {
		memory::vector<rotation_key_v1_t> rot_keys_v1;
		res &= efx::read_struct_array(file, rot_keys_v1, count);
		keys_out.reserve(keys_out.size() + rot_keys_v1.size());
		for (rotation_key_v1_t const& key : rot_keys_v1) {
			keys_out.emplace_back(convert_to<Quaternion>(key.rotation), key.time);
		}
	} break;
	case TWO: {
		memory::vector<rotation_key_v2_t> rot_keys_v2;
		res &= efx::read_struct_array(file, rot_keys_v2, count);
		keys_out.reserve(keys_out.size() + rot_keys_v2.size());
		for (rotation_key_v2_t const& key : rot_keys_v2) {
			keys_out.emplace_back(convert_to<Quaternion>(key.rotation), key.time);
		}
	} break;
	default: break;
	}

	return res;
}

bool XsmLoader::_read_skeletal_submotion(Ref<FileAccess> const& file, skeletal_submotion_t& submotion, efx::Version version) {
	bool res = true;

	switch (version) {
		using enum efx::Version;
	case ONE: { // float component quats (v1)
		submotion_rot_v1_pack_t rot_comps; // NOLINT(cppcoreguidelines-pro-type-member-init)
		res &= rot_comps.read_from(file);
		submotion.pose_rotation = convert_to<Quaternion>(rot_comps.pose_rotation);
		submotion.bind_pose_rotation = convert_to<Quaternion>(rot_comps.bind_pose_rotation);
		submotion.pose_scale_rotation = convert_to<Quaternion>(rot_comps.pose_scale_rotation);
		submotion.bind_pose_scale_rotation = convert_to<Quaternion>(rot_comps.bind_pose_scale_rotation);
	} break;
	case TWO: { // int16 component quats (v2)
		submotion_rot_v2_pack_t rot_comps; // NOLINT(cppcoreguidelines-pro-type-member-init)
		res &= rot_comps.read_from(file);
		submotion.pose_rotation = convert_to<Quaternion>(rot_comps.pose_rotation);
		submotion.bind_pose_rotation = convert_to<Quaternion>(rot_comps.bind_pose_rotation);
		submotion.pose_scale_rotation = convert_to<Quaternion>(rot_comps.pose_scale_rotation);
		submotion.bind_pose_scale_rotation = convert_to<Quaternion>(rot_comps.bind_pose_scale_rotation);
	} break;
	default: break;
	}

	ERR_FAIL_COND_V(submotion.packed.read_from(file) != OK, false);
	res &= efx::read_string(file, submotion.node_name, true);
	res &= efx::read_struct_array(file, submotion.position_keys, submotion.packed.position_key_count);
	res &= _read_rot_keys(file, submotion.rotation_keys, submotion.packed.rotation_key_count, version);
	res &= efx::read_struct_array(file, submotion.scale_keys, submotion.packed.scale_key_count);
	res &= _read_rot_keys(file, submotion.scale_rotation_keys, submotion.packed.scale_rotation_key_count, version);

	return res;
}

bool XsmLoader::_read_xsm_bone_animation(Ref<FileAccess> const& file, bone_animation_t& bone_animation, efx::Version version) {
	bone_animation.submotion_count = file->get_32();
	bool ret = true;

	for (uint32_t i = 0; i < bone_animation.submotion_count; i++) {
		skeletal_submotion_t submotion;
		ret &= _read_skeletal_submotion(file, submotion, version);
		bone_animation.submotions.push_back(submotion);
	}

	return true;
}

bool XsmLoader::_read_node_submotion(Ref<FileAccess> const& file, memory::vector<node_submotion_t>& submotions) {
	bool res = true;
	node_submotion_t submotion;

	submotion_rot_v1_pack_t rot_comps; // NOLINT(cppcoreguidelines-pro-type-member-init)
	res &= rot_comps.read_from(file);
	submotion.pose_rotation = convert_to<Quaternion>(rot_comps.pose_rotation);
	submotion.bind_pose_rotation = convert_to<Quaternion>(rot_comps.bind_pose_rotation);
	submotion.pose_scale_rotation = convert_to<Quaternion>(rot_comps.pose_scale_rotation);
	submotion.bind_pose_scale_rotation = convert_to<Quaternion>(rot_comps.bind_pose_scale_rotation);

	res &= submotion.packed.read_from(file);
	res &= efx::read_string(file, submotion.node_name);
	res &= efx::read_struct_array(file, submotion.position_keys, submotion.packed.position_key_count);
	res &= _read_rot_keys(file, submotion.rotation_keys, submotion.packed.rotation_key_count, efx::Version::ONE);
	res &= efx::read_struct_array(file, submotion.scale_keys, submotion.packed.scale_key_count);
	res &= _read_rot_keys(file, submotion.scale_rotation_keys, submotion.packed.scale_rotation_key_count, efx::Version::ONE);
	submotions.push_back(submotion);

	return res;
}

// returns highest time in the submotion
template<typename T>
float XsmLoader::_add_submotion(Ref<Animation>& anim, T const& submotion) {
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

Ref<Animation> XsmLoader::load_xsm_animation(Ref<FileAccess> const& file) {
	_read_xsm_header(file);

	xsm_metadata_t metadata;
	bone_animation_t bone_anim;
	memory::vector<node_submotion_t> node_submotions;

	bool res = true;

	while (!file->eof_reached()) {
		efx::chunk_header_t header; // NOLINT(cppcoreguidelines-pro-type-member-init)
		if (header.read_from(file) != OK) {
			break;
		}

		switch (header.type) {
			using enum efx::Type;
		case XSM_METADATA:
			_read_xsm_metadata(file, metadata, header.version); // in zulu_moving.xsm, this is v1
			break;
		case XSM_SUBMOTION:
			_read_node_submotion(file, node_submotions); //
			break;
		case XSM_BONE_ANIMATION:
			_read_xsm_bone_animation(file, bone_anim, header.version); //
			break;
		default: //
		{
			UtilityFunctions::print(vformat(
				"Unsupported XSM chunk: file: %s, type = %x, length = %x, version = %d at %x", file->get_path(),
				static_cast<int32_t>(header.type), header.length, static_cast<int32_t>(header.version), file->get_position()
			));

			// Skip unsupported chunks
			if (header.length + file->get_position() < file->get_length()) {
				PackedByteArray buf = file->get_buffer(header.length);
				UtilityFunctions::print(buf);
			} else {
				res = false;
				goto EXIT_LOOP;
			}
		} break;
		}
	}
EXIT_LOOP:

	float animation_length = 0.0;
	Ref<Animation> anim = Ref<Animation>();
	anim.instantiate();

	anim->set_step(1.0 / metadata.packed.fps);
	anim->set_loop_mode(Animation::LOOP_LINEAR);

	if (res == false) {
		return anim; // exit early if reading the chunks in failed
	}

	for (skeletal_submotion_t const& submotion : bone_anim.submotions) {
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
