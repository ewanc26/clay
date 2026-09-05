extends Node

func _ready():
	set_process(true)
	await get_tree().process_frame
	print("Clay GDExtension native node loaded: ", get_class())
	get_tree().quit()
