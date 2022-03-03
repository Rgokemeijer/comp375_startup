/**
 * ToreroServe: A Lean Web Server
 * COMP 375 - Project 02
 *
 * This program should take two arguments:
 * 	1. The port number on which to bind and listen for connections
 * 	2. The directory out of which to serve files.
 *
 * 	Author info with names and USD email addresses
 *	Author: Matthew Gloriani
 *			matthewgloriani@sandiego.edu
 *	Author: Russell Gokemeijer
 *			rgokemeijer@sandiego.edu
 *
 */

// standard C libraries
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cerrno>

// operating system specific libraries
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/uio.h>
#include <unistd.h>

// C++ standard libraries
#include <vector>
#include <thread>
#include <string>
#include <iostream>
#include <system_error>
#include <filesystem>
#include <fstream>
#include <regex>
#include <mutex>

// shorten the std::filesystem namespace down to just fs
namespace fs = std::filesystem;

using std::cout;
using std::string;
using std::vector;
using std::thread;

// This will limit how many clients can be waiting for a connection.
static const int BACKLOG = 10;

// forward declarations
int createSocketAndListen(const int port_num);
void acceptConnections(const int server_sock, string base_dir);
void handleClient(const int client_sock);
void sendData(int socked_fd, const char *data, size_t data_length);
int receiveData(int socked_fd, char *dest, size_t buff_size);
void badRequest(const int client_sock);
void notFoundRequest(const int client_sock);
void okayResponse(const int client_sock, string file_type);
void contentResponse(const int client_sock, string file_name, string base_dir);
void check_dir(int client_sock, string file_name, string base_dir);
void send_page(int client_sock, string file_name, string base_dir);
void send_dir(int client_sock, string file_name, string base_dir);
void thread_function(int &count, int &tail, int buff_size, string base_dir, int sock_buff[], std::mutex &count_mutex, std::mutex &get_mutex);



/**
 * Main function that runs the server and other functions
 */
int main(int argc, char** argv) {
	/* Make sure the user called our program correctly. */
	if (argc != 3) {
		cout << "INCORRECT USAGE!\n";
		cout << "Proper Usage ./torero-serve 7101 \"some dir\"\n";
		exit(1);
	}	

    /* Read the port number from the first command line argument. */
    int port = std::stoi(argv[1]);

	/* Create a socket and start listening for new connections on the
	 * specified port. */
	int server_sock = createSocketAndListen(port);

	/* Now let's start accepting connections. */
	acceptConnections(server_sock, argv[2]);

    close(server_sock);

	return 0;
}

/**
 * Sends message over given socket, raising an exception if there was a problem
 * sending.
 *
 * @param socket_fd The socket to send data over.
 * @param data The data to send.
 * @param data_length Number of bytes of data to send.
 */
void sendData(int socked_fd, const char *data, size_t data_length) {
	// This keeps sending until
	// the data has been completely sent.
	size_t total_sent = 0;
	while (total_sent != data_length){	
		int num_bytes_sent = send(socked_fd, data, data_length, 0);
		if (num_bytes_sent == -1) {
			std::error_code ec(errno, std::generic_category());
			throw std::system_error(ec, "send failed");
		}
		total_sent += num_bytes_sent;
	}
}

/**
 * Receives message over given socket, raising an exception if there was an
 * error in receiving.
 *
 * @param socket_fd The socket to send data over.
 * @param dest The buffer where we will store the received data.
 * @param buff_size Number of bytes in the buffer.
 * @return The number of bytes received and written to the destination buffer.
 */
int receiveData(int socked_fd, char *dest, size_t buff_size) {
	int num_bytes_received = recv(socked_fd, dest, buff_size, 0);
	if (num_bytes_received == -1) {
		std::error_code ec(errno, std::generic_category());
		throw std::system_error(ec, "recv failed");
	}

	return num_bytes_received;
}

/**
 * Receives a request from a connected HTTP client and sends back the
 * appropriate response.
 *
 * @note After this function returns, client_sock will have been closed (i.e.
 * may not be used again).
 *
 * @param client_sock The client's socket file descriptor.
 */
void handleClient(const int client_sock, string base_dir) {
	// Step 1: Receive the request message from the client
	char received_data[2048];
	int bytes_received = receiveData(client_sock, received_data, 2048);
	string received_str = std::string(received_data);

	// Turn the char array into a C++ string for easier processing.
	string request_string(received_data, bytes_received);
	
	// Step 2: Parse the request string to determine what response to generate.
	// We used regular expressions (specifically C++'s std::regex) to
	// determine if a request is properly formatted.
	std::vector<string> received_lines;
	string::size_type pos = 0;
	string::size_type prev = 0;
	while(( pos = received_str.find("\r\n", prev)) != std::string::npos){
		received_lines.push_back(received_str.substr(prev, pos - prev));
		prev = pos + 1;
	}
	received_lines.push_back(received_str.substr(prev));
 	
	//Check if valid request
    // This is a regex that matches all valid requests
	std::regex http_request_regex("GET( *)/([a-zA-Z0-9_\\-/.]*)( *)HTTP/([0-9]*).([0-9]*)",
	             std::regex_constants::ECMAScript);
 	std::smatch request_match;
	// if no match send badRequest message
	if (! std::regex_match(received_lines[0], request_match, http_request_regex)) {
		badRequest(client_sock);
		close(client_sock);
		return;
	}
	
	//Process the data to determine request
 	int end_index = received_lines[0].find(" ", 4) - 4;
	string file_name = received_lines[0].substr(4, end_index);
	
	// Step 3: Generate HTTP response message based on the request you received.
	if (file_name == "/favicon.ico"){
		close(client_sock);
		return;
	}
	
	// parse file type
	if (file_name.back() == '/'){
		check_dir(client_sock, file_name, base_dir);
	}
	else{
		send_page(client_sock, file_name, base_dir);
	}
	// Close connection with client.
	close(client_sock);
}

/**
 * This checks to see if the directory is valid
 *
 * @param client_sock - the client sock
 * @param file_name - the file name
 * @param base_dir - the wanted directory on the command line
 */
void check_dir(int client_sock, string file_name, string base_dir){
	if (fs::is_regular_file(base_dir + file_name + "index.html")){
		send_page(client_sock, file_name + "index.html", base_dir);
	}
	else if (fs::is_directory(base_dir + file_name)){
		okayResponse(client_sock, "html");
		send_dir(client_sock, file_name, base_dir);
	}
	else{
		notFoundRequest(client_sock);
	}
}

/**
 * This sends the directory in a nicely formatted HTML page
 *
 * @param client_sock - the client sock
 * @param file_name - the file name
 * @param base_dir - the wanted directory on the command line
 */
void send_dir(int client_sock, string file_name, string base_dir){
	string dir_html = "<html>\n<body>\n<ul>\n";
	for (const auto& entry: fs::directory_iterator( base_dir + file_name)) {	
		string path_name = entry.path().filename();
		dir_html += "<li><a href=\"" + path_name + "/\">" + path_name + "/</a></li>\n";
	}
	dir_html += "</ul>\n</body>\n</html>";
	sendData(client_sock, dir_html.c_str(), dir_html.length());
}

/**
 * This sends the page that was requested by the user, or clicked on by the
 * user using a hyperlink
 *
 * @param client_sock - the client sock
 * @param file_name - the file name
 * @param base_dir - the wanted directory on the command line
 */
void send_page(int client_sock, string file_name, string base_dir){
	// Step 4: Send response to client using the sendData function.
	if (fs::is_regular_file( base_dir + file_name)){
		int start_index = file_name.find(".") + 1;
		string file_type = file_name.substr(start_index);
		okayResponse(client_sock, file_type);
		contentResponse(client_sock, file_name, base_dir);	
	}
	else{
		notFoundRequest(client_sock);
	}
}	

/**
 * Bad request void function
 *
 * @param client_sock - the client sock
 */
void badRequest(const int client_sock) {
	string bad_request_string = "HTTP/1.0 400 BAD REQUEST\r\n\r\n";	
	sendData(client_sock, bad_request_string.c_str(), bad_request_string.length());
}

/**
 * Not found request void function
 *
 * @param client_sock - the client sock
 */
void notFoundRequest(const int client_sock) {
	string n_found_str = "HTTP/1.0 404 NOT FOUND\r\nContent-Type: text/html\r\n\r\n<html>\n<head>\n<title>Ruh-roh! Page not found!</title>\n</head>\n<body>\n404 Page Not Found! :'( :'( :'(\n</body>\n</html>";
	sendData(client_sock, n_found_str.c_str(), n_found_str.length());
}

/**
 * HTTP 200 OK response message void function
 * Also sends header
 *
 * @param client_sock - the client sock
 * @param file_type - the file type (such as html, css, txt, etc.)
 */
void okayResponse(const int client_sock, string file_type) {
	if (file_type == "html" || file_type == "css" || file_type == "txt"){
		file_type = "text/" + file_type;
	}
	else if(file_type == "jpeg" || file_type == "gif" || file_type == "png"){
		file_type = "image/" + file_type;
	}
	else{
		file_type = "application/" + file_type;
	}
	// messaged
	string okay_string = "HTTP/1.0 200 OK\r\n";
	
	// header
	string content_type = "Content-Type: " + file_type + "\r\n";
		
	// putting em together
	string together = okay_string + content_type + "\r\n";
	sendData(client_sock, together.c_str(), together.length());
}

/**
 * This is the content response for sending back the data to the requester
 *
 * @param client_sock - the client sock
 * @param file_name - the file name
 * @param base_dir - the wanted directory listed on the command line
 */
void contentResponse(const int client_sock, string file_name, string base_dir){
	//read_file
	std::ifstream file(base_dir + file_name, std::ios::binary);
	if (! file.is_open()){
		notFoundRequest(client_sock);
		close(client_sock);
		return;	
	}
	const unsigned int buffer_size = 4096;
    char file_data[buffer_size];
    while(!file.eof()) {
        file.read(file_data, buffer_size);
        int bytes_read = file.gcount();
		sendData(client_sock, file_data, bytes_read);
	}
    file.close();
}

/**
 * Creates a new socket and starts listening on that socket for new
 * connections.
 *
 * @param port_num The port number on which to listen for connections.
 * @returns The socket file descriptor
 */
int createSocketAndListen(const int port_num) {
    int sock = socket(AF_INET, SOCK_STREAM, 0);
    if (sock < 0) {
        perror("Creating socket failed");
        exit(1);
    }

    /* 
	 * A server socket is bound to a port, which it will listen on for incoming
     * connections.  By default, when a bound socket is closed, the OS waits a
     * couple of minutes before allowing the port to be re-used.  This is
     * inconvenient when you're developing an application, since it means that
     * you have to wait a minute or two after you run to try things again, so
     * we can disable the wait time by setting a socket option called
     * SO_REUSEADDR, which tells the OS that we want to be able to immediately
     * re-bind to that same port. See the socket(7) man page ("man 7 socket")
     * and setsockopt(2) pages for more details about socket options.
	 */
    int reuse_true = 1;

	int retval; // for checking return values

    retval = setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, &reuse_true,
                        sizeof(reuse_true));

    if (retval < 0) {
        perror("Setting socket option failed");
        exit(1);
    }

    /*
	 * Create an address structure.  This is very similar to what we saw on the
     * client side, only this time, we're not telling the OS where to connect,
     * we're telling it to bind to a particular address and port to receive
     * incoming connections.  Like the client side, we must use htons() to put
     * the port number in network byte order.  When specifying the IP address,
     * we use a special constant, INADDR_ANY, which tells the OS to bind to all
     * of the system's addresses.  If your machine has multiple network
     * interfaces, and you only wanted to accept connections from one of them,
     * you could supply the address of the interface you wanted to use here.
	 */
    struct sockaddr_in addr;
    addr.sin_family = AF_INET;
    addr.sin_port = htons(port_num);
    addr.sin_addr.s_addr = INADDR_ANY;

    /* 
	 * As its name implies, this system call asks the OS to bind the socket to
     * address and port specified above.
	 */
    retval = bind(sock, (struct sockaddr*)&addr, sizeof(addr));
    if (retval < 0) {
        perror("Error binding to port");
        exit(1);
    }

    /* 
	 * Now that we've bound to an address and port, we tell the OS that we're
     * ready to start listening for client connections. This effectively
	 * activates the server socket. BACKLOG (a global constant defined above)
	 * tells the OS how much space to reserve for incoming connections that have
	 * not yet been accepted.
	 */
    retval = listen(sock, BACKLOG);
    if (retval < 0) {
        perror("Error listening for connections");
        exit(1);
    }

	return sock;
}

/**
 * Sit around forever accepting new connections from client.
 *
 * @param server_sock The socket used by the server.
 */
void acceptConnections(const int server_sock, string base_dir) {
	const int buff_size = 20;
	int sock_buff[buff_size];
	int tail = 0, head = 0, count = 0;
	std::mutex count_mutex;
	std::mutex get_mutex;
	const int num_threads = 8;
	vector<thread> threads;
	for (int i = 0; i < num_threads; i++){
		threads.push_back(thread(thread_function, std::ref(count), std::ref(tail), buff_size, base_dir, sock_buff, std::ref(count_mutex), std::ref(get_mutex)));
		threads[i].detach();
	}
	while (true) {
        // Declare a socket for the client connection.
        int sock;

        /* 
		 * Another address structure.  This time, the system will automatically
         * fill it in, when we accept a connection, to tell us where the
         * connection came from.
		 */
        struct sockaddr_in remote_addr;
        unsigned int socklen = sizeof(remote_addr); 

        /* 
		 * Accept the first waiting connection from the server socket and
         * populate the address information.  The result (sock) is a socket
         * descriptor for the conversation with the newly connected client.  If
         * there are no pending connections in the back log, this function will
         * block indefinitely while waiting for a client connection to be made.
         */
        sock = accept(server_sock, (struct sockaddr*) &remote_addr, &socklen);
        if (sock < 0) {
            perror("Error accepting connection");
            exit(1);
        }

        /* 
		 * At this point, you have a connected socket (named sock) that you can
         * use to send() and recv(). The handleClient function should handle all
		 * of the sending and receiving to/from the client.
		 *
		 * We don't call handleClient directly here. Instead it
		 * should be called from a separate thread. You'll just need to put sock
		 * in a shared buffer that is synchronized using condition variables.
		 * You'll implement this shared buffer in one of the labs and can use
		 * it directly here.
		 */
		while (count == buff_size){} //Wait until a spot is open
		sock_buff[head] = sock;
		// allow threads to start processing ASAP but after added to buff and
		// we  need to protect with mutex
		count_mutex.lock();
		count += 1;
		count_mutex.unlock();
		head = (head + 1) % buff_size;
    }
}

/**
 * This is the thread function that implements the threads for running
 * multiple browsers for the user, or multiple users
 *
 * @param count - an incrementer
 * @param tail - the next mutex to be listed
 * @param buff_size - the buff size
 * @param base_dir - the wanted directory listed on the command line
 * @param buff[] - the buff array
 * @param count_mutex - keeps track of the count for mutexes
 * @param get_mutex - gets the mutex to unlock and lock the threads:
 */
void thread_function(int &count, int &tail, int buff_size, string base_dir, int buff[], std::mutex &count_mutex, std::mutex &get_mutex){
	while (true){
		// This locks and unlocks twice to make sure 
		get_mutex.lock();
		while(count == 0){
			get_mutex.unlock();
			get_mutex.lock();	
		}
		count_mutex.lock();
		count = (count - 1) % buff_size;
		count_mutex.unlock();
		int socket = buff[tail];
		tail = (tail + 1) % buff_size;
		get_mutex.unlock();
		handleClient(socket, base_dir);
	}
}
