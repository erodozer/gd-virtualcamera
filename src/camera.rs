use std::fs::OpenOptions;
use std::io::Write;

use godot::prelude::*;
use godot::classes::RenderingServer;
use godot::classes::Viewport;
use godot::classes::Engine;

use v4l::prelude::*;
use v4l::v4l_sys::v4l2_format;
use v4l::v4l_sys::v4l2_format__bindgen_ty_1;
use v4l::v4l2;
use v4l::capability::Flags;

#[derive(GodotClass)]
#[class(init, base=Node)]
pub struct VirtualCamera {
    base: Base<Node>,
	#[export]
	pub loopback_device: GString,
    is_valid_device: bool,
}

#[godot_api]
impl VirtualCamera {
    fn bind_to_viewport(&mut self, viewport: Gd<Viewport>) -> Result<(), &str> {
        if Engine::singleton().is_editor_hint() {
            return Err("virtual cameras can not be initialized within the editor");
        }

        let rect = viewport.get_visible_rect();

        let dev = Device::with_path(self.loopback_device.to_string()).expect("unable to open device path");
        let caps = dev.query_caps().expect("unable to query device capabilities");

        if !caps.capabilities.contains(Flags::VIDEO_OUTPUT) {
            return Err("device is not capable of video output (not a loopback device)");
        }

        // built in set_format on Devices is strictly set to be for type VideoCapture
        // so we have to manually replicate functionality and target VideoOutput instead
        let result = unsafe {
            let mut fmt = v4l::Format::new(
                rect.size.x as u32,
                rect.size.y as u32,
                v4l::FourCC::new(b"AB24"),
            );
            fmt.field_order = v4l::format::FieldOrder::Progressive;
            fmt.size = 4 * (rect.size.x as u32) * (rect.size.y as u32);
            let mut v4l2_fmt = v4l2_format {
                type_: v4l::buffer::Type::VideoOutput as u32,
                fmt: v4l2_format__bindgen_ty_1 { 
                    pix: fmt.into() 
                },
            };
            v4l2::ioctl(
                dev.handle().fd(),
                v4l2::vidioc::VIDIOC_S_FMT,
                &mut v4l2_fmt as *mut _ as *mut std::os::raw::c_void,
            )
        };

        if let Err(_) = result {
            return Err("unable to set video format");
        }

        Ok(())
    }

    fn update_camera(&mut self) {
        if !self.is_valid_device {
            return;
        }

        let data = self.base().get_viewport()
            .and_then(|vp| vp.get_texture())
            .and_then(|tex| tex.get_image())
            .map(|img| img.get_data());

        if let Some(data) = data {
            let result = OpenOptions::new().write(true).open(self.loopback_device.to_string())
                .and_then(|mut f| f.write(data.as_slice()));

            if let Err(e) = result {
                self.is_valid_device = false;
                godot_error!("{}", e);
            }
        }        
    }

    #[func]
    fn get_devices(&self) -> Array<VarDictionary> {
        Array::from_iter(
            std::fs::read_dir("/dev").expect("unable to enumerate /dev directory")
                .into_iter()
                .filter_map(|entry| {
                    let path_buf = entry.ok()?.path();
                    let path = path_buf.to_str()?;
                    if path.starts_with("/dev/video") {
                        let dev = Device::with_path(path).ok()?;
                        let cap = dev.query_caps().ok()?;
                        if cap.capabilities.contains(Flags::VIDEO_OUTPUT) {
                            return Some(dict!{
                                "name" => &cap.card.to_gstring(),
                                "id" => &path.to_gstring()
                            });
                        }
                    }
                    return None;
                }
            )
        )
    }
}

#[godot_api]
impl INode for VirtualCamera {
    fn ready(&mut self) {
        if !self.loopback_device.is_empty() {
            let vp = self.base().get_viewport().expect("node is not within viewport");
            let binding = self.bind_to_viewport(vp);
            if let Ok(_) = binding {
                self.is_valid_device = true;
            } else if let Err(msg) = binding {
                godot_error!("{}", msg)
            }
        }

        if !Engine::singleton().is_editor_hint() {
            RenderingServer::singleton().signals()
                .frame_post_draw()
                .connect_other(self, VirtualCamera::update_camera);
        }
    }

    fn on_set(&mut self, property: StringName, value: Variant) -> bool {
        if property == "loopback_device" {
            if let Ok(device) = value.try_to::<GString>() {
                self.loopback_device = device.clone();
                
                // clear value
                if device.is_empty() {
                    self.is_valid_device = false;
                    return true;
                }

                let vp = self.base().get_viewport().expect("node is not within viewport");
                let binding = self.bind_to_viewport(vp);
                if let Ok(_) = binding { 
                    self.is_valid_device = true;
                    return true;
                } else if let Err(msg) = binding {
                    godot_error!("{}", msg)
                }
            }
        }

        return false;
    }
}