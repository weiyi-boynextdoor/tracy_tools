#include <iostream>
#include <sstream>
#include <fstream>
#include <string>
#include <string_view>
#include "tracy_lz4.hpp"

// 见TracyFileMeta.hpp
constexpr size_t FileBufSize = 64 * 1024; // 输入块大小
constexpr size_t FileBoundSize = LZ4_COMPRESSBOUND(FileBufSize); // 压缩后输出块上限

class Lz4Writer {
public:
	Lz4Writer(const char* input_file=nullptr, bool append=true) {
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
	void write(const pybind11::bytes& input, bool flush=false) {
		std::string_view view(input);
		auto src = view.data();
		auto size = view.size();
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
	pybind11::bytes dump() {
		if (m_dstlen <= 0 || m_dstlen > FileBoundSize)
			return {};
		return pybind11::bytes(m_dst, m_dstlen);
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
