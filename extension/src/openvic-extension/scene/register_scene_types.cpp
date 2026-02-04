#include "register_scene_types.hpp"

#include <godot_cpp/classes/ref.hpp>
#include <godot_cpp/classes/resource_loader.hpp>
#include <godot_cpp/core/class_db.hpp>

#include "openvic-extension/scene/3d/MapInstance3D.hpp"
#include "openvic-extension/scene/resources/ResourceFormatLoaderXSM.hpp"
#include "openvic-extension/scene/resources/UnitModel.hpp"
#include "openvic-extension/scene/resources/XACMesh.hpp"
#include "openvic-extension/scene/resources/XACModel.hpp"
#include "openvic-extension/scene/resources/XACSkeleton.hpp"

using namespace OpenVic;
using namespace godot;

static Ref<ResourceFormatLoaderXSM> _resource_format_loader_xsm;
static Ref<ResourceFormatLoaderXAC> _resource_format_loader_xac;
static Ref<ResourceFormatLoaderUnitModel> _resource_format_loader_unit_model;

void OpenVic::register_scene_types() {
	GDREGISTER_INTERNAL_CLASS(ResourceFormatLoaderXSM);
	GDREGISTER_INTERNAL_CLASS(ResourceFormatLoaderXAC);
	GDREGISTER_INTERNAL_CLASS(ResourceFormatLoaderUnitModel);

	GDREGISTER_CLASS(XACSkeleton);
	GDREGISTER_CLASS(XACMesh);
	GDREGISTER_CLASS(XACModel);
	GDREGISTER_CLASS(UnitModel);
	GDREGISTER_CLASS(MapInstance3D);

	_resource_format_loader_xsm.instantiate();
	ResourceLoader::get_singleton()->add_resource_format_loader(_resource_format_loader_xsm);

	_resource_format_loader_xac.instantiate();
	ResourceLoader::get_singleton()->add_resource_format_loader(_resource_format_loader_xac);

	_resource_format_loader_unit_model.instantiate();
	ResourceLoader::get_singleton()->add_resource_format_loader(_resource_format_loader_unit_model, true);
}

void OpenVic::unregister_scene_types() {
	ResourceLoader::get_singleton()->remove_resource_format_loader(_resource_format_loader_unit_model);
	_resource_format_loader_unit_model.unref();

	ResourceLoader::get_singleton()->remove_resource_format_loader(_resource_format_loader_xac);
	_resource_format_loader_xac.unref();

	ResourceLoader::get_singleton()->remove_resource_format_loader(_resource_format_loader_xsm);
	_resource_format_loader_xsm.unref();
}

void OpenVic::startup_scene_loop() {
	ResourceFormatLoaderXAC::_on_startup_mainloop();
}
