a python script to compress data into tracy lz4/zstd format

build dll:
```
python setup.py build
```
example:
```python
import tracy_lz4 as lz4
writer = lz4.Writer(filename, append)
writer.write(b'tracy'*100000)
```