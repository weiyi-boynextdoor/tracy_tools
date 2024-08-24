#include <Python.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include "tracy_lz4.hpp"

// 见TracyFileMeta.hpp
constexpr size_t FileBufSize = 64 * 1024; // 输入块大小
constexpr size_t FileBoundSize = LZ4_COMPRESSBOUND(FileBufSize); // 压缩后输出块上限

class Lz4Writer {
public:
	Lz4Writer(const char* input_file, bool append=true) {
		m_src = new char[FileBufSize];
		m_dst = new char[FileBoundSize];
		if (input_file && std::strlen(input_file)) {
			auto mode = std::fstream::out | std::fstream::binary;
			if (append)
				mode |= std::fstream::app;
			m_fs.open(input_file, mode);
		}
	}

	~Lz4Writer() {
		if (m_offset > 0) {
			// dump buffer
			compress(m_src, m_offset);
		}
		m_fs.close();
		delete[] m_src;
		delete[] m_dst;
	}

	// 一般缓冲区满了再压缩输出
	// flush为true时就算缓存区不满也强制压缩
	// tracy里是多线程压缩，我们就不管了吧
	void write(const char* src, size_t size, bool flush=false) {
		if (m_offset + size < FileBufSize) {
			if (flush) {
				memcpy(m_src, src, size);
				m_offset += size;
				compress(m_src, m_offset);
				m_offset = 0;
			} else {
				memcpy(m_src + m_offset, src, size);
				m_offset += size;
			}
			
		} else {
			while (m_offset + size >= FileBufSize) {
				size_t delta = FileBufSize - m_offset;
				memcpy(m_src + m_offset, src, delta);
				compress(m_src, FileBufSize);
				src += delta;
				size -= delta;
				m_offset = 0;
			}
			if (size > 0) {
				if (flush) {
					memcpy(m_src, src, size);
					m_offset += size;
					compress(m_src, m_offset);
					m_offset = 0;
				} else {
					memcpy(m_src + m_offset, src, size);
					m_offset += size;
				}
			}
		}
	}

	// 调试用，看最近一个block压缩结果
	std::string_view dump() {
		if (m_dstlen <= 0 || m_dstlen > FileBoundSize)
			return {};
		return std::string_view(m_dst, m_dstlen);
	}

private:
	int compress(const char* src, int size) {
		int len = tracy::LZ4_compress_default(src, m_dst, size, FileBoundSize);
		m_dstlen = len;
		if (len <= 0) {
			return len;
		}
		else {
			if (m_fs) {
				m_fs.write(reinterpret_cast<char*>(&len), 4);
				m_fs.write(m_dst, m_dstlen);
			}
		}
		return m_dstlen;
	}

private:
	// output to file
	std::ofstream m_fs;
	// input buffer
	char *m_src{};
	size_t m_offset{};
	// output buffer
	char *m_dst{};
	size_t m_dstlen{};
};

struct Py_Lz4Writer {
	PyObject_HEAD
	Lz4Writer* stream{};
};

static PyObject* Lz4Writer_init(PyTypeObject* cls, PyObject* args, PyObject* kwds) {
	static const char *kwlist[] = {"filename", "append", nullptr};
	const char* filename = nullptr;
	bool append = false;
	if (!PyArg_ParseTupleAndKeywords(args, kwds, "|sb", (char**)kwlist, &filename, &append)) {
		return nullptr;
	}
	auto self = (Py_Lz4Writer*)cls->tp_alloc(cls, 0);
	self->stream = new Lz4Writer(filename, append);
	return (PyObject*)self;
}

static void Lz4Writer_dealloc(Py_Lz4Writer *self)
{
  delete self->stream;
  Py_TYPE(self)->tp_free((PyObject *)self);
}

static PyObject *Lz4Writer_write(Py_Lz4Writer *self, PyObject *args) {
	Py_buffer view;
	bool flush = false;
	if (!PyArg_ParseTuple(args, "y*|b", &view, &flush))
		return NULL;
	self->stream->write((const char*)view.buf, view.len, flush);
	Py_RETURN_NONE;
}

static PyObject *Lz4Writer_dump(Py_Lz4Writer *self) {
	std::string str(self->stream->dump());
	return PyBytes_FromStringAndSize(str.c_str(), str.size());
}

static PyMethodDef Lz4Writer_methods[] = {
	{"write", (PyCFunction)Lz4Writer_write, METH_VARARGS, nullptr},
	{"dump", (PyCFunction)Lz4Writer_dump, METH_NOARGS, nullptr},
    {nullptr, nullptr, 0, nullptr},
};

PyTypeObject Lz4Writer_Type = {
    /*ob_base*/ PyVarObject_HEAD_INIT(nullptr, 0)
    /*tp_name*/ "Stream",
    /*tp_basicsize*/ sizeof(Py_Lz4Writer),
    /*tp_itemsize*/ 0,
    /*tp_dealloc*/ (destructor)Lz4Writer_dealloc,
    /*tp_vectorcall_offset*/ 0,
    /*tp_getattr*/ nullptr,
    /*tp_setattr*/ nullptr,
    /*tp_as_async*/ nullptr,
    /*tp_repr*/ nullptr,
    /*tp_as_number*/ nullptr,
    /*tp_as_sequence*/ nullptr,
    /*tp_as_mapping*/ nullptr,
    /*tp_hash*/ nullptr,
    /*tp_call*/ nullptr,
    /*tp_str*/ nullptr,
    /*tp_getattro*/ nullptr,
    /*tp_setattro*/ nullptr,
    /*tp_as_buffer*/ nullptr,
    /*tp_flags*/ Py_TPFLAGS_DEFAULT,
    /*tp_doc*/ nullptr,
    /*tp_traverse*/ nullptr,
    /*tp_clear*/ nullptr,
    /*tp_richcompare*/ nullptr,
    /*tp_weaklistoffset*/ 0,
    /*tp_iter*/ nullptr,
    /*tp_iternext*/ nullptr,
    /*tp_methods*/ Lz4Writer_methods,
    /*tp_members*/ nullptr,
    /*tp_getset*/ nullptr,
    /*tp_base*/ nullptr,
    /*tp_dict*/ nullptr,
    /*tp_descr_get*/ nullptr,
    /*tp_descr_set*/ nullptr,
    /*tp_dictoffset*/ 0,
    /*tp_init*/ 0,
    /*tp_alloc*/ nullptr,
    /*tp_new*/ Lz4Writer_init,
};

int Lz4Writer_Init(PyObject *module)
{
  if (module == nullptr) {
    return -1;
  }

  if (PyType_Ready(&Lz4Writer_Type) < 0) {
    return -1;
  }

  Py_INCREF(&Lz4Writer_Type);
  PyModule_AddObject(module, "Writer", (PyObject *)&Lz4Writer_Type);
  return 0;
}

static PyObject* lz4_compress(PyObject *self, PyObject *args)
{
	Py_buffer view;
	if (!PyArg_ParseTuple(args, "y*", &view))
		return NULL;

	char dst[1000];

	int dst_len = tracy::LZ4_compress_default((const char*)view.buf, dst, view.len, 1000);

	return PyBytes_FromStringAndSize(dst, dst_len);
}

static PyMethodDef LZ4_Methods[] = {
	{"compress", lz4_compress, METH_VARARGS, "compress a string"},
	{NULL, NULL, 0, NULL}        /* Sentinel */
};

PyDoc_STRVAR(
	TRACY_LZ4_doc,
	"This module provides access to blenders bmesh data structures.\n");

static struct PyModuleDef LZ4_MODULE = {
	/*m_base*/ PyModuleDef_HEAD_INIT,
	/*m_name*/ "tracy_lz4",
	/*m_doc*/ TRACY_LZ4_doc,
	/*m_size*/ -1,
	/*m_methods*/ LZ4_Methods,
	/*m_slots*/ nullptr,
	/*m_traverse*/ nullptr,
	/*m_clear*/ nullptr,
	/*m_free*/ nullptr,
};

PyObject* PyInit_tracy_lz4(void)
{
	PyObject *mod = PyModule_Create(&LZ4_MODULE);
	Lz4Writer_Init(mod);
	return mod;
}