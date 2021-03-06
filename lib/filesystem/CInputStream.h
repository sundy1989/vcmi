#pragma once

/*
 * CInputStream.h, part of VCMI engine
 *
 * Authors: listed in file AUTHORS in main folder
 *
 * License: GNU General Public License v2.0 or later
 * Full text of license available in license.txt file, in main folder
 *
 */

/**
 * Abstract class which provides method definitions for reading from a stream.
 */
class DLL_LINKAGE CInputStream : private boost::noncopyable
{
public:
	/**
	 * D-tor.
	 */
	virtual ~CInputStream() {}

	/**
	 * Reads n bytes from the stream into the data buffer.
	 *
	 * @param data A pointer to the destination data array.
	 * @param size The number of bytes to read.
	 * @return the number of bytes read actually.
	 */
	virtual si64 read(ui8 * data, si64 size) = 0;

	/**
	 * Seeks the internal read pointer to the specified position.
	 *
	 * @param position The read position from the beginning.
	 * @return the position actually moved to, -1 on error.
	 */
	virtual si64 seek(si64 position) = 0;

	/**
	 * Gets the current read position in the stream.
	 *
	 * @return the read position.
	 */
	virtual si64 tell() = 0;

	/**
	 * Skips delta numbers of bytes.
	 *
	 * @param delta The count of bytes to skip.
	 * @return the count of bytes skipped actually.
	 */
	virtual si64 skip(si64 delta) = 0;

	/**
	 * Gets the length in bytes of the stream.
	 *
	 * @return the length in bytes of the stream.
	 */
	virtual si64 getSize() = 0;

	/**
	 * @brief for convenience, reads whole stream at once
	 *
	 * @return pair, first = raw data, second = size of data
	 */
	std::pair<std::unique_ptr<ui8[]>, size_t> readAll()
	{
		std::unique_ptr<ui8[]> data(new ui8[getSize()]);

		seek(0);
		size_t readSize = read(data.get(), getSize());
		assert(readSize == getSize());
		UNUSED(readSize);

		return std::make_pair(std::move(data), getSize());
	}

	/**
	 * @brief calculateCRC32 calculates CRC32 checksum for the whole file
	 * @return calculated checksum
	 */
	virtual ui32 calculateCRC32()
	{
		si64 originalPos = tell();

		boost::crc_32_type checksum;
		auto data = readAll();
		checksum.process_bytes(reinterpret_cast<const void *>(data.first.get()), data.second);

		seek(originalPos);

		return checksum.checksum();
	}
};
