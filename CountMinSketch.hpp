#pragma once
#include <vector>
#include <random>
#include <chrono>

class CountMinRow {
public:
	explicit CountMinRow(size_t countNum) 
		: _bits((countNum < 8 ? 8 : countNum) / 8, 0)
	{}

private:
	friend class CountMinSketch;
	std::vector<int> _bits;

	int get(int num) const {
		int countIndex = num / 8;
		int countOffset = num % 8 * 4;

		return (_bits[countIndex] >> countOffset) & 0x7;
	}


	//增加计数
	void increment(int num) {
		int countIndex = num / 8;
		int countOffset = num % 8 * 4;

		int value = (_bits[countIndex] >> countOffset) & 0x7;

		if (value < 15) {
			_bits[countIndex] += (1 << countOffset);
		}
	}

	void clear() {
		for (auto& it : _bits) {
			it = 0;
		}
	}

	//保鲜算法，使计数值减半
	void reset() {
		for (auto& it : _bits) {
			it = (it >> 1) & 0x77777777;
		}
	}
};

class CountMinSketch {
public:
	explicit CountMinSketch(size_t countNum)
		: _seed(4)
	{
		countNum = next2Power(countNum);
		if (countNum < 8) {
			countNum = 8;
		}
		_cmRows.resize(4, CountMinRow(countNum));
		_mask = countNum - 1;


		//利用当前时间作为generator的seed
		unsigned time = std::chrono::system_clock::now().time_since_epoch().count();
		std::mt19937 generator(time);
		for (int i = 0; i < COUNT_MIN_SKETCH_DEPTH; i++) {
			//随机生成一个32位seed
			generator.discard(generator.state_size);
			_seed[i] = generator();
		}
	}

	//选择几个cmRow最小的一个作为有效值返回
	int getCountMin(uint32_t hash) {
		int min = 16, value = 0;
		for (int i = 0; i < _cmRows.size(); i++) {
			value = _cmRows[i].get((hash ^ _seed[i]) & _mask);

			min = (value < min) ? value : min;
		}
		return min;
	}

	void Increment(uint32_t hash) {
		for (int i = 0; i < _cmRows.size(); i++) {
			_cmRows[i].increment((hash ^ _seed[i]) & _mask);
		}
	}

	void Reset() {
		for (auto& cmRow : _cmRows) {
			cmRow.reset();
		}
	}

	void Clear() {
		for (auto& cmRow : _cmRows) {
			cmRow.clear();
		}
	}

private:
	std::vector<CountMinRow> _cmRows;
	std::vector<uint32_t> _seed;	//用于打散哈希值
	uint32_t _mask;					//掩码用于取低n位

	static const int COUNT_MIN_SKETCH_DEPTH = 4;

	//用于计算下一个最接近x的二次幂
	int next2Power(int x) {
		x--;
		x |= x >> 1;
		x |= x >> 2;
		x |= x >> 4;
		x |= x >> 8;
		x |= x >> 16;
		x |= x >> 32;
		x++;
		return x;
	}
};
