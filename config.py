def can_build(env, platform):
	return (platform == "x11" or platform == "server" or platform == "windows" or platform == "osx")  

def configure(env):
	pass
