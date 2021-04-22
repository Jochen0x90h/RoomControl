import subprocess
from cpt.packager import ConanMultiPackager
from conanfile import Project

# linux:
# install conan-package-tools: pip3 install --user conan-package-tools
# upgrade conan-package-tools: pip3 install --upgrade --user conan-package-tools

# macos:
# install conan-package-tools: brew install conan-package-tools

def getVersion():
    branch = subprocess.check_output("git tag -l --points-at HEAD", shell=True).decode()
    if not branch:
        branch = subprocess.check_output("git rev-parse --abbrev-ref HEAD", shell=True).decode()
    return branch.replace("/", "-").replace("(", "_").replace(")", "_")

if __name__ == "__main__":
    version = getVersion()
    builder = ConanMultiPackager(reference = f"{Project.name}/{version}@")
    
    for buildType in ["Debug", "Release"]:
        builder.add(
            settings = {
                "build_type": buildType})
    builder.run()
