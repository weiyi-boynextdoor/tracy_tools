#include <Python.h>
#include <pybind11/pybind11.h>
#include "lz4_writer.h"

namespace py = pybind11;

pybind11::bytes lz4_compress(const pybind11::bytes& input)
{
	std::string_view src(input);

	char dst[1000];

	int dst_len = tracy::LZ4_compress_default(src.data(), dst, src.size(), 1000);

	return pybind11::bytes(dst, dst_len);
}

PYBIND11_MODULE(tracy_lz4, m) {
	m.doc() = "Tracy LZ4 compress";
	m.def("compress", &lz4_compress, "compress a short string, just for testing");
	py::class_<Lz4Writer>(m, "Writer")
        .def(py::init<const char*, bool>(), py::arg("filename")=nullptr, py::arg("append")=true)
        .def("write", &Lz4Writer::write, py::arg("input"), py::arg("flush")=false)
        .def("dump", &Lz4Writer::dump);
}
