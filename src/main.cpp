#include <cstdint>
#include <memory>
#include <vector>
#include <iostream>

typedef uint8_t byte;

const uint32_t MAX_SIZE = 0x02000000;

template <uint64_t min_size = 0, uint64_t max_size = 0>
class network_stream
{
	public:
		network_stream();
		~network_stream() = default;
		
		size_t size();
		byte* data();
		
		template <typename T>
		bool write(const T *data, size_t size);
	
	private:
		std::vector<byte> _buf;
};

template <uint64_t min_size, uint64_t max_size>
network_stream<min_size, max_size>::network_stream()
{
	_buf.reserve(min_size);
}

template <uint64_t min_size, uint64_t max_size>
size_t network_stream<min_size, max_size>::size()
{
	return _buf.size();
}

template <uint64_t min_size, uint64_t max_size>
byte* network_stream<min_size, max_size>::data()
{
	return reinterpret_cast<byte*>(_buf.data());
}

template <uint64_t min_size, uint64_t max_size>
template <typename T>
bool network_stream<min_size, max_size>::write(const T *data, const size_t size)
{
	if (max_size > 0 && this->size() + size > max_size)
		return false;
	
	_buf.insert(_buf.end(), data, data + size);
	
	return true;
}

int main()
{
	network_stream<0, MAX_SIZE> ns;
	
	const char *e = "test";
	
	std::cout << ns.write(reinterpret_cast<const byte *>(e), 4) << '\n';
	
	std::cout.write(reinterpret_cast<char*>(ns.data()), static_cast<std::streamsize>(ns.size()));
}