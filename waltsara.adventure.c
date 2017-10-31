
#include <dirent.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
#include <pthread.h>

/* Helpful constants */
#define MAX_ROOM_NAME_LENGTH 32
#define NUM_REQUIRED_ROOMS 7
#define MAX_ROOM_COUNT 10
#define MIN_ROOM_CONNECTIONS 3
#define MAX_ROOM_CONNECTIONS 6

/* Bool doesn't exist in ANSI C, so I chose to define it */
typedef enum { false, true } bool;

/* Mutex and condition variables */
pthread_mutex_t timeMutex = PTHREAD_MUTEX_INITIALIZER;
pthread_cond_t condition = PTHREAD_COND_INITIALIZER;

/* Predicates used to control reading/writing time and terminating the thread. */
bool doTime = false;
bool timeDone = false;
bool gameDone = false;

/* Enumeration for Room type */
typedef enum {
  START_ROOM = 0,
  MID_ROOM,
  END_ROOM
} Type;

/* Struct for Room data, gives us everything we need to know about the room */
typedef struct
{
    char  name[MAX_ROOM_NAME_LENGTH];
    char  connections[MAX_ROOM_CONNECTIONS][MAX_ROOM_NAME_LENGTH];
    int   connectCount;
    Type  roomType;
} Room;

/* The purpose of the graph is just to serve as a container for rooms */
typedef struct
{
    Room rooms[NUM_REQUIRED_ROOMS];
} Graph;

/* Forward-declarations */
Room *GetRoomFromName(Graph *g, char *name);		// Get room in graph with specified name
void InitializeGraph(Graph *g, char *directory);	// Use directory to initialize graph
void InitializeRoom(Room *r, char *filename);		// Initialize room with contents of its file
char *GetPossibleConnections(Room *r);			// Returns string containing all possible connections to r
Room *GetStartRoom(Graph *g);				// Gets pointer to start room of graph
Room *GetEndRoom(Graph *g);				// Gets pointer to end room of graph

/*
 * To be executed by the time thread. timeFile is a pointer to a FILE* that
 * will be used for writing the time. Loops indefinitely and waits to be
 * woken up by the main thread before processing. 
 */
void *WriteTime(void *timeFile)
{
    while(1)
    {
        pthread_mutex_lock(&timeMutex);					// Lock the mutex object 					
        while(doTime == false && gameDone == false)
        {
            pthread_cond_wait(&condition, &timeMutex);			// Release mutex until condition complete and mutex available
        }

        if(gameDone)							// Break when we finish playing
        {
            break;
        }
		/* Get things set up to write to the file */
        FILE *file = (FILE*)timeFile;
        time_t theTime;
        time(&theTime);
        struct tm *timeinfo =  localtime(&theTime);

        char buffer[80];
        strftime(buffer, 80, "%I:%M%P, %A, %B %e, %G\n", timeinfo); 		// Format the time and date
        file = fopen("currentTime.txt", "w");					// File handling
        fputs(buffer, file);							// Writes the character array to the file
        fclose(file);

        doTime = false;
        timeDone = true;
        pthread_mutex_unlock(&timeMutex);					// Release the mutex object
        pthread_cond_signal(&condition);
    }
    pthread_exit(0);
}

/* Main entry point */
int main(int argc, char** argv)
{
    /* Find the appropriate directory - search for substring then see if rest of the string is an int
	* (the PID). If it is, compare the timestamp to find the most current one
	*/
    DIR *dp = opendir(".");
    struct dirent *curEntry = NULL;
    struct dirent latestEntry;
    memset(&latestEntry, 0, sizeof(struct dirent));
    time_t latestTime;
    memset(&latestTime, 0, sizeof(time_t));
    struct stat st;
    char *searchStr = "waltsara.rooms.";			// Search for directory with matching substring waltsara.rooms

    /* Check all directory entries that match the search string */
    while((curEntry = readdir(dp)) != NULL)
    {
        stat(curEntry->d_name, &st);
        if(S_ISDIR(st.st_mode) && (strlen(curEntry->d_name) > strlen(searchStr)))
        {
            if(strstr(curEntry->d_name, searchStr) != NULL)
            {
                int searchStrLen = strlen(searchStr);			// Take the length of the search string (file name) 
				char *pid = &curEntry->d_name[searchStrLen];
                if(strtol(pid, NULL, 0) > 0)
                {
                    if(st.st_mtime> latestTime)
                    {
                        memcpy(&latestEntry, curEntry, sizeof(struct dirent));
                        latestTime = st.st_mtime;
                    }
                }
            }
        }
    }

    closedir(dp);

    /* No suitable entry found, exit. */
    if(latestEntry.d_name == NULL)
    {
        fprintf(stderr, "No room directories found.");
        return -1;
    }

    /* Initialize the graph using the directory that we found. */
    Graph graph;
    InitializeGraph(&graph, latestEntry.d_name);

    /* Graph has been created */
    Room *start = GetStartRoom(&graph);
    Room *end   = GetEndRoom(&graph);
    Room *cur = start;

    /* Allocate buffer to store the user's path */
    int bufferSize = 100;
    char *pathTaken = (char*)calloc(bufferSize, sizeof(char));

    /* Create the thread for time feature */
    pthread_t   timeThread;
    FILE       *timeFile;
    if(pthread_create(&timeThread, NULL, WriteTime, timeFile) != 0)
    {
        fprintf(stderr, "Failed to create time thread.");
        return -1;
    }

    /* Begin main game loop */
    int steps = 0;
    while(cur != end)
    {

        /* Display the current state */
        char *connections = GetPossibleConnections(cur);
        printf("CURRENT LOCATION: %s\n", cur->name);
        printf("POSSIBLE CONNECTIONS: %s\n", connections);
        printf("WHERE TO? > ");

        /* Get input from the user */
        char line[MAX_ROOM_NAME_LENGTH + 1];
        if(fgets(line, MAX_ROOM_NAME_LENGTH + 1, stdin) != NULL)
        {
            /* Replace newline with null terminator */
            int length = strlen(line);
            line[length-1] = '\0';

            /* Split the string on space, comma, and period.
             ** Continue until we run out or find a match. */
            char *validName = strtok(connections, " ,.");
            while(validName != NULL && (strcmp(line, validName) != 0))
            {
                validName = strtok(NULL, " ,."); 
            }

            
            if(validName != NULL) /* Match found */
            {
                /* Retrieve the target room and update the path taken */
                cur = GetRoomFromName(&graph, line);
                if(strlen(pathTaken) + strlen(line) + 2 > bufferSize)
                {
                    /* May need to resize the buffer if the next location
                     ** would cause it to overflow. */
                    bufferSize += 100;
                    char *newPath = (char *)calloc(bufferSize, sizeof(char));
                    strcat(newPath, pathTaken);
                    free(pathTaken);
                    pathTaken = newPath;
                }

                strcat(pathTaken, cur->name);
                strcat(pathTaken, "\n");
                steps++;
            }
            else if(strcmp("time", line) == 0) /* User wants the time */
            {
                /* Signal the thread to write time to the file
                 ** wait for it to complete */
                pthread_mutex_lock(&timeMutex);
                doTime = true;
                pthread_cond_signal(&condition); 
                while(!timeDone)
                {
                    pthread_cond_wait(&condition, &timeMutex);
                }

                /* Open the file and read its contents to display */
                char buffer[80];
                timeFile = fopen("currentTime.txt", "r");
                fgets(buffer, 80, timeFile);
                fclose(timeFile);
                timeDone = false;
                pthread_mutex_unlock(&timeMutex);
                printf("%s", buffer);
            }
            else    /* Invalid input */
            {
                printf("HUH? I DON’T UNDERSTAND THAT ROOM. TRY AGAIN.\n");
            }
        }

        /*  GetPossibleConnections returns a malloc'd string, so free it */
        free(connections);
    }

    /* User has found the end room */
    printf("YOU HAVE FOUND THE END ROOM. CONGRATULATIONS!\n");
    printf("YOU TOOK %d STEPS. YOUR PATH TO VICTORY WAS:\n", steps);
    printf(pathTaken);

    /* Lock the mutex to update the gameDone variable, 
     ** signal, and wait to join */
    pthread_mutex_lock(&timeMutex);
    gameDone = true;
    pthread_mutex_unlock(&timeMutex);
    pthread_cond_signal(&condition);
    pthread_join(timeThread, NULL);

    free(pathTaken);
    return 0;
}

/*
 * Retreives the room in the graph with the specified name, or NULL if not found.
 */
Room *GetRoomFromName(Graph *graph, char *name)
{
    Room *result = NULL;

    int i;
    for(i = 0; i < NUM_REQUIRED_ROOMS && result == NULL; i++)
    {
        if(strcmp(name, graph->rooms[i].name) == 0)
        {
            result = &graph->rooms[i];
        }
    }

    return result;
}

/*
 * Initializes the specified graph with the room files in the specified directory.
 */
void InitializeGraph(Graph *graph, char *directory)
{
    DIR *dp = opendir(directory);
    if(dp != NULL)
    {
        chdir(directory);
        struct dirent *curEntry = NULL;

        int curRoom = 0;
        struct stat st;
        while((curEntry = readdir(dp)) != NULL)				// Make sure we're looking in a valid location
        {
            stat(curEntry->d_name, &st);
            if(S_ISREG(st.st_mode))
            {
                InitializeRoom(&graph->rooms[curRoom], curEntry->d_name); 	// Initialize the room
                curRoom++;							// Then iterate through
            }

        }
        /* Go back */
        chdir("..");
        closedir(dp);
    }
}

/*
 * Initializes the specified room with the contents of the specified filename.
 */
void InitializeRoom(Room *room, char *filename)
{

    FILE *f = fopen(filename, "r");
    char line[80];
    int roomCount = 0;
    while(fgets(line, 80, f) != NULL)
    {
        /* Replace newline with null terminator */
        int length = strlen(line);
        line[length-1] = '\0';
        if(strstr(line, "NAME") != NULL)
        {
            strcpy(room->name, &line[11]);
        }
        else if(strstr(line, "CONNECTION") != NULL)
        {
            strcpy(room->connections[roomCount], &line[14]);
            roomCount++;
            room->connectCount = roomCount;
        }
        else if(strstr(line, "END_ROOM") != NULL)
        {
            room->roomType = END_ROOM;
        }
        else if(strstr(line, "MID_ROOM") != NULL)
        {
            room->roomType = MID_ROOM;
        }
        else if(strstr(line, "START_ROOM") != NULL)
        {
            room->roomType = START_ROOM;
        }
    }
    fclose(f);
}

/*
 * Gets a pointer to the start room of the specified graph.
 */
Room *GetStartRoom(Graph *graph)
{
    Room *result = NULL;

    int i;
    for(i = 0; i < NUM_REQUIRED_ROOMS && result == NULL; i++)
    {
        if(graph->rooms[i].roomType == START_ROOM)		// Look for the start room
        {
            result = &graph->rooms[i];
        }
    }

    return result;
}

/*
 * Gets a pointer to the end room of the specified graph.
 */
Room *GetEndRoom(Graph *graph)
{
    Room *result = NULL;

    int i;
    for(i = 0; i < NUM_REQUIRED_ROOMS && result == NULL; i++)
    {
        if(graph->rooms[i].roomType == END_ROOM)		// Look for the end room
        {
            result = &graph->rooms[i];
        }
    }

    return result;
}

/*
 * Returns a string containing all possible connections to r.
 *
 * Format of string is "<name1>, <name2>, ..., <nameN>."
 *
 * NOTE: the returned string is malloc'd and must be freed by the caller.
 */
char *GetPossibleConnections(Room *r)
{
    char *result = (char*)calloc(256, sizeof(char));

    int i;
    for(i = 0; i < r->connectCount; i++)
    {
        strcat(result, r->connections[i]);
        if(i < r->connectCount - 1)
        {
            strcat(result, ", ");			// Used to format our string with commas between room names
        }
        else
        {
            strcat(result, ".");			// Format string with a period at the end of the room list
        }
    }

    return result;
}


