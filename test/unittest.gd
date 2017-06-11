# Simple unit testing framework loosely modeled on Python's unittest class

# The MIT License (MIT)
#
# Copyright (c) 2016 michaelb
#
# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files (the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions:
#
# The above copyright notice and this permission notice shall be included in
# all copies or substantial portions of the Software.
#
# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
# THE SOFTWARE.

var ut_success = true
var ut_all_success = true
var ut_failures = 0
var ut_successes = 0
var ut_tests = 0
var ut_current_case = ""

func assert_true(a, message):
    if not a:
        ut_success = false
        print("- [FAIL] ", ut_current_case, " '", a, "' is false. Assertion failed: ", message)

func assert_false(a, message):
    if a:
        ut_success = false
        print("- [FAIL] ", ut_current_case, " '", a, "' is true. Assertion failed: ", message)

func assert_gt(a, b, message):
    if not a > b:
        ut_success = false
        print("- [FAIL] ", ut_current_case, " '", a, "' is not > than '", b, "'. Assertion failed: ", message)

func assert_lt(a, b, message):
    if not a < b:
        ut_success = false
        print("- [FAIL] ", ut_current_case, " '", a, "' is not < than '", b, "'. Assertion failed: ", message)

func assert_eq(a, b, message):
    if not a == b:
        ut_success = false
        print("- [FAIL] ", ut_current_case, " '", a, "' != '", b, "'. Assertion failed: ", message)

func assert_ne(a, b, message):
    if not a != b:
        ut_success = false
        print("- [FAIL] ", ut_current_case, " '", a, "' == '", b, "'. Assertion failed: ", message)

func assert_array_equal(a, b, message):
    if not a.size() == b.size():
        ut_success = false
        print("- [FAIL] ", ut_current_case, " array '", a, "' has a different size than array '", b, "'. Assertion failed: ", message)
        return
    for i in range(a.size()):
        if not a[i] == b[i]:
            ut_success = false
            print("- [FAIL] ", ut_current_case, " array '", a, "' has a different values than array '", b, "'. Assertion failed: ", message)
            return

func assert_dict_equal(a, b, message):
    if not a.size() == b.size():
        ut_success = false
        print("- [FAIL] ", ut_current_case, " dict '", a, "' has a different size than dict '", b, "'. Assertion failed: ", message)
        return
    for key in a:
        if not b.has(key):
            ut_success = false
            print("- [FAIL] ", ut_current_case, " dict '", a, "' has a key ", key, " that is not present in dict '", b, "'. Assertion failed: ", message)
            return
        if not a[key] == b[key]:
            ut_success = false
            print("- [FAIL] ", ut_current_case, " dict '", a, "' has a different values than dict '", b, "'. Assertion failed: ", message)
            return
    for key in b:
        if not a.has(key):
            ut_success = false
            print("- [FAIL] ", ut_current_case, " second dict '", b, "' has a key ", key, " that is not present in dict '", a, "'. Assertion failed: ", message)
            return

func testcase(name):
    ut_success = true
    setup()
    ut_tests += 1
    ut_current_case = name

func endcase():
    teardown()
    ut_all_success = ut_all_success and ut_success
    if not ut_success:
        return
    else:
        # print("[SUCCESS] Test " + ut_current_case + ".")
        pass
    ut_success = true
    ut_successes += 1

func setup():
    pass

func teardown():
    pass

func run_test(path):
    tests()
    var munged_path = path.replace(".gd", "").replace("res://", "").replace("tests", "")
    if ut_all_success:
        print ("- [OK] ", ut_successes, "/", ut_tests, " - ", munged_path)
    else:
        print ("- [FAILURE] Testing failed: ", munged_path)
    return ut_all_success

static func run(paths):
    var all_success = true
    var count = 0
    var successes = 0
    for path in paths:
        count += 1
        if path.match('res://*.gd'):
            pass # path is already good
        else:
            # Assume is a "." style
            path = "res://" + path.replace(".", "/") + ".gd"

        # Run module
        if load(path).new().run_test(path):
            successes += 1
        else:
            all_success = false

    if all_success:
        print("- [SUCCESS] ", successes, "/", count)
    else:
        print("- [FAILURES] ", successes, "/", count)
