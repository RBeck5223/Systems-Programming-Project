////////////////////////////////////////////////////////////////////////////////
//
//  File          : hdd_client.c
//  Description   : This is the client side of the CRUD communication protocol.
//
//   Author       : Patrick McDaniel
//  Last Modified : Thu Oct 30 06:59:59 EDT 2014
//

// Include Files
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <arpa/inet.h>
#include <errno.h>
#include <string.h>
#include <unistd.h>
#include <assert.h>
#include <stdint.h>

// Project Include Files
#include <hdd_network.h>
#include <cmpsc311_log.h>
#include <cmpsc311_util.h>
#include <hdd_driver.h>

int sfd = -1;                // socket file descriptor
static struct sockaddr_in a;  // socket address

///////////////////////////////////////////////////////////////////////////////
//  get_op: extracts op from HddBitCmd

int get_op(HddBitCmd command)
{
	command = (command >> 62);   // shift op to LSB
    int op = (command & 3);      // AND with mask

	return op;
}

///////////////////////////////////////////////////////////////////////////////
//  get_flag: extracts flag from HddBitCmd

int get_flag(HddBitCmd command)
{
	command = (command >> 33);   // shift flag to LSB
    int flag = (command & 7);      // AND with mask

	return flag;
}

///////////////////////////////////////////////////////////////////////////////
//  get_size: extracts block size from HddBitResp

int get_size(HddBitResp response)
{
	response = (response >> 36);   // shift size to LSB
    int size = (response & 67108863);      // AND with mask

	return size;
}





////////////////////////////////////////////////////////////////////////////////
//
// Function     : hdd_client_operation
// Description  : This the client operation that sends a request to the CRUD
//                server.   It will:
//
//                1) if INIT make a connection to the server
//                2) send any request to the server, returning results
//                3) if CLOSE, will close the connection
//
// Inputs       : cmd - the request opcode for the command
//                buf - the block to be read/written from (READ/WRITE)
// Outputs      : the response structure encoded as needed

HddBitResp hdd_client_operation(HddBitCmd cmd, void *buf) {

    //int sfd = -1;

	if (get_flag(cmd) == HDD_INIT)  // check if initializing
	{
		 printf("INIT flagged\n");

        // create socket // 

        sfd = socket(PF_INET, SOCK_STREAM, 0);

        if (sfd == -1)
        {
            printf("Error on socket creation\n");   // make sure socket created successfully
            return(-1);
        }
        	
	

	    // specify address //

	    //struct sockaddr_in a;

	    a.sin_family = AF_INET;
	    a.sin_port = htons(HDD_DEFAULT_PORT);

	    if (inet_aton(HDD_DEFAULT_IP, &(a.sin_addr)) == 0)
		   return -1;

	    // connect //

	    if (connect(sfd, (const struct sockaddr *)&a, 
		sizeof(struct sockaddr)) == -1)
	    {
		    printf("Error connecting to server\n");   // check for server connection
		    return -1;
	    }
	}

	//printf("made it this far\n");
	

	// send //

	printf("attempting send\n");

	HddBitCmd *command_nbo = malloc(sizeof(HddBitCmd));  // allocate size for command pointer
	*command_nbo = htonll64(cmd);                          // convert to network byte order

	int written = write(sfd, command_nbo, sizeof(HddBitCmd)); // write data
	// check
	
	while (written < sizeof(HddBitCmd))   // make sure all bytes written
	{
		
		written += write(sfd, &((char*)command_nbo)[written], sizeof(HddBitCmd) - written);
	}

	free(command_nbo);

	//  see if buffer also needed   //

	if ((get_op(cmd) == HDD_BLOCK_CREATE || get_op(cmd) == HDD_BLOCK_OVERWRITE) &&
		(get_flag(cmd) == HDD_NULL_FLAG || get_flag(cmd) == HDD_META_BLOCK))
	{
		
		 int written_buf = write(sfd, buf, get_size(cmd));  // send buffer
		
		 
		while (written_buf < get_size(cmd))  // make sure all bytes sent
	    {
	    	
	    	written_buf += write(sfd, &(((char*)buf)[written_buf]), get_size(cmd) - written_buf);
	    }

	}

	

	// receive //

	

	HddBitResp *resp = malloc(sizeof(HddBitResp)); // allocate size for response pointer

	int red = read(sfd, resp, sizeof(HddBitResp));  // read data
	//check

	while (red < sizeof(HddBitResp))      // make sure all bytes read
	{
		
		red += read(sfd, &((char*)resp)[red], sizeof(HddBitResp) - red);
	}

	HddBitResp resp_hbo = ntohll64(*resp);  // convert back to host byte order
	free(resp);

	if (get_op(resp_hbo) == HDD_BLOCK_READ)   // check if buffer is needed
	{
        

		int read_buf = read(sfd, buf, get_size(resp_hbo)); // read biffer
		// check

		while (read_buf < get_size(resp_hbo))    // make sure all bytes are read
	    {
	    	
	    	read_buf += read(sfd, &((char*)buf)[read_buf], get_size(resp_hbo) - read_buf);
	    }
	}

	

	HddBitResp response = resp_hbo; // used as return value

	// close if necessary //

	if (get_flag(cmd) == HDD_SAVE_AND_CLOSE)   // close socket
	{
		close(sfd);
		sfd = -1;
		printf("closed\n");
	}

	return response;
    
}


