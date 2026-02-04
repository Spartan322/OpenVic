#pragma once

#include <godot_cpp/classes/mesh.hpp>
#include <godot_cpp/classes/resource.hpp>
#include <godot_cpp/classes/wrapped.hpp>

namespace godot {
	class MeshInstance3D;
}

namespace OpenVic {
	class XACSkeleton;

	class XACMesh : public godot::Resource {
		GDCLASS(XACMesh, godot::Resource);

		godot::Ref<godot::Mesh> mesh;
		godot::Ref<XACSkeleton> skeleton;
		float extra_cull_margin;

	protected:
		static void _bind_methods();

	public:
		void set_mesh(godot::Ref<godot::Mesh> const& mesh);
		godot::Ref<godot::Mesh> get_mesh() const;

		void set_extra_cull_margin(float margin);
		float get_extra_cull_margin() const;

		void set_skeleton(godot::Ref<XACSkeleton> const& skeleton);
		godot::Ref<XACSkeleton> get_skeleton() const;

		godot::MeshInstance3D* generate_mesh_instance() const;

		virtual ~XACMesh();
	};
}
