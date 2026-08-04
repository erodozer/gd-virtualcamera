extends Node

func _ready() -> void:
	var loopback = %VirtualCamera.get_devices()
	if loopback.is_empty():
		return
	%VirtualCamera.loopback_device = loopback.get(0).id
