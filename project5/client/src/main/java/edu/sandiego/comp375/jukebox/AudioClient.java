package edu.sandiego.comp375.jukebox;

import java.io.BufferedInputStream;
import java.io.DataOutputStream;
import java.net.Socket;
import java.util.Scanner;

/**
 * Class representing a client to our Jukebox.
 */
public class AudioClient {
	public static void main(String[] args) throws Exception {
		Scanner s = new Scanner(System.in);
		BufferedInputStream in = null;
		DataOutputStream dOut = null; //Added this
		Thread player = null;
		int port = 0;
		String ip;
		if (args.length < 2){
			System.out.println("Incorrect Usage: AudioClient {port} {ip}"); //TODO
			System.exit(0);
		}
		try{
			port = Integer.valueOf(args[0]);
		}
		catch(NumberFormatException e){
			System.out.println("Incorrect Usage: AudioClient {port} {ip}"); //TODO
			System.exit(0);
		}
		ip = args[1];
		System.out.println("Client: Connecting to " + ip + " on port: " + port);

		while (true) {
			System.out.print(">> ");
			String command = s.nextLine();
			String commands[] = command.split(" ", 2);
			if (commands[0].equals("play")) {
				try {
					if (player != null){
						player.stop();
					}
					// This will throw an error if the command is invalid
					Integer.valueOf(commands[1]); 
					Socket socket = new Socket(ip, port);
					if (socket.isConnected()) {
						in = new BufferedInputStream(socket.getInputStream(), 2048);
						//THIS IS NEW CODE
						dOut = new DataOutputStream(socket.getOutputStream());
						dOut.writeUTF(command);
						dOut.flush(); // send play to server

						//BACK TO GIVEN CODE
						AudioPlayerThread thread = new AudioPlayerThread(in);
						player = new Thread(thread);
						player.start();
					}
				}
				catch(ArrayIndexOutOfBoundsException e){
					System.out.println("Invalid song index");
				}
				catch (Exception e) {
					System.out.println(e);
				}
			}
			else if (command.equals("exit")) {
				// Currently this doesn't actually stop the music from
				// playing.
				// Your final solution should make sure that the exit command
				// causes music to stop playing immediately.
				if (player != null){
					player.stop();
				}
				System.out.println("Goodbye!");
				break;
			}
			else if (command.equals("list")){
				Socket socket = new Socket(ip, port);
				if (socket.isConnected()) {
					dOut = new DataOutputStream(socket.getOutputStream());
					dOut.writeUTF("list");
					dOut.flush();
					in = new BufferedInputStream(socket.getInputStream(), 2048);
					// Thread.sleep(1000);
					while(in.available() == 0){} // Wait till recv data
					while (in.available() > 0) {
  
						// Read the byte and
						// convert the integer to character
						char c = (char)in.read();
						// Print the characters
						System.out.print(c);
					}
				}
				socket.close();
			}
			else if (commands[0].equals("info")){
				Socket socket = new Socket(ip, port);
				try{
					Integer.valueOf(commands[1]); // make sure second arg is an integer
					if (socket.isConnected()) {
						dOut = new DataOutputStream(socket.getOutputStream());
						dOut.writeUTF(command);
						dOut.flush();
						in = new BufferedInputStream(socket.getInputStream(), 2048);
						while(in.available() == 0){} // Wait till recv data
						while (in.available() > 0) {
	
							// Read the byte and
							// convert the integer to character
							char c = (char)in.read();
							// Print the characters
							System.out.print(c);
						}
						System.out.println();
					}
				}
				catch(Exception e){
					System.out.println(e);
				}
				socket.close();
			}
			else if (command.equals("stop")){
				if (player != null){
					player.stop();
				}
			}
			else {
				System.err.println("ERROR: unknown command");
			}
		}

		System.out.println("Client: Exiting");
	}
}
