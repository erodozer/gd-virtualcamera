use godot::prelude::*;
use godot::classes::RenderingServer;
use godot::classes::Viewport;

use v4l::prelude::*;
use v4l::video::Capture;
use v4l::capability::Flags;

#[derive(GodotClass)]
#[class(init, base=Node)]
pub struct VirtualCamera {
    base: Base<Node>,
	#[export]
	pub loopback_device: GString,
    device: Option<Device>
}

#[godot_api]
impl VirtualCamera {
    fn bind_to_viewport(&mut self, viewport: Gd<Viewport>) -> Result<Device, &str> {
        let rect = viewport.get_visible_rect();

        let dev = Device::with_path(self.loopback_device.to_string()).expect("unable to open device path");
        let caps = dev.query_caps().expect("unable to query device capabilities");

        if !caps.capabilities.contains(Flags::VIDEO_OUTPUT) {
            return Err("device is not capable of video output (not a loopback device)");
        }

        dev.set_format(
            &v4l::Format::new(
                rect.size.x as u32,
                rect.size.y as u32,
                v4l::FourCC::new(b"RGBA")
            )
        ).expect("unable to set loopback device video format");

        Ok(dev)
    }

    fn update_camera(&mut self) {
        if let Some(dev) = &self.device {
            let vp = self.base().get_viewport().expect("viewport is missing");
            let vp_tex = vp.get_texture().expect("unable to read viewport texture");
            let img = vp_tex.get_image().expect("viewport missing texture");
            let data = img.get_data();
            unsafe {
                libc::write(
                    dev.handle().fd(), 
                    data.to_vec().as_ptr() as *const libc::c_void,
                    img.get_data_size() as usize
                );
            }
        }
    }

    #[func]
    fn get_loopback_devices(&self) -> Array<GString> {
        array![]
    }
}

#[godot_api]
impl INode for VirtualCamera {
    fn ready(&mut self) {
        if !self.loopback_device.is_empty() {
            let vp = self.base().get_viewport().expect("node is not within viewport");
            if let Ok(dev) = self.bind_to_viewport(vp) {
                self.device = Some(dev);
            }
        }

        RenderingServer::singleton().signals().frame_post_draw().connect_other(self, VirtualCamera::update_camera);
    }

    fn on_set(&mut self, property: StringName, value: Variant) -> bool {
        if property == "loopback_device" {
            if let Ok(device) = value.try_to::<GString>() {
                self.loopback_device = device;
                let vp = self.base().get_viewport().expect("node is not within viewport");
                if let Ok(dev) = self.bind_to_viewport(vp) {
                    self.device = Some(dev);
                    return true;
                }
            }
        }

        return false;
    }
}