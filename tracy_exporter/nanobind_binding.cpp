#include <nanobind/nanobind.h>
#include "lz4_writer.h"

namespace nb = nanobind;

pybind11::bytes lz4_compress(const pybind11::bytes& input)
{
	std::string_view src(input);

	char dst[1000];

	int dst_len = tracy::LZ4_compress_default(src.data(), dst, src.size(), 1000);

	return pybind11::bytes(dst, dst_len);
}

NB_MODULE(tracy_lz4, m) {
	m.doc() = "Tracy LZ4 compress";
	m.def("compress", &lz4_compress, "compress a short string, just for testing");
	nb::class_<Lz4Writer>(m, "Writer")
        .def(nb::init<const char*, bool>(), nb::arg("filename")=nullptr, nb::arg("append")=true)
        .def("write", &Lz4Writer::write, nb::arg("input"), nb::arg("flush")=false)
        .def("dump", &Lz4Writer::dump);
}
