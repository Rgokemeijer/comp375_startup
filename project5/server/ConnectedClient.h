#ifndef CONNECTEDCLIENT_H
#define CONNECTEDCLIENT_H

#include <vector>
#include <string>

using std::vector;
using std::string;

/**
 * Represents the state of a connected client.
 */
enum ClientState { RECEIVING, SENDING };

/**
 * Class that models a connected client.
 * 
 * One object of this class will be created for every client that you accept a
 * connection from.
 */
class ConnectedClient {
  public:
	// Member Variables (i.e. fields)
	int client_fd;
	ChunkedDataSender *sender;
	ClientState state;

	// Constructors
	/**
	 * Constructor that takes the client's socket file descriptor and the
	 * initial state of the client.
	 */
	ConnectedClient(int fd, ClientState initial_state);

	/**
	 * No argument constructor.
	 */
	ConnectedClient() : client_fd(-1), sender(NULL), state(RECEIVING) {}


	// Member Functions (i.e. Methods)
	
	/**
	 * Sends a response to the client.
	 * Note that this is just to demonstrate sending to the client: it doesn't
	 * send anything intelligent.
	 *
	 * @param epoll_fd File descriptor for epoll.
	 */
	void send_dummy_response(int epoll_fd);

	/**
	 * Sends a response of the current audio file to the client.
	 *
	 * @param epoll_fd File descriptor for epoll.
	 */
	void send_audio(int epoll_fd, fs::path file_path);
	
	/**
	 * Is called after receiving an EPOLLOUT message and starts sending data
	 * again.
	 *
	 * @param epoll_fd File descriptor for epoll.
	 */
	void continue_response(int epoll_fd);


	/**
	 * Handles new input from the client.
	 *
	 * @param epoll_fd File descriptor for epoll.
	 */
	void handle_input(int epoll_fd, vector<fs::path> song_list);

	/**
	 * Handles a close request from the client.
	 *
	 * @param epoll_fd File descriptor for epoll.
	 */
	void handle_close(int epoll_fd);


	void list(int epoll_fd, char *dir);

	void send_message(int epoll_fd, string DataToSend);
};

#endif
