from setuptools import setup, Extension

# the c++ extension module
extension_mod = Extension(
	name="tracy_lz4", 
	sources=["binding.cpp", "tracy_lz4.cpp"],
	include_dirs=["."],
	extra_compile_args=['/std:c++17'], # -std=c++17
)

setup(name="tracy_lz4", ext_modules=[extension_mod])