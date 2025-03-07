#ifndef CHUNKEDDATASENDER_H
#define CHUNKEDDATASENDER_H

#include <cstddef>

using std::ifstream;

namespace fs = std::filesystem;

const size_t CHUNK_SIZE = 4096;

/**
 * An interface for sending data in fixed-sized chunks over a network socket.
 * This interface contains one function, send_next_chunk, which should send
 * the next chunk of data.
 */
class ChunkedDataSender {
  public:
	virtual ~ChunkedDataSender() {}

	virtual ssize_t send_next_chunk(int sock_fd) = 0;
};

/**
 * Class that allows sending an array of over a network socket.
 */
class ArraySender : public virtual ChunkedDataSender {
  private:
	char *array; // the array of data to send
	size_t array_length; // length of the array to send (in bytes)
	size_t curr_loc; // index in array where next send will start

  public:
	/**
	 * Constructor for ArraySender class.
	 */
	ArraySender(const char *array_to_send, size_t length);

	/**
	 * Destructor for ArraySender class.
	 */
	~ArraySender() {
		delete[] array;
	}

	/**
	 * Sends the next chunk of data, starting at the spot in the array right
	 * after the last chunk we sent.
	 *
	 * @param sock_fd Socket which to send the data over.
	 * @return -1 if we couldn't send because of a full socket buffer,
	 * 	otherwise the number of bytes actually sent over the socket.
	 */
	virtual ssize_t send_next_chunk(int sock_fd);
};


class FileSender : public virtual ChunkedDataSender {
  private:
	
	ifstream *indata;
	int file_length;
	int curr_loc;

  public:

	/**
	 * Constructor for ArraySender class.
	 */
	FileSender(fs::path song_path);

	/**
	 * Destructor for ArraySender class.
	 */
	~FileSender() {
		(*indata).close();
	}

	/**
	 * Sends the next chunk of data, starting at the spot in the array right
	 * after the last chunk we sent.
	 *
	 * @param sock_fd Socket which to send the data over.
	 * @return -1 if we couldn't send because of a full socket buffer,
	 * 	otherwise the number of bytes actually sent over the socket.
	 */
	virtual ssize_t send_next_chunk(int sock_fd);
};

#endif // CHUNKEDDATASENDER_H
