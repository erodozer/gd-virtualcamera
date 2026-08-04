# gd-virtualcamera

V4L2 Loopback bindings for Godot.

Useful for enabling your application to output to a loopback device, for use as a Virtual Webcam or to simply forward image data between applications in Linux.

## Adding to your Project

gd-virtualcamera is a Rust based GDExtension.  Compile it from source or grab an artifact from the github builds

Requires
- v4l2-loopback dkms module and system libraries
- Godot >= 4.7

## Usage

Adds a new `VirtualCamera` Node type that you can add to any scene.  The node will transmit the texture of its closest Viewport in the scenetree to the V4L2 Loopback device specified.

If you wish to enumerate the number of V4L2 Loopback devices available on your machine, you can use the `get_devices()` function on any VirtualCamera instance.

```
var stream = %VirtualCamera
for feed in stream.get_devices():
	dropdown.add_item(feed.name)
	dropdown.set_item_metadata(dropdown.item_count - 1, feed.id)
```

Make sure your loopback accepts 32-bit RGBA data and that your SubViewport's dimensions match the expected resolution.

You can do this with

https://github.com/user-attachments/assets/e8d83431-0099-427e-b414-b947e7ff822b

