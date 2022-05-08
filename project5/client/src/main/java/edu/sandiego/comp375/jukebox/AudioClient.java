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
		System.out.println("The args.length is: " + args.length);
		for(int i = 0; i < args.length; i++){
			System.out.println(args[i]);
		}
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
			if (command.equals("play")) {
				try {
					System.out.println("About to attemp to connect");
					Socket socket = new Socket(ip, port);
					System.out.println("Attempted to connect to socket.");
					if (socket.isConnected()) {
						System.out.println("Connected to socket");
						in = new BufferedInputStream(socket.getInputStream(), 2048);
						System.out.println("Buffered reader created");
						//THIS IS NEW CODE
						dOut= new DataOutputStream(socket.getOutputStream());
						dOut.writeUTF("play");
						dOut.flush();
						//BACK TO GIVEN CODE
						player = new Thread(new AudioPlayerThread(in));
						System.out.println("player created");
						player.start();
						System.out.println("player started");
					}
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
				System.out.println("Goodbye!");
				break;
			}
			else {
				System.err.println("ERROR: unknown command");
			}
		}

		System.out.println("Client: Exiting");
	}
}
