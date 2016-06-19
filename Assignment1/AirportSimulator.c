#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include <semaphore.h>
#include <stdbool.h> // for bool type
#include <inttypes.h> // for portable integer declarations
#include <unistd.h>
#include <sys/time.h>

#define MAX_BUFFER_SIZE 10
#define PLANECODE_CHAR_SIZE 2
#define PLANECODE_MAX_SIZE 6

//pthread object
pthread_mutex_t mutex;
pthread_cond_t cond;
pthread_t mainThread;
pthread_t landing;
pthread_t taking;
pthread_t monitor;
pthread_attr_t attr;
sem_t empty;
sem_t full;

//plane info
typedef struct airplane airplane_t;
struct airplane
{
	char *code;
	struct timeval time;
};

//for storing command line parameters
typedef struct pthArg pthArg_t;
struct pthArg
{
	int probLand;//the chance of landing
	int probTake;//the chance of tanking off
};


//airport bays
airplane_t **PlaneBay = NULL;
// for plane code
char code_char[26] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";
char code_num[10] = "0123456789";
//for sleep 0.5 sec
struct timespec timeSpec = {0, 500 * 1000000 };

int *probLandAry = NULL;
int *probTakeAry = NULL;

//thread cancell flag
bool isCancell;
//the number of occupied bay
int occuNum;

//function definitions
void *main_thread(void *param);
void *landing_thread();
void *takingoff_thread();
void *monitor_thread();
void displayBay();
double getDiffTime(struct timeval, struct timeval);
int getIndex(void);
void generatePlaneCode(airplane_t*, int, int);
bool isNoPlane(airplane_t*);


/**
 * This function:
 *	free all allocated memory
 **/	
void dispose()
{
	int i;
	airplane_t *plane;
	for(i = 0; i < MAX_BUFFER_SIZE; i++)
	{
		//plane = &PlaneBay[i];
		plane = PlaneBay[i];
		if(plane != NULL && plane->code != NULL)
		{
			free(plane->code);
			plane->code = NULL;
		}
		free(PlaneBay[i]);
	}
	
	free(PlaneBay);
	PlaneBay = NULL;
	free(probLandAry);
	probLandAry = NULL;
	free(probTakeAry);
	probTakeAry = NULL;
}

/**
 * This function:
 *	randomly generates plane code that contains two alphabet and four digits
 *
 * @param newPlane 	target plane object
 * @param charNum	the number of alphabets
 * @param maxLength	the maximum length of the plane code
 */
void generatePlaneCode(airplane_t *newPlane, int charNum, int maxLength)
{
	int i;
	int n;

	char *newCode = (char *)malloc(sizeof(char) * PLANECODE_MAX_SIZE);
	
	if(newCode == NULL)
	{
		printf("memory allocation error has occured in generatePlaneCode()\n");
		//return -1;
	}
	
	//get seed to generate random plane code
	srand((unsigned int)time(NULL));
	
	for(i = 0; i < charNum; i++)
	{
		//get the first two characters
		n = rand() % (sizeof(code_char) / sizeof(code_char[0]));
		*(newCode + i) = code_char[n];
	}
	
	for(i; i < maxLength; i++)
	{
		//get the last four numbers
		n = rand() % (sizeof(code_num) / sizeof(code_num[0]));
		*(newCode + i) = code_num[n];
	}
    newPlane->code = newCode;
}


/**
 * This function:
 *	simulates that a plane lands on the runway.
 *		generates plane code
 *		randomly selects a bay that allows a plane parks 
 *		updates the status of the bay locking/unlocking mutex object
 *		blocked by sem_wait() until plane_takingOff() calls sem_post() when the airport is full
 *		wakes up taking off thread when it is sleeping
 *		displays messages on screen
 *
 * @return true if no errors occur
 */
bool plane_landing() {
    // allocate new item
    airplane_t *landPlane = (airplane_t *)malloc(sizeof(airplane_t));
    if (landPlane == NULL) {
        return false;
    }
	
	generatePlaneCode(landPlane, PLANECODE_CHAR_SIZE, PLANECODE_MAX_SIZE);

	int i;
	airplane_t *temp;
	
	if(occuNum == -1)
	{
		occuNum = 0;
	}

	sem_wait(&empty);
	//mutex lock to protect buffer
	pthread_mutex_lock(&mutex);

	//set seed to get random numbers between 0 - 9
	//do not call srand() in for/while loop, or process would be slow
	srand((unsigned int)time(NULL));
	while(true)
	{
		i = getIndex();
		//temp = &PlaneBay[i];
		temp = PlaneBay[i];
		if(isNoPlane(temp))
		{
			break;
		}
	}
	
	printf("Plane %s is landing...\n", landPlane->code);
	
	//has to sleep for 2 seconds as the duration of landing
	sleep(2);
	
	//set park time;
	struct timeval start;
	gettimeofday(&start, NULL);
	landPlane->time = start;
	
	//set plane info in bay
	//PlaneBay[i] = *landPlane;
	PlaneBay[i] = landPlane;
	//occupy one bay
	occuNum++;
	
	printf("Plane %s parked in landing bay %d\n", landPlane->code, i);
	
	//display message if bay is full
	if(occuNum >= MAX_BUFFER_SIZE)
	{
		occuNum = MAX_BUFFER_SIZE;
		printf("The airport is full.\n\n");
	}
	//Release mutex lock and full semaphore
	pthread_mutex_unlock(&mutex);
	sem_post(&full);
	
    return true;
}

/**
 * This function:
 *	generates random number between 0 to 9
 *
 * @return number between 0 to 9
 */
int getIndex()
{
	return rand() % MAX_BUFFER_SIZE;
}


/**
 * This function:
 *	checks if a plane in the parameter exists at any bays.
 *
 * @param 	plane a target plane to be searched
 * @return trur if the plane exists at any bay, or false if the plane does not exist.
 */
bool isNoPlane(airplane_t *plane)
{
	bool ret;
	ret = false;
	if(plane == NULL)
	{
		ret = true;
	}
	else if(plane->code == NULL || plane->code == '\0' || plane->code == "")
	{
		ret = true;
	}
	
	return ret;
}

/**
 * This function:
 *	simulates that a plane takes off from the runway.
 *		randomly selects a plane that takes off
 *		blocked by sem_wait() until plane_landing() sem_post() when the airport is empty
 *		updates the status of the bay locking/unlocking mutex object
 *		wakes up landing thread when it is sleeping
 *		displays messages on screen
 *
 * @return true if no errors occur
 */
bool plane_takingOff()
{
	//get index to pick up one plane
	int index = 0;
	airplane_t *plane;
	
	double diff;

	sem_wait(&full);
	//lock mutex
	pthread_mutex_lock(&mutex);

	//set seed to get random numbers between 0 - 9
	//do not call srand() in for/while loop, or process would be slow
	srand((unsigned int)time(NULL));
	while(true)
	{
		index = getIndex();
		//plane = &PlaneBay[index];
		plane = PlaneBay[index];
		if(!isNoPlane(plane))
		{
			break;
		}
	}
	
	struct timeval end;
	gettimeofday(&end, NULL);
	diff = getDiffTime(plane->time, end);
	printf("After staying at bay %d for %.2f seconds, plane %s is taking off... \n",
		index, diff, plane->code);
	
	//has to sleep for 2 seconds as the duration of landing
	sleep(2);
	//decrement the number of occupied bays
	occuNum--;
	
	printf("Plane %s has finished taking off\n", plane->code);

	// Set NULL because free() frees allocated memory, 
	// but does not set NULL to the variable
	free(plane->code);
	plane->code = NULL;
	free(PlaneBay[index]);
	PlaneBay[index] = NULL;
	
	//display message if bay is empty
	if(occuNum <= 0)
	{
		occuNum = 0;
		printf("The airport is empty.\n\n");
	}
	
	//unlock mutex
	pthread_mutex_unlock(&mutex);
	sem_post(&empty);

    return true;
}

/**
 * This function:
 *	displays the current status of the airport culcurating time margin 
 *		between current time and the one that plane was generated.
 */
void displayBay()
{
	int i;
	double diff = 0;

	printf("\n");
	printf("Airport state:\n");
	
	for(i = 0; i < MAX_BUFFER_SIZE; i++)
	{
		//airplane_t *temp = &PlaneBay[i];
		airplane_t *temp = PlaneBay[i];
		
		if(isNoPlane(temp))
		{
			printf("%d: Empty \n", i);
		}
		else
		{
			//get time margin
			struct timeval end;
			gettimeofday(&end, NULL);
			diff = getDiffTime(temp->time, end);
			printf("%d: %s (has parked for %.2f seconds)\n", i, temp->code, diff);
		}
	}
	printf("\n");
}

/**
 * This function:
 *	culculates time margin between start time and end time with milli second.
 *
 * @param start start time
 * @param end 	end time
 * @return		time margin
 */
double getDiffTime(struct timeval start, struct timeval end)
{
	double ds;
	double de;
	ds = start.tv_sec + (start.tv_usec * 1e-6);
	de = end.tv_sec + (end.tv_usec * 1e-6);
	return (de - ds);
}

	
/**
 * This function:
*	checks the number of planes in the airport and calls plane_landing() function.
 *		calls plane_landing() to simulate an airplane landing on a runway.
 *		ends process when isCancell is true.
 *		blocked until taking off thread wake up this function when the airport is full.
 * 		
 **/
void *landing_thread()
{
	//init array
	PlaneBay = (airplane_t **)calloc(MAX_BUFFER_SIZE, sizeof(airplane_t));
	if(PlaneBay == NULL) {
		printf("memory allocation error has occured.\n");
		//return -1;
	}
	
	int i = 0;
	bool cancell = false;
	while(true)
	{
		while(true)
		{
			//sleep 0.5 sec
			nanosleep(&timeSpec, NULL);

			//probability
			i = rand() % 100;
			if(probLandAry[i] == 1)
			{
				break;
			}
			if(isCancell)
			{
				cancell = true;
				break;
			}
		}
		
		if(cancell)
		{
			break;
		}
		
		if(!plane_landing())
		{
			printf("Error has occured in plane_landing() \n");
			break;
		}
		
		if(isCancell)
		{
			//thread ends by user input(q/Q)
			break;
		}
	}
}


/**
 * This function:
 *	randomly picks a landed plane in the airport to take off
 * 	calls plane_takingOff() to simulate an airplane taking off from runway
 * 	ends process when isCancell is true.
 **/
void *takingoff_thread()
{
	int i = -1;
	bool cancell = false;
	srand((unsigned int)time(NULL));
	while(true)
	{
		while(true)
		{
			//sleep 0.5 sec
			nanosleep(&timeSpec, NULL);
			i = rand() % 100;
			//probability
			if(probTakeAry[i] == 1)
			{
				break;
			}
			if(isCancell)
			{
				cancell = true;
				break;
			}
		}
		
		if(cancell)
		{
			break;
		}
		//struct timespec tim = {5, 500 * 1000000 };
		//nanosleep(&tim, NULL);
		
		if(!plane_takingOff())
		{
			printf("---------- error has occured in takingoff_thread() ---------- \n");
			//sem_post(&empty);
			break;
		}
		
		if(isCancell)
		{
			//thread ends by user input(q/Q)
			break;
		}
	}
}


/**
 * This function displays the current state of airplane when p/P is entered.
 * Terminate both landing and taking off thread when q/Q is entered.
 *
 **/
void *monitor_thread()
{
	int index = 0;
	char input;
	char first;
	char second;
	
	/** 
	 *  Same as main(), avoiding carriage return problem,
	 *  check the number of characters entered and what characters enter.
	 **/
	while(true)
	{
		input = getchar();
		
		if(index == 0)
		{
			first = input;
			index++;
		}
		else if(index == 1)
		{
			second = input;
			index++;
		}
		
		if(input == '\n')
		{
			if(index == 2)
			{
				//lock mutex
				pthread_mutex_lock(&mutex);
				
				//all characters are ignored except for the followings
				if((first == 'p' || first == 'P') && second == '\n')
				{
					//only when "p/P(enter key)" is entered
					displayBay();
					fflush(stdout);
				}
				else if((first == 'q' || first == 'Q') && second == '\n')
				{
					//only when "q/Q(enter key)" is entered
					//cancel landing and takingOff thread

					//cancell flag on
					//** landing and taking off thread will end because of this flag
					isCancell = true;
					//unlock
					pthread_mutex_unlock(&mutex);
					break;
				}
				//unlock
				pthread_mutex_unlock(&mutex);
			}
			index = 0;
		}
	}
}


/**
 * This function creates three children threads to start airplane simulator.
 * when all children threads end, display the current status of airport and dispose allocated memory.
 *
 * @param param command line parameters sent from main().
 *
 **/
void *main_thread(void *param)
{
	//initial message
	printf("Welcome to the airport simulator. \n");
	printf("Press p or P followed by return to display the state of the airport. \n");
	printf("Press q or Q followed by return to terminate the simulation.\n");
	fflush(stdout);
	
	char input;
	int probLand;
	int probTake;
	int ret;
	int i;
	
	//for cancell threads
	isCancell = false;
	
	//get probability from command line
	probLand = ((pthArg_t *)param)->probLand;
	probTake = ((pthArg_t *)param)->probTake;
	
	int max = 100;
	probLandAry = (int*)calloc(max, sizeof(int));
	if(probLandAry == NULL) {
		printf("memory allocation error has occured in main_thread() .\n");
		//return -1;
	}
	probTakeAry = (int*)calloc(max, sizeof(int));
	if(probTakeAry == NULL) {
		printf("memory allocation error has occured in main_thread() .\n");
		//return -1;
	}
	
	//probability culculation
	int *tempLand = (int*)calloc(probLand, sizeof(int));
	int *tempTake = (int*)calloc(probTake, sizeof(int));
	int j = 0;
	int k = 0;
	int l = 0;
	i = 0;
	
	while(true)
	{
		j = rand() % 100;
		for(i = 0; i < probLand; i++)
		{
			//printf("*(tempLand + %d) = %d\n", i, *(tempLand + i));
			if(*(tempLand + i) == j)
			{
				k = 1;
				break;
			}
		}
		if(k == 0)
		{
			*(tempLand + l++) = j;
		}
		k = 0;
		if(l == probLand)
		{
			break;
		}
	}
	i = 0;
	j = 0;
	k = 0;
	l = 0;
	while(true)
	{
		j = rand() % 100;
		for(i = 0; i < probTake; i++)
		{
			if(*(tempTake + i) == j)
			{
				k = 1;
				break;
			}
		}
		if(k == 0)
		{
			*(tempTake + l++) = j;
		}
		k = 0;
		if(l == probTake)
		{
			break;
		}
	}
	
	srand((unsigned int)time(NULL));
	for(i = 0; i < max; i++)
	{
		for(j = 0; j < probLand; j++)
		{
			if(i == tempLand[j])
			{
				*(probLandAry + i) = 1;
			}
		}
	}
	for(i = 0; i < max; i++)
	{
		for(j = 0; j < probTake; j++)
		{
			if(i == tempTake[j])
			{
				*(probTakeAry + i) = 1;
			}
		}
		
	}
	
	free(tempLand);
	free(tempTake);
	
	i = 0;
	printf("\n");
	printf("Press return to start the simulation. \n");
	fflush(stdout);
	
	/** 
	*  Avoiding carriage return problem, check both character entered and index(i).
	 *  e.g.
	 *    "abc(return key)" is entered, inside while() should be repeated four times,
	 *    then break the loop if index is not checked
	 *    because of the specification of getchar(). (same to scanf(), gets())
	 **/
	while(true)
	{
		//wait for user input
		input = getchar();
		
		//when only return key is pressed, break this loop
		if(input == '\n' && i == 0)
		{
			i = 0;
			break;
		}
		if(i == 0)
		{
			//display message once again if any other keys are typed
			printf("Press return to start the simulation. \n");
		}
		
		//initialize i for waiting for user input again
		i++;
		if(input == '\n')
		{
			i = 0;
		}
	}

	//init threads attribute
	pthread_attr_init(&attr);
	pthread_mutex_init(&mutex, NULL);
	//pthread_cond_init(&cond, NULL);
	sem_init(&empty, 0, MAX_BUFFER_SIZE);
	sem_init(&full, 0, 0);
	
	//generate landing thread
    if ( pthread_create(&landing, &attr, landing_thread, NULL) )
	{
		printf("error creating landing_thread.");
		abort();
	}
	//generate taking off thread
	if ( pthread_create(&taking, &attr, takingoff_thread, NULL) )
	{
		printf("error creating takingoff_thread.");
		abort();
	}
	//generate monitor thread
    if ( pthread_create(&monitor, &attr, monitor_thread, NULL) )
    {
        printf("error creating monitor_thread.");
        abort();
    }
	
	//wait all threads end
    if(pthread_join(landing, NULL))
    {
        printf("error joining landing_thread");
        abort();
    }
    if(pthread_join(taking, NULL))
    {
        printf("error joining takingoff_thread");
        abort();
    }
    if(pthread_join(monitor, NULL))
    {
        printf("error joining monitor_thread");
        abort();
    }
	
	//printf("all threads got cancelled\n");
	
	//this function will be execute when all three threads end.
	displayBay();
	//free all allocated memory 
	dispose();
}

/**
 * This function creates main thread, and wait until it ends.
 *
 * @param argc 	The number of arguments
 * @param argv 	array that contains parameters input by user
 * @return 0 if system ends with no errors or -1 if an error occur
 *
 **/
int main(int argc, char *argv[])
{
	if(argc != 3)
	{
		printf("\t\t The number of arguments is invalid. \n");
		printf("useage: <probablity of producing plane(1-90) <probability of taking-off plane>(1-90) \n");
		return -1;
	}

	pthArg_t pargs;
	int probLand;
	int probTake;
	int i;
	
	//check input data
	for(i = 0; i < argv[1][i] != '\0'; i++)
	{
		if(!isdigit(*(argv[1] + i)))
		{
			probLand = -1;
		}
	}
	for(i = 0; i < argv[2][i] != '\0'; i++)
	{
		if(!isdigit(*(argv[2] + i)))
		{
			probTake = -1;
		}
	}
	
	//if input data is only number, check the validity
	if(probLand != -1)
	{
		probLand = atoi(argv[1]);
	}
	if(probTake != -1)
	{
		probTake = atoi(argv[2]);
	}
	
	//validity check
	if(probLand < 1 || probLand > 90)
	{
		printf("Invalid number of the first argument.\n");
		return -1;
	}
	if(probTake < 1 || probTake > 90)
	{
		printf("Invalid number of the second argument.\n");
		return -1;
	}
	
	//for passing args to main_thread()
	pargs.probLand = probLand;
	pargs.probTake = probTake;
	
	//generates main thread
    if(pthread_create(&mainThread, &attr, main_thread, (void *)&pargs))
    {
		printf("error creating mainThread.");
    	return -1;
    }
	//wait for main thread ends
    if(pthread_join(mainThread, NULL))
    {
        printf("error joining main_thread");
    	return -1;
    }
	
	return 0;
	
}

