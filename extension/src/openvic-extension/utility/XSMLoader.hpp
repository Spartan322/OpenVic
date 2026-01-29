#pragma once

#include <cstdint>

#include <godot_cpp/classes/animation.hpp>
#include <godot_cpp/classes/file_access.hpp>
#include <godot_cpp/variant/packed_byte_array.hpp>
#include <godot_cpp/variant/packed_vector3_array.hpp>
#include <godot_cpp/variant/packed_vector4_array.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/utility_functions.hpp>

#include <openvic-simulation/utility/Containers.hpp>

#include "openvic-extension/core/io/EfxPackedStructs.hpp"

namespace OpenVic {
	class XsmLoader {
		static constexpr uint32_t FORMAT_SPECIFIER = ' MSX'; /* Order reversed due to little endian */
		static constexpr uint8_t VERSION_MAJOR = 1, XSM_VERSION_MINOR = 0;

#pragma pack(push, 1)
		struct xsm_header_t : efx::readable_struct_t<xsm_header_t> {
			uint32_t format_identifier;
			uint8_t version_major;
			uint8_t version_minor;
			uint8_t big_endian;
			uint8_t pad;
		};

		// v2 of the header adds these 2 properties at the start
		struct xsm_metadata_v2_pack_extra_t : efx::readable_struct_t<xsm_metadata_v2_pack_extra_t> {
			float unused;
			float max_acceptable_error;
		};

		struct xsm_metadata_pack_t : efx::readable_struct_t<xsm_metadata_pack_t> {
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
		struct submotion_rot_v2_pack_t : efx::readable_struct_t<submotion_rot_v2_pack_t> {
			efx::quat_v2_t pose_rotation;
			efx::quat_v2_t bind_pose_rotation;
			efx::quat_v2_t pose_scale_rotation;
			efx::quat_v2_t bind_pose_scale_rotation;
		};

		struct submotion_rot_v1_pack_t : efx::readable_struct_t<submotion_rot_v1_pack_t> {
			efx::quat_v1_t pose_rotation;
			efx::quat_v1_t bind_pose_rotation;
			efx::quat_v1_t pose_scale_rotation;
			efx::quat_v1_t bind_pose_scale_rotation;
		};

		struct skeletal_submotion_pack_t : efx::readable_struct_t<skeletal_submotion_pack_t> {
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

		struct node_submotion_pack_t : efx::readable_struct_t<node_submotion_pack_t> {
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

		struct xsm_metadata_t {
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

		struct skeletal_submotion_t {
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
		struct node_submotion_t {
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

		bool _read_xsm_header(godot::Ref<godot::FileAccess> const& file);

		bool _read_xsm_metadata(godot::Ref<godot::FileAccess> const& file, xsm_metadata_t& metadata, efx::Version version);

		bool _read_rot_keys(
			godot::Ref<godot::FileAccess> const& file, memory::vector<rotation_key_t>& keys_out, int32_t count,
			efx::Version version
		);

		bool _read_skeletal_submotion(//
			godot::Ref<godot::FileAccess> const& file, skeletal_submotion_t& submotion, efx::Version version
		);

		bool _read_xsm_bone_animation(//
			godot::Ref<godot::FileAccess> const& file, bone_animation_t& bone_animation, efx::Version version
		);

		bool _read_node_submotion(godot::Ref<godot::FileAccess> const& file, memory::vector<node_submotion_t>& submotions);

		template<typename T>
		float _add_submotion(godot::Ref<godot::Animation>& anim, T const& submotion);

	public:
		godot::Ref<godot::Animation> load_xsm_animation(godot::Ref<godot::FileAccess> const& file);
	};
}
