#include <stdio.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/videodev2.h>

#include "virtual_camera.h"
#include "godot_cpp/variant/utility_functions.hpp"
#include "godot_cpp/classes/engine.hpp"

VirtualCamera::VirtualCamera()
	: source(nullptr)
	,output(-1)
	,camera("") {

}

VirtualCamera::~VirtualCamera() {
	if (this->output != -1 ) {
		// clear the virtual webcam on close
		auto buf = PackedByteArray();
		buf.resize(this->frame_size);
		write(this->output, buf.ptrw(), frame_size);
	}
}

int32_t VirtualCamera::_connect_to_device() {
	// do not run in editor
	if (Engine::get_singleton()->is_editor_hint()) {
		return -1;
	}

	// not fully configured
	if (this->get_viewport() == nullptr || this->camera.is_empty()) {
		return -1;
	}

	struct v4l2_format v;

	int dev_fd = open(this->camera.utf8(), O_RDWR);

	ERR_FAIL_COND_V_MSG(dev_fd == -1, -1, "Could not connect to v4l2 loopback device");

	v.type = V4L2_BUF_TYPE_VIDEO_OUTPUT;

	ERR_FAIL_COND_V_MSG(ioctl(dev_fd, VIDIOC_G_FMT, &v) == -1, -1, "Could not setup v4l2 loopback device");
	
	v.fmt.pix.width = this->frame_width;
	v.fmt.pix.height = this->frame_height;
	v.fmt.pix.pixelformat = V4L2_PIX_FMT_RGBA32;
	v.fmt.pix.sizeimage = this->frame_size;
	v.fmt.pix.field = V4L2_FIELD_NONE;
	ERR_FAIL_COND_V_MSG(ioctl(dev_fd, VIDIOC_S_FMT, &v) == -1, -1, "Could not setup v4l2 loopback device");

	print_line("V4L2 device connected");
	return dev_fd;
}

void VirtualCamera::_process(double _delta) {
	// do not run in editor
	if (Engine::get_singleton()->is_editor_hint()) {
		return;
	}

	if (this->source == nullptr || this->output == -1) {
		return;
	}

	auto img = this->source->get_texture()->get_image();
	img->convert(Image::FORMAT_RGBA8);
	auto frame = img->get_data().ptrw();
	auto frame_size = img->get_data_size();

	write(this->output, frame, frame_size);
}
