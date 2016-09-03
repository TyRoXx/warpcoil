from conans import ConanFile

class WarpcoilConan(ConanFile):
    name = "warpcoil"
    version = "0.1"
    generators = "cmake"
    requires = "ventura/0.6@TyRoXx/master", "utfcpp/2.3.4@TyRoXx/stable", "Beast/2016.9.2@TyRoXx/master", "duktape/1.5.0@TyRoXx/stable", "google-benchmark/1.0.0@TyRoXx/stable"
    url="https://github.com/TyRoXx/warpcoil"
    license="MIT"
    exports="warpcoil/*"

    def package(self):
        self.copy(pattern="*.hpp", dst="include/warpcoil", src="warpcoil", keep_path=True)

    def imports(self):
        self.copy("*.dll", dst="bin", src="bin")
        self.copy("*.dylib*", dst="bin", src="lib")
