from __future__ import print_function
Import("env")
import os
try:
    import configparser
except ImportError:
    import ConfigParser as configparser

def prepareImage(version, buildDir, progName, env_name, outputNames):
    print("{} {} {} {}".format(version, buildDir, progName, outputNames))
    command=[]
    directory="{}/../../deployable".format(buildDir)
    if not os.path.exists(directory):
        print("Creating deployable directory at:{}".format(directory))
        os.makedirs(directory)
    for name in outputNames:
        with open("{}/{}.version".format(directory, name), "w") as text_file:
            print(version, end="", file=text_file)
        executeString="cp {}/{}/{}.bin {}/{}.bin".format(buildDir, env_name, progName, directory, name)
        command+=env.VerboseAction(executeString, "Copy {} bin file to {}".format(name, directory))
    return env.VerboseAction(command, "Preparing version files")

config = configparser.ConfigParser()
config.read("platformio.ini")
env_name = str(env["PIOENV"])
outputNamesRaw = config.get("env:"+env_name,"custom_outputNames")
if outputNamesRaw is None:
    print("No custom names found")
else:
    outputNames=outputNamesRaw.split(" ")
    print(outputNames)
    my_flags = env.ParseFlags(env['BUILD_FLAGS'])
    defines = {k: v for (k, v) in my_flags.get("CPPDEFINES")}
    version="{}.{}.{}".format(defines['VER_MAJ'],defines['VER_MIN'],defines['VER_PAT'])
    print("Version:{}".format(version))
    env.AddPostAction(
        "$BUILD_DIR/${PROGNAME}.bin",
        prepareImage(version, env['PROJECT_BUILD_DIR'], env['PROGNAME'], env_name, outputNames)
    )
