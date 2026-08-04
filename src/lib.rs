use godot::prelude::*;

pub mod camera;

struct VirtualCamExtension;

#[gdextension]
unsafe impl ExtensionLibrary for VirtualCamExtension {

}

