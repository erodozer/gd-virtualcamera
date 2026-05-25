#include <cstdlib>
#include <dirent.h>
#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

#include "virtual_camera.h"
#include "godot_cpp/variant/utility_functions.hpp"
#include "godot_cpp/classes/engine.hpp"
#include "godot_cpp/classes/rendering_server.hpp"

#define UNINITIALIZED -1
#define ERR_CODE -1

VirtualCamera::VirtualCamera()
	: output(UNINITIALIZED)
	,camera("") {
}

VirtualCamera::~VirtualCamera() {
	
}

void VirtualCamera::_notification(int p_what) {
    if (p_what == NOTIFICATION_READY && !Engine::get_singleton()->is_editor_hint()) {
        auto _update = callable_mp(this, &VirtualCamera::encode);
		auto rs = RenderingServer::get_singleton();

		this->_using_gl_renderer = rs->get_current_rendering_method() == String("gl_compatibility");

        rs->connect(
            "frame_post_draw",
            _update
        );
    }
    else if (p_what == NOTIFICATION_PREDELETE) {
		
    }
}

void VirtualCamera::set_camera(const String device_id) {
	if (this->output > UNINITIALIZED) {
		close(this->output);
		this->output = UNINITIALIZED;
	}

	this->camera = device_id;
}


void VirtualCamera::connect_to_device() {
	// do not run in editor
	if (Engine::get_singleton()->is_editor_hint()) {
		this->output = UNINITIALIZED;
		return;
	}

	if (this->camera.is_empty()) {
		this->output = UNINITIALIZED;
		return;
	}

	auto vp = this->get_viewport();
	ERR_FAIL_NULL_MSG(vp, "Virtual Camera Sender must belong to a Viewport.");

	auto img = vp->get_texture()->get_image();
	ERR_FAIL_NULL_MSG(img, "Virtual Camera vp image is empty.");

	// not fully configured
	ERR_FAIL_NULL_MSG(this->get_viewport(), "Virtual Camera Sender must belong to a Viewport.");

	struct v4l2_format v;

	int dev_fd = open(this->camera.utf8(), O_RDWR);

	ERR_FAIL_COND_MSG(dev_fd == ERR_CODE, "Could not connect to v4l2 loopback device");

	v.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

	ERR_FAIL_COND_MSG(ioctl(dev_fd, VIDIOC_G_FMT, &v) == ERR_CODE, "Could not setup v4l2 loopback device");
	
	v.fmt.pix.width = img->get_width();
	v.fmt.pix.height = img->get_height();
	v.fmt.pix.pixelformat = V4L2_PIX_FMT_RGBA32;
	v.fmt.pix.sizeimage = 4 * img->get_width() * img->get_height();
	v.fmt.pix.field = V4L2_FIELD_NONE;
	print_line("output ", img->get_width(), "x", img->get_height(), " rgba");
	ERR_FAIL_COND_MSG(ioctl(dev_fd, VIDIOC_S_FMT, &v) == ERR_CODE, "Could not setup v4l2 loopback device");

	print_line("V4L2 device connected ", this->camera);

	this->output = dev_fd;
}

void VirtualCamera::encode() {
	if (this->output == UNINITIALIZED) {
		this->connect_to_device();
		if (this->output == UNINITIALIZED) {
			this->camera = "";
		}
		return;
	}

	auto vp = this->get_viewport();
	ERR_FAIL_NULL_MSG(vp, "Virtual Camera Sender must belong to a Viewport.");

	auto img = vp->get_texture()->get_image();
	ERR_FAIL_NULL_MSG(img, "Virtual Camera vp image is empty.");

	// make sure the data is arranged in the correct format
	img->convert(Image::FORMAT_RGBA8);

	auto data = img->get_data();  // note: this costs CPU cycles as it creates a copy
	auto frame_size = img->get_data_size();
	

	// swizzle the bytes, since OpenGL RGBA does not match V4L2 RGBA
	if (this->_using_gl_renderer) {
		for (int i = 0; i < frame_size; i += 4) {
			auto r = data[i];
			auto g = data[i+1];
			auto b = data[i+2];
			auto a = data[i+3];

			data[i] = b;
			data[i+1] = g;
			data[i+2] = r;
			data[i+3] = a;
		}
	}

	auto frame = data.ptrw();
	
	write(this->output, frame, frame_size);
}

bool try_connect(String camera) {
	struct v4l2_format v;
	int dev_fd = open(camera.utf8(), O_RDWR);
	if (dev_fd == ERR_CODE) {
		// Could not connect to v4l2 device
		return false;
	}

	v.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;
	auto success = ioctl(dev_fd, VIDIOC_G_FMT, &v);
	close(dev_fd);

	if (success == ERR_CODE) {
		// v4l2 device could not take output (is not loopback)
		return false;
	}

	return true;
}

// simplied version of CameraServer get feeds that is filtered to only v4l2 loopback/output devices
Array VirtualCamera::get_devices() {
	Array out;

	struct dirent **devices;
	int count = scandir("/dev", &devices, nullptr, alphasort);

	if (count != -1) {
		for (int i = 0; i < count; i++) {
			struct dirent *device = devices[i];
			String device_name = String("/dev/") + String(device->d_name);
			std::free(device);

			if (strncmp(device->d_name, "video", 5) != 0) {
				continue;
			}
			
			String name;
			int file_descriptor = open(device_name.ascii().get_data(), O_RDWR | O_NONBLOCK, 0);
			if (file_descriptor != ERR_CODE) {
				struct v4l2_capability capability;
				if (ioctl(file_descriptor, VIDIOC_QUERYCAP, &capability) != ERR_CODE) {
					name = String((char *)capability.card);	
				}
			}
			close(file_descriptor);

			if (name.is_empty()) {
				continue;
			}

			if (try_connect(device_name)) {
				Dictionary d;
				d["name"] = name;
				d["id"] = device_name;

				out.append(d);
			}
		}
	}

	std::free(devices);

	return out;
}
