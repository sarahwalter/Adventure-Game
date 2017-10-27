#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>

/* Defining helpful constants */
#define MAX_ROOM_NAME_LENGTH 32
#define NUM_REQUIRED_ROOMS 7
#define MAX_ROOM_COUNT 10
#define MIN_ROOM_CONNECTIONS 3
#define MAX_ROOM_CONNECTIONS 6

/* Bool doesn't exist in ANSI C, so I chose to define it */
typedef enum { false, true } bool;

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
bool isGraphFull(const Graph *g);				// Used to determine if graph is full, rooms have required connections
void AddRandomConnection(Graph *g);				// Used to add connections between rooms
Room* GetRandomRoom(Graph *g);					// Used to randomly assign rooms
bool IsConnected(Room *from, Room *to);			// Used to determine if a connection exists between rooms
bool CanAddConnectionFrom(const Room *r);		// Used to determine if a valid connection can be made 
void ConnectRoom(Room *a, const Room* b);		// Used to create a connection between two rooms
bool IsSameRoom(const Room *a, const Room *b);	// Determine if rooms pointed to by a and b are same

/* Main entry point */
int main(int argc, char** argv)
{
    /* Create 10 room names. I envision my game in a mansion, murder mystery style */
    char roomNames[MAX_ROOM_COUNT][MAX_ROOM_NAME_LENGTH];
    memset(roomNames, 0, sizeof(char) * MAX_ROOM_COUNT * MAX_ROOM_NAME_LENGTH);
    memcpy(roomNames[0], "Conservatory", 12);
    memcpy(roomNames[1], "Lounge", 6);
    memcpy(roomNames[2], "Kitchen", 7);
    memcpy(roomNames[3], "Library", 7);
    memcpy(roomNames[4], "Hall", 4);
    memcpy(roomNames[5], "Study", 5);
    memcpy(roomNames[6], "Ballroom", 8);
    memcpy(roomNames[7], "DiningRoom", 10);
    memcpy(roomNames[8], "BilliardRoom", 12);
    memcpy(roomNames[9], "Courtyard", 9);

    Graph graph;
    memset(&graph, 0, sizeof(Graph));

    /* The purpose here is to keep track of the rooms we add to the graph. */
    Room allRooms[MAX_ROOM_COUNT];
    int numUsedRooms = 0;
    int numAllRooms = 0;

    int i;
    /* Fill out the allRoomes array in the graph. */
    for(i = 0; i < MAX_ROOM_COUNT; i++)
    {
        Room r;
        r.roomType = MID_ROOM;
        r.connectCount = 0;
        memcpy(r.name, &roomNames[i], sizeof(char) * MAX_ROOM_NAME_LENGTH);
        memcpy(&allRooms[i], &r, sizeof(Room));
        numAllRooms++;
    }

    srand(time(NULL));
    /* Move room definitions from the allRooms array to the usedRooms array 
    ** until we have the correct number of rooms.  
    ** TODO: might be able to move the allRooms array out of the graph */
    for(i = 0; i < NUM_REQUIRED_ROOMS; i++)
    {
        /* Get random index */
        int idx = rand() % numAllRooms;

        /* Copy the room at this index into the rooms array, 
         ** and zero-out the old entry in allRooms array */
        memcpy(&graph.rooms[numUsedRooms], &allRooms[idx], sizeof(Room));
        memset(&allRooms[idx], 0, sizeof(Room));
        numAllRooms--;
        numUsedRooms++;

        /* Collapes allRooms array, so Rooms are once again tightly packed */
        while(idx <= numAllRooms)
        {
            memcpy(&allRooms[idx], &allRooms[idx+1], sizeof(Room));
            idx++;
        }
    }


    /* Randomly assign the start and end rooms since we need a different path every time */
    Room *start = NULL;
    Room *end = NULL;
    do
    {
        start = GetRandomRoom(&graph);
        end = GetRandomRoom(&graph);
    } while (IsSameRoom(start, end));		// Since the start and end point need to be different

    start->roomType = START_ROOM;
    end->roomType = END_ROOM;

    /* Build the random room connections */
    while(!isGraphFull(&graph))
    {
        AddRandomConnection(&graph);
    }

    /* Write room files */
    int pid = getpid();
    int length = snprintf(NULL, 0, "waltsara.rooms.%d", pid);
    char directory[length];
    sprintf(directory, "waltsara.rooms.%d", pid);

    /* Only create if directory does not exist */
    struct stat st;
    if(stat(directory, &st) == -1)
    {
        if(mkdir(directory, 0755) == -1)
        {
            perror("Failed to create file directory.");
        }
    }

    /* Move to the new directory and create the room files */
    chdir(directory);
    for(i = 0; i < NUM_REQUIRED_ROOMS; i++)
    {
        int len = strlen(graph.rooms[i].name);
        char *nameBuffer = (char*)calloc(len + 6, sizeof(char));
        strcat(nameBuffer, graph.rooms[i].name);
        strcat(nameBuffer, "_room");			// I chose to append _room to each room name to use as a file name
        FILE* theFile = fopen(nameBuffer, "w");
        if(theFile != NULL)
        {
            /* Write the file name */
            fprintf(theFile, "ROOM NAME: %s\n", graph.rooms[i].name);

            /* Write all connection data */
            int j;
            for(j = 1; j <= graph.rooms[i].connectCount; j++)
            {
                fprintf(theFile, "CONNECTION %d: %s\n", j, graph.rooms[i].connections[j-1]);
            }

            /* Write the type of room */
            if(graph.rooms[i].roomType == START_ROOM)
            {
                fputs("ROOM TYPE: START_ROOM\n", theFile);
            }
            else if(graph.rooms[i].roomType == MID_ROOM) // So for example, if my room type is a mid room it needs to be labeled as such in the file directory
            {
                fputs("ROOM TYPE: MID_ROOM\n", theFile);
            }
            else if(graph.rooms[i].roomType == END_ROOM)
            {
                fputs("ROOM TYPE: END_ROOM\n", theFile);
            }

            if(fclose(theFile) != 0)
            {
                perror("Unable to close file.");
            }
        }
        else
        {
            perror("Failed to create file.");
        }
    }

    return 0;
}

/*
 *  Determines if the specified graph is full
 *  
 *  A full graph is defined in the assignment as a graph whose rooms all
 *  contain between 3 and 6 outgoing connections.
 */
bool isGraphFull(const Graph* graph)
{
    bool result = true;

    /* Loop will break out when result is set to false */
    int i;
    for(i = 0; i < NUM_REQUIRED_ROOMS && result; i++)
    {
        int numRooms = graph->rooms[i].connectCount;
        result = (numRooms <= MAX_ROOM_CONNECTIONS) && (numRooms >= MIN_ROOM_CONNECTIONS);
    }

    return result;
}

/*
 *  Adds one valid connection between two rooms in the specified graph.
 */
void AddRandomConnection(Graph* graph)
{
    Room* a;
    Room* b;

    while(true)
    {
        a = GetRandomRoom(graph);
        if(CanAddConnectionFrom(a))
        {
            break;
        }
    }

    do {
        b = GetRandomRoom(graph);
    } while(!CanAddConnectionFrom(b) || IsSameRoom(a, b) || IsConnected(a, b));

    ConnectRoom(a, b);		// If rooms aren't the same or already connected, make connections
    ConnectRoom(b, a);
}

/*
 *  Returns a pointer to a random room stored inside the specified graph.
 */
Room* GetRandomRoom(Graph* g) {
    int idx = rand() % NUM_REQUIRED_ROOMS;
    return &g->rooms[idx];
}

/*
 *  Determines if a connection exists between the rooms pointed 
 *  to by 'from' and 'to'
 */
bool IsConnected(Room *from, Room *to)
{
    bool result = false;
    /* Checks all connections of 'from' to make sure 
    ** that there is no existing connection to 'to' */
    int i;
    for(i = 0; i < from->connectCount && !result; i++)
    {
        result = (strcmp(to->name, from->connections[i]) == 0);
    }

    return result;
}

/*
 *  Determines if a room can be connected to.
 */
bool CanAddConnectionFrom(const Room* r)
{
    return r->connectCount != MAX_ROOM_CONNECTIONS;
}

/*
 *  Creates a connection from room 'a' to room 'b'
 *
 *  Note that this function only creates a connection in a single 
 *  direction (a to b). Thus, to create a bi-directional connection,
 *  this function must be called twice. i.e...
 *
 *  ConnectRoom(a, b)
 *  ConnectRoom(b, a)
 */
void ConnectRoom(Room *a, const Room* b)
{
    /* Add b as a connection to a */
    int numConnectionsA = a->connectCount;
    memcpy(a->connections[numConnectionsA], b->name, MAX_ROOM_NAME_LENGTH * sizeof(char));
    a->connectCount = numConnectionsA + 1;
}

/*
 *  Determines if the rooms pointed to by 'a' and 'b' are the same room.
 */
bool IsSameRoom(const Room* a, const Room* b)
{
    /* Since names must be unique, only the same room if the names are equal. */
    const char *aName = a->name;
    const char *bName = b->name;

    return (strcmp(aName, bName) == 0);
}
