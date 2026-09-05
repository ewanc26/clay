extends Node

func _ready():
	print("Clay GDExtension native node loaded: ", get_class())
	get_tree().quit()
