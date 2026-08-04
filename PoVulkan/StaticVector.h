#pragma once

#include <array>
#include <stdexcept>
#include <string>

// a pool <reserve> num of inline <T>s with vector like semantics for adding and removing.
// removing does not actually destruct
template<typename T, size_t reserve>
class CStaticVector
{
	std::array<T, reserve> mData;
	size_t mCount = 0;

public:
	using array_type = typename std::array<T, reserve>;
	using value_type = typename std::array<T, reserve>::value_type;
	using size_type = typename std::array<T, reserve>::size_type;
	using difference_type = typename std::array<T, reserve>::difference_type;
	using reference = typename std::array<T, reserve>::reference;
	using const_reference = typename std::array<T, reserve>::const_reference;
	using pointer = typename std::array<T, reserve>::pointer;
	using const_pointer = typename std::array<T, reserve>::const_pointer;
	using iterator = typename std::array<T, reserve>::iterator;
	using const_iterator = typename std::array<T, reserve>::const_iterator;
	using reverse_iterator = typename std::array<T, reserve>::reverse_iterator;
	using const_reverse_iterator = typename std::array<T, reserve>::const_reverse_iterator;

	constexpr void check_index(size_type index) const
	{
		if (index >= mCount)
		{
			throw std::out_of_range("Index " + std::to_string(index) + " is out of bounds " + std::to_string(mCount));
		}
	}
	constexpr void check_count(size_type count) const
	{
		if (count > reserve)
		{
			throw std::length_error("Count " + std::to_string(count) + "is greater than max size " + std::to_string(reserve));
		}
	}
	void resize(size_type count)
	{
		check_count(count);
		mCount = count;
	}
	constexpr reference at(size_type pos)
	{
		check_index(pos);
		return mData.at(pos);
	}
	constexpr const_reference at(size_type pos) const
	{
		check_index(pos);
		return mData.at(pos);
	}
	constexpr reference operator[](size_type pos)
	{
		check_index(pos);
		return mData[pos];
	}
	constexpr const_reference operator[](size_type pos) const
	{
		check_index(pos);
		return mData[pos];
	}
	constexpr reference front()
	{
		return at(0);
	}
	constexpr const_reference front() const
	{
		return at(0);
	}
	constexpr reference back()
	{
		return at(mCount - 1);
	}
	constexpr const_reference back() const
	{
		return at(mCount - 1);
	}
	T *data() noexcept
	{
		return mData.data();
	}
	constexpr const T *data() const noexcept
	{
		return mData.data();
	}
	constexpr iterator begin() noexcept
	{
		return mData.begin();
	}
	constexpr const_iterator begin() const noexcept
	{
		return mData.begin();
	}
	constexpr const_iterator cbegin() const noexcept
	{
		return mData.cbegin();
	}
	constexpr iterator end() noexcept
	{
		return mData.begin() + mCount;
	}
	constexpr const_iterator end() const noexcept
	{
		return mData.begin() + mCount;
	}
	constexpr const_iterator cend() const noexcept
	{
		return mData.cbegin() + mCount;
	}
	constexpr reverse_iterator rbegin() noexcept
	{
		return mData.rend() - mCount;
	}
	constexpr const_reverse_iterator rbegin() const noexcept
	{
		return mData.rend() - mCount;
	}
	constexpr const_reverse_iterator crbegin() const noexcept
	{
		return mData.crend() - mCount;
	}
	constexpr reverse_iterator rend() noexcept
	{
		return mData.rend();
	}
	constexpr const_reverse_iterator rend() const noexcept
	{
		return mData.rend();
	}
	constexpr const_reverse_iterator crend() const noexcept
	{
		return mData.crend();
	}
	[[nodiscard]] bool empty() const noexcept
	{
		return mCount == 0;
	}
	[[nodiscard]] constexpr size_type size() const noexcept
	{
		return mCount;
	}
	constexpr size_type max_size() const noexcept
	{
		return mData.max_size();
	}
	[[nodiscard]] constexpr size_type capacity() const noexcept
	{
		return reserve;
	}
	void clear()
	{
		mCount = 0;
	}
	iterator insert(const_iterator pos, const T &value)
	{
		size_type posIndex = std::distance(mData.cbegin(), pos);
		size_type moveIndex = posIndex + 1;
		resize(1 + mCount);

		for (size_type i = mCount - 1; i-- > moveIndex; )
		{
			mData[i] = std::move(mData[i - 1]);
		}
		mData[posIndex] = value;
		return mData.begin() + posIndex;
	}
	iterator insert(const_iterator pos, T &&value)
	{
		size_type posIndex = std::distance(mData.cbegin(), pos);
		size_type moveIndex = posIndex + 1;
		resize(1 + mCount);

		for (size_type i = mCount - 1; i-- > moveIndex;)
		{
			mData[i] = std::move(mData[i - 1]);
		}
		mData[posIndex] = std::move(value);
		return mData.begin() + posIndex;
	}
	iterator insert(const_iterator pos,
		size_type count, const T &value)
	{
		size_type posIndex = std::distance(mData.cbegin(), pos);
		size_type moveIndex = posIndex + count;
		resize(count + mCount);

		for (size_type i = mCount - 1; i-- > moveIndex;)
		{
			mData[i] = std::move(mData[i - count]);
		}
		for (size_type i = posIndex; i < posIndex + count; ++i)
		{
			mData[i] = value;
		}
		return mData.begin() + posIndex;
	}
	template< class InputIt >
	iterator insert(const_iterator pos, InputIt first, InputIt last)
	{
		size_type posIndex = std::distance(mData.cbegin(), pos);
		size_type count = std::distance(first, last);
		size_type moveIndex = posIndex + count;
		resize(count + mCount);

		for (size_type i = mCount - 1; i-- > moveIndex;)
		{
			mData[i] = std::move(mData[i - count]);
		}
		iterator itr = mData.begin() + posIndex;
		for (InputIt current = first; current != last; ++current, ++itr)
		{
			*itr = *current;
		}
		return (mData.begin() + posIndex);
	}
	iterator insert(const_iterator pos, std::initializer_list<T> ilist)
	{
		return insert(pos, ilist.begin(), ilist.end());
	}

	template< class... Args >
	iterator emplace(const_iterator pos, Args&&... args)
	{
		size_type posIndex = std::distance(mData.cbegin(), pos);
		size_type count = 1;
		size_type moveIndex = posIndex + count;
		resize(count + mCount);

		for (size_type i = mCount - 1; i-- > moveIndex;)
		{
			mData[i] = std::move(mData[i - count]);
		}

		mData[posIndex] = T(std::forward<Args>(args)...);
		return (mData.begin() + posIndex);
	}

	iterator erase(iterator pos)
	{
		size_type posIndex = std::distance(mData.begin(), pos);
		size_type count = 1;
		size_type moveIndex = posIndex + count;
		for (size_type i = posIndex; i < mCount - 1; ++i)
		{
			mData[i] = std::move(mData[i + count]);
		}
		resize(mCount - count);
		return pos;
	}
	iterator erase(const_iterator pos)
	{
		size_type posIndex = std::distance(mData.cbegin(), pos);
		size_type count = 1;
		size_type moveIndex = posIndex + count;
		for (size_type i = posIndex; i < mCount - 1; ++i)
		{
			mData[i] = std::move(mData[i + count]);
		}
		resize(mCount - count);
		return pos;
	}
	iterator erase(iterator first, iterator last)
	{
		size_type posIndex = std::distance(mData.begin(), first);
		size_type count = std::distance(first, last);;
		size_type moveIndex = posIndex + count;
		for (size_type i = posIndex; i < mCount - 1; ++i)
		{
			mData[i] = std::move(mData[i + count]);
		}
		resize(mCount - count);
		return first;
	}
	iterator erase(const_iterator first, const_iterator last)
	{
		size_type posIndex = std::distance(mData.cbegin(), first);
		size_type count = std::distance(first, last);
		size_type moveIndex = posIndex + count;
		for (size_type i = posIndex; i + count < mCount - 1; ++i)
		{
			mData[i] = std::move(mData[i + count]);
		}
		resize(mCount - count);
		return (begin() + posIndex);
	}
	void push_back(const T &value)
	{
		insert(cend(), value);
	}
	void push_back(T &&value)
	{
		insert(cend(), std::move(value));
	}
	template< class... Args >
	reference emplace_back(Args&&... args)
	{
		emplace(cend(), std::forward<Args>(args)...);
		return back();
	}
	void pop_back()
	{
		resize(mCount - 1);
	}
	void resize(size_type count, const value_type &value)
	{
		if (mCount < count)
		{
			size_type num = count - mCount;
			insert(end(), num, value);
		}
		else if (mCount > count)
		{
			resize(count);
		}
	}
	void fill(const T &value)
	{
		mData.fill(value);
	}
	void swap(array_type &other) noexcept
	{
		mData.swap(other);
	}
	void swap(CStaticVector &other) noexcept
	{
		mData.swap(other.mData);
		std::swap(other.mCount, mCount);
	}
};