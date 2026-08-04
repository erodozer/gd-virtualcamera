debug:
	cargo build
	cp target/debug/libgd_virtualcamera.so addons/virtualcamera/lib/libgd_virtualcamera.debug.so

release:
	cargo build --release
	cp target/release/libgd_virtualcamera.so addons/virtualcamera/lib/libgd_virtualcamera.release.so

package: debug | release
