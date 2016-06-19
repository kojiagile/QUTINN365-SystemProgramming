#include <semaphore.h>
#include <pthread.h>
#include <sched.h>
#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <errno.h>
#include <string.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <sys/wait.h>
#include <unistd.h>
#include <stdbool.h>
#include <ctype.h>
#include <signal.h>
#include "applib.h"

/// Default port number
#define INT_DEFAULT_PORT 12345
/// Back log
#define BACKLOG 10
/// Maximum receive data size
#define INT_MAX_RECV_DATA_SIZE 512
/// Csv file name
#define STR_CSV_FILE_NAME "calories.csv"
/// Character: " " (space)
#define STR_SPACE " "
/// Max int size
#define INT_MAX_SIZE 10
/// Max client number to be connected to server at once
#define INT_MAX_CLIENT_NUMBER 10
/// Function type: search
#define INT_TYPE_SEARCH 0
/// Function type: add new food information
#define INT_TYPE_ADD 1

/// socket information
typedef struct socketInfo socketInfo_t;
struct socketInfo
{
	int fd;
	struct sockaddr_in addr;
	socklen_t size;
};

/// The number of food info
int gFoodListCount;
/// The number of food info added by user
int gNewFoodListCount;
/// The number of all food info (gFoodListCount + gNewFoodListCount);
int gSaveFoodListCount;
/// Listening port number
int gPortNum;
/// true: debug false: normal
bool gIsDebug = false;
/// true: SIGINT has been issued
bool gIsCancel = false;
/// Server log header: info
char STR_PRINT_INFO[] = "[info ]";
/// Server log header: error
char STR_PRINT_ERR[]  = "[error]";
/// Server log header: debug
char STR_PRINT_DEBUG[]  = "[debug]";
/// No food found
char STR_NO_FOOD_FOUND[] = "0";
/// Message for client when food info sent by user successfully added 
char STR_ADD_STATUS_SUCCESS[] = "success";

/// Array of food info
foodinfo_t **gFoodList;
/// Array of new food info added by user
foodinfo_t **gNewFoodList;
/// Array of all food info (used when SIGINT is issued)
foodinfo_t **gSaveFoodList;
/// Socket info
socketInfo_t *gClientList;

/// socket information
int sockfd;
struct sockaddr_in serverAddr;

//pthread object
pthread_mutex_t mutex;
pthread_t acceptor;
pthread_t *pIdList;
pthread_attr_t attr;
pthread_cond_t cond;

//semaphore object
sem_t empty;
sem_t full;



/// Function definition
void initializeSignalHandler();
void sigHandler();
void checkParameter(int, char**);
void initializeSocket(int*, struct sockaddr_in*, char*);
int receiveClientData(int*, int*, char*);
bool isTargetFood(char*, foodinfo_t*);
void convertToLowerChar(char*, char*);
void search(char*, char*, int*, int*, bool);
bool sendToClient(int*, char*, int);
void registerNewFood(char*);
bool saveFoodInfo();
void sortFoodInfo();
//void writeToCSV();
void dispose(foodinfo_t*);
void disposeAll();

/// pthread functions
void *accepter();
void *executor();


/**
 * Main function.
 *	Initialize necessary variables (socket, mutex, semaphore)
 *	Load csv data (food info)
 *	Register signal handler
 *	Check parameters
 *	Create 10 threads (executor) to implement searching and adding food data
 *	Create acceptor thread to listen to client request
 *	
 *	@param argc The number of parameter input by user
 *	@param argv Array that has parameter input by user
 */
int main(int argc, char *argv[])
{
	gNewFoodListCount = 0;
	initializeSignalHandler();
	checkParameter(argc, argv);
	gFoodList = readCSV(&gFoodListCount, STR_CSV_FILE_NAME);
	if(gCSVResult == APPLIB_ERR_OPEN)
	{
		printf("%s File open error. File name = %s\n", STR_PRINT_ERR, STR_CSV_FILE_NAME);
		exit(EXIT_FAILURE);
	}
	else printf("%s Load csv complete. \n", STR_PRINT_INFO);
	initializeSocket(&sockfd, &serverAddr, argv[1]);

	//init threads attribute
	pthread_attr_init(&attr);
	pthread_mutex_init(&mutex, NULL);
	sem_init(&empty, 0, INT_MAX_CLIENT_NUMBER);
	sem_init(&full, 0, 0);
	
	int i;
	//create acceptor thread to listen to client request
	gClientList = (socketInfo_t *)calloc(INT_MAX_CLIENT_NUMBER, sizeof(socketInfo_t));
	for(i = 0; i < INT_MAX_CLIENT_NUMBER; i++)
	{
		gClientList[i].fd = -1;
	}

	//create 10 threads
	pIdList = (pthread_t *)calloc(INT_MAX_CLIENT_NUMBER, sizeof(pthread_t));
	for(i = 0; i < INT_MAX_CLIENT_NUMBER; i++)
	{
		pthread_create(&pIdList[i], &attr, executor, NULL);
	}
	
	pthread_create(&acceptor, &attr, accepter, NULL);
	pthread_join(acceptor, NULL);
	
	//disposeAll();
}

/**
 * Implement search and response food data. 
 * Store new food info in array when user adds.
 */
void *executor()
{
	int i;
	int length = 0;
	int hitCount = 0;
	int clientFd = -1;
	struct sockaddr_in clientAddr;
	socklen_t clientSize;
	
	while(!gIsCancel)
	{
		sem_wait(&full);
		//mutex lock and copy socket info
		pthread_mutex_lock(&mutex);
		for(i = 0; i < INT_MAX_CLIENT_NUMBER; i++)
		{
			if(gClientList[i].fd >= 0)
			{
				clientFd = gClientList[i].fd;
				clientAddr = gClientList[i].addr;
				clientSize = gClientList[i].size;
				gClientList[i].fd = -1;
				break;
			}
		}
		pthread_mutex_unlock(&mutex);
		
		//output log
		printf("[Th %x]%s Connection from %s\n", 
			(unsigned int)pthread_self(), STR_PRINT_INFO, inet_ntoa(clientAddr.sin_addr));
		
		char recvData[INT_MAX_RECV_DATA_SIZE];
		int recvSize = 0;
		int type = INT_TYPE_SEARCH;
		//receive request data
		if(( type = receiveClientData(&clientFd, &recvSize, recvData)) == -1)
		{
			close(clientFd);
			disposeAll();
		}
		char *foodInfo;
		//when search required
		if(type == INT_TYPE_SEARCH)
		{
			//get all length of found food chars
			search(recvData, NULL, &length, &hitCount, true);
			foodInfo = (char *)calloc(length, sizeof(char));
			//search and get food info
			search(recvData, foodInfo, &length, &hitCount, false);
		}
		else
		{
			//when new food info sent from client
			hitCount = -1;
			registerNewFood(recvData);
			foodInfo = STR_ADD_STATUS_SUCCESS;
		}
		//send search result/add food info result("success" will be sent when succeed)
		if(!sendToClient(&clientFd, foodInfo, hitCount))
		{
			close(clientFd);
			disposeAll();
			exit(EXIT_FAILURE);
		}
		if(type == INT_TYPE_SEARCH)
		{
			//free memory
			free(foodInfo);
			foodInfo = NULL;
		}
		close(clientFd);
		
		//reset variable for next use
		clientFd = -1;
		hitCount = 0;
		length = 0;
		sem_post(&empty);
	}
}

/**
 * Listening to client request.
 *	Wait for client connection with accept() function.
 *	Set client connection info in socket info variable.
 *
 */
void *accepter()
{
	int i;
	int newFd;
	struct sockaddr_in clientAddr;
	socklen_t size;

	//NOTE: 
	//	accept() function blocks until connection from a client recieves.
	//	for every accepted connection, use a sepetate process or thread to serve it.
	//while(1)
	while(!gIsCancel)
	{
		sem_wait(&empty);
		size = sizeof(struct sockaddr_in);
		//wait for connection from client
		if ((newFd = accept(sockfd, (struct sockaddr *)&clientAddr, &size)) == -1)
		{
			printf("[Th %x]%s accept() error. Error code = %d\n", 
				(unsigned int)pthread_self(), STR_PRINT_ERR, errno );
			perror("accept");
			continue;
		}
		//mutex lock before add socket info in shared variable
		pthread_mutex_lock(&mutex);
		for(i = 0; i < INT_MAX_CLIENT_NUMBER; i++)
		{
			if(gClientList[i].fd < 0)
			{
				gClientList[i].fd = newFd;
				gClientList[i].addr = clientAddr;
				gClientList[i].size = size;
				break;
			}
		}
		pthread_mutex_unlock(&mutex);
		sem_post(&full);
	}
	
	close(sockfd);
	//disposeAll();
}

/**
 * Write new food info in csv file.
 *	Sort all food info including new food info added by user.
 *	Write all food info in csv file.
 *
 *	@return true: process is complete.
 */
bool saveFoodInfo()
{
	printf("\n");
	printf("%s %d new food info found.\n", STR_PRINT_INFO, gNewFoodListCount);
	
	bool ret = true;
	if(gNewFoodListCount <= 0) return ret;
	printf("%s Sort all data.....\n", STR_PRINT_INFO);
	sortFoodInfo();
	printf("%s Saving the data in the csv file.....\n", STR_PRINT_INFO);
	
	//call write function and check the result status
	writeCSV(STR_CSV_FILE_NAME, gSaveFoodList, gSaveFoodListCount);
	if(gCSVResult == APPLIB_ERR_OPEN)
	{
		printf("%s Save file open error.\n", STR_PRINT_ERR);
		exit(EXIT_FAILURE);
	}
	else if(gCSVResult == APPLIB_ERR_REMOVE)
	{
		printf("%s File remove error.\n", STR_PRINT_ERR);
		exit(EXIT_FAILURE);
	}
	else if(gCSVResult == APPLIB_ERR_RENAME)
	{
		printf("%s File name rename error.\n", STR_PRINT_ERR);
		exit(EXIT_FAILURE);
	}
	
	printf("%s All food has been saved.\n", STR_PRINT_INFO);
	
	return true;
}

/**
 * Sort all food information including new food information added by client.
 *
 */
void sortFoodInfo()
{
	//copy all info to new list.
	gSaveFoodList = (foodinfo_t **)calloc(gNewFoodListCount + gFoodListCount, sizeof(foodinfo_t));
	int i, j;
	int index = 0;
	for(i = 0; i < gFoodListCount; i++)
	{
		gSaveFoodList[index++] = gFoodList[i];
	}
	for(i = 0; i < gNewFoodListCount; i++)
	{
		gSaveFoodList[index++] = gNewFoodList[i];
	}
	
	gSaveFoodListCount = gFoodListCount + gNewFoodListCount;
	char orgLower[INT_MAX_RECV_DATA_SIZE];
	char newLower[INT_MAX_RECV_DATA_SIZE];
	int cmpRet = 0;
	foodinfo_t *temp;
	
	//sort
	for(i = 0; i < gSaveFoodListCount - 1; i++)
	{
		for(j = i + 1; j < gSaveFoodListCount; j++)
		{
			convertToLowerChar(gSaveFoodList[i]->name, orgLower);
			convertToLowerChar(gSaveFoodList[j]->name, newLower);
			if(strcmp(orgLower, newLower) > 0)
			{
				temp = gSaveFoodList[i];
				gSaveFoodList[i] = gSaveFoodList[j];
				gSaveFoodList[j] = temp;
			}
		}
	}
}


/**
 * Register new food information in gNewFoodList valiable.
 *
 *	@param newFood new food information entered by user.
 */
void registerNewFood(char* newFood)
{
	foodinfo_t *info = getFoodInfo(newFood);
	if(gNewFoodList == NULL)
	{
		//allocate memory to store when the first information receives
		pthread_mutex_lock(&mutex);
		gNewFoodList = (foodinfo_t **)malloc(sizeof(foodinfo_t));
		gNewFoodList[0] = info;
		gNewFoodListCount = 1;
		pthread_mutex_unlock(&mutex);
	}
	else
	{
		foodinfo_t **temp;
		pthread_mutex_lock(&mutex);
		temp = (foodinfo_t **)realloc(gNewFoodList, sizeof(foodinfo_t) * (gNewFoodListCount + 1));
		if(temp == NULL)
		{
			//printf("%s Memory re-allocation error.\n", STR_PRINT_ERR);
			printf("[Th %x]%s Memory re-allocation error.\n", 
				(unsigned int)pthread_self(), STR_PRINT_ERR);
			disposeAll();
			exit(EXIT_FAILURE);
		}
		temp[gNewFoodListCount++] = info;
		gNewFoodList = temp;
		pthread_mutex_unlock(&mutex);
	}
}

/**
 * Check parameters.
 *	If inappropriate values are entered, shows error message and exit program.
 *
 *	@param argc	The number of parameters.
 *	@param argv	Array of parameters.
 */
void checkParameter(int argc, char** argv)
{
	if (argc != 2)
	{
		printf("Command line parameter error. Usage: ./<This file name> <Port number> \n");
		exit(EXIT_FAILURE);
	}
	
	int i;
	//check input data
	for(i = 0; i < argv[1][i] != '\0'; i++)
	{
		if(!isdigit(*(argv[1] + i)))
		{
			printf("Command line parameter error: Port number is not digit.\n");
			exit(EXIT_FAILURE);
		}
	}
	//if the number is less than 1024
	if(atoi(argv[1]) < 1024)
	{
		printf("Note: The port number is in the range of well-known port.\n");
		exit(EXIT_FAILURE);
	}
}

/**
 * Send data to client.
 *	When user sends a search word, the function returns the result.
 *	When user add new food information, the function returns the message "success".
 *
 *	@param fd		Socket information
 *	@param sendData	The data to be sent to client
 *	@param hitCount	The number of food information found
 */
bool sendToClient(int *fd, char *sendData, int hitCount)
{
	//if(hitCount != -1) printf("%s Hit = %d\n", STR_PRINT_INFO, hitCount);
	if(hitCount != -1)
	{
		//display hit count as a log
		printf("[Th %x]%s Hit = %d\n", 
			(unsigned int)pthread_self(), STR_PRINT_INFO, hitCount);
	}
	
	bool ret = true;
	if(hitCount > 0)
	{
		//send data to client
		int sendLen = send(*fd, sendData, strlen(sendData), 0);
		if(sendLen == -1)
		{
			//error handling and log message on console
			printf("[Th %x]%s send() error. Error code = %d\n", 
				(unsigned int)pthread_self(), STR_PRINT_ERR, errno);
			printf("[Th %x]%s Data Length = %d\n", 
				(unsigned int)pthread_self(), STR_PRINT_ERR, strlen(sendData));
			if(gIsDebug) printf("[Th %x][%s} sendToClient() Sent data: \n%s\n", 
				(unsigned int)pthread_self(), STR_PRINT_DEBUG, sendData);
			if(errno == EMSGSIZE)
			{
				printf("[Th %x]%s The data may be too big to send.\n", 
					(unsigned int)pthread_self(), STR_PRINT_ERR);
			}
			perror("send()");
			ret = false;
		}
	}
	else
	{
		//when no food found or new food information has been successfully added
		char *data;
		if(hitCount == 0) data = STR_NO_FOOD_FOUND;
		else data = STR_ADD_STATUS_SUCCESS;
		if(send(*fd, data, strlen(data), 0) == -1)
		{
			printf("[Th %x]%s send() error. Error code = %d\n", 
				(unsigned int)pthread_self(), STR_PRINT_ERR, errno);
			printf("[Th %x]%s No food found.\n", (unsigned int)pthread_self(), STR_PRINT_ERR);
			perror("send()");
			ret = false;
		}
	}
	return ret;
}

/**
 * Search and get food information.
 *	The function sets the number of food information found in hitCount variable.
 *
 *	@param searchWord	Search word
 *	@param ret			Variable to store all food information found
 *	@param retLength	The number of the characters of the food information.
 *	@param hitCount		The number of the information found.
 *	@param needLength	true: The function only set the value into retLength.
 */
void search(char *searchWord, char *ret, int *retLength, int *hitCount, bool needLength)
{
	int i;
	int count = 0;
	foodinfo_t *info;
	int fullCharLength = 0;
	
	for(i = 0; i < gFoodListCount; i++)
	{
		//search food
		if(!isTargetFood(searchWord, gFoodList[i])) continue;

		//increment count
		count++;
		info = gFoodList[i];
		char weight[INT_MAX_SIZE];
		char kCal[INT_MAX_SIZE];
		char fat[INT_MAX_SIZE];
		char carbo[INT_MAX_SIZE];
		char protein[INT_MAX_SIZE];
		//convert int into char
		sprintf(weight,"%d", info->weight);
		sprintf(kCal,"%d", info->kCal);
		sprintf(fat,"%d", info->fat);
		sprintf(carbo,"%d", info->carbo);
		sprintf(protein,"%d", info->protein);

		fullCharLength += strlen(info->name) + strlen(info->measure) + strlen(weight);
		fullCharLength += strlen(kCal) + strlen(fat) + strlen(carbo) + strlen(protein);
		
		//INT_DEFAULT_SPLIT_COUNT is for separation (comma) between each fields
		fullCharLength +=  INT_DEFAULT_SPLIT_COUNT;
		//when the number of chars is required
		if(needLength) continue;

		//add food info in ret variable
		createFoodInfoText(ret, info->name, info->measure, weight, kCal, fat, carbo, protein);
	}
	if(needLength) *retLength = fullCharLength + 1;
	
	*hitCount = count;
	if(gIsDebug) printf("[Th %x]%s search() needLength(0:false, 1:true) = %d, Hit = %d\n", 
		(unsigned int)pthread_self(), STR_PRINT_DEBUG, needLength, *hitCount);
	if(gIsDebug) printf("[Th %x]%s search() Search Result : \n%s\n", 
		(unsigned int)pthread_self(), STR_PRINT_DEBUG, ret);
}

/**
 * Compare search word with food name.
 *
 *	@param searchWord	Search word sent by client
 *	@param info			Single food information
 *	@return true: The food is to be sent to client.
 */
bool isTargetFood(char *searchWord, foodinfo_t *info)
{
	char word[strlen(searchWord) + 1];
	char name[strlen(info->name) + 1];
	convertToLowerChar(searchWord, word);
	convertToLowerChar(info->name, name);
	
	//compare the number of chars and the first character of the search word and the food name
	if(strlen(word) > strlen(name) || word[0] != (name)[0])
	{
		return false;
	}
	
	int wordLength = strlen(word);
	int nameLength = strlen(name);
	
	//check if the last char is a comma or a space
	if(word[wordLength - 1] == STR_COMMA[0])
	{
		//replace with '\0'
		word[wordLength - 1] = '\0';
		wordLength = strlen(word);
	}
	
	//when the last char in the search word is a space
	if(word[wordLength - 1] == STR_SPACE[0])
	{
		if(strncmp(name, word, wordLength) == 0)
		{
			return true;
		}
	}
	else
	{
		//when the number of chars of the search word and that of the name is the same
		if((wordLength == nameLength) && (strcmp(word, name) == 0) )
		{
			return true;
		}
		else if(nameLength > wordLength && strncmp(name, word, wordLength) == 0)
		{
			//When the number of char of food name is bigger than that of the search word,
			//check the char of the food name at "searchWordLength" element
			//
			//E.g. 
			//	searchWord is "apricots" check the next char of "s" in the food name 
			//	to ignore other kind of food such as "ApricotsJam".
			//
			//	If the next char is a comma or a space, the food has to be displayed.
			//	If it is anything else, the food should not be displayed.
			if(name[wordLength] == STR_SPACE[0] || name[wordLength] == STR_COMMA[0])
			{
				return true;
			}
		}
	}
	
	return false;
}

/**
 * Convert character to lower case.
 *
 *	@param target	The character to be converted.
 *	@param ret		The character converted to lower case.
 */
void convertToLowerChar(char *target, char *ret)
{
	int i;
	for(i = 0; i < strlen(target); i++)
	{
		ret[i] = tolower(target[i]);
	}
	ret[i] = '\0';
}

/**
 * Receive data sent by client.
 *
 *	@param newFd	Socket information
 *	@param recvSize	The size of received data.
 *	@param recvData	Received data
 *	@return Function type
 */
int receiveClientData(int *newFd, int *recvSize, char *recvData)
{
	int ret = INT_TYPE_SEARCH;
	//*recvSize = recv(*newFd, recvData, strlen(recvData) + 1, 0);
	*recvSize = recv(*newFd, recvData, INT_MAX_RECV_DATA_SIZE, 0);
	if(*recvSize == -1)
	{
		//error handling
		printf("[Th %x]%s recv() error. Error code = %d\n", 
			(unsigned int)pthread_self(), STR_PRINT_ERR, errno);
		if(recvSize != NULL)
		{
			printf("[Th %x]%s recvSize = %d\n", (unsigned int)pthread_self(), STR_PRINT_ERR, *recvSize);
		}
		if(recvData != NULL)
		{
			printf("[Th %x]%s recvData = \n%s\n", (unsigned int)pthread_self(), STR_PRINT_ERR, recvData);
		}
		perror("recv()");
		return -1;
	}
	char *splitChar;
	char *temp;
	//if '\n' is detected, that means add new food data
	if(strchr(recvData, '\n') != NULL)
	{
		strcpy(temp, recvData);
		splitChar = strtok(temp, STR_CR);
		recvData = splitChar;
		ret = INT_TYPE_ADD;
	}
	
	char *typeName;
	if(ret == INT_TYPE_SEARCH) typeName = "Search";
	else typeName = "Add";
	//output log
	printf("[Th %x]%s Received data(length) = %s(%d) Type: %s\n", 
		(unsigned int)pthread_self(), STR_PRINT_INFO, recvData, *recvSize, typeName);
	
	return ret;
}

/**
 * Initialize socket.
 *	@param sockfd		Socket information
 *	@param serverAddr	Server address
 *	@param port			Port number
 */
void initializeSocket(int *sockfd, struct sockaddr_in *serverAddr, char *port)
{
	gPortNum = atoi(port);
	serverAddr->sin_family = AF_INET;
	serverAddr->sin_port = htons(gPortNum);
	serverAddr->sin_addr.s_addr = INADDR_ANY;

	if ((*sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		printf("%s socket() failed. Error code = %d\n", STR_PRINT_ERR, errno);
		perror("socket()");
		disposeAll();
		exit(EXIT_FAILURE);
	}
	if (bind(*sockfd, (struct sockaddr *)serverAddr, sizeof(struct sockaddr)) == -1)
	{
		//if the port is in use
		if(errno == EADDRINUSE)
		{
			//try to use the default port
			printf("%s Port %d is in use. Trying to use default port:%d\n", 
				STR_PRINT_ERR, gPortNum, INT_DEFAULT_PORT);
			gPortNum = INT_DEFAULT_PORT;
			serverAddr->sin_port = htons(gPortNum);
			if (bind(*sockfd, (struct sockaddr *)serverAddr, sizeof(struct sockaddr)) == -1)
			{
				printf("%s bind() failed. Port %d is in use.\n", STR_PRINT_ERR, gPortNum);
				perror("bind()");
				disposeAll();
				exit(EXIT_FAILURE);
			}
		}
		else
		{
			printf("%s bind() failed. Error code = %d\n", STR_PRINT_ERR, errno);
			perror("bind()");
			disposeAll();
			exit(EXIT_FAILURE);
		}
	}
	//start to listen
	if (listen(*sockfd, BACKLOG) == -1)
	{
		printf("%s listen() failed. Error code = %d\n", STR_PRINT_ERR, errno);
		perror("listen()");
		disposeAll();
		exit(EXIT_FAILURE);
	}
	
	printf("%s Initialize socket complete. \n", STR_PRINT_INFO);
	printf("%s Server is lisning on port %d..... \n", STR_PRINT_INFO, gPortNum);
}


/**
 * Initialize signal handler.
 */
void initializeSignalHandler()
{
	if(signal(SIGINT, sigHandler) == SIG_ERR)
	{
		printf("%s signal() failed. \n", STR_PRINT_ERR);
		exit(EXIT_FAILURE);
	}
	printf("%s Initialize signal handler complete. \n", STR_PRINT_INFO);
}

/**
 * Implement the last process when SIGINT has been issued.
 */
void sigHandler()
{
	if(!saveFoodInfo())
	{
		//printf("%s New food info could not write in the csv.\n", STR_PRINT_ERR);
	}
	disposeAll();
	gIsCancel = true;
	exit(0);
}



/**
 * Free all allocated memories.
 */
void disposeAll()
{
	printf("\n%s Disposing allocated memory.....", STR_PRINT_INFO);
	//if(gFoodList == NULL) return;
	int i;
	foodinfo_t *info;
	for(i = 0; i < gFoodListCount; i++)
	{
		info = gFoodList[i];
		dispose(info);
	}
	for(i = 0; i < gNewFoodListCount; i++)
	{
		info = gNewFoodList[i];
		dispose(info);
	}
	free(gFoodList);
	gFoodList = NULL;
	free(gClientList);
	free(gNewFoodList);
	gClientList = NULL;
	gNewFoodList = NULL;
	
	//free thread
	pthread_attr_destroy(&attr);
	pthread_cond_destroy(&cond);
	pthread_mutex_destroy(&mutex);
	
	printf("Done. \n");
}

/**
 * Free single food information.
 */
void dispose(foodinfo_t *info)
{
	if(info != NULL)
	{
		if(info->name != NULL)
		{
			free(info->name);
			info->name = NULL;
		}
		if(info->measure != NULL)
		{
			free(info->measure);
			info->measure = NULL;
		}
	}
	free(info);
	info = NULL;
}
