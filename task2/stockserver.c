/* 
 * echoserveri.c - An iterative echo server 
 */ 
/* $begin echoserverimain */

#include "csapp.h"
#define SBUFSIZE 1024

typedef struct stock_item* stock_link;
typedef struct stock_item {
	int stock_id;           // Stock ID
	int left_stock;         // Number of stocks left
	int stock_price;        // Stock price
	int stock_readcnt;      // Read count of the stock
	sem_t mutex;            // Mutex semaphore for controlling access to the stock
	sem_t writer;           // Writer semaphore for controlling write access to the stock
	stock_link left;        // Pointer to the left child in the binary tree
	stock_link right;       // Pointer to the right child in the binary tree
	stock_link next;        // Pointer to the next stock item (temporary, used when creating a list)
} STOCK_ITEM;

int total_stock_num = 0;    // Total number of stock items
STOCK_ITEM* stock_head = NULL;    // Pointer to the head of the stock list
STOCK_ITEM* stock_tail = NULL;    // Pointer to the tail of the stock list
STOCK_ITEM* root = NULL;    // Pointer to the root of the binary tree

sem_t file_mutex;    // Mutex semaphore for controlling access to the file

typedef struct {
	int* buf;         // Buffer for storing integers
	int n;            // Size of the buffer
	int front;        // Front index of the buffer
	int rear;         // Rear index of the buffer
	sem_t mutex;      // Mutex semaphore for controlling access to the buffer
	sem_t slots;      // Semaphore for available slots in the buffer
	sem_t items;      // Semaphore for available items in the buffer
} sbuf_t;
sbuf_t sbuf;

void sbuf_init(sbuf_t* sp, int n)
{
	sp->buf = Calloc(n, sizeof(int));   // Allocate memory for the buffer
	sp->n = n;                          // Set the size of the buffer
	sp->front = sp->rear = 0;           // Initialize front and rear indices
	Sem_init(&sp->mutex, 0, 1);         // Initialize the mutex semaphore with value 1
	Sem_init(&sp->slots, 0, n);         // Initialize the slots semaphore with value n
	Sem_init(&sp->items, 0, 0);         // Initialize the items semaphore with value 0
}

void sig_int_handler(int sig)
{
	update_file();            // Call a function to update the file
	sbuf_deinit(&sbuf);       // Deinitialize the bounded buffer
	free_tree(root);          // Free the memory used by the binary tree
	exit(0);                  // Exit the program
}

void sbuf_deinit(sbuf_t* sp)
{
	Free(sp->buf);   // Free the memory allocated for the buffer
}

void sbuf_insert(sbuf_t* sp, int item)
{
	P(&sp->slots);          // Wait for an available slot in the buffer
	P(&sp->mutex);          // Acquire the mutex to protect buffer access
	sp->buf[(++sp->rear) % (sp->n)] = item;   // Insert the item into the buffer at the rear index
	V(&sp->mutex);          // Release the mutex
	V(&sp->items);          // Signal that an item is available in the buffer
}

int sbuf_remove(sbuf_t *sp)
{
	int item;
	P(&sp->items);          // Wait for an available item in the buffer
	P(&sp->mutex);          // Acquire the mutex to protect buffer access
	item = sp->buf[(++sp->front) % (sp->n)];   // Remove the item from the buffer at the front index
	V(&sp->mutex);          // Release the mutex
	V(&sp->slots);          // Signal that a slot is available in the buffer
	return item;            // Return the removed item
}

void free_tree(STOCK_ITEM* ptr)
{
	if (ptr)
	{
		free_tree(ptr->left);    // Recursively free the left subtree
		free_tree(ptr->right);   // Recursively free the right subtree
		free(ptr);               // Free the memory allocated for the current node
	}
}

void stock_add_to_list(int stock_id, int left_stock, int stock_price)
{
	STOCK_ITEM* item = (STOCK_ITEM*)malloc(sizeof(STOCK_ITEM));   // Allocate memory for a new stock item
	item->stock_id = stock_id;                                    // Set the stock ID
	item->left_stock = left_stock;                                // Set the number of stocks left
	item->stock_price = stock_price;                              // Set the stock price
	total_stock_num++;                                            // Increment the total stock count

	if (stock_head == NULL)  // If the list is empty, set both head and tail to the new item
	{
		stock_head = item;
		stock_tail = item;
	}
	else  // Otherwise, append the new item to the tail of the list
	{
		stock_tail->next = item;
		stock_tail = item;
	}
}

int less(void* a, void* b)
{
	return (*(STOCK_ITEM*)a).stock_id - (*(STOCK_ITEM*)b).stock_id;  // Compare stock IDs and return the result
}

STOCK_ITEM* stock_arr_to_bst(STOCK_ITEM* stock_arr, int start, int end)
{
	if (start > end)  // Base case: If the start index exceeds the end index, return NULL
		return NULL;

	int mid = (start + end) / 2;  // Calculate the middle index

	STOCK_ITEM* item = (STOCK_ITEM*)malloc(sizeof(STOCK_ITEM));  // Allocate memory for a new stock item
	item->stock_id = stock_arr[mid].stock_id;                     // Set the stock ID
	item->left_stock = stock_arr[mid].left_stock;                 // Set the number of stocks left
	item->stock_price = stock_arr[mid].stock_price;               // Set the stock price
	item->next = NULL;
	item->stock_readcnt = 0;
	Sem_init(&item->mutex, 0, 1);    // Initialize the mutex semaphore with value 1
	Sem_init(&item->writer, 0, 1);   // Initialize the writer semaphore with value 1

	// Recursively build the binary search tree
	item->left = stock_arr_to_bst(stock_arr, start, mid - 1);   // Build the left subtree
	item->right = stock_arr_to_bst(stock_arr, mid + 1, end);    // Build the right subtree

	return item;   // Return the root of the constructed binary search tree
}

void stock_list_to_bst()
{
	STOCK_ITEM* stock_arr = (STOCK_ITEM*)malloc(sizeof(STOCK_ITEM) * total_stock_num);   // Allocate memory for an array of STOCK_ITEM objects
	STOCK_ITEM* ptr = stock_head;
	int i;

	for (i = 0; i < total_stock_num; i++)
	{
		STOCK_ITEM* prev_ptr = ptr;
		stock_arr[i].stock_id = ptr->stock_id;               // Copy the stock ID to the array
		stock_arr[i].left_stock = ptr->left_stock;           // Copy the number of stocks left to the array
		stock_arr[i].stock_price = ptr->stock_price;         // Copy the stock price to the array
		ptr = ptr->next;
		free(prev_ptr);                                      // Free the memory of the current list item as it has been moved to the array
	}

	stock_head = NULL;
	stock_tail = NULL;

	// Sort the array in ascending order based on stock IDs
	qsort(stock_arr, total_stock_num, sizeof(STOCK_ITEM), less);

	// Construct the binary search tree (BST) based on the sorted array
	root = stock_arr_to_bst(stock_arr, 0, total_stock_num - 1);

	free(stock_arr);   // Free the memory allocated for the array as the BST has been constructed
}


void inorder(char* stocks, STOCK_ITEM* ptr)
{
	char temp[100];

	if (ptr)
	{
		inorder(stocks, ptr->left);   // Traverse the left subtree

		P(&(ptr->mutex));             // Acquire the mutex semaphore to protect access to stock_readcnt
		ptr->stock_readcnt++;

		if (ptr->stock_readcnt == 1)   // If this is the first reader
			P(&(ptr->writer));         // Acquire the writer semaphore to block writers

		V(&(ptr->mutex));             // Release the mutex semaphore

		// Critical Section: Reading
		sprintf(temp, "%d %d %d	", ptr->stock_id, ptr->left_stock, ptr->stock_price);
		strcat(stocks, temp);
		// End of Critical Section: Reading

		P(&(ptr->mutex));             // Acquire the mutex semaphore to protect access to stock_readcnt
		ptr->stock_readcnt--;

		if (ptr->stock_readcnt == 0)   // If there are no more readers
			V(&(ptr->writer));         // Release the writer semaphore to unblock writers

		V(&(ptr->mutex));             // Release the mutex semaphore

		inorder(stocks, ptr->right);  // Traverse the right subtree
	}
}

void show(int fd)
{
	char stocks[MAXLINE];
	stocks[0] = '\0';
	inorder(stocks, root);          // Perform inorder traversal of the BST and store the stock information in the 'stocks' string
	strcat(stocks, "\n");
	Rio_writen(fd, stocks, strlen(stocks));   // Write the stock information to the specified file descriptor
}

void buy(int fd, int stock_id, int stock_num)
{
	STOCK_ITEM* ptr = root;
	while (ptr)   // Search for the stock_id in the binary search tree
	{
		if (ptr->stock_id == stock_id)
		{
			break;
		}
		else if (stock_id < ptr->stock_id)
		{
			ptr = ptr->left;
		}
		else
		{
			ptr = ptr->right;
		}
	}
	if (ptr == NULL)   // Stock_id does not exist
	{
		Rio_writen(fd, "stock_id not exists\n", strlen("stock_id not exists\n"));
	}
	else   // Stock_id exists
	{
		P(&(ptr->writer));   // Acquire the writer semaphore to block other writers

		// Critical Section: Writing
		if (ptr->left_stock >= stock_num)   // Sufficient stocks are available
		{
			ptr->left_stock -= stock_num;
			Rio_writen(fd, "[buy] success\n", strlen("[buy] success\n"));
		}
		else   // Insufficient stocks available
		{
			Rio_writen(fd, "Not enough left stocks\n", strlen("Not enough left stocks\n"));
		}
		// End of Critical Section: Writing

		V(&(ptr->writer));   // Release the writer semaphore to allow other writers
	}
}

void sell(int fd, int stock_id, int stock_num)
{
	STOCK_ITEM* ptr = root;
	while (ptr)   // Search for the stock_id in the binary search tree
	{
		if (ptr->stock_id == stock_id)
		{
			break;
		}
		else if (stock_id < ptr->stock_id)
		{
			ptr = ptr->left;
		}
		else
		{
			ptr = ptr->right;
		}
	}
	if (ptr == NULL)   // Stock_id does not exist
	{
		Rio_writen(fd, "stock_id not exists\n", strlen("stock_id not exists\n"));
	}
	else   // Stock_id exists
	{
		P(&(ptr->writer));   // Acquire the writer semaphore to block other writers

		// Critical Section: Writing
		ptr->left_stock += stock_num;
		Rio_writen(fd, "[sell] success\n", strlen("[sell] success\n"));
		// End of Critical Section: Writing

		V(&(ptr->writer));   // Release the writer semaphore to allow other writers
	}
}

void load_stock_to_memory()
{
	FILE* fp = fopen("stock.txt", "r");   // Open the file "stock.txt" in read mode
	int res;
	int stock_id;
	int left_stock;
	int stock_price;
	while (1)
	{
		res = fscanf(fp, "%d %d %d", &stock_id, &left_stock, &stock_price);
		if (res == EOF)
			break;
		stock_add_to_list(stock_id, left_stock, stock_price);   // Add the stock information to the temporary list
	}
	stock_list_to_bst();   // Convert the temporary list to a binary search tree (BST)
	fclose(fp);   // Close the file
}


void inorder_print(STOCK_ITEM* ptr, FILE* fp)
{
	if (ptr)
	{
		inorder_print(ptr->left, fp);

		P(&(ptr->mutex));
		ptr->stock_readcnt++;
		if (ptr->stock_readcnt == 1)   // First reader
			P(&(ptr->writer));
		V(&(ptr->mutex));
		// Critical Section: Reading

		fprintf(fp, "%d %d %d\n", ptr->stock_id, ptr->left_stock, ptr->stock_price);   // Write the stock information to the file

		// End of Critical Section: Reading

		P(&(ptr->mutex));
		ptr->stock_readcnt--;
		if (ptr->stock_readcnt == 0)   // Last reader
			V(&(ptr->writer));
		V(&(ptr->mutex));

		inorder_print(ptr->right, fp);
	}
}

void update_file()
{
	P(&file_mutex);   // Acquire the file_mutex semaphore to ensure exclusive access to the file
	// FILE WRITE, Critical Section
	FILE* fp = fopen("stock.txt", "w");   // Open the file "stock.txt" in write mode
	inorder_print(root, fp);   // Perform an inorder traversal of the BST and write the stock information to the file
	fclose(fp);   // Close the file
	// End of FILE WRITE, Critical Section
	V(&file_mutex);   // Release the file_mutex semaphore
}

void execute_command(int fd, char* command)
{
	char order[MAXLINE];   // Command
	int stock_id;   // Stock ID (as an integer)
	int stock_num;   // Stock quantity

	sscanf(command, "%s %d %d", order, &stock_id, &stock_num);   // Parse the command string

	if (!strcmp(order, "show"))
	{
		show(fd);   // Call the "show" function to display the stock information
	}
	else if (!strcmp(order, "buy"))
	{
		buy(fd, stock_id, stock_num);   // Call the "buy" function to purchase stocks
	}
	else if (!strcmp(order, "sell"))
	{
		sell(fd, stock_id, stock_num);   // Call the "sell" function to sell stocks
	}
	else
	{
		Rio_writen(fd, "invalid command\n", strlen("invalid command\n"));   // Invalid command, send an error message to the client
	}
}

void* thread(void* vargp)
{
	int n;
	rio_t rio;
	char command[MAXLINE];
	Pthread_detach(pthread_self());

	while (1)
	{
		int connfd = sbuf_remove(&sbuf);

		while (1)
		{
			int i;
			for (i = 0; i < MAXLINE; i++)
				command[i] = '\0';

			Rio_readinitb(&rio, connfd);
			if ((n = Rio_readlineb(&rio, command, MAXLINE)) != 0)
			{
				// Command received
				printf("server received %d bytes\n", n);

				if (!strcmp(command, "exit\n"))
				{
					update_file();
					Rio_writen(connfd, "exit\n", strlen("exit\n"));
					Close(connfd);
					break;
				}
				else if (!strcmp(command, "\n"))
				{
					Rio_writen(connfd, "\n", strlen("\n"));
				}
				else
					execute_command(connfd, command);
			}
			else
			{
				// Client closed connection
				update_file();
				Close(connfd);
				break;
			}
		}
	}
}


int main(int argc, char** argv)
{
	int i, listenfd, connfd;
	socklen_t clientlen;
	struct sockaddr_storage clientaddr;
	char client_hostname[MAXLINE], client_port[MAXLINE];

	pthread_t tid;

	if (argc != 2)
	{
		fprintf(stderr, "usage: %s <port>\n", argv[0]);
		exit(0);
	}

	// Load stock to memory
	Signal(SIGINT, sig_int_handler);
	load_stock_to_memory();
	Sem_init(&file_mutex, 0, 1);

	listenfd = Open_listenfd(argv[1]);
	sbuf_init(&sbuf, SBUFSIZE);

	for (i = 0; i < SBUFSIZE; i++)
		Pthread_create(&tid, NULL, thread, NULL);

	while (1)
	{
		clientlen = sizeof(struct sockaddr_storage);
		connfd = Accept(listenfd, (SA*)&clientaddr, &clientlen);
		Getnameinfo((SA*)&clientaddr, clientlen, client_hostname, MAXLINE, client_port, MAXLINE, 0);
		printf("Connected to (%s, %s)\n", client_hostname, client_port);
		sbuf_insert(&sbuf, connfd);
	}
}
/* $end echoserverimain */
