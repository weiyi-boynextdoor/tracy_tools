from setuptools import setup, Extension
import pybind11

# the c++ extension module
extension_mod = Extension(
	name="tracy_lz4", 
	sources=["pybind11_binding.cpp", "tracy_lz4.cpp"],
	include_dirs=[pybind11.get_include()],
	extra_compile_args=['/std:c++17'], # -std=c++17
)

setup(name="tracy_lz4", ext_modules=[extension_mod])