#ifndef CSVLIB_H
#define CSVLIB_H

#include <stdio.h> 
#include <stdlib.h> 
#include <string.h> 
#include <stdbool.h>

/// Max buffer size of csv single line 
#define MAX_LINE_BUFFER 512
/// File open mode: read
#define STR_FILE_OPEN_MODE_READ "r"
/// File open mode: write
#define STR_FILE_OPEN_MODE_WRITE "w"
/// Default(minimum) number of separation chara(comma) in single food info
#define INT_DEFAULT_SPLIT_COUNT 7
/// Implemantation status: success
#define APPLIB_SUCCESS 0
/// Implemantation status: file open error
#define APPLIB_ERR_OPEN -2
/// Implemantation status: file remove error
#define APPLIB_ERR_REMOVE -3
/// Implemantation status: file rename error
#define APPLIB_ERR_RENAME -4

/// Comma
#define STR_COMMA ","

/// Food information
typedef struct foodInfo foodinfo_t;
struct foodInfo
{
	char *name;
	char *measure;
	int weight;
	int kCal;
	int fat;
	int carbo;
	int protein;
	bool isAdded;
};


/// Comment character: #
char STR_COMMENT_CHAR[] = "#";
/// Carrige return character
char STR_CR[] = "\n";
/// Variable to store status
int gCSVResult;

/// ----- Function definitions
int getCharCount(char*, char*);
int getLineCount(FILE*, int);
int getCommentLineCount(FILE*, int);
int getFoodNameLength(char**, int);
foodinfo_t **readCSV(int*, char*);
foodinfo_t *getFoodInfo(char*);
void createFoodInfoText(char*, char*, char*, char*, char*, char*, char*, char*);
void writeCSV(char*, foodinfo_t**, int);


/**
 * Write food info in specified csv file.
 *
 *	@param fileName	Target file name.
 *	@param saveData	Food data to be written in the csv file.
 *	@param count 	The number of food info.
 */
void writeCSV(char *fileName, foodinfo_t **saveData, int count)
{
	FILE *fp, *fp2;
	
    char readLine[MAX_LINE_BUFFER];
	gCSVResult = APPLIB_SUCCESS;
	//file open mode: read
    if ((fp = fopen(fileName, STR_FILE_OPEN_MODE_READ)) == NULL)
	{
		gCSVResult = APPLIB_ERR_OPEN;
		//exit(EXIT_FAILURE);
		return;
    }
	int commentCount = getCommentLineCount(fp, MAX_LINE_BUFFER);
	char **comment = (char **)calloc(commentCount, sizeof(char));
	
	int i;
    char *ret;
	int index = 0;
	char *line;
	while ( fgets(readLine, sizeof(readLine), fp) != NULL )
	{
		//look up comment line
    	ret = strstr(readLine, STR_COMMENT_CHAR);
    	if(ret != NULL)
    	{
    		//add comment line (the line that has "#" in the first char)
    		if(ret - readLine == 0)
    		{
    			line = (char *)calloc(strlen(readLine), sizeof(char));
    			strcpy(line, readLine);
    			comment[index++] = line;
    		}
    	}
    }
    fclose(fp);
	
	char *tempName = "temp.csv";
	foodinfo_t *info;
    //file open mode: write
	if ((fp2 = fopen(tempName, STR_FILE_OPEN_MODE_WRITE)) == NULL)
	{
		gCSVResult = APPLIB_ERR_OPEN;
		//exit(EXIT_FAILURE);
		return;
    }
	for(i = 0; i < commentCount; i++)
	{
		//printf("comment[%d] = %s\n", i, comment[i]);
		fprintf(fp2, "%s", comment[i]);
	}
	for(i = 0; i < count; i++)
	{
		//write single line
		info = saveData[i];
		fprintf(fp2, "%s,%s,%d,%d,%d,%d,%d\n", info->name, info->measure, info->weight,
			info->kCal, info->fat, info->carbo, info->protein);
	}
	
	fclose(fp2);
	char *tempName2 = "temp2.csv";
	//rename and remove file
	if(rename(fileName, tempName2) != 0)
	{
		gCSVResult = APPLIB_ERR_RENAME;
		return;
	}
	if(rename(tempName, fileName) != 0)
	{
		gCSVResult = APPLIB_ERR_RENAME;
		return;
	}
	if(remove(tempName2) != 0)
	{
		gCSVResult = APPLIB_ERR_REMOVE;
		return;
	}
	
}

/**
 * Read csv file and load all contents except for comment.
 *
 *	@param count	The number of food information in the file
 *	@param fileName	csv file name
 */
foodinfo_t **readCSV(int *count, char *fileName)
{
    FILE *fp;
    char readLine[MAX_LINE_BUFFER];
	char *splitChar;
	int lineCount = 0;
    char *ret;
	gCSVResult = APPLIB_SUCCESS;
	// file open
    if ((fp = fopen(fileName, STR_FILE_OPEN_MODE_READ)) == NULL)
	{
		gCSVResult = APPLIB_ERR_OPEN;
		return NULL;
    }

	//initialize foodList
	lineCount = getLineCount(fp, MAX_LINE_BUFFER);
	foodinfo_t **foodList = (foodinfo_t **)calloc(lineCount, sizeof(foodinfo_t));
	
	int index = 0;
	*count = lineCount;
	while ( fgets(readLine, sizeof(readLine), fp) != NULL )
	{
		//search comment line
    	ret = strstr(readLine, STR_COMMENT_CHAR);
    	if(ret != NULL)
    	{
    		//ignore comment line (the line that has "#" in the first char)
    		if(ret - readLine == 0) continue;
    	}
    	
    	foodinfo_t *info = getFoodInfo(readLine);
		foodList[index++] = info;
    }
    fclose(fp);
	return foodList;
}


/**
 * Get single food information.
 *
 *	@param infoText Single food information in csv file
 *	@return Single food information
 */
foodinfo_t *getFoodInfo(char *infoText)
{
	char *splitChar;
	//get the number of comma in infoText variable so that 
	//the value in infoText can be split by comma into an array
	int cnt = getCharCount(infoText, STR_COMMA);
	char *oneLine[cnt];
	int i = 0;
	
	//split current line by comma and place in an array
	splitChar = strtok(infoText, STR_COMMA);
	while(splitChar != NULL)
	{
		oneLine[i++] = splitChar;
		splitChar = strtok(NULL, STR_COMMA);
	}
	oneLine[i++] = splitChar;
	
	//food name may be split by the process above, this process may marge the name with comma.
	//if the name does not contain comma, the variable offset should be 1.
	int offset = cnt - INT_DEFAULT_SPLIT_COUNT + 1;
	int nameBufSize = getFoodNameLength(oneLine, offset);
	char *foodName = (char *)calloc((nameBufSize), sizeof(char));
	
	for(i = 0; i < offset; i++)
	{
		//oneLine[0] ... oneLine[(offset-1)] have a part of food name.
		strcat(foodName, oneLine[i]);
		if(i != offset - 1) strcat(foodName, STR_COMMA);
	}
	char *measure = (char *)calloc((strlen(oneLine[offset]) + 1), sizeof(char));
	strcat(measure, oneLine[offset]);
	
	i = offset + 1;
	foodinfo_t *info = (foodinfo_t *)malloc(sizeof(foodinfo_t));
	info->name = foodName;
	info->measure = measure;
	info->weight = atoi(oneLine[i++]);
	info->kCal = atoi(oneLine[i++]);
	info->fat = atoi(oneLine[i++]);
	info->carbo = atoi(oneLine[i++]);
	info->protein = atoi(oneLine[i++]);
	return info;
}


/**
 * Get the number of characterds of food name.
 *
 *	@param charArray	food name
 *	@return The number of characters of food name
 */
int getFoodNameLength(char **charArray, int max)
{
	if(charArray == NULL)
	{
		return 0;
	}
	
	int i;
	int ret = 0;
	for(i = 0; i < max; i++)
	{
		ret += strlen(charArray[i]);
		ret++;//for comma
	}
	ret++;//for '\0'
	//printf("ret = %d\n", ret);
	
	return ret;
}


/**
 * Count the number of a target character in a target string
 * 
 * @param target	target string
 * @param chr		target character
 * @return the number of a target character in target string.
 */
int getCharCount(char* target, char* chr)
{
	if(target == NULL || chr == '\0')
	{
		return 0;
	}
	
	int i = 0;
	int ret = 0;
	while(target[i] != '\0')
	{
		if(target[i] == *chr)
		{
			ret++;
		}
		i++;
	}
	ret++;
	
	return ret;
}

/**
 * Get the number of lines in csv file
 *
 *	@param fp		File pointer object
 *	@param bufSize	Buffer size for reading single line
 *	@return The number of lines
 */
int getLineCount(FILE *fp, int bufSize)
{
	int count = 0;
	char *ret;
    char buf[bufSize];
	
	while ( fgets(buf, sizeof(buf), fp) != NULL )
	{
    	ret = strstr(buf, STR_COMMENT_CHAR);
    	if(ret != NULL)
    	{
    		if(ret - buf == 0)
    		{
    			//the first character is "#" means the line is comment, thus ignore.
    		}
    		else
    		{
    			count++;
    		}
    	}
		else
		{
			count++;
		}
	}

	rewind(fp);
	return count;
}

/**
 * Get comment in csv file.
 *
 *	@param fp		File pointer object
 *	@param bufSize	Buffer size for reading single line
 *	@return The number of comment lines
 */
int getCommentLineCount(FILE *fp, int bufSize)
{
	int count = 0;
	char *ret;
    char buf[bufSize];
	
	while ( fgets(buf, sizeof(buf), fp) != NULL )
	{
    	ret = strstr(buf, STR_COMMENT_CHAR);
    	if(ret != NULL)
    	{
    		if(ret - buf == 0)
    		{
    			//the first character is "#" means the line is comment, thus ignore.
    			count++;
    		}
    	}
	}

	rewind(fp);
	return count;
}


/**
 * Create food information string
 *
 *	@param target	The variable to store all characters
 *	@param	name	food name
 *	@param measure 	measure
 *	@param weight	weight
 *	@param kCal		kCal
 *	@param fat		fat
 *	@param carbo	carbo
 *	@param protein	protein
 */
void createFoodInfoText(char *target, char *name, char *measure, char *weight, 
	char *kCal, char *fat, char *carbo, char *protein)
{
	if(name == '\0' || measure == '\0' || weight == '\0' || kCal == '\0' 
		|| fat == '\0' || carbo == '\0' || protein == '\0')
	{
		return;
	}
	strcat(target, name);
	strcat(target, STR_COMMA);
	strcat(target, measure);
	strcat(target, STR_COMMA);
	strcat(target, weight);
	strcat(target, STR_COMMA);
	strcat(target, kCal);
	strcat(target, STR_COMMA);
	strcat(target, fat);
	strcat(target, STR_COMMA);
	strcat(target, carbo);
	strcat(target, STR_COMMA);
	strcat(target, protein);
	strcat(target, "\n");
}


#endif
