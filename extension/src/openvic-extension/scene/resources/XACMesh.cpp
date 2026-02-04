#include "XACMesh.hpp"

#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/mesh_instance3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/core/property_info.hpp>
#include <godot_cpp/variant/variant.hpp>

#include "openvic-extension/core/Bind.hpp"
#include "openvic-extension/scene/resources/XACSkeleton.hpp"

using namespace OpenVic;
using namespace godot;

XACMesh::~XACMesh() {}

void XACMesh::set_mesh(Ref<Mesh> const& mesh) {
	this->mesh = mesh;
}

Ref<Mesh> XACMesh::get_mesh() const {
	return mesh;
}

void XACMesh::set_extra_cull_margin(float margin) {
	extra_cull_margin = margin;
}

float XACMesh::get_extra_cull_margin() const {
	return extra_cull_margin;
}

void XACMesh::set_skeleton(Ref<XACSkeleton> const& skeleton) {
	this->skeleton = skeleton;
}

Ref<XACSkeleton> XACMesh::get_skeleton() const {
	return skeleton;
}

MeshInstance3D* XACMesh::generate_mesh_instance() const {
	MeshInstance3D* mesh_instance = memnew(MeshInstance3D);
	mesh_instance->set_extra_cull_margin(extra_cull_margin);
	mesh_instance->set_mesh(mesh);
	return mesh_instance;
}

void XACMesh::_bind_methods() {
	OV_BIND_METHOD(XACMesh::set_mesh, { "mesh" });
	OV_BIND_METHOD(XACMesh::get_mesh);
	ADD_PROPERTY(
		PropertyInfo(Variant::OBJECT, "mesh", PROPERTY_HINT_RESOURCE_TYPE, Mesh::get_class_static()), //
		"set_mesh", "get_mesh"
	);

	OV_BIND_METHOD(XACMesh::set_extra_cull_margin, { "margin" });
	OV_BIND_METHOD(XACMesh::get_extra_cull_margin);
	ADD_PROPERTY(
		PropertyInfo(Variant::FLOAT, "extra_cull_margin"), //
		"set_extra_cull_margin", "get_extra_cull_margin"
	);

	OV_BIND_METHOD(XACMesh::set_skeleton, { "skeleton" });
	OV_BIND_METHOD(XACMesh::get_skeleton);
	ADD_PROPERTY(
		PropertyInfo(Variant::OBJECT, "skeleton", PROPERTY_HINT_RESOURCE_TYPE, XACSkeleton::get_class_static()), //
		"set_skeleton", "get_skeleton"
	);

	OV_BIND_METHOD(XACMesh::generate_mesh_instance);
}
