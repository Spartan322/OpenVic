extends Node

@export var _map_view : MapView
@export var _model_manager : ModelManager
@export var _game_session_menu : Control

func _ready() -> void:
	Events.Options.load_settings_from_file()
	if GameSingleton.start_game_session() != OK:
		push_error("Failed to setup game")

	_model_manager.generate_units()
	_model_manager.generate_buildings()
	MusicConductor.generate_playlist()
	MusicConductor.select_next_song()
	# In game, the province selector uses the normal glove cursor.
	CursorManager.set_compat_cursor(&"normal", Input.CURSOR_IBEAM)

func _process(_delta : float) -> void:
	GameSingleton.update_clock()

# REQUIREMENTS:
# * SS-42
func _on_game_session_menu_button_pressed() -> void:
	_game_session_menu.visible = !_game_session_menu.visible

func _on_map_view_ready() -> void:
	# Set the camera's starting position
	_map_view._camera.position = _map_view._map_to_world_coords(
		# Start at the player country's capital position (when loading a save game in the lobby or entering the actual game)
		GameSingleton.get_viewed_country_capital_position()
	)

func _on_map_view_province_hovered(index: int) -> void:
	_map_view.set_hovered_province_index(index)

func _on_map_view_province_unhovered() -> void:
	_map_view.unset_hovered_province()

func _on_map_view_province_clicked(index: int) -> void:
	GameSingleton.set_selected_province(index)

func _on_map_view_province_right_clicked(index: int) -> void:
	# TODO - open diplomacy screen on province owner or viewed country if province has no owner
	#Events.NationManagementScreens.open_nation_management_screen(NationManagement.Screen.DIPLOMACY)
	GameSingleton.set_viewed_country_by_province_index(index)
