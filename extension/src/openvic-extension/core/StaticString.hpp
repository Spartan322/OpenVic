#pragma once

#include <godot_cpp/variant/string_name.hpp>

#include "openvic-simulation/utility/Typedefs.hpp"

// Inline Names, if the same string comes up multiple times, add it to StaticStrings and use OV_SNAME
#define OV_INAME(STR) \
	([]() -> const godot::StringName& { \
		static godot::StringName sname = godot::StringName { STR, true }; \
		return sname; \
	})()

namespace OpenVic {
	class StaticStrings {
		inline static StaticStrings* singleton = nullptr;

	public:
		using StringName = godot::StringName;

		static void create() {
			singleton = memnew(StaticStrings);
		}
		static void free() {
			godot::memdelete(singleton);
			singleton = nullptr;
		}

		OV_ALWAYS_INLINE static StaticStrings* get_singleton() {
			return singleton;
		}

		const StringName name = "name";
		const StringName texture = "texture";
		const StringName scale = "scale";
		const StringName identifier = "identifier";
		const StringName position = "position";
		const StringName size = "size";
		const StringName changed = "changed";
		const StringName emit_changed = "emit_changed";
		const StringName hover = "hover";
		const StringName pressed = "pressed";
		const StringName disabled = "disabled";
		const StringName selected = "selected";
		const StringName change = "change";
		const StringName type = "type";
		const StringName tooltip = "tooltip";
		const StringName colour = "colour";
		const StringName weight = "weight";
		const StringName normal = "normal";
		const StringName font = "font";
		const StringName font_color = "font_color";
		const StringName font_hover_color = "font_hover_color";
		const StringName font_hover_pressed_color = "font_hover_pressed_color";
		const StringName font_pressed_color = "font_pressed_color";
		const StringName font_disabled_color = "font_disabled_color";
		const StringName scroll_index_changed = "scroll_index_changed";
		const StringName set_scroll_index = "set_scroll_index";
		const StringName set_z_index = "set_z_index";
		const StringName mouse_entered = "mouse_entered";
		const StringName mouse_exited = "mouse_exited";
		const StringName value_changed = "value_changed";
		const StringName gamestate_updated = "gamestate_updated";
		const StringName mapmode_changed = "mapmode_changed";
		const StringName rotation = "rotation";
		const StringName model = "model";
		const StringName culture = "culture";
		const StringName province_selected = "province_selected";
		const StringName REGION_NAME = "REGION_NAME";
		const StringName DAYS = "DAYS";
		const StringName KPH = "KPH";
		const StringName SFX_BUS = "SFX_BUS";
		const StringName caret_color = "caret_color";
		const StringName focus = "focus";
		const StringName panel = "panel";
		const StringName province = "province";
		const StringName state = "state";
		const StringName index = "index";
		const StringName primary_colour = "primary_colour";
		const StringName secondary_colour = "secondary_colour";
		const StringName tertiary_colour = "tertiary_colour";
		const StringName G_PLUS = "G+";
		const StringName G = "G";
		const StringName R = "R";
		const StringName Y = "Y";
		const StringName Master = "Master";
		const StringName mobilisation_impact_tooltip = "mobilisation_impact_tooltip";
		const StringName is_mobilised = "is_mobilised";
	};
}

// Static Names, prefer if the same string comes up multiple times
#define OV_SNAME(STR) (::OpenVic::StaticStrings::get_singleton()->STR)
