extends "res://unittest.gd"

func tests():
    testcase("Test1")
    var packet = GDNetPacket.new()
    packet.push_int16(24)
    packet.push_uint32(43)

    var value = packet.pop_int16()
    print("Value ", value)

    assert_eq(value, 24, "pop 24")

    var value = packet.pop_uint32()
    print("Value ", value)

    assert_eq(value, 43, "pop 43")

    #assert_true(true, "Passing assertion")
    #assert_true(false, "Failing assertion")
    endcase()
