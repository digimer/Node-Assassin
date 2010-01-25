#include <Ethernet.h>	// Arduino's ethernet library.
#include <ctype.h>	// Library for testing and character manipulation.
#include <stdint.h>	// Library for standard integer types (guarantees the size of an int).

/*
Author:
 - Digimer
 
Version: 1.0.2
 - Release: 2010-01-24

License:
 - The GNU GPL v2.0

Thanks:
 - Hacklab.TO:       The idea for this device was born there.
 - Christopher Olah; Came up with the name "Node Assassin".
 - Mark Loit:        Taught me enough C to write version 1.0 of NaOS!

Bugs:
 - None known at this time.

Protocol:
 - Telnet (or similar) to the IP and Port set below.
  - To query the state of the nodes, send:
    - 00:0
  - The integer after the '00:' is reserved for future queries.
 - To set the state of a node, send:
  - XX:Y
  - XX is the zero-padded node ID number; 01, 02, 03, 04 or 05
  - Y  is the state to set
    - 0 fences the requested node.
    - 1 releases the fence and lets the node boot.
 - Example:
   - To fence Node 01, send:
     - 01:0
   - To release the fence and thus let the node boot, send:
     - 01:1
 - Sending any other non-standard command will generate an error message and no
   action will be taken.
  
Note:
 - This device implements NO security. You MUST install in on a private, secure
   intranet or similar back channel. Installing it on the same LAN as the
   storage devices is advised. 
 - Changing this file will have no effect until the program is recompiled and
   uploaded to the Node Assassin.
*/

// MAC Address; Array of six bytes.
byte mac[] = { 0xDE, 0xAD, 0xBE, 0xEF, 0xFE, 0xEF };
// Arduino IP, netmask and gateway.
byte ip[] = { 192, 168, 1, 66 };
// Netmask defaults to 255.255.255.0.
byte nm[] = { 255, 255, 255, 0 };
// Default gateway defaults to IP with the last octal set to 1.
byte dg[] = { 192, 168, 1, 1 };

// This is the port that I will listen on.
#define PORT 238

// Setup the server.
Server server = Server(PORT);

// Setup my digital out pins.
// CONSTRAINT: Output pins must be ssigned sequentially
#define NODECOUNT    5
#define FIRSTNODEPIN 2

// My function prototypes.
void printError(const char *message);
void printMessage(const char *message);

// Setup the Arduino on boot.
void setup()
{
	// Setup the IP info.
	Ethernet.begin(mac, ip, dg, nm);
	
	// Print the serial port welcom message.
	Serial.begin(9600);
	Serial.println("Node Assassin: 'Ariel' now listening for orders.");
	
	// Iterator to setup the digital pins to output and to set them
	// initially to LOW.
	for (int pin = FIRSTNODEPIN; pin < (FIRSTNODEPIN+NODECOUNT); pin++)
	{
		pinMode(pin, OUTPUT);
		digitalWrite(pin, LOW);
	}
	
	// Start the server listening for connections.
	server.begin();
}

// And GO!
void loop()
{	
	// Variables
	uint8_t node=0;		// The node I will work on.
	uint8_t state=0;	// The (new?) state of the node.
	char nodeASCII[3];	// ASCII representation of node number. This is
				// '3' because of 'first char' + 'second char' + terminating <NUL>
	char command[5];	// 4 chars "XX:Y" + <NUL>
	int  index = 0;		// Just an index to increment and reset in loops.
	
	// Start the network library.
	Client client=server.available();
	if (client)
	{
		// process the input in a line-based manner, allowing for 1 command per line
		while ((-1 != (command[index] = client.read()) ) && (5 > index))
		{
			// exit at the end of line
			if( ('\n' == command[index]) || ('\r' == command[index]) )
			{
				break; // EOL found, break out of the while loop.
			}
			index++; // advance the index.
		}
		// on a valid line the above while loop will exit with index == 4
		
		// If there is no message, nothing to do but exit. 
		// Coding note: By putting 0 first, I can never accidentally
		// set the variable to '0' with an accidental single-equal.
		if (0 == index)
		{
			return;
		}
		
		// sanity check on length
		if (4 > index)
		{
			printMessage("Message too short. Format is 'XX:Y' where 'XX' is the zero-padded node number and Y is the state to set.\n");
			return;
		}
		
		// Spool off whatever is left in the buffer/line in case it was a string longer than 4.
		if (5 == index)
		{
			char ch;
			printMessage("Message too long. Format is 'XX:Y' where 'XX' is the zero-padded node number and Y is the state to set.\n");
			while (-1 != (ch = client.read()) )
			{
				// exit at the end of line
				if( ('\n' == ch) || ('\r' == ch) )
				{
					break; // break out of the while loop
				}
			}
			return;
		}
		
		// <NUL> terminate the string
		command[index] = 0;
		
		// Parse the string; Error if anything isn't right.
		// Make sure we have a colon in the right location
		if (':' != command[2])
		{
			// Error
			printError(command);
			return;
		}
		// Make sure the other characters are digits
		if (!isdigit(command[0]) || !isdigit(command[1]) || !isdigit(command[3]))
		{
			// Error
			printError(command);
			return;
		}
		
		// No need to check for the terminator or newline at the end,
		// that was taken care of in the read loop.
		// Do the math to turn the ASCII node number into a binary
		// value.
		node=command[0]-'0';	// First digit convertion (ie: '1' (0x31)-'0' (0x30) = 0x01 = "0000 0001 (dec. 1)").
		node*=10;		// Shift to the first base-10 position.
		node+=command[1]-'0';	// Now 'node' contains the binary version of the ASCII two-digit value read off of telnet.
		
		// Do the math to turn the state number into a binary value.
		state=command[3]-'0';	// Now 'state' contains the binary version.
		
		// copy the ASCII node name for the response messages [so we don't have to convert it back later]
		nodeASCII[0] = command[0];
		nodeASCII[1] = command[1];
		nodeASCII[2] = 0; // <NUL> terminate it
		
		// Check the node.
		if (node > NODECOUNT)
		{
			// Node number can't be higher than NODECOUNT on this model.
			
			// Make my NODECOUNT an ASCII value so that I can print it by reversing the convertion to binary done earlier.
			// the below 2 lines will be converted by the compiler, so there is no run-time penalty for the math here
			nodeASCII[0]=(NODECOUNT/10)+'0';	// Move from the 'tens' posiition into the '1' position and add '0' to get the ASCII value.
			nodeASCII[1]=(NODECOUNT%10)+'0';	// The modulous returns my real one position.
			// nodeASCII was <NUL> terminated earlier at 3, so no need to do it again here
			
			printMessage("This fence only supports up to "); printMessage(nodeASCII); printMessage("nodes.\n");
			return;
		}
		
		// Check that the requested state is sane.
		if (state > 1)
		{
			// Node number can't be higher than '1' on this model.
			printMessage("Invalid state received. Send 'XX:0' to kill a node, XX:1 to release a node\n");
			return;
		}
		
		// Check is this is an info request.
		if (0 == node)
		{
			// Send states
			printMessage("Node states: \n");
			
			// Make my NODECOUNT an ASCII value so that I can print it by reversing the convertion to binary done earlier.
			// the below 2 lines will be converted by the compiler, so there is no run-time penalty for the math here
			nodeASCII[0]=(NODECOUNT/10)+'0';	// Move from the 'tens' posiition into the '1' position and add '0' to get the ASCII value.
			nodeASCII[1]=(NODECOUNT%10)+'0';	// The modulous returns my real one position.
			// nodeASCII was <NUL> terminated earlier at 3, so no need to do it again here
			
			printMessage("- Max Node: "); printMessage(nodeASCII); printMessage("\n");
			
			/*
			Future optimization:
			The division and modulus in the loop can be expensive
			processing wise, as the compiler cannot do the
			calculation at compile time. As we are simply
			itteratively looping and incrementing, we can increment
			the ASCII value directly, removing the need for any
			division or modulus operations.
			 */
			
			for (int i=0; i<NODECOUNT; i++)
			{
				// 'i' is the current, zero-based node number.
				nodeASCII[0]=((i+1)/10)+'0';	// The '+1' makes the node 1-based instead of 0-based.
				nodeASCII[1]=((i+1)%10)+'0';	// The modulous returns my real one position.
				
				state = digitalRead(i+FIRSTNODEPIN);	// i + pin offset.
				printMessage("- Node "); printMessage(nodeASCII); printMessage((LOW == state) ? ": Running\n" : ": Fenced!\n");
			}
			printMessage("End Message.\n");
			
			return;
		}
		
		// Subtract 1 from node to make it zero-based.
		node--;
		// Set the pin based on whether 'state' is '0' or not.
		digitalWrite(node+FIRSTNODEPIN, (0 == state) ? HIGH : LOW);
		printMessage("Node "); printMessage(command); printMessage((1 == state) ? ": Now running.\n" : ": Now Fenced!\n");
	}
}

// The error handling function.
void printError(const char *message)
{
	// Print the message to the serial bus and the client.
	// I know this is dirty but it represents the one line string.
	printMessage("Bad command: [" ); printMessage(message); printMessage("]\n" );
}

void printMessage(const char *message)
{
	// Print the message to the serial bus and the client.
	Serial.print(message);
	server.write(message);
}
