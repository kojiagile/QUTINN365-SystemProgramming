#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdbool.h>
#include <netdb.h>
#include <sys/types.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>
#include <errno.h>
#include "applib.h"

/// Max input size
#define INT_MAX_INPUT_TOTAL_BUF 256
/// Max size of food name
#define INT_MAX_INPUT_FOOD_NAME_BUF 150
/// Max size of measure
#define INT_MAX_INPUT_FOOD_MEASURE_BUF 50
/// Max size of weight, kCal, fat, carbo, protein
#define INT_MAX_INPUT_FOOD_NUM_BUF 6
/// Max size of receive data sent from server
#define INT_MAX_RECV_DATA_SIZE 4096

//global variables
int gServerPortNum;
char STR_MSG_FOOD_NOT_FOUND[] = "No food item found.\nPlease check your spelling and try again.\n";
char STR_ADD_STATUS_SUCCESS[] = "success";
char STR_NO_FOOD_FOUND[] = "0";
char STR_KEY_QUIT[] = "q";
char STR_KEY_ADD[] = "a";



/// Function definition
void checkParameter(int, char**, struct hostent**);
void initializeConnection(int*, struct sockaddr_in*, int*);
void sendRequest(int*, char*);
void *getResponse(int*, char*);
void display(char*);
bool getInputChar(char*, int);

//char *addNewFood();
bool addNewFood(char**);
bool isDigit(char*);


/**
 * Main function.
 *	Check parameters
 *	Wait for user's input
 *	Send a food name to find food information and display the search result
 *	
 *	@param argc The number of parameter input by user
 *	@param argv Array that has parameter input by user
 */
int main(int argc, char *argv[])
{
	int sockfd;
	struct hostent *he;
	struct sockaddr_in serverAddr;

	checkParameter(argc, argv, &he);
	gServerPortNum = atoi(argv[2]);
	char inputChar[INT_MAX_INPUT_TOTAL_BUF];
	while(true)
	{
		printf("Enter the food name to search for, or 'q' to quit or 'a' to add new food data.\n");
		if(!getInputChar(inputChar, INT_MAX_INPUT_TOTAL_BUF))
		{
			printf("Enter food name within %d characters.\n\n", INT_MAX_INPUT_TOTAL_BUF);
			continue;
		}
		
		if(inputChar[0] == '\n' || inputChar[0] == '\0') continue;
		//end the application with "q"
		if(strcmp(inputChar, STR_KEY_QUIT) == 0) break;
		
		char *newFood = NULL;
		if(strcmp(inputChar, STR_KEY_ADD) == 0)
		{
			//add new food info
			if(!addNewFood(&newFood)) continue;
		}
		
		serverAddr.sin_family = AF_INET;
		serverAddr.sin_port = htons(gServerPortNum);
		serverAddr.sin_addr = *((struct in_addr *)he->h_addr);
		bzero(&(serverAddr.sin_zero), 8);

		//initialize connection
		initializeConnection(&sockfd, &serverAddr, &gServerPortNum);
		if(strcmp(inputChar, STR_KEY_ADD) == 0)
		{
			strcpy(inputChar, newFood);
		}
		sendRequest(&sockfd, inputChar);
		char buf[INT_MAX_RECV_DATA_SIZE];
		getResponse(&sockfd, buf);
		display(buf);
		
		close(sockfd);
	}
	//close(sockfd);
}

/**
 * Get input character entered by user
 *
 *	@param ret		Entered character
 *	@param maxBuf	Max buffer size
 *	@return true: process successfully finished
 */
bool getInputChar(char *ret, int maxBuf)
{
	char temp[maxBuf + 2];
	fgets(temp, sizeof(temp), stdin);
	
	//check if return char is contained
	if(strchr(temp, '\n') != NULL)
	{
		if(strlen(temp) == 1 && temp[0] == '\n')
		{
			//if user did not type anything
			return false;
		}
		//replace return char
		temp[strlen(temp) - 1] = '\0';
	}
	else
	{
		//clear input stream
		while(getchar() != '\n');
		return false;
	}
	strcpy(ret, temp);
	return true;
}

/**
 * Add new food information.
 *
 *	@param newFood	The new food information entered by user.
 *	@return true: process successfully finished
 */
bool addNewFood(char **newFood)
{
	bool ret = false;
	int nameBuf = INT_MAX_INPUT_FOOD_NAME_BUF;
	int measureBuf = INT_MAX_INPUT_FOOD_MEASURE_BUF;
	//int numBuf = INT_MAX_INPUT_FOOD_NUM_BUF;
	int numBuf = 5;
	char name[nameBuf];
	char measure[measureBuf];
	char weight[numBuf];
	char fat[numBuf];
	char kCal[numBuf];
	char carbo[numBuf];
	char protein[numBuf];
	
	char errMsg[] = "*** Error *** Enter %s witin %d characters.\n";
	char errMsgDigit[] = "***Error*** Enter digits.\n";
	while(true)
	{
		printf("Enter food name, or enter 'q' to quit.\n");
		while(!getInputChar(name, nameBuf)) printf(errMsg, "food name", nameBuf);
		if(strcmp(name, STR_KEY_QUIT) == 0) break;
		
		printf("Enter measure of the food.\n");
		while(!getInputChar(measure, measureBuf)) printf(errMsg, "measure", measureBuf);
		
		printf("Enter weight (g) of the food.\n");
		while(true)
		{
			//parameter check
			while(!getInputChar(weight, numBuf)) printf(errMsg, "weight (g)", numBuf);
			if(!isDigit(weight)) printf("%s", errMsgDigit);
			else break;
		}
		printf("Enter kCal of the food.\n");
		while(true)
		{
			//parameter check
			while(!getInputChar(kCal, numBuf)) printf(errMsg, "kCal", numBuf);
			if(!isDigit(kCal)) printf("%s", errMsgDigit);
			else break;
		}
		printf("Enter fat (g) of the food.\n");
		while(true)
		{
			//parameter check
			while(!getInputChar(fat, numBuf)) printf(errMsg, "fat (g)", numBuf);
			if(!isDigit(fat)) printf("%s", errMsgDigit);
			else break;
		}
		printf("Enter carbo (g) of the food.\n");
		while(true)
		{
			//parameter check
			while(!getInputChar(carbo, numBuf)) printf(errMsg, "carbo (g)", numBuf);
			if(!isDigit(carbo)) printf("%s", errMsgDigit);
			else break;
		}
		printf("Enter protein (g) of the food.\n");
		while(true)
		{
			//parameter check
			while(!getInputChar(protein, numBuf)) printf(errMsg, "protein (g)", numBuf);
			if(!isDigit(protein)) printf("%s", errMsgDigit);
			else break;
		}
		
		//confirm massage
		printf("\n");
		printf("Input data: %s, %s, %s, %s, %s, %s, %s\n", 
			name, measure, weight, kCal, fat, carbo, protein);
		printf("Do you want to add this data? ");
		char answer[3];
		while(true)
		{
			printf("Enter y or n \n");
			while(!getInputChar(answer, 3)) printf("***Error*** Enter y or n \n");
			if(strcmp(answer, "y") == 0)
			{
				ret = true;
				break;
			}
			if(strcmp(answer, "n") == 0) break;
		}
		if(ret) break;
		printf("Enter data again.\n");
	}
	if(ret)
	{
		*newFood = (char *)calloc(INT_MAX_INPUT_TOTAL_BUF, sizeof(char));
		createFoodInfoText(*newFood, name, measure, weight, kCal, fat, carbo, protein);

		//'\na'represents adding new food data.
		//(newFood contain '\n' after calling createFoodInfoText() function.)
		strcat(*newFood, STR_KEY_ADD);
	}
	
	return ret;
}

/**
 * Check if target character are digits.
 *
 *	@param true: all characters are digits.
 */
bool isDigit(char target[])
{
	bool ret = true;
	if(target == NULL || target[0] == '\0' || target[0] == '\n')
	{
		return false;
	}
	
	int i;
	for(i = 0; i < target[i] != '\0'; i++)
	{
		if(!isdigit(*(target + i)))
		{
			ret = false;
		}
	}
	return ret;
}

/**
 * Display serch result.
 *
 *	@param response	The search result sent by server
 */
void display(char *response)
{
	if(response == NULL || strcmp(response, STR_NO_FOOD_FOUND) == 0)
	{
		printf("\n");
		printf("%s\n", STR_MSG_FOOD_NOT_FOUND);
		return;
	}
	//when new food was added
	if(strcmp(response, STR_ADD_STATUS_SUCCESS) == 0)
	{
		printf("\n");
		printf("New food has been added.\n");
		printf("\n");
		return;
	}
	// the number of the result
	int hitCount = getCharCount(response, STR_CR) - 1;

	int idx = 0;
	int index = 0;
	char *splitChar;
	char *foodList[hitCount];
	splitChar = strtok(response, STR_CR);
	while(splitChar != NULL)
	{
		foodList[idx++] = splitChar;
		splitChar = strtok(NULL, STR_CR);
	}
	
	printf("\n%d food items found.\n\n", hitCount);
	for(index = 0; index < hitCount; index++)
	{
		foodinfo_t *info = getFoodInfo(foodList[index]);

		printf("Food: %s\n", info->name);
		printf("Measure: %s\n", info->measure);
		printf("Weight (g): %d\n", info->weight);
		printf("kCal: %d\n", info->kCal);
		printf("Fat (g): %d\n", info->fat);
		printf("Carbo (g): %d\n", info->carbo);
		printf("Protein (g): %d\n", info->protein);
		printf("\n");
		free(info->name);
		free(info->measure);
		free(info);
	}
}

/**
 * Get response from server.
 *
 *	@param fd	server information
 *	@param buf	buffer for receiving the data
 */
void *getResponse(int *fd, char *buf)
{
	int numbytes = 0;
	if ((numbytes = recv(*fd, buf, INT_MAX_RECV_DATA_SIZE, 0)) == -1)
	{
		printf("recv() error. Error code = %d\n", errno);
		perror("recv()");
		exit(EXIT_FAILURE);
	}
	buf[numbytes] = '\0';
}


/**
 * Send a request to server
 *
 *	@param fd		server information
 *	@param sendData	The data to be sent to server
 */
void sendRequest(int *fd, char *sendData)
{
	int sendLen = send(*fd, sendData, strlen(sendData), 0);
	if(sendLen == -1)
	{
		printf("send() error. Error code = %d\n", errno);
		printf("Data Length = %d\n", strlen(sendData));
		if(errno == EMSGSIZE) printf("The data may be too big to send.\n");
		perror("send()");
		exit(EXIT_FAILURE);
	}
}



/**
 * Initialize connection to server
 *
 * @param sockfd		socket information
 * @param serverAddr	Server address
 * @param port			Port number
 */
void initializeConnection(int *sockfd, struct sockaddr_in *serverAddr, int *port)
{
	if ((*sockfd = socket(AF_INET, SOCK_STREAM, 0)) == -1)
	{
		printf("socket() failed. Error code = %d\n", errno);
		perror("socket()");
		exit(EXIT_FAILURE);
	}
	if (connect(*sockfd, (struct sockaddr *)serverAddr, sizeof(struct sockaddr)) == -1)
	{
		printf("connection() failed. Error code = %d\n", errno);
		perror("connection()");
		exit(EXIT_FAILURE);
	}
}

/**
 * Check parameters.
 *
 *	@param argc The number of parameter input by user
 *	@param argv Array that has parameter input by user
 *	@param he	Serve host name/IP address
 */
void checkParameter(int argc, char** argv, struct hostent **he)
{
	bool ret = true;
	if (argc != 3)
	{
		printf("Command line parameter error. Usage: <Server IP address> <port number> \n");
		exit(EXIT_FAILURE);
	}
	
	int i;
	if(!isDigit(argv[2]))
	{
		printf("Command line parameter error: Port number is not digit.\n");
		exit(EXIT_FAILURE);
	}
	if ((*he = gethostbyname(argv[1])) == NULL)
	{
		printf("Command line parameter error: Invalid hostname/server IP address.\n");
		exit(EXIT_FAILURE);
	}
}

