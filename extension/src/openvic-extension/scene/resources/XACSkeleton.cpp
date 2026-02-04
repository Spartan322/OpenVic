#include "XACSkeleton.hpp"

#include <cstdint>

#include <godot_cpp/classes/skeleton3d.hpp>
#include <godot_cpp/core/error_macros.hpp>
#include <godot_cpp/variant/quaternion.hpp>
#include <godot_cpp/variant/string.hpp>
#include <godot_cpp/variant/transform3d.hpp>
#include <godot_cpp/variant/vector3.hpp>

#include "openvic-extension/core/Bind.hpp"

using namespace OpenVic;
using namespace godot;

Transform3D XACSkeleton::Bone::get_bone_pose() const {
	Transform3D bone_pose;
	bone_pose.basis.set_quaternion_scale(pose_rotation, pose_scale);
	bone_pose.origin = pose_position;
	return bone_pose;
}

XACSkeleton::~XACSkeleton() {}

uint64_t XACSkeleton::get_bone_count() const {
	return bones.size();
}

int32_t XACSkeleton::add_bone(String const& name) {
	ERR_FAIL_COND_V_MSG(
		name.is_empty() || name.contains(":") || name.contains("/"), -1,
		vformat("Bone name cannot be empty or contain ':' or '/'.", name)
	);
	ERR_FAIL_COND_V_MSG(
		names_to_bone_index.has(name), -1, vformat("XACSkeleton \"%s\" already has a bone with name \"%s\".", to_string(), name)
	);

	int32_t bone_index = bones.size();
	bones.push_back({ name });
	names_to_bone_index.insert(name, bone_index);
	return bone_index;
}

void XACSkeleton::set_bone_name(int32_t bone, String const& name) {
	const int bone_size = bones.size();
	ERR_FAIL_INDEX(bone, bone_size);

	const int* bone_index_ptr = names_to_bone_index.getptr(name);
	if (bone_index_ptr != nullptr) {
		ERR_FAIL_COND_MSG(
			*bone_index_ptr != bone, "XACSkeleton: '" + get_name() + "', bone name:  '" + name + "' already exists."
		);
		return; // No need to rename, the bone already has the given name.
	}

	names_to_bone_index.erase(bones[bone].name);
	bones[bone].name = name;
	names_to_bone_index.insert(name, bone);
}

String XACSkeleton::get_bone_name(int32_t bone) const {
	ERR_FAIL_INDEX_V(bone, bones.size(), "");
	return bones[bone].name;
}

void XACSkeleton::set_bone_parent(int32_t bone, int32_t parent) {
	ERR_FAIL_INDEX(bone, bones.size());
	ERR_FAIL_COND(parent != -1 && (parent < 0));
	ERR_FAIL_COND(bone == parent);

	bones[bone].parent = parent;
}

int32_t XACSkeleton::get_bone_parent(int32_t bone) const {
	ERR_FAIL_INDEX_V(bone, bones.size(), -1);
	return bones[bone].parent;
}

Transform3D XACSkeleton::get_bone_pose(int p_bone) const {
	ERR_FAIL_INDEX_V(p_bone, bones.size(), Transform3D());
	return bones[p_bone].get_bone_pose();
}

void XACSkeleton::set_bone_pose(int p_bone, const Transform3D& p_pose) {
	ERR_FAIL_INDEX(p_bone, bones.size());
	bones[p_bone].pose_position = p_pose.origin;
	bones[p_bone].pose_rotation = p_pose.basis.get_rotation_quaternion();
	bones[p_bone].pose_scale = p_pose.basis.get_scale();
}

void XACSkeleton::clear_bones() {
	bones.clear();
	names_to_bone_index.clear();
}

Skeleton3D* XACSkeleton::generate_skeleton() const {
	static const StringName node_name = { "skeleton", true };

	Skeleton3D* skeleton = memnew(Skeleton3D);
	skeleton->set_name(node_name);
	for (size_t current_bone = 0; Bone const& bone : bones) {
		skeleton->add_bone(bone.name);
		skeleton->set_bone_parent(current_bone, bone.parent);
		Transform3D transform = bone.get_bone_pose();
		skeleton->set_bone_rest(current_bone, transform);
		skeleton->set_bone_pose(current_bone, transform);
		current_bone++;
	}
	return skeleton;
}

void XACSkeleton::_bind_methods() {
	OV_BIND_METHOD(XACSkeleton::get_bone_count);

	OV_BIND_METHOD(XACSkeleton::add_bone, { "name" });

	OV_BIND_METHOD(XACSkeleton::set_bone_name, { "bone_idx", "name" });
	OV_BIND_METHOD(XACSkeleton::get_bone_name, { "bone_idx" });

	OV_BIND_METHOD(XACSkeleton::set_bone_parent, { "bone_idx", "parent_idx" });
	OV_BIND_METHOD(XACSkeleton::get_bone_parent, { "bone_idx" });

	OV_BIND_METHOD(XACSkeleton::set_bone_pose, { "bone", "pose" });
	OV_BIND_METHOD(XACSkeleton::get_bone_pose, { "bone" });

	OV_BIND_METHOD(XACSkeleton::clear_bones);

	OV_BIND_METHOD(XACSkeleton::generate_skeleton);
}
