extends Node

var bs: BoardshapesData

func _ready():
	bs = BoardshapesData.new()
	
	var data = FileAccess.get_file_as_bytes("res://output.bshapes")
	var start = Time.get_ticks_msec()
	bs.load_from_binary(data)
	print("Time elapsed: %dus" % (Time.get_ticks_msec() - start))
	print(bs.version)
	print(bs.shapes.size())
	for shape in bs.shapes:
		print(shape.number)
		print(shape.color_string)
	
