extends SceneTree

var UnitTests = load('res://unittest.gd')

func _init():
    UnitTests.run([
        'res://gdnet_packet.gd'
    ])

    quit()
