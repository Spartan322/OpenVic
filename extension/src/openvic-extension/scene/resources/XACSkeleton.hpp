#pragma once

#include <cstdint>

#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/wrapped.hpp>
#include <godot_cpp/templates/a_hash_map.hpp>
#include <godot_cpp/templates/vector.hpp>
#include <godot_cpp/variant/packed_string_array.hpp>
#include <godot_cpp/variant/vector3.hpp>

namespace godot {
	class String;
	class Transform3D;
	class Quaternion;
	class Skeleton3D;
}

namespace OpenVic {
	class XACSkeleton : public godot::Resource {
		GDCLASS(XACSkeleton, godot::Resource);

		struct Bone {
			godot::String name;
			int32_t parent = -1;

			godot::Vector3 pose_position;
			godot::Quaternion pose_rotation;
			godot::Vector3 pose_scale = godot::Vector3(1, 1, 1);

			godot::Transform3D get_bone_pose() const;
		};

		godot::LocalVector<Bone> bones;
		godot::AHashMap<godot::String, int> names_to_bone_index;

	protected:
		static void _bind_methods();

	public:
		uint64_t get_bone_count() const;

		int32_t add_bone(godot::String const& name);

		void set_bone_name(int32_t bone, godot::String const& name);
		godot::String get_bone_name(int32_t bone) const;

		void set_bone_parent(int32_t bone, int32_t parent);
		int32_t get_bone_parent(int32_t bone) const;

		void set_bone_pose(int p_bone, const godot::Transform3D& p_pose);
		godot::Transform3D get_bone_pose(int p_bone) const;

		void clear_bones();

		godot::Skeleton3D* generate_skeleton() const;

		virtual ~XACSkeleton();
	};
}
