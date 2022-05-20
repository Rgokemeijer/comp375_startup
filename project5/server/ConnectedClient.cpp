#include <iostream>

#include <cstring>

#include <unistd.h>
#include <sys/epoll.h>
#include <sys/types.h>
#include <sys/socket.h>

#include <vector>
#include <filesystem>
#include <fstream>
#include <string>
#include "ChunkedDataSender.h"
#include "ConnectedClient.h"

using std::cout;
using std::cerr;
using std::string;

using std::vector;
namespace fs = std::filesystem;

ConnectedClient::ConnectedClient(int fd, ClientState initial_state) :
	client_fd(fd), sender(NULL), state(initial_state) {}

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

	/*
	 * 1. update our state field to be sending
	 * 2. set our sender field to be the ArraySender object we created
	 * 3. update epoll so that it also watches for EPOLLOUT for this client
	 *    socket (use epoll_ctl with EPOLL_CTL_MOD).
	 */
	if (num_bytes_sent >= 0) {
		// Sent everything with no problem so we are done with our ArraySender
		// object.
		struct epoll_event client_ev;
		client_ev.data.fd = this->client_fd;
		client_ev.events = EPOLLOUT;
		cout << "Stopped listening for epollOUT\n";
		if(epoll_ctl(epoll_fd, EPOLL_CTL_DEL, this->client_fd, &client_ev) == -1){
			perror("Error updating epoll to stop watch for EPOLLOUT");
			exit(1);	
		}

		delete this->sender;
	}
}

void ConnectedClient::handle_input(int epoll_fd, vector<fs::path> song_list) {
	char data[1024];
	memset(data, 0, 1024);
	ssize_t bytes_received = recv(this->client_fd, data, 1024, 0);
	if (bytes_received < 0) {
		perror("client_read recv");
		exit(EXIT_FAILURE);
	}
	// cout << "data: ";
	// for (int i = 0; i < 1024; i++) {
	// 	cout << data[i];
	// }
	// cout  << "\n";
	//remove blank bytes of client input
	int blank_bytes = 0;
	while(!(isalpha(data[blank_bytes]))){
		blank_bytes+=1;
		if (blank_bytes > 500){
			return; // Is a bad epoll event/ ignore
		}
	}
	char formatted[1024-blank_bytes];
	memcpy(formatted, data+blank_bytes, 1024-blank_bytes);

	// Replace the space with null terminator so that comparison will be succesufll and the second
	// argument the song num is still the same
	formatted[4] = '\0';
	//play"\0"5"\0"
	if (strcmp(formatted, "play") == 0){ // Bring pack
		try{
			int song_id = abs(std::stoi(formatted + 5) % (int)song_list.size()); // get the int starting at the 
			send_audio(epoll_fd, song_list.at(song_id));
		}
		catch(const std::invalid_argument &err){
			cout << "Invalid data sent with play command: ";
			std::cerr << err.what();
		}
	}
	else if (strcmp(formatted, "list") == 0){ 
		list(epoll_fd, song_list);
	}
	else if (strcmp(formatted, "info") == 0){
		try{
			get_info(epoll_fd, song_list, std::stoi(formatted + 5));
		}
		catch(const std::invalid_argument &err){
			cout << "Invalid data sent with info command: ";
			std::cerr << err.what();
		}
	}
	// this->send_dummy_response(epoll_fd);
}

void ConnectedClient::send_audio(int epoll_fd, fs::path song_path){
	// Create a large array, just to make sure we can send a lot of data in
	// smaller chunks.

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

	/*
	 * 1. update our state field to be sending
	 * 2. set our sender field to be the ArraySender object we created
	 * 3. update epoll so that it also watches for EPOLLOUT for this client
	 *    socket (use epoll_ctl with EPOLL_CTL_MOD).
	 */
	if (num_bytes_sent < 0) {
		// Fill this in with the three steps listed in the comment above.
		// WARNING: Do NOT delete array_sender here (you'll need it to continue
		// sending later).
		this->state = SENDING;
		this->sender = file_sender;
		struct epoll_event client_ev;
		client_ev.data.fd = this->client_fd;
		client_ev.events = EPOLLOUT; // Changed to out instead of in
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

	if (epoll_ctl(epoll_fd, EPOLL_CTL_DEL, this->client_fd, NULL) == -1) {
		perror("handle_close epoll_ctl");
		exit(EXIT_FAILURE);
	}

	close(this->client_fd);
}

void ConnectedClient::list(int epoll_fd,vector<fs::path> song_list) {
	std::stringstream ss;
	for(size_t i = 0; i < song_list.size(); ++i){
		ss << "(" << i << ") " << song_list[i] << "\n";
	}
	std::string file_list = ss.str();
	send_message(epoll_fd, file_list);
}


void ConnectedClient::get_info(int epoll_fd, vector<fs::path> song_list, int song_index){
	if (song_index < 0 || song_index >= (int)song_list.size()){
		send_message(epoll_fd, "Invalid song index specified: " + std::to_string(song_index));
		return;
	}
	fs::path info_path = song_list[song_index].replace_extension(".mp3.info");
	if (not(fs::exists(info_path))) {
		send_message(epoll_fd, "Song does not have an info file.");
		return;
	}
	FileSender *file_sender = new FileSender(info_path); // pass this object to the File

	ssize_t num_bytes_sent;
	ssize_t total_bytes_sent = 0;

	// keep sending the next chunk until it says we either didn't send
	// anything (0 return indicates nothing left to send) or until we can't
	// send anymore because of a full socket buffer (-1 return value)
	while((num_bytes_sent = file_sender->send_next_chunk(this->client_fd)) > 0) {
		total_bytes_sent += num_bytes_sent;
	}
	/*
	 * 1. update our state field to be sending
	 * 2. set our sender field to be the ArraySender object we created
	 * 3. update epoll so that it also watches for EPOLLOUT for this client
	 *    socket (use epoll_ctl with EPOLL_CTL_MOD).
	 */
	if (num_bytes_sent < 0) {
		this->state = SENDING;
		this->sender = file_sender;
		struct epoll_event client_ev;
		client_ev.data.fd = this->client_fd;
		client_ev.events = EPOLLOUT;
		if(epoll_ctl(epoll_fd, EPOLL_CTL_MOD, this->client_fd, &client_ev) == -1){
			perror("Error updating epoll to watch for EPOLLOUT");
			exit(1);	
		}
	}
	else {
		delete file_sender;
	}
}



void ConnectedClient::send_message(int epoll_fd, string data_to_send) {
	// Create a large array, just to make sure we can send a lot of data in
	// smaller chunks.
	const char *c_message = data_to_send.c_str();


	ArraySender *array_sender = new ArraySender(c_message, data_to_send.length());
	// delete[] c_message; // The ArraySender creates its own copy of the data so let's delete this copy

	ssize_t num_bytes_sent;
	ssize_t total_bytes_sent = 0;

	// keep sending the next chunk until it says we either didn't send
	// anything (0 return indicates nothing left to send) or until we can't
	// send anymore because of a full socket buffer (-1 return value)
	while((num_bytes_sent = array_sender->send_next_chunk(this->client_fd)) > 0) {
		total_bytes_sent += num_bytes_sent;
	}

	/*
	 * 1. update our state field to be sending
	 * 2. set our sender field to be the ArraySender object we created
	 * 3. update epoll so that it also watches for EPOLLOUT for this client
	 *    socket (use epoll_ctl with EPOLL_CTL_MOD).
	 */
	if (num_bytes_sent < 0) {
		// Fill this in with the three steps listed in the comment above.
		// WARNING: Do NOT delete array_sender here (you'll need it to continue
		// sending later).
		this->state = SENDING;
		this->sender = array_sender;
		struct epoll_event client_ev;
		client_ev.data.fd = this->client_fd;
		client_ev.events = EPOLLOUT;
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