#include "godot_cpp/classes/animation.hpp"
#pragma one

#include <godot_cpp/classes/node3d.hpp>
#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/wrapped.hpp>
#include <godot_cpp/templates/a_hash_map.hpp>
#include <godot_cpp/templates/local_vector.hpp>
#include <godot_cpp/variant/color.hpp>

namespace godot {
	class AnimationPlayer;
	class Skeleton3D;
	class StringName;
}

namespace OpenVic {
	class XACModel;
	class UnitModel;

	class MapInstance3D : public godot::Node3D {
		GDCLASS(MapInstance3D, godot::Node3D);

		friend class UnitModel;

		MapInstance3D* parent_instance = nullptr;
		godot::LocalVector<MapInstance3D*> child_instances;
		bool is_dirty = false;

		mutable godot::Skeleton3D* skeleton = nullptr;
		godot::AnimationPlayer* animation_player = nullptr;

		godot::LocalVector<godot::Color> colour_overrides;

		godot::AHashMap<godot::StringName, float> animation_to_texture_scroll_override;

		int32_t flag_index_override = -1;

	protected:
		static void _bind_methods();

		void _notification(int p_what);

	public:
		godot::Skeleton3D* get_skeleton() const;
		godot::AnimationPlayer* get_animation_player() const;

		void add_animation(godot::StringName const& animation_name, godot::Ref<godot::Animation> const& animation);
		void remove_animation(godot::StringName const& animation_name);
		godot::Ref<godot::Animation> get_animation(godot::StringName const& animation_name) const;

		void set_current_animation(godot::StringName const& animation);
		godot::StringName get_current_animation() const;

		godot::Error attach_model(godot::StringName const& bone_name, godot::Node3D* instance);

		void set_colour_index_override(uint32_t index, godot::Color color);
		godot::Color get_colour_from_index(uint32_t index) const;

		void set_texture_animation_scroll_override(godot::StringName const& animation, float scroll);
		float get_texture_animation_scroll(godot::StringName const& animation) const;

		void set_flag_index_override(uint16_t index);
		void reset_flag_index();
		uint16_t get_flag_index() const;

		void update_shader_data();

		MapInstance3D();
		virtual ~MapInstance3D();
	};
}
