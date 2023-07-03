/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */
#include "csapp.h"

typedef struct stock_item* stock_link;

// Definition of the stock_item structure
typedef struct stock_item {
	int stock_id;       // Unique identifier for the stock item
	int left_stock;     // Number of items left in stock
	int stock_price;    // Price of the stock item
	int stock_readcnt;  // Number of readers accessing the stock item
	sem_t mutex;        // Semaphore for controlling mutual exclusion
	sem_t writer;       // Semaphore for controlling write access
	stock_link left;    // Pointer to the left child in a binary tree structure
	stock_link right;   // Pointer to the right child in a binary tree structure
	stock_link next;    // Pointer to the next item in a temporary list
} STOCK_ITEM;

// Definition of the fd_item structure
typedef struct fd_item* fd_link;
typedef struct fd_item {
	int fd;          // File descriptor
	fd_link next;    // Pointer to the next fd_item in the list
} FD_ITEM;

int total_stock_num = 0;     // Variable to store the total number of stock items
STOCK_ITEM* stock_head = NULL;   // Pointer to the head of the stock_item linked list
STOCK_ITEM* stock_tail = NULL;   // Pointer to the tail of the stock_item linked list
STOCK_ITEM* root = NULL;         // Pointer to the root of the binary tree structure

FD_ITEM* fd_head = NULL;   // Pointer to the head of the fd_item linked list
FD_ITEM* fd_tail = NULL;   // Pointer to the tail of the fd_item linked list

// Signal handler for the SIGINT signal
void sig_int_handler(int sig) {
	update_file(); // Call the update_file() function
	free_tree(root); // Free the memory occupied by the binary tree
	exit(0); // Terminate the program
}

// Function to add a file descriptor to the linked list
void fd_add(int fd) {
	// Allocate memory for a new FD_ITEM
	FD_ITEM* fd_item = (FD_ITEM*)malloc(sizeof(FD_ITEM));
	fd_item->fd = fd;
	fd_item->next = NULL;

	if (fd_head == NULL) {
		// If the fd list is empty, make both head and tail point to the new fd_item
		fd_head = fd_item;
		fd_tail = fd_item;
	}
	else {
		// Add the new fd_item to the end of the list
		fd_tail->next = fd_item;
		fd_tail = fd_item;
	}
}

// Function to delete a file descriptor from the linked list
void fd_delete(int fd) {
	FD_ITEM* ptr;
	FD_ITEM* prev_ptr = fd_head;

	for (ptr = fd_head; ptr != NULL; ptr = ptr->next) {
		if (ptr->fd == fd) {
			break; // Found the fd in the list
		}
		prev_ptr = ptr;
	}

	if (ptr == fd_head && ptr == fd_tail) {
		// If there is only one fd_item in the list, now the list becomes empty
		fd_head = NULL;
		fd_tail = NULL;
		free(ptr);
	}
	else if (ptr != fd_head && ptr != fd_tail) {
		// If the fd_item is in the middle of the list
		prev_ptr->next = ptr->next; // Set the previous node to point to the next node of ptr
		free(ptr);
	}
	else if (ptr == fd_head) {
		// If the fd_item is the head of the list
		fd_head = ptr->next; // Set the head of the fd list to the next node of ptr
		free(ptr);
	}
	else if (ptr == fd_tail) {
		// If the fd_item is the tail of the list
		fd_tail = prev_ptr; // Set the tail of the fd list to the previous node of ptr
		fd_tail->next = NULL;
		free(ptr);
	}
}

// Function to add a stock item to the linked list
void stock_add_to_list(int stock_id, int left_stock, int stock_price) {
	STOCK_ITEM* item = (STOCK_ITEM*)malloc(sizeof(STOCK_ITEM));
	item->stock_id = stock_id;
	item->left_stock = left_stock;
	item->stock_price = stock_price;
	total_stock_num++;

	if (stock_head == NULL) {
		// If the stock list is empty, make both head and tail point to the new item
		stock_head = item;
		stock_tail = item;
	}
	else {
		// Add the new item to the end of the list
		stock_tail->next = item;
		stock_tail = item;
	}
}


// Comparison function used by qsort to compare stock items based on their stock_id
int less(void* a, void* b) {
	return (*(STOCK_ITEM*)a).stock_id - (*(STOCK_ITEM*)b).stock_id;
}

// Recursive function to convert an array of stock items to a binary search tree (BST)
STOCK_ITEM* stock_arr_to_bst(STOCK_ITEM* stock_arr, int start, int end) {
	if (start > end) {
		// Base case to exit the recursion
		return NULL;
	}

	int mid = (start + end) / 2;

	STOCK_ITEM* item = (STOCK_ITEM*)malloc(sizeof(STOCK_ITEM));
	item->stock_id = stock_arr[mid].stock_id;
	item->left_stock = stock_arr[mid].left_stock;
	item->stock_price = stock_arr[mid].stock_price;
	item->next = NULL;
	item->stock_readcnt = 0;
	sem_init(&item->mutex, 0, 1);
	sem_init(&item->writer, 0, 1);

	item->left = stock_arr_to_bst(stock_arr, start, mid - 1);
	item->right = stock_arr_to_bst(stock_arr, mid + 1, end);

	return item;
}

// Function to convert the stock linked list to a binary search tree (BST)
void stock_list_to_bst() {
	STOCK_ITEM* stock_arr = (STOCK_ITEM*)malloc(sizeof(STOCK_ITEM) * total_stock_num);
	STOCK_ITEM* ptr = stock_head;
	int i;

	// Copy stock items from the linked list to the array
	for (i = 0; i < total_stock_num; i++) {
		STOCK_ITEM* prev_ptr = ptr;
		stock_arr[i].stock_id = ptr->stock_id;
		stock_arr[i].left_stock = ptr->left_stock;
		stock_arr[i].stock_price = ptr->stock_price;
		ptr = ptr->next;
		free(prev_ptr); // Free each item from the list as it is copied to the array
	}

	stock_head = NULL;
	stock_tail = NULL;

	// Sort the array in ascending order based on stock_id
	qsort(stock_arr, total_stock_num, sizeof(STOCK_ITEM), less);

	// Convert the sorted array to a binary search tree (BST)
	root = stock_arr_to_bst(stock_arr, 0, total_stock_num - 1);

	free(stock_arr); // Free the array as the BST is created using the array
}

// Function to perform an inorder traversal of the binary search tree (BST)
void inorder(char* stocks, STOCK_ITEM* ptr) {
	char temp[100];
	if (ptr) {
		// Traverse the left subtree
		inorder(stocks, ptr->left);

		// Process the current node (stock item)
		sprintf(temp, "%d %d %d\t", ptr->stock_id, ptr->left_stock, ptr->stock_price);
		strcat(stocks, temp);

		// Traverse the right subtree
		inorder(stocks, ptr->right);
	}
}

// Function to display the stock information by performing an inorder traversal and sending the data to a file descriptor
void show(int fd) {
	char stocks[MAXLINE];
	stocks[0] = '\0';
	inorder(stocks, root);
	strcat(stocks, "\n");
	// printf("%s", stocks);
	Rio_writen(fd, stocks, strlen(stocks));
}

// Function to process a buy request for a stock item
void buy(int fd, int stock_id, int stock_num) {
	STOCK_ITEM* ptr = root;
	while (ptr) {
		// Search for the stock_id in the binary search tree
		if (ptr->stock_id == stock_id) {
			break;
		}
		else if (stock_id < ptr->stock_id) {
			ptr = ptr->left;
		}
		else {
			ptr = ptr->right;
		}
	}
	if (ptr == NULL) {
		// stock_id does not exist
		Rio_writen(fd, "stock_id not exists\n", strlen("stock_id not exists\n"));
	}
	else {
		if (ptr->left_stock >= stock_num) {
			// Sufficient quantity is available
			ptr->left_stock -= stock_num;
			Rio_writen(fd, "[buy] success\n", strlen("[buy] success\n"));
		}
		else {
			// Insufficient quantity is available
			Rio_writen(fd, "Not enough left stocks\n", strlen("Not enough left stocks\n"));
		}
	}
}

// Function to process a sell request for a stock item
void sell(int fd, int stock_id, int stock_num) {
	STOCK_ITEM* ptr = root;
	while (ptr) {
		// Search for the stock_id in the binary search tree
		if (ptr->stock_id == stock_id) {
			break;
		}
		else if (stock_id < ptr->stock_id) {
			ptr = ptr->left;
		}
		else {
			ptr = ptr->right;
		}
	}
	if (ptr == NULL) {
		// stock_id does not exist
		Rio_writen(fd, "stock_id not exists\n", strlen("stock_id not exists\n"));
	}
	else {
		ptr->left_stock += stock_num;
		Rio_writen(fd, "[sell] success\n", strlen("[sell] success\n"));
	}
}

// Function to free the memory occupied by the binary tree
void free_tree(STOCK_ITEM* ptr) {
	if (ptr) {
		// Recursively free the left and right subtrees
		free_tree(ptr->left);
		free_tree(ptr->right);
		free(ptr); // Free the current node
	}
}

// Function to load stock information from a file into memory
void load_stock_to_memory() {
	FILE* fp = fopen("stock.txt", "r");
	int res;
	int stock_id;
	int left_stock;
	int stock_price;

	while (1) {
		res = fscanf(fp, "%d %d %d", &stock_id, &left_stock, &stock_price);
		if (res == EOF)
			break;
		stock_add_to_list(stock_id, left_stock, stock_price); // Store the stock information temporarily in a list
	}
	stock_list_to_bst(); // Convert the stock list into a binary search tree (BST)
	fclose(fp);
}

// Function to perform an inorder traversal of the BST and print the stock information to a file
void inorder_print(STOCK_ITEM* ptr, FILE* fp) {
	if (ptr) {
		inorder_print(ptr->left, fp);
		fprintf(fp, "%d %d %d\n", ptr->stock_id, ptr->left_stock, ptr->stock_price);
		inorder_print(ptr->right, fp);
	}
}

// Function to update the stock.txt file with the current stock information from the BST
void update_file() {
	FILE* fp = fopen("stock.txt", "w");
	inorder_print(root, fp);
	fclose(fp);
}

// Function to execute a command received from the client
void execute_command(int fd, char* command) {
	char order[MAXLINE]; // Command type
	int stock_id; // Stock ID (integer)
	int stock_num; // Number of stocks (integer)

	// Extract the command, stock ID, and stock quantity from the received command string
	sscanf(command, "%s %d %d", order, &stock_id, &stock_num);

	// Check the type of command and perform the corresponding action
	if (!strcmp(order, "show")) {
		show(fd); // Display the stock information to the client
	}
	else if (!strcmp(order, "buy")) {
		buy(fd, stock_id, stock_num); // Process a buy order for the specified stock ID and quantity
	}
	else if (!strcmp(order, "sell")) {
		sell(fd, stock_id, stock_num); // Process a sell order for the specified stock ID and quantity
	}
	else {
		Rio_writen(fd, "invalid command\n", strlen("invalid command\n")); // Send an error message to the client for an invalid command
	}
}

int main(int argc, char** argv) {
	int listenfd, connfd;
	fd_set watch_set, pending_set;
	int fd_max;
	FD_ITEM* ptr;
	socklen_t clientlen;
	struct sockaddr_storage clientaddr;  // Structure to hold client address information
	char client_hostname[MAXLINE], client_port[MAXLINE];  // Arrays to store client hostname and port
	char command[MAXLINE];  // Array to store the received command from the client
	rio_t rio;  // Rio (Robust I/O) object for buffered I/O operations

	// Check the number of command-line arguments
	if (argc != 2) {
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(0);
	}

	// Load stock information into memory
	Signal(SIGINT, sig_int_handler);
	load_stock_to_memory();

	// Initialize the file descriptor set for monitoring active connections
	FD_ZERO(&watch_set);

	// Create a listening socket and add it to the file descriptor set
	listenfd = Open_listenfd(argv[1]);
	fd_max = listenfd;
	FD_SET(listenfd, &watch_set);
	fd_add(listenfd);

	// Main server loop
	while (1) {
		pending_set=watch_set;
		Select(fd_max+1, &pending_set, NULL, NULL, NULL);

		// Iterate through the managed file descriptor list
		for (ptr = fd_head; ptr != NULL;) {
			int fd = ptr->fd;
			FD_ITEM* next_ptr = ptr->next;

			// Check if the file descriptor is set and it is not the listening socket
			if (FD_ISSET(fd, &pending_set) && fd != listenfd) {
				int n;
				Rio_readinitb(&rio, fd);
				command[0] = '\0';

				// Read the received command from the client
				if ((n = Rio_readlineb(&rio, command, MAXLINE)) != 0) {
					printf("server received %d bytes\n", n);

					// Check if the command is an "exit" command
					if (!strncmp(command, "exit", 4)) {
						update_file();
						FD_CLR(fd, &watch_set);  // Remove the connfd from the watch set
						fd_delete(fd);  // Remove the connfd from the fd list
						Close(fd);  // Close the connection

					}
					// Check if the command is an empty command (blank line)
					else if (!strcmp(command, "\n")) {
						Rio_writen(fd, "\n", strlen("\n"));
					}
					// Execute the received command
					else {
						execute_command(fd, command);
					}
				}
				else {
					update_file();
					FD_CLR(fd, &watch_set);  // Remove the connfd from the watch set
					fd_delete(fd);  // Remove the connfd from the fd list
					Close(fd);  // Close the connection
					// If no data is received, it indicates that the connection has been closed
				}
			}
			// Check if the file descriptor is set and it is the listening socket
			else if(FD_ISSET(fd, &pending_set)){
				clientlen = sizeof(struct sockaddr_storage); 
				connfd = Accept(listenfd, (SA *)&clientaddr, &clientlen);
				Getnameinfo((SA *) &clientaddr, clientlen,client_hostname, MAXLINE, client_port, MAXLINE, 0); 
				printf("Connected to (%s, %s)\n", client_hostname, client_port);
				FD_SET(connfd, &watch_set);
                fd_add(connfd);
				if(fd_max<connfd)
					fd_max=connfd;
			}
            ptr=next_ptr;
		}
	}
}
/* $end echoserverimain */
