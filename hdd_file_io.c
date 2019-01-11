////////////////////////////////////////////////////////////////////////////////
//
//  File           : hdd_file_io.c
//  Description    : 
//
//  Author         :
//  Last Modified  : 
//

// Includes
#include <malloc.h>
#include <string.h>

// Project Includes
#include <hdd_file_io.h>
#include <hdd_driver.h>
#include <cmpsc311_log.h>
#include <cmpsc311_util.h>
#include <hdd_network.h>

// Defines
#define CIO_UNIT_TEST_MAX_WRITE_SIZE 1024
#define HDD_IO_UNIT_TEST_ITERATIONS 10240


// Type for UNIT test interface
typedef enum {
	CIO_UNIT_TEST_READ   = 0,
	CIO_UNIT_TEST_WRITE  = 1,
	CIO_UNIT_TEST_APPEND = 2,
	CIO_UNIT_TEST_SEEK   = 3,
} HDD_UNIT_TEST_TYPE;

char *cio_utest_buffer = NULL;  // Unit test buffer
// Global data structure to keep track of file data

typedef struct  {
        uint32_t loc;
        uint32_t bid;
        //uint16_t fh;
        char name[MAX_FILENAME_LENGTH];
        int open;
        uint32_t size;
} File; 

 File files[MAX_HDD_FILEDESCR];  // array of file objects

int init = 1;     // flag for initialization


///////////////////////////////////////////////////////////////////////////////

// Construct: receives file parameters and formats them into 64-bit HddBitCmd

uint64_t construct (uint32_t bid, int r, int flags, int32_t block_size, int op)
{
        uint64_t command = 0;   // initialize to zero
        command += op;
        uint64_t shift_command = (command << 62); // shift opcode to bits 62-63
        uint64_t temp1 = 0;
        temp1 += block_size;
        uint64_t shift_temp1 = (temp1 << 36);  // shift block size into position
        command = (shift_command | shift_temp1);  // OR opcode and block size together
        uint64_t temp2 = 0;
        temp2 += flags;
        uint64_t shift_temp2 = (temp2 << 33);   // shift flags into position
        command = (command | shift_temp2);   // OR it onto the command
        uint64_t temp3 = 0;
        temp3 += bid;                // append the block ID
        command = (command | temp3);   // OR it onto the command

        return command;                 // return 64-bit command
}

///////////////////////////////////////////////////////////////////////////////
//  get_response: extracts R from HddBitResp

int get_response(uint64_t response)
{
    response = (response >> 32);     // shift R to LSB
    uint64_t mask = 1;               // bit mask
    uint64_t r = (mask & response);   // AND with mask to get R

    if (r  == 0)
    {
       return 0;  
    }

    else                              // return R as int
    {
       return 1;
    }
}

///////////////////////////////////////////////////////////////////////////////

//  get_bid: extracts block id from HddBitResp 


uint32_t get_bid(uint64_t response)
{
    uint64_t mask = 4294967295;  // set all 32 LS bits to 1
    return  (mask & response);   // AND with response to get block id
   
}




//
// Implementation

////////////////////////////////////////////////////////////////////////////////
//
// Function     : hdd_format
// Description  : Initializes device if necessary, deletes all blocks, creates meta block
//
// Inputs       : void
// Outputs      : 0 on success, -1 on failure
//
uint16_t hdd_format(void) {
	
	if (init != 0)     // initialize device if needed
	{
		HddBitCmd initialize = construct(0, 0, HDD_INIT, 0, HDD_DEVICE);
		HddBitResp init_resp = hdd_client_operation(initialize, NULL);

		init = get_response(init_resp);
	}
             

        if (init != 0)               // make sure init was successful
        	return -1;

    // send format request //
    
    HddBitCmd format = construct(0, 0, HDD_FORMAT, 0, HDD_DEVICE);
    HddBitResp format_resp = hdd_client_operation(format, NULL);

    if (get_response(format_resp) == 1)  // make sure format request was successful
        return -1;

    // clear array //

    for (int i = 0; i < MAX_HDD_FILEDESCR; i++)
    {
    	files[i].loc = 0;
    	files[i].bid = 0;
    	strcpy(files[i].name, " ");
    	files[i].open = 0;
    	files[i].size = 0;
    }

    // create meta block //

    HddBitCmd create_meta = construct(0, 0, HDD_META_BLOCK, MAX_HDD_FILEDESCR*sizeof(File), HDD_BLOCK_CREATE);

    HddBitResp meta_resp = hdd_client_operation(create_meta, files); // pass array of files, save to meta block

    if (get_response(meta_resp) == 1)  // make sure format request was successful
        return -1;

    return 0; 
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : hdd_mount
// Description  : intitializes device if needed, reads data from meta block
//
// Inputs       : none
// Outputs      : 0 on success, -1 on failure
//
uint16_t hdd_mount(void) {
	
	if (init != 0)     // initialize device if needed
	{
		HddBitCmd initialize = construct(0, 0, HDD_INIT, 0, HDD_DEVICE);
		HddBitResp init_resp = hdd_client_operation(initialize, NULL);

		init = get_response(init_resp);
	}
        if (init != 0)               // make sure init was successful
        	return -1;

    // read from meta block into data structure //

    HddBitCmd read_meta = construct(0, 0, HDD_META_BLOCK, MAX_HDD_FILEDESCR*sizeof(File), HDD_BLOCK_READ );
    
    HddBitResp read_resp = hdd_client_operation(read_meta, files);

    if (get_response(read_resp) == 1)  // make sure read was successful
    	return -1;

    return 0;
}


////////////////////////////////////////////////////////////////////////////////
//
// Function     : hdd_unmount
// Description  : saves current state of data structure to meta block, closes the device
//
// Inputs       : none
// Outputs      : 0 on success, -1 on failure
//
uint16_t hdd_unmount(void) {

	// save data to meta block //

	HddBitCmd save_meta = construct(0, 0, HDD_META_BLOCK, MAX_HDD_FILEDESCR*sizeof(File), HDD_BLOCK_OVERWRITE);
    
    HddBitResp save_meta_resp = hdd_client_operation(save_meta, files);	

    if (get_response(save_meta_resp) == 1)  // make sure save was successful
    	return -1;

    // save and close device // 

    HddBitCmd save_close = construct(0, 0, HDD_SAVE_AND_CLOSE, 0, HDD_DEVICE);
    
    HddBitResp save_close_resp = hdd_client_operation(save_close, NULL);	

    if (get_response(save_close_resp) == 1)  // make sure save/close was successful
    	return -1;

    init = 1;   //uninitialize

    return 0;	
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : hdd_open
// Description  : initializes driver, opens a file, and sets needed metadata
//
// Inputs       : filename
// Outputs      : unique integer file handle
//
int16_t hdd_open(char *path) {
	
    if (init != 0)     // initialize device if needed
	{
		HddBitCmd initialize = construct(0, 0, HDD_INIT, 0, HDD_DEVICE);
		HddBitResp init_resp = hdd_client_operation(initialize, NULL);

		init = get_response(init_resp);
	}
        if (init != 0)               // make sure init was successful
        	return -1;

        if (strlen(path) <= 0 || strlen(path) > MAX_HDD_FILEDESCR) // make sure filename is within range
        	return -1;

        uint16_t fh = 0;

        for (int i = 0; i < MAX_HDD_FILEDESCR; i++) //search the array of structs for the file
        {
        	if (strcmp(files[fh].name, path) != 0)
        		fh++;
        }

        if (fh == MAX_HDD_FILEDESCR)   // file doesn't exist
        {
        	fh = 0;

        	while (strcmp(files[fh].name, " ") != 0) // find available spot in array of files
        		fh++;

        	if (fh == MAX_HDD_FILEDESCR)  // make sure file array isn't full 
        		return -1;

        	files[fh].loc = 0;
        	files[fh].bid = 0;
        	strcpy(files[fh].name, path);     // set file metadata
        	files[fh].open = 1;
        	files[fh].size = 0;
        }

        else // file already exists
        {
        	if (files[fh].open == 1)   // make sure file isn't already open
        		return -1;

        	files[fh].loc = 0;         // set position to 0
        	files[fh].open = 1;       // open the file
        }

    return fh;                        // return the file handle
}





////////////////////////////////////////////////////////////////////////////////
//
// Function     : hdd_close
// Description  : closes file and deletes all its data
//
// Inputs       : file handle
// Outputs      : -1 on failure, 0 on success
//
int16_t hdd_close(int16_t fh) {
	

       if (files[fh].open == 0)
       {
          return -1;       // error if file not open
       }

       files[fh].open = 0;    // close the file
       files[fh].loc = 0;    // reset seek

       return 0;
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : hdd_read
// Description  : reads a count number of bytes from current seek position, 
//                places them in data buffer
//
// Inputs       : file handle, data buffer, byte count
// Outputs      : -1 on failure, # of bytes read on success
//
int32_t hdd_read(int16_t fh, void * data, int32_t count) {

	    if (init != 0)     // initialize device if needed
	    {
		    HddBitCmd initialize = construct(0, 0, HDD_INIT, 0, HDD_DEVICE);
		    HddBitResp init_resp = hdd_client_operation(initialize, NULL);

		    init = get_response(init_resp);
	    }

	
         char *buf = malloc(files[fh].size); // create buffer for data 
         HddBitCmd command6 = construct(files[fh].bid, 0, 0, files[fh].size, HDD_BLOCK_READ);   // command to read data from block 

         HddBitResp response6 = hdd_client_operation(command6, buf);  // read data into buffer

         if (get_response(response6) == 1)
         {
             free(buf);           // failure if R = -1
             return -1;
         }

//     if count is greater than bytes available, just read what's available

         if (count > files[fh].size - files[fh].loc)
         {
            memcpy(data, &buf[files[fh].loc], files[fh].size - files[fh].loc);        
         }
   
//     otherwise read count bytes

         else
         {
            memcpy(data,&buf[files[fh].loc], count);
         }

         free(buf);    // free memory

         if (count > files[fh].size - files[fh].loc)
         {
             int bytes = files[fh].size - files[fh].loc;
             files[fh].loc = files[fh].size;  // return bytes read and update seek position
             return bytes;
         }

         else
         {
            files[fh].loc += count;
            return count;        // return bytes read and update position
         }
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : hdd_write
// Description  : writes count bytes from data buffer at file's seek position
//
// Inputs       : file handle, data buffer, byte count
// Outputs      : -1 on failure, number of bytes written on success
//
int32_t hdd_write(int16_t fh, void *data, int32_t count) {

//       Case for previously non-existent block
	
      if (files[fh].bid == 0)
      {
         HddBitCmd  command = construct( 0, 0, 0, count, HDD_BLOCK_CREATE);
         HddBitResp response = hdd_client_operation(command, data);  // create new block, write data to it
 
         int r = get_response(response);
         
         if (r == 0)
         {
            files[fh].loc = count;
            files[fh].bid = get_bid(response); // if successful command, update file info and return count
            files[fh].size = count;
 
            return count;
         }
         
         else
         {
           
             return -1;   // error
         }
      }  
      else    // if block already exists
      {

             // if block is too small to hold old + new data:

         if ((files[fh].loc + count) > files[fh].size)
         {
            char *oldbuf = malloc(files[fh].size); // buffer to hold old data
            HddBitCmd command2 = construct(files[fh].bid, 0, 0, files[fh].size, HDD_BLOCK_READ);
            HddBitResp response2 = hdd_client_operation(command2, oldbuf);  // read old data into buffer

            if ((get_response(response2)) == 1)
            {
                free(oldbuf);
                return -1;         // error
            }

            char *newbuf = malloc(files[fh].loc + count);  // buffer to hold new data+ old data
            memcpy(newbuf, oldbuf, files[fh].size); // copy old data into buffer
           memcpy(&newbuf[files[fh].loc], data, count);   // copy new data into buffer


            HddBitCmd command3 = construct(0, 0, 0, (files[fh].loc + count), HDD_BLOCK_CREATE);

            HddBitResp response3 = hdd_client_operation(command3, newbuf);  // create new block for all the data

            if (get_response(response3) == 1)
            {
               return -1;     // error
            }

            HddBitCmd delete = construct(files[fh].bid, 0, 0, 0, HDD_BLOCK_DELETE);   // delete the file
            HddBitResp delete_resp = hdd_client_operation(delete, NULL);

            if (get_response(delete_resp) == 1)   // make sure delete was succesful
            	return -1;
     
            free(oldbuf);
            free(newbuf);       // free memory


            files[fh].size = files[fh].loc + count;
            files[fh].loc += count;
            files[fh].bid = get_bid(response3);   // update file info
            
         
            return count;                    // return bytes written
         }

         else   
                     // if new data will fit into buffer
         {
            char *oldbuf = malloc(files[fh].size); // create buffer for old data
            HddBitCmd command4 = construct(files[fh].bid, 0, 0,files[fh].size, HDD_BLOCK_READ); 
            HddBitResp response4 = hdd_client_operation(command4, oldbuf); // read old data into buffer

            if (get_response(response4) == 1)
            {
                free(oldbuf);
                return -1;      // error
            }

            memcpy(&oldbuf[files[fh].loc], data, count); // add new data to buffer

            HddBitCmd command5 = construct(files[fh].bid, 0, 0, files[fh].size, HDD_BLOCK_OVERWRITE);
            HddBitResp response5 = hdd_client_operation(command5, oldbuf); // overwrite block to include new data

            free(oldbuf);    // free memory
       
           if (get_response(response5) == 1)
           {
              return -1;      // check for error
           }

           files[fh].loc += count;  // update seek position


           return count;      // return bytes written
         }   
            
      }
}

////////////////////////////////////////////////////////////////////////////////
//
// Function     : hdd_seek
// Description  : changes seek position of file
//
// Inputs       : file handle, new position
// Outputs      : -1 on failure, 0 on success
//
int32_t hdd_seek(int16_t fh, uint32_t loc) {

	    if (init != 0)     // initialize device if needed
	    {
		    HddBitCmd initialize = construct(0, 0, HDD_INIT, 0, HDD_DEVICE);
		    HddBitResp init_resp = hdd_client_operation(initialize, NULL);

		    init = get_response(init_resp);
	    }

        if (loc < 0 || loc > files[fh].size)
        {
           return -1;        // check range
        }

        files[fh].loc = loc;     // update seek position

        return 0;
}







////////////////////////////////////////////////////////////////////////////////
//
// Function     : hddIOUnitTest
// Description  : Perform a test of the HDD IO implementation
//
// Inputs       : None
// Outputs      : 0 if successful or -1 if failure

int hddIOUnitTest(void) {

	// Local variables
	uint8_t ch;
	int16_t fh, i;
	int32_t cio_utest_length, cio_utest_position, count, bytes, expected;
	char *cio_utest_buffer, *tbuf;
	HDD_UNIT_TEST_TYPE cmd;
	char lstr[1024];

	// Setup some operating buffers, zero out the mirrored file contents
	cio_utest_buffer = malloc(HDD_MAX_BLOCK_SIZE);
	tbuf = malloc(HDD_MAX_BLOCK_SIZE);
	memset(cio_utest_buffer, 0x0, HDD_MAX_BLOCK_SIZE);
	cio_utest_length = 0;
	cio_utest_position = 0;

	// Format and mount the file system
	if (hdd_format() || hdd_mount()) {
		logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : Failure on format or mount operation.");
		return(-1);
	}

	// Start by opening a file
	fh = hdd_open("temp_file.txt");
	if (fh == -1) {
		logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : Failure open operation.");
		return(-1);
	}

	// Now do a bunch of operations
	for (i=0; i<HDD_IO_UNIT_TEST_ITERATIONS; i++) {

		// Pick a random command
		if (cio_utest_length == 0) {
			cmd = CIO_UNIT_TEST_WRITE;
		} else {
			cmd = getRandomValue(CIO_UNIT_TEST_READ, CIO_UNIT_TEST_SEEK);
		}
		logMessage(LOG_INFO_LEVEL, "----------");

		// Execute the command
		switch (cmd) {

		case CIO_UNIT_TEST_READ: // read a random set of data
			count = getRandomValue(0, cio_utest_length);
			logMessage(LOG_INFO_LEVEL, "HDD_IO_UNIT_TEST : read %d at position %d", count, cio_utest_position);
			bytes = hdd_read(fh, tbuf, count);
			if (bytes == -1) {
				logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : Read failure.");
				return(-1);
			}

			// Compare to what we expected
			if (cio_utest_position+count > cio_utest_length) {
				expected = cio_utest_length-cio_utest_position;
			} else {
				expected = count;
			}
			if (bytes != expected) {
				logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : short/long read of [%d!=%d]", bytes, expected);
				return(-1);
			}
			if ( (bytes > 0) && (memcmp(&cio_utest_buffer[cio_utest_position], tbuf, bytes)) ) {

				bufToString((unsigned char *)tbuf, bytes, (unsigned char *)lstr, 1024 );
				logMessage(LOG_INFO_LEVEL, "CIO_UTEST R: %s", lstr);
				bufToString((unsigned char *)&cio_utest_buffer[cio_utest_position], bytes, (unsigned char *)lstr, 1024 );
				logMessage(LOG_INFO_LEVEL, "CIO_UTEST U: %s", lstr);

				logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : read data mismatch (%d)", bytes);
				return(-1);
			}
			logMessage(LOG_INFO_LEVEL, "HDD_IO_UNIT_TEST : read %d match", bytes);


			// update the position pointer
			cio_utest_position += bytes;
			break;

		case CIO_UNIT_TEST_APPEND: // Append data onto the end of the file
			// Create random block, check to make sure that the write is not too large
			ch = getRandomValue(0, 0xff);
			count =  getRandomValue(1, CIO_UNIT_TEST_MAX_WRITE_SIZE);
			if (cio_utest_length+count >= HDD_MAX_BLOCK_SIZE) {

				// Log, seek to end of file, create random value
				logMessage(LOG_INFO_LEVEL, "HDD_IO_UNIT_TEST : append of %d bytes [%x]", count, ch);
				logMessage(LOG_INFO_LEVEL, "HDD_IO_UNIT_TEST : seek to position %d", cio_utest_length);
				if (hdd_seek(fh, cio_utest_length)) {
					logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : seek failed [%d].", cio_utest_length);
					return(-1);
				}
				cio_utest_position = cio_utest_length;
				memset(&cio_utest_buffer[cio_utest_position], ch, count);

				// Now write
				bytes = hdd_write(fh, &cio_utest_buffer[cio_utest_position], count);
				if (bytes != count) {
					logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : append failed [%d].", count);
					return(-1);
				}
				cio_utest_length = cio_utest_position += bytes;
			}
			break;

		case CIO_UNIT_TEST_WRITE: // Write random block to the file
			ch = getRandomValue(0, 0xff);
			count =  getRandomValue(1, CIO_UNIT_TEST_MAX_WRITE_SIZE);
			// Check to make sure that the write is not too large
			if (cio_utest_length+count < HDD_MAX_BLOCK_SIZE) {
				// Log the write, perform it
				logMessage(LOG_INFO_LEVEL, "HDD_IO_UNIT_TEST : write of %d bytes [%x]", count, ch);
				memset(&cio_utest_buffer[cio_utest_position], ch, count);
				bytes = hdd_write(fh, &cio_utest_buffer[cio_utest_position], count);
				if (bytes!=count) {
					logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : write failed [%d].", count);
					return(-1);
				}
				cio_utest_position += bytes;
				if (cio_utest_position > cio_utest_length) {
					cio_utest_length = cio_utest_position;
				}
			}
			break;

		case CIO_UNIT_TEST_SEEK:
			count = getRandomValue(0, cio_utest_length);
			logMessage(LOG_INFO_LEVEL, "HDD_IO_UNIT_TEST : seek to position %d", count);
			if (hdd_seek(fh, count)) {
				logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : seek failed [%d].", count);
				return(-1);
			}
			cio_utest_position = count;
			break;

		default: // This should never happen
			CMPSC_ASSERT0(0, "HDD_IO_UNIT_TEST : illegal test command.");
			break;

		}

	}

	// Close the files and cleanup buffers, assert on failure
	if (hdd_close(fh)) {
		logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : Failure close close.", fh);
		return(-1);
	}
	free(cio_utest_buffer);
	free(tbuf);

	// Format and mount the file system
	if (hdd_unmount()) {
		logMessage(LOG_ERROR_LEVEL, "HDD_IO_UNIT_TEST : Failure on unmount operation.");
		return(-1);
	}

	// Return successfully
	return(0);
}
