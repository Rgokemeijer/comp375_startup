#include <iostream>

#include <cstring>

#include <unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <vector>
#include <filesystem>
#include <fstream>

#include "ChunkedDataSender.h"
#include "ConnectedClient.h"

using std::cout;
using std::cerr;
using std::vector;
namespace fs = std::filesystem;

ConnectedClient::ConnectedClient(int fd, ClientState initial_state) :
	client_fd(fd), sender(NULL), state(initial_state) {}

void ConnectedClient::send_dummy_response(int epoll_fd) {
	// Create a large array, just to make sure we can send a lot of data in
	// smaller chunks.
	cout << "Sending Dummy Response\n";
	char *data_to_send = new char[CHUNK_SIZE*2000];
	memset(data_to_send, 117, CHUNK_SIZE*2000); // 117 is ascii 'u'

	ArraySender *array_sender = new ArraySender(data_to_send, CHUNK_SIZE*2000);
	delete[] data_to_send; // The ArraySender creates its own copy of the data so let's delete this copy

	ssize_t num_bytes_sent;
	ssize_t total_bytes_sent = 0;

	// keep sending the next chunk until it says we either didn't send
	// anything (0 return indicates nothing left to send) or until we can't
	// send anymore because of a full socket buffer (-1 return value)
	while((num_bytes_sent = array_sender->send_next_chunk(this->client_fd)) > 0) {
		total_bytes_sent += num_bytes_sent;
	}
	cout << "sent " << total_bytes_sent << " bytes to client\n";

	/*
	 * TODO: if the last call to send_next_chunk indicated we couldn't send
	 * anything because of a full socket buffer, we should do the following:
	 *
	 * 1. update our state field to be sending
	 * 2. set our sender field to be the ArraySender object we created
	 * 3. update epoll so that it also watches for EPOLLOUT for this client
	 *    socket (use epoll_ctl with EPOLL_CTL_MOD).
	 *
	 * WARNING: These steps are to be done inside of the following if statement,
	 * not before it.
	 */
	if (num_bytes_sent < 0) {
		// Fill this in with the three steps listed in the comment above.
		// WARNING: Do NOT delete array_sender here (you'll need it to continue
		// sending later).
		this->state = SENDING;
		this->sender = array_sender;
		struct epoll_event client_ev;
		client_ev.data.fd = this->client_fd;
		client_ev.events = EPOLLIN;
		if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, this->client_fd, &client_ev) == -1){
			perror("Error updating epoll to watch for EPOLLOUT");
			exit(1);	
		}
	}
	else {
		// Sent everything with no problem so we are done with our ArraySender
		// object.
		delete array_sender;
	}
}

void ConnectedClient::continue_response(int epoll_fd) {
	// Create a large array, just to make sure we can send a lot of data in
	// smaller chunks.
	ssize_t num_bytes_sent;
	ssize_t total_bytes_sent = 0;

	// keep sending the next chunk until it says we either didn't send
	// anything (0 return indicates nothing left to send) or until we can't
	// send anymore because of a full socket buffer (-1 return value)
	while((num_bytes_sent = this->sender->send_next_chunk(this->client_fd)) > 0) {
		total_bytes_sent += num_bytes_sent;
	}
	cout << "sent " << total_bytes_sent << " bytes to client\n";

	/*
	 * TODO: if the last call to send_next_chunk indicated we couldn't send
	 * anything because of a full socket buffer, we should do the following:
	 *
	 * 1. update our state field to be sending
	 * 2. set our sender field to be the ArraySender object we created
	 * 3. update epoll so that it also watches for EPOLLOUT for this client
	 *    socket (use epoll_ctl with EPOLL_CTL_MOD).
	 *
	 * WARNING: These steps are to be done inside of the following if statement,
	 * not before it.
	 */
	if (num_bytes_sent >= 0) {
		// Sent everything with no problem so we are done with our ArraySender
		// object.
		struct epoll_event client_ev;
		client_ev.data.fd = this->client_fd;
		client_ev.events = EPOLLOUT;

		if(epoll_ctl(epoll_fd, EPOLL_CTL_DEL, this->client_fd, &client_ev) == -1){
			perror("Error updating epoll to watch for EPOLLOUT");
			exit(1);	
		}

		delete this->sender;
	}
}

void ConnectedClient::handle_input(int epoll_fd, vector<fs::path> song_list) {
	cout << "Ready to read from client " << this->client_fd << "\n";
	char data[1024];
	ssize_t bytes_received = recv(this->client_fd, data, 1024, 0);
	if (bytes_received < 0) {
		perror("client_read recv");
		exit(EXIT_FAILURE);
	}

	// if (strcmp(data[0], "\0") == 0){
		// for (int i = 0; i < bytes_received; i++){
		// 	data[i] = data[i + 1];
		// }
	// }
	char formatted[1023];
	memcpy(formatted, data+1, 1023);
	for (int i = 0; i < 15; i++)
		cout << "data: " << i << ": \"" << data[i] << "\"\n";

	// TODO: Eventually you need to actually look at the response and send a
	// response based on what you got from the client (e.g. did they ask for a
	// list of songs or for you to send them a song?)
	// For now, the following function call just demonstrates how you might
	// send data.
	int song_id = 0;
	cout << "Command was \"" << formatted << "\"\n";
	// if (strcmp(formatted, "play") == 0){ // Bring pack
		cout << "about to call send_audio\n";
		send_audio(epoll_fd, song_list.at(song_id));
	// }
	// this->send_dummy_response(epoll_fd);
}

void ConnectedClient::send_audio(int epoll_fd, fs::path song_path){
	// Create a large array, just to make sure we can send a lot of data in
	// smaller chunks.
	cout << "Sending Audio Response\n";

	//The rest of this is not yet right
	ifstream indata (song_path); // Create ifstram object from path
	FileSender *file_sender = new FileSender(song_path); // pass this object to the File

	ssize_t num_bytes_sent;
	ssize_t total_bytes_sent = 0;

	// keep sending the next chunk until it says we either didn't send
	// anything (0 return indicates nothing left to send) or until we can't
	// send anymore because of a full socket buffer (-1 return value)
	while((num_bytes_sent = file_sender->send_next_chunk(this->client_fd)) > 0) {
		total_bytes_sent += num_bytes_sent;
	}
	cout << "sent " << total_bytes_sent << " bytes to client\n";

	/*
	 * TODO: if the last call to send_next_chunk indicated we couldn't send
	 * anything because of a full socket buffer, we should do the following:
	 *
	 * 1. update our state field to be sending
	 * 2. set our sender field to be the ArraySender object we created
	 * 3. update epoll so that it also watches for EPOLLOUT for this client
	 *    socket (use epoll_ctl with EPOLL_CTL_MOD).
	 *
	 * WARNING: These steps are to be done inside of the following if statement,
	 * not before it.
	 */
	if (num_bytes_sent < 0) {
		// Fill this in with the three steps listed in the comment above.
		// WARNING: Do NOT delete array_sender here (you'll need it to continue
		// sending later).
		this->state = SENDING;
		this->sender = file_sender;
		struct epoll_event client_ev;
		client_ev.data.fd = this->client_fd;
		client_ev.events = EPOLLIN;
		if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, this->client_fd, &client_ev) == -1){
			perror("Error updating epoll to watch for EPOLLOUT");
			exit(1);	
		}
	}
	else {
		// Sent everything with no problem so we are done with our ArraySender
		// object.
		delete file_sender;
	}
}



// You likely should not need to modify this function.
void ConnectedClient::handle_close(int epoll_fd) {
	cout << "Closing connection to client " << this->client_fd << "\n";

	if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, this->client_fd, NULL) == -1) {
		perror("handle_close epoll_ctl");
		exit(EXIT_FAILURE);
	}

	close(this->client_fd);
}

