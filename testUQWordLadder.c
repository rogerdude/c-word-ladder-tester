/*
 * testUQWordLadder
 * CSSE2310 A3
 * Author: Hamza
 */

#include <csse2310a3.h>
#include <stdio.h>
#include <stdlib.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <sys/wait.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <errno.h>
#include <signal.h>

// Required number of command line arguments
#define MIN_ARGC 3
#define MAX_ARGC 8
#define NUM_REQUIRED_ARGS 2
#define NUM_OPTIONAL_ARGS 2

// Valid command line arguments
#define TESTDIR_ARG "--testdir"
#define RECREATE_ARG "--recreate"
#define JOBSPECFILE_ARG "jobspecfile"
#define PROGRAM_ARG "program"

// Default path for test directory
#define DEFAULT_TESTDIR_PATH "./tmp"

// Index for these within split string.
#define TEST_ID 0
#define INPUT_FILEPATH 1

// File types for output files
#define NUM_OF_TYPES 3
#define STDOUT_TYPE ".stdout"
#define STDERR_TYPE ".stderr"
#define EXITSTATUS_TYPE ".exitstatus"

// Message when rebuilding output files.
#define REBUILD_MSG "Rebuilding expected output for test %s\n"

// Command to run good-uqwordladder
#define GOOD_UQWORDLADDER "good-uqwordladder"

// Buffer for reading or writing to .exitstatus file.
#define EXITSTATUS_BUFFER 5

// Indexes for pid array to increase clarity.
#define TOTAL_PIDS 3
#define STDOUT_PID 0
#define STDERR_PID 1
#define UQWORDLADDER_PID 2

// Read and write end for pipes
#define READ_END 0
#define WRITE_END 1

// Filepath for redirecting cmp stdout and stderr 
#define EMPTY_FILE "/dev/null"

// Command for executing cmp
#define CMP_ARG "cmp"

// 1.5s delay for each test
#define TEST_DELAY 1500000

// Constants for reporting test results
#define STDOUT_REPORT "Stdout"
#define STDERR_REPORT "Stderr"
#define EXITSTATUS_REPORT "Exit status"
#define REPORT_MSG "Job %s: %s %s\n"
#define REPORT_MATCHES "matches"
#define REPORT_DIFFERS "differs"
#define MATCHES 0
#define RUN_TEST_MSG "Running test %s\n"
#define SUCCESSFUL_TEST_MSG "testuqwordladder: %d out of %d tests passed\n"
#define NO_TEST_MSG "testuqwordladder: No tests have been completed\n"

// Required number of matches for a successful test
#define REQUIRED_MATCHES 3

// Error messages
#define USAGE_ERR_MSG "Usage: testuqwordladder [--diffshow N] " \
    "[--testdir dir] [--recreate] jobspecfile program\n"
#define JOBSPECFILE_OPEN_ERR_MSG "testuqwordladder: Unable to open job " \
    "file \"%s\"\n"
#define JOBSPECFILE_SYNTAX_ERR_MSG "testuqwordladder: Syntax error on line" \
    " %d of job file \"%s\"\n"
#define JOBSPECFILE_EMPTY_ERR_MSG "testuqwordladder: Test file \"%s\" " \
    "was empty\n"
#define JOBSPECFILE_DUP_ERR_MSG "testuqwordladder: Line %d of file \"%s\":" \
    " duplicate test ID\n"
#define INPUTFILE_OPEN_ERR_MSG "testuqwordladder: Unable to open input file" \
    " \"%s\" specified on line %d of job file \"%s\"\n" 
#define TESTDIR_CREATE_ERR_MSG "testuqwordladder: Unable to create " \
    "directory named \"%s\"\n"
#define OUTPUT_FILE_ERR_MSG "testuqwordladder: Can't open output file " \
    "\"%s\" for writing\n"
#define TEST_ERR_MSG "Unable to execute test %s\n"

// Program exit statuses
enum ExitStatus {
    OK = 0,
    USAGE_ERR = 14,
    JOBSPECFILE_OPEN_ERR = 20,
    JOBSPECFILE_SYNTAX_ERR = 16,
    JOBSPECFILE_EMPTY_ERR = 11,
    JOBSPECFILE_DUP_ERR = 5,
    INPUTFILE_OPEN_ERR = 19,
    TESTDIR_CREATE_ERR = 10,
    OUTPUT_FILE_ERR = 6,
    NO_TESTS = 9,
    UNSUCCESSFUL_TESTS = 18,
    UNEXPECTED_ERR = 99
};

// Job specifications file data struct
typedef struct {
    char* testID;
    char* inputFile;
    char** args;
    char* outputFile;
    char* errorFile;
    char* exitStatusFile;
} JobSpecs;

// Values of command line arguments
typedef struct {
    char* jobSpecFilePath;
    int numOfTests;
    JobSpecs* jobSpecs;
    char* program;
    char* testDir;
    bool recreate;
} ProgramParameters;

/* Function prototypes */
void check_num_args(int argc);
void check_args_validity(int argc, char** argv); 
void check_repeated_args(int argc, char** argv); 
void check_args_index(int argc, char** argv);
char* get_required_arg(int argc, char** argv, char* arg);
char* get_testdir(int argc, char** argv);
bool get_recreate(int argc, char** argv);
JobSpecs* open_jobspecfile(int argc, char** argv, int* numOfTests); 
char** add_test_args(char** splitLine, char* program); 
void check_line_syntax(char* line, int lineNumber, char* jobSpecFilePath);
bool check_test_id_syntax(char* testID); 
void free_split_string(char** splitLine); 
void check_jobspecs(JobSpecs* jobSpecs, char* jobSpecFilePath, 
	int lineNumber, int numOfTests);
void create_testdir(ProgramParameters parameters); 
char* get_filepath(char* testDir, char* type, char* testID); 
void create_output_files(ProgramParameters parameters); 
void store_output_filepath(ProgramParameters parameters, char* filePath,
	int typeNum, int testNum); 
void check_output_file(int fd, char* filePath); 
bool check_modification_time(char* outputFilePath, char* jobSpecFilePath); 
void run_gooduqwordladder(ProgramParameters parameters); 
void run_testjob(ProgramParameters parameters); 
void interrupt_handler(int sig);
void setup_sigaction(void);
void run_uqwordladder(ProgramParameters parameters, int* outputPipe,
	int* errorPipe, int testNum);
void run_three_processes(ProgramParameters parameters, pid_t* pid,
	int* outputPipe, int* errorPipe, int testNum);
void run_cmp(int* firstPipe, int* secondPipe, char* filePath); 
int* get_exit_codes(pid_t* pid);
bool check_test_error(JobSpecs* jobspecs, int* exitCodes, int testNum);
int report_cmp_results(JobSpecs* jobSpecs, int testNum, int* exitCodes); 
void check_interrupt(pid_t* pid, int successfulTests, int numOfRunTests); 
void kill_processes(pid_t* pid); 
void free_program_parameters(ProgramParameters parameters); 

// Global variable that signfies if the program was interrupt by SIGINT.
bool interrupted = false;

/* interrupt_handler()
 * -------------------
 * Sets the interrupted global variable to true if program was interrupted by
 * 	SIGINT.
 *
 * sig: the number of the signal
 *
 * Returns: void
 */
void interrupt_handler(int sig) {
    interrupted = true;
}

int main(int argc, char** argv) {
    // Check command line arguments
    check_num_args(argc);
    check_args_validity(argc, argv);
    check_repeated_args(argc, argv); 
    check_args_index(argc, argv);

    // Get all the required parameters from command line arguments.
    ProgramParameters parameters;
    parameters.program = get_required_arg(argc, argv, PROGRAM_ARG);
    parameters.testDir = get_testdir(argc, argv);
    parameters.recreate = get_recreate(argc, argv);
    parameters.jobSpecFilePath = get_required_arg(argc, argv, JOBSPECFILE_ARG);

    // Create a data struct of all tests from job specification file.
    parameters.jobSpecs = open_jobspecfile(argc, argv, &parameters.numOfTests);
    
    // Create the test directory with expected output from good-uqwordladder.
    create_testdir(parameters);
    create_output_files(parameters);

    // Run all tests for the uqwordladder program to be tested, compare its
    // output the expected results and report them.
    run_testjob(parameters);

    return 0;
}

/* check_num_args()
 * ----------------
 * Checks whether number of command line arguments are valid.
 *
 * argc: the number of command line arguments
 *
 * Errors: Exits with status 14 and the usage error message if number of
 * 	command line arguments is less than 3 or more than 8.
 */
void check_num_args(int argc) {
    if (argc < MIN_ARGC || argc > MAX_ARGC) {
	fprintf(stderr, USAGE_ERR_MSG);
	exit(USAGE_ERR);
    }
}

/* check_args_validity()
 * ---------------------
 * Checks if the optional command line arguments are valid
 *
 * argc: the number of command line arguments
 * argv: an array of arrays of the command line arguments
 *
 * Errors: Exits with exit status 14 and the usage error message if the
 * 	optional command line arguments are invalid.
 */
void check_args_validity(int argc, char** argv) {
    char* validArgs[NUM_OPTIONAL_ARGS] = {TESTDIR_ARG, RECREATE_ARG};
    
    // Iterate through all the command line args
    int numRequiredArgs = 0;
    int numInvalidArgs = 0;
    for (int i = 1; i < argc; i++) {
	if (argv[i][0] == '-') {
	    // Check if the optional arg is valid.
	    for (int j = 0; j < NUM_OPTIONAL_ARGS; j++) {
		if (strcmp(argv[i], validArgs[j]) != 0) {
		    numInvalidArgs++;
		}
	    }
	    // Exit program if command line arg is invalid.
	    if (numInvalidArgs >= NUM_OPTIONAL_ARGS) {
		fprintf(stderr, USAGE_ERR_MSG);
		exit(USAGE_ERR);
	    }

	    // Check if arg is "--testdir" and increase j by 1 to skip
	    // the 'dir' parameter
	    if (strcmp(argv[i], validArgs[0]) == 0) {
		i++;
	    }
	    // Reset counter for ntype arg that starts with '-'
	    numInvalidArgs = 0;
	} else {
	    numRequiredArgs++;
	}
    }
    // Checks if number of args that do not begin with '-' are equal to 2.
    if (numRequiredArgs != NUM_REQUIRED_ARGS) {
	fprintf(stderr, USAGE_ERR_MSG);
	exit(USAGE_ERR);
    }
}

/* check_repeated_args()
 * ---------------------
 * Checks if any optional command line arguments were repeated.
 *
 * argc: the number of command line arguments
 * argv: an array of arrays of the command line arguments
 *
 * Errors: Exits with exit status 14 and usage error message if any command
 * 	line arguments were repeated.
 */
void check_repeated_args(int argc, char** argv) { 
    char* validArgs[NUM_OPTIONAL_ARGS] = {TESTDIR_ARG, RECREATE_ARG};

    // Iterate through each valid arg
    int numRepeatedArgs;
    for (int i = 0; i < NUM_OPTIONAL_ARGS; i++) {
	numRepeatedArgs = 0;
	// Check each arg in command line argument and compare to validArgs
	for (int j = 1; j < argc; j++) {
	    if (strcmp(argv[j], validArgs[i]) == 0) {
		numRepeatedArgs++;
	    }
	    // Check if arg is "--testdir" and increase j by 1 to skip
	    // the 'dir' parameter
	    if (strcmp(argv[j], validArgs[0]) == 0) {
		j++;
	    }
	}
	// Check if any args were repeated.
	if (numRepeatedArgs > 1) {
	    fprintf(stderr, USAGE_ERR_MSG);
	    exit(USAGE_ERR);
	}
    }
}

/* check_args_index()
 * ------------------
 * Checks if the index of any optional args is after the required args.
 *
 * argc: the number of command line arguments
 * argv: an array of arrays of the command line arguments
 *
 * Errors: Exits with exit status 14 and prints usage error message if any
 * 	optional arguments are placed after any required arguments.
 */
void check_args_index(int argc, char** argv) {
    // Initialise variables to store index of arguments.
    int optionalArgsIndex[NUM_OPTIONAL_ARGS];
    int requiredArgsIndex[NUM_REQUIRED_ARGS];

    // Iterate through all args and sort the indexes.
    int numOptionalArgs = 0;
    int numRequiredArgs = 0;
    for (int i = 1; i < argc; i++) {
	if (argv[i][0] == '-') {
	    // Increase i by 1 if arg is "--testdir" to avoid 'dir' value.
	    if (strcmp(argv[i], TESTDIR_ARG) == 0) {
		optionalArgsIndex[numOptionalArgs++] = i++;
	    } else {
		optionalArgsIndex[numOptionalArgs++] = i;
	    }
	} else {
	    requiredArgsIndex[numRequiredArgs++] = i;
	}
    }

    // Check if the index of any required arg is less than any optional arg.
    for (int i = 0; i < numOptionalArgs; i++) {
	for (int j = 0; j < numRequiredArgs; j++) {
	    if (optionalArgsIndex[i] > requiredArgsIndex[j]) {
		fprintf(stderr, USAGE_ERR_MSG);
		exit(USAGE_ERR);
	    }
	}
    }
}

/* get_required_arg()
 * ------------------
 * Returns the specified 'required' argument's value from the command line
 * 	arguments
 *
 * argc: the number of command line arguments
 * argv: an array of arrays of the command line arguments
 *
 * Returns: the specified argument's value as a char pointer. But returns
 * 	NULL if it cannot be found.
 */
char* get_required_arg(int argc, char** argv, char* arg) {
    // Iterate through all command line arguments.
    for (int i = 1; i < argc; i++) {
	// Ignore the testdir and recreate arguments.
	if (strcmp(argv[i], TESTDIR_ARG) == 0) {
	    i++;
	    continue;
	}
	if (argv[i][0] != '-') {
	    if (strcmp(arg, JOBSPECFILE_ARG) == 0) {
		return argv[i];
	    } else {
		return argv[i + 1];
	    }
	}
    }
    return NULL;
}

/* get_testdir()
 * -------------
 * Returns the testdir argument's parameter 'dir' from command line arguments
 *
 * argc: the number of command line arguments
 * argv: an array of arrays of the command line arguments
 *
 * Returns: the 'dir' parameter given in the command line arguments. However
 * 	it returns NULL if it cannot be found.
 */
char* get_testdir(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
	if (strcmp(argv[i], TESTDIR_ARG) == 0) {
	    return argv[i + 1];
	}
    }
    return DEFAULT_TESTDIR_PATH;
}

/* get_recreate()
 * --------------
 * Returns whether the recreate argument has been specified in the command
 * 	line arguments.
 *
 * argc: the number of command line arguments
 * argv: an array of arrays of the command line arguments
 *
 * Returns: true if '--recreate' has been specified, else returns false.
 */
bool get_recreate(int argc, char** argv) {
    for (int i = 1; i < argc; i++) {
	if (strcmp(argv[i], RECREATE_ARG) == 0) {
	    return true;
	}
    }
    return false;
}

/* open_jobspecfile()
 * ------------------
 * Opens the job specification file specified in the command line arguments
 * 	to create a data structure of the tests to conduct.
 *
 * argc: the number of command line arguments.
 * argv: an array of arrays of the command line arguments.
 * numOfTests: a pointer to a integer which will store the number of tests.
 *
 * Returns: a pointer to an array of structs with all the tests from
 * 	jobspecfile, including test-id, inputfile, and command line arguments.
 * Errors: Exits with status 11 and empty file error if the jobspecfile does
 * 	not have any tests listed.
 */
JobSpecs* open_jobspecfile(int argc, char** argv, int* numOfTests) {
    // Check if job spec file can be opened.
    char* jobSpecFilePath = get_required_arg(argc, argv, JOBSPECFILE_ARG);
    FILE* jobSpecFile = fopen(jobSpecFilePath, "r");
    if (jobSpecFile == NULL) {
	fprintf(stderr, JOBSPECFILE_OPEN_ERR_MSG, jobSpecFilePath);
	exit(JOBSPECFILE_OPEN_ERR);
    }

    // Initialise JobSpecs struct and required variables.
    char* program = get_required_arg(argc, argv, PROGRAM_ARG);
    int numTests = 0;
    JobSpecs* jobSpecs = malloc(sizeof(JobSpecs) * numTests);

    // Read each line in the file
    int lineNumber = 1;
    char* line;
    while ((line = read_line(jobSpecFile)) != NULL) {
	// Skip the line if it is a comment or if empty.
	if (line[0] == '#' || strlen(line) == 0) {
	    lineNumber++;
	    free(line);
	    continue;
	}
	// Check if line has met the validity requirements
	check_line_syntax(strdup(line), lineNumber, jobSpecFilePath);
	
	// Add args to jobSpecs data struct
	// Make data structure with all elements of each line in file.
	char** splitLine = split_string(line, '\t');
	jobSpecs = realloc(jobSpecs, sizeof(JobSpecs) * ++numTests);
	jobSpecs[numTests - 1].testID = strdup(splitLine[TEST_ID]);
	jobSpecs[numTests - 1].inputFile = strdup(splitLine[INPUT_FILEPATH]);
	jobSpecs[numTests - 1].args = add_test_args(splitLine, program);
	
	// Check all arguments for errors.
	check_jobspecs(jobSpecs, jobSpecFilePath, lineNumber, numTests);

	free(line);
	free(splitLine);
	lineNumber++;
    }
    if (numTests == 0) {
	fprintf(stderr, JOBSPECFILE_EMPTY_ERR_MSG, jobSpecFilePath);
	exit(JOBSPECFILE_EMPTY_ERR);
    }
    fclose(jobSpecFile);
    *numOfTests = numTests;
    return jobSpecs;
}

/* add_test_args()
 * ---------------
 * Makes an array of arrays of all the command line arguments for the current
 * 	test.
 *
 * splitLine: an array of arrays from split_string() with all elements from a
 * 	line from the job specifications file.
 * program: a pointer to the name of the program to test
 *
 * Returns: an array of arrays with all the command line arguments for a test
 * 	from the job specifications file.
 */
char** add_test_args(char** splitLine, char* program) {
    char** args = malloc(sizeof(char*));
    
    args[0] = strdup(program);

    // Put all args into the args array to put into jobSpecs.
    int numOfArgs = 1;
    for (int i = 2; splitLine[i] != NULL; i++) {
	args = realloc(args, sizeof(char*) * ++numOfArgs);
	args[numOfArgs - 1] = strdup(splitLine[i]);
    }

    // Put NULL at end of args
    args = realloc(args, sizeof(char*) * ++numOfArgs);
    args[numOfArgs - 1] = NULL;
    return args;
}

/* check_line_syntax()
 * -------------------
 * Checks whether each line in the job specifications file is syntactically
 * 	correct.
 *
 * line: a line returned from read_line(jobspecfile) to check its syntax.
 * lineNumber: the current line number of the line that is being checked.
 * jobSpecFilePath: an array of the path to the jobspecfile given in command
 * 	line arguments.
 *
 * Errors: Exits with status 16 and syntax error if the file is syntactically
 * 	incorrect.
 */
void check_line_syntax(char* line, int lineNumber, char* jobSpecFilePath) {
    int lineLength = strlen(line);
    char** splitLine = split_string(line, '\t');

    // First checks if there are no tab spaces, then if first character is tab
    // space, then if remainder of line is empty, then if inputfile is empty,
    // and checks if test id syntax is correct.
    if (lineLength == strlen(splitLine[0]) ||
	    strcmp(splitLine[0], "\0") == 0 ||
	    splitLine[1] == NULL ||
	    !strlen(splitLine[INPUT_FILEPATH]) ||
	    check_test_id_syntax(splitLine[TEST_ID])) {
	free(line);
	free(splitLine);
	fprintf(stderr, JOBSPECFILE_SYNTAX_ERR_MSG, lineNumber,
		jobSpecFilePath);
	exit(JOBSPECFILE_SYNTAX_ERR);
    }
    free(line);
    free(splitLine);
}

/* check_test_id_syntax()
 * ----------------------
 * Checks if the specified testID contains any forward slash.
 *
 * testID: a pointer to an array of the first element from split_string()
 * 	when it is used in check_line_syntax()
 *
 * Returns: true if the testID contains a forward slash character, else
 * 	returns false.
 */
bool check_test_id_syntax(char* testID) {
    int length = strlen(testID);
    for (int i = 0; i < length; i++) {
	if (testID[i] == '/') {
	    return true;
	}
    }
    return false;
}

/* check_jobspecs()
 * ----------------
 * After the file has been checked for syntax error, this function conducts
 * 	further tests to validate the job specification file. This function
 * 	is used in open_jobspecfile() when it is being checked for validity.
 *
 * jobSpecs: a pointer to an array of structs containing information on the 
 * 	tests specified in the jobspecfile.
 * jobSpecFilePath: an array of the path to the jobspecfile given in command
 * 	line arguments.
 * lineNumber: the current line number of the line that is being checked.
 * numofTests: the number of tests in jobSpecs.
 *
 * Errors: Exits with status 6 and duplicate test-ID error if the test-ID of
 * 	the current line is the same as any of the previous test-IDs.
 * 	Exits with status 19 and inputfile error if the specified input file
 * 	path in the current line cannot be opened.
 */
void check_jobspecs(JobSpecs* jobSpecs, char* jobSpecFilePath, 
	int lineNumber, int numOfTests) {
    // Check for any repeated test IDs
    char* testID = strdup(jobSpecs[numOfTests - 1].testID);
    for (int i = 0; i < numOfTests - 1; i++) {
	if (strcmp(testID, jobSpecs[i].testID) == 0) {
	    fprintf(stderr, JOBSPECFILE_DUP_ERR_MSG, lineNumber,
		    jobSpecFilePath);
	    free(testID);
	    exit(JOBSPECFILE_DUP_ERR);
	}
    }
    free(testID);

    // Check if the "inputfile" can be opened.
    char* inputFilePath = strdup(jobSpecs[numOfTests - 1].inputFile);
    FILE* inputFile = fopen(inputFilePath, "r");
    if (inputFile == NULL) {
	fprintf(stderr, INPUTFILE_OPEN_ERR_MSG, inputFilePath, lineNumber,
		jobSpecFilePath);
	free(inputFilePath);
	exit(INPUTFILE_OPEN_ERR);
    }
    free(inputFilePath);
    fclose(inputFile);

}

/* create_testdir()
 * ---------------
 * Creates a test directory using the specified testdir parameter.
 *
 * parameters: the parameters from the command line arguments, including the
 * 	test directory filepath.
 *
 * Errors: Exits with exit status 10 and testdir exist error if the test
 * 	directory cannot be created.
 */
void create_testdir(ProgramParameters parameters) {
    // Try opening testdir
    char* testDirFilePath = parameters.testDir;
    int dirExitStatus = mkdir(testDirFilePath, S_IRWXU);
    if (dirExitStatus && errno != EEXIST) {
	fprintf(stderr, TESTDIR_CREATE_ERR_MSG, testDirFilePath);
	exit(TESTDIR_CREATE_ERR);
    }
}

/* get_filepath()
 * --------------
 * Generates and makes the filepath for the file with specified testID with
 * 	specified type.
 *
 * testDir: the test directory specified by the user in the command line
 * 	arguments.
 * type: the type of the file to be created, which can be either '.stdout',
 * 	'.stderr', or '.exitstatus'.
 * testID: the name of test for the file to be created.
 * 
 * Returns: a pointer to a dynamically allocated array with the filepath to
 * 	the file to be created for the specified testID and type.
 */
char* get_filepath(char* testDir, char* type, char* testID) {
    // Initialise dynamically allocated array to store filepath in.
    int testDirLength = strlen(testDir);
    int testIDLength = strlen(testID);
    int typeLength = strlen(type);
    int totalLength = testIDLength + testDirLength + typeLength + 1;
    char* filePath = malloc(sizeof(char) * (totalLength + 1));

    // Add test directory path to filepath.
    int length = testDirLength;
    for (int j = 0; j < length; j++) {
	filePath[j] = testDir[j];
    }

    // Add slash to filepath.
    length++;
    filePath[length - 1] = '/';

    // Add testID to filepath
    length += testIDLength;
    for (int j = length - testIDLength; j < length; j++) {
	filePath[j] = testID[j - testDirLength - 1];
    }
    
    // Add type of file to end of filepath
    length += typeLength;
    for (int j = length - typeLength; j < length; j++) {
	filePath[j] = type[j - 1 - testIDLength - testDirLength];
    }
    filePath[length] = '\0';
    return filePath;
}

/* create_output_files()
 * ---------------------
 * Creates the output files for each test in the job specifications file.
 *
 * parameters: the parameters from the command line arguments, including the
 * 	data structure with all tests from jobSpecFile.
 *
 * Returns: void
 */
void create_output_files(ProgramParameters parameters) {
    int numOfTests = parameters.numOfTests;
    JobSpecs* jobSpecs = parameters.jobSpecs;
    char* testDir = parameters.testDir;
    char* jobSpecFilePath = parameters.jobSpecFilePath;
    char* type[NUM_OF_TYPES] = {STDOUT_TYPE, STDERR_TYPE, EXITSTATUS_TYPE};

    bool jobSpecModified = false;
    bool runGoodUQWordLadder = false;
    // Iterate through all tests and types and make the required files
    for (int test = 0; test < numOfTests; test++) {
	bool recreateFiles = false;
	int printMsg = 0;
	for (int i = 0; i < NUM_OF_TYPES; i++) {
	    char* testID = strdup(jobSpecs[test].testID);
	    char* filePath = get_filepath(testDir, type[i], testID);

	    store_output_filepath(parameters, filePath, i, test);
	    
	    // Make output files if they are missing, or if jobSpecFile was
	    // modified after output files were created.
	    int outputFile = open(filePath, O_WRONLY);
	    if (outputFile != -1) {
		jobSpecModified = 
		    check_modification_time(filePath, jobSpecFilePath);
	    }
	    if (recreateFiles || outputFile == -1 || jobSpecModified ||
		    parameters.recreate) {
		runGoodUQWordLadder = true;
		recreateFiles = true;
		close(outputFile);
		printMsg++;
		if (printMsg == 1) {
		    fprintf(stdout, REBUILD_MSG, testID);
		}
		int create = open(filePath, O_WRONLY | O_CREAT | O_TRUNC,
			S_IRUSR | S_IWUSR);
		check_output_file(create, filePath);
		close(create);
	    }
	    free(testID);
	    free(filePath);
	}
    }
    if (runGoodUQWordLadder) {
	run_gooduqwordladder(parameters);
    }
}

/* store_output_filepath()
 * -----------------------
 * Stores the filepath of the specified ouput file to the JobSpecs struct.
 *
 * parameters: a struct containing the program's main parameters including
 * 	the jobSpecs data struct.
 * filePath: a pointer to the array with the filePath made in get_filepath().
 * typeNum: the index of the type from the 'type' array in
 * 	create_output_files().
 * testNum: the current test number to store the filePath into.
 *
 * Returns: void
 */
void store_output_filepath(ProgramParameters parameters, char* filePath,
	int typeNum, int testNum) {
    // Store filePath to corresponding testID for future reference.
    switch (typeNum) {
	case 0:
	    parameters.jobSpecs[testNum].outputFile = strdup(filePath);
	    break;
	case 1:
	    parameters.jobSpecs[testNum].errorFile = strdup(filePath);
	    break;
	case 2:
	    parameters.jobSpecs[testNum].exitStatusFile = strdup(filePath);
	    break;
    }
}

/* check_output_file()
 * -------------------
 * Check if the created output file can be opened.
 *
 * fd: the file descriptor of the opened output file which was 
 * 	returned from the open() function.
 * filePath: a pointer to an array with the filePath from get_filepath().
 *
 * Errors: Exits with status 6 and output file error if the created output
 * 	file cannot be opened.
 */
void check_output_file(int fd, char* filePath) {
    if (fd == -1) {
	fprintf(stderr, OUTPUT_FILE_ERR_MSG, filePath);
	close(fd);
	exit(OUTPUT_FILE_ERR);
    }
}

/* check_modification_time()
 * -------------------------
 * Checks if the last modification time of the output file was before the
 * 	the last modification time of the job specifications file.
 *
 * outputFilePath: a pointer to an array with filePath from get_filePath().
 * jobSpecFilePath: a pointer to an array with job specifications filepath
 * 	from the ProgramParameters struct.
 *
 * Returns: true if the jobSpecFile was modified after the last modification
 * 	time of any output file, else returns false.
 * Errors: Exits with status 6 and output file error if the file's stats
 * 	cannot be read.
 */
bool check_modification_time(char* outputFilePath, char* jobSpecFilePath) {
    // Initialise struct for stat
    struct stat output;
    struct stat jobSpec;
    if (stat(outputFilePath, &output) == -1 || 
	    stat(jobSpecFilePath, &jobSpec) == -1) {
	fprintf(stderr, OUTPUT_FILE_ERR_MSG, outputFilePath);
	exit(OUTPUT_FILE_ERR);
    }
    
    // Get the time of last modification and compare them
    int timeDiff = compare_timespecs(output.st_mtim, jobSpec.st_mtim);
    if (timeDiff < 0) {
	return true;
    }
    return false;
}

/* run_gooduqwordladder()
 * ----------------------
 * Runs the tests in job specifications file with good-uqwordladder and puts
 * 	the output in the corresponding files in the test directory.
 *
 * parameters: a struct containing the program's main parameters including
 * 	the jobSpecs data struct.
 *
 * Returns: void
 */
void run_gooduqwordladder(ProgramParameters parameters) {
    JobSpecs* jobSpecs = parameters.jobSpecs;
    int numOfTests = parameters.numOfTests;

    // Create child processes for each test to be run on command line.
    pid_t pid[numOfTests];
    for (int test = 0; test < numOfTests; test++) {
	fflush(stdout);
	pid[test] = fork();
	if (pid[test] == 0) {
	    jobSpecs[test].args[0] = strdup(GOOD_UQWORDLADDER);

	    // Open corresponding output files and redirect to them.
	    int in = open(jobSpecs[test].inputFile, O_RDONLY);
	    int out = open(jobSpecs[test].outputFile, O_WRONLY | O_TRUNC);
	    int err = open(jobSpecs[test].errorFile, O_WRONLY | O_TRUNC);
	    dup2(in, STDIN_FILENO);
	    dup2(out, STDOUT_FILENO);
	    dup2(err, STDERR_FILENO);
	    close(in);
	    close(out);
	    close(err);
	    execvp(jobSpecs[test].args[0], jobSpecs[test].args);
	    exit(99);
	}
    }

    // Get exit status for each child process in same order as above, and
    // store it in corresponding .exitstatus file.
    int status;
    for (int test = 0; test < numOfTests; test++) {
	waitpid(pid[test], &status, 0);
	if (WIFEXITED(status)) {
	    int exitStatus = open(jobSpecs[test].exitStatusFile,
		    O_WRONLY | O_TRUNC);
	    char buffer[EXITSTATUS_BUFFER];
	    sprintf(buffer, "%d\n", WEXITSTATUS(status));
	    write(exitStatus, buffer, strlen(buffer));
	    close(exitStatus);
	}
    }
}

/* run_testjob()
 * -------------
 * Runs all tests specified in the job specifications file and reports results
 *
 * parameters: a struct containing the program's main parameters including 
 * 	the jobSpecs data struct.
 *
 * Errors: Exits with status 18 if a test fails, or exits with status 9 if
 * 	no tests were done.
 */
void run_testjob(ProgramParameters parameters) {
    JobSpecs* jobSpecs = parameters.jobSpecs;
    int numOfTests = parameters.numOfTests;
    int numOfRunTests = 0;
    int successfulTests = 0;
    // Setup signal handler to detect SIGINT.
    struct sigaction interrupt;
    interrupt.sa_handler = interrupt_handler;
    sigaction(SIGINT, &interrupt, 0);
    memset(&interrupt, 0, sizeof(struct sigaction));

    for (int test = 0; test < numOfTests; test++) {
	fprintf(stdout, RUN_TEST_MSG, jobSpecs[test].testID);
	fflush(stdout);
	pid_t pid[TOTAL_PIDS];
	// Make two pipes
	int outputPipe[2];
	int errorPipe[2];
	pipe(outputPipe);
	pipe(errorPipe);

	// Create a process for uqwordladder, cmp for stdout, and cmp for 
	// stderr.
	run_three_processes(parameters, pid, outputPipe, errorPipe, test);

	// Wait 1.5 seconds, kill all child processes, and check if the test
	// was interrupted by SIGINT.
	usleep(TEST_DELAY);
	kill_processes(pid);
	check_interrupt(pid, successfulTests, numOfRunTests);
	numOfRunTests++;

	// Use wait and get exit statuses
	int* exitCodes = get_exit_codes(pid);

	// Check if processes were successful and stdout, stderr, and exit
	// statuses match. Success will be 3 if all match.
	int success = report_cmp_results(jobSpecs, test, exitCodes);
	if (success == REQUIRED_MATCHES) {
	    successfulTests++;
	}
	free(exitCodes);
    }
    free_program_parameters(parameters);

    // Print number of successful tests
    fprintf(stdout, SUCCESSFUL_TEST_MSG, successfulTests, numOfRunTests);
    if (successfulTests != numOfTests) {
	exit(UNSUCCESSFUL_TESTS);
    }
}

/* run_three_processes()
 * ---------------------
 * Creates three processes and two pipes to run uqwordladder and redirect its
 * 	stdout and stderr to be input for the two cmp processes.
 *
 * parameters: a struct containing the program's main parameters including
 * 	the jobSpecs data struct.
 * pid: a pointer to an array to store the pids of the processes to be created
 * outputPipe: a pointer to the array for the pipe that carries stdout from
 * 	uqwordladder process to the cmp process.
 * errorPipe: a pointer to the array for the pipe that carries stderr from
 * 	uqwordladder process to the other cmp process.
 * testNum: the 'n'th test to conduct.
 *
 * Returns: void
 */
void run_three_processes(ProgramParameters parameters, pid_t* pid,
	int* outputPipe, int* errorPipe, int testNum) {
    // Create a process to run uqwordladder
    pid[UQWORDLADDER_PID] = fork();
    if (!pid[UQWORDLADDER_PID]) {
	run_uqwordladder(parameters, outputPipe, errorPipe, testNum);
    }

    // Create a process to run cmp to compare stdout
    pid[STDOUT_PID] = fork();
    if (!pid[STDOUT_PID]) {
	run_cmp(errorPipe, outputPipe,
		parameters.jobSpecs[testNum].outputFile);
    }

    // Create a process to run cmp to compare stderr
    pid[STDERR_PID] = fork();
    if (!pid[STDERR_PID]) {
	run_cmp(outputPipe, errorPipe,
		parameters.jobSpecs[testNum].errorFile);
    }

    // Close both pipes for parent process.
    close(outputPipe[WRITE_END]);
    close(outputPipe[READ_END]);
    close(errorPipe[WRITE_END]);
    close(errorPipe[READ_END]);
}

/* run_uqwordladder()
 * ------------------
 * Runs the uqwordladder program under test and redirects stdout and stderr
 * 	to the corresponding pipes for cmp.
 *
 * parameters: a struct containing the program's main parameters including
 * 	the jobSpecs data struct.
 * pid: a pointer to an array to store the pids of the processes to be created
 * outputPipe: a pointer to the array for the pipe that carries stdout from
 * 	uqwordladder process to the cmp process.
 * errorPipe: a pointer to the array for the pipe that carries stderr from
 * 	uqwordladder process to the other cmp process.
 * testNum: the 'n'th test to conduct.
 *
 * Errors: Exits with status 99 if running the uqwordladder program fails on
 * 	the command line.
 */
void run_uqwordladder(ProgramParameters parameters, int* outputPipe,
	int* errorPipe, int testNum) {
    // Put the name of uqwordladder program being tested at the first index
    // of args for command line.
    parameters.jobSpecs[testNum].args[0] = parameters.program;

    // Use inputfile for each test as stdin, and redirect stdout and stderr
    // to pipes.
    int in = open(parameters.jobSpecs[testNum].inputFile, O_RDONLY);
    dup2(in, STDIN_FILENO);
    dup2(outputPipe[WRITE_END], STDOUT_FILENO);
    dup2(errorPipe[WRITE_END], STDERR_FILENO);

    close(in);
    close(outputPipe[WRITE_END]);
    close(outputPipe[READ_END]);
    close(errorPipe[WRITE_END]);
    close(errorPipe[READ_END]);

    execvp(parameters.jobSpecs[testNum].args[0],
	    parameters.jobSpecs[testNum].args);
    exit(UNEXPECTED_ERR);
}

/* run_cmp()
 * ---------
 * Runs cmp on the command line for stdout or stderr, depending on firstPipe
 * 	and secondPipe.
 *
 * firstPipe: a pointer to the array of the pipe that will not be used either
 * 	for stdout or stderr.
 * secondPipe: a pointer to the array of the pipe that contains the redirected
 * 	output from uqwordladder, which will be used as stdin for cmp.
 * filePath: the file that contains the stdout and stderr for the current test
 * 	to compare the output against.
 *
 * Returns: Exits with status 99 if cmp fails on the command line.
 */
void run_cmp(int* firstPipe, int* secondPipe, char* filePath) {
    // Close unneccessary ends of pipes.
    close(firstPipe[WRITE_END]);
    close(firstPipe[READ_END]);
    close(secondPipe[WRITE_END]);

    // Redirect stdout and stderr to '/dev/null'
    dup2(secondPipe[READ_END], STDIN_FILENO);
    close(secondPipe[READ_END]);
    int fd = open(EMPTY_FILE, O_WRONLY | O_TRUNC);
    dup2(fd, STDOUT_FILENO);
    dup2(fd, STDERR_FILENO);
    close(fd);

    execlp(CMP_ARG, CMP_ARG, filePath, NULL);
    exit(UNEXPECTED_ERR);
}

/* get_exit_codes()
 * ----------------
 * Gets the exit statuses of all three processes.
 *
 * pid: a pointer to an array with the pids of the three processes.
 *
 * Returns: a pointer to the array with the exit statuses of all three
 * 	processes
 */
int* get_exit_codes(pid_t* pid) {
    int* exitCodes = malloc(sizeof(int) * TOTAL_PIDS);
    int status;
    for (int i = 0; i < TOTAL_PIDS; i++) {
	waitpid(pid[i], &status, 0);
	if (WIFEXITED(status)) {
	    exitCodes[i] = WEXITSTATUS(status);
	}
    }
    return exitCodes;
}

/* check_test_error()
 * ------------------
 * Checks if any of the processes failed and prints the fail message.
 *
 * jobSpecs: a pointer to the array of the struct with all the parameters of
 * 	each test.
 * exitCodes: a pointer to the array with exit statuses for all three
 * 	processes
 * testNum: the 'n'th test to check the exit statuses for.
 *
 * Returns: true if at least one process failed, else returns false.
 */
bool check_test_error(JobSpecs* jobSpecs, int* exitCodes, int testNum) {
    for (int i = 0; i < TOTAL_PIDS; i++) {
	if (exitCodes[i] == UNEXPECTED_ERR) {
	    fprintf(stdout, TEST_ERR_MSG, jobSpecs[testNum].testID);
	    return true;
	    break;
	}
    }
    return false;
}

/* report_cmp_results()
 * --------------------
 * Prints the results of the current test if none of the procesess failed.
 *
 * jobSpecs: a pointer to the array of the struct with all the parameters for
 * 	each test.
 * testNum: the 'n'th test to check the results for.
 * exitCodes: a pointer to the array with exit statuses for all three
 * 	processes
 *
 * Returns: the number of successful tests i.e. 3 if stdout, stderr, and exit
 * 	status match the expected results.
 */
int report_cmp_results(JobSpecs* jobSpecs, int testNum, int* exitCodes) {
    // Check if any processes of the current test failed.
    bool errorHappened = check_test_error(jobSpecs, exitCodes, testNum);
    if (errorHappened) {
	return 0;
    }

    int success = 0;
    
    // Get expected exit status from file
    int exitStatusFile = open(jobSpecs[testNum].exitStatusFile, O_RDONLY);
    char exitStatusBuffer[EXITSTATUS_BUFFER];
    read(exitStatusFile, exitStatusBuffer, EXITSTATUS_BUFFER);
    int exitStatus = atoi(exitStatusBuffer);
    close(exitStatusFile);

    // Check if stdout, stderr, and exit status matches.
    char* type[TOTAL_PIDS] = {STDOUT_REPORT, STDERR_REPORT,
	    EXITSTATUS_REPORT};
    int resultForSuccess[TOTAL_PIDS] = {MATCHES, MATCHES, exitStatus};

    char* result;
    for (int i = 0; i < TOTAL_PIDS; i++) {
	if (exitCodes[i] == resultForSuccess[i]) {
	    result = strdup(REPORT_MATCHES);
	    success++;
	} else {
	    result = strdup(REPORT_DIFFERS);
	}
	fprintf(stdout, REPORT_MSG, jobSpecs[testNum].testID, type[i],
		result);
	fflush(stdout);
	free(result);
    }
    return success;
}

/* kill_processes()
 * ----------------
 * Kills all three processes by sending SIGKILL to them.
 *
 * pid: a pointer to the array with the pids of the three processes
 *
 * Returns: void
 */
void kill_processes(pid_t* pid) {
    for (int i = 0; i < TOTAL_PIDS; i++) {
	kill(pid[i], SIGKILL);
    }
}

/* check_interrupt()
 * -----------------
 * If the current test was interrupted with SIGINT, it reaps all three
 * 	processes after they have been killed and exits program.
 *
 * pid: a pointer to the array with the pids of the three processes
 * successfulTests: the number of successful tests before SIGINT.
 * numOfRunTests: the total number of tests run before SIGINT.
 *
 * Returns: Exits with exit status 0 and 'successful tests' message if at
 * 	least one test was conducted. If not, then it exits with status 9.
 */
void check_interrupt(pid_t* pid, int successfulTests, int numOfRunTests) {
    if (interrupted) {
	
	for (int i = 0; i < TOTAL_PIDS; i++) {
	    waitpid(pid[i], NULL, 0);
	}
	if (numOfRunTests > 0) {
	    fprintf(stdout, SUCCESSFUL_TEST_MSG, successfulTests,
		    numOfRunTests);
	    exit(OK);
	} else {
	    fprintf(stdout, NO_TEST_MSG);
	    exit(NO_TESTS);
	}
    }
}

/* free_program_parameters()
 * -------------------------
 * Frees the memory allocated by the parameters of the jobSpec data struct.
 *
 * parameters: the parameters from the command line arguments, including the
 * 	data structure with all tests from jobSpecFile.
 *
 * Returns: void
 */
void free_program_parameters(ProgramParameters parameters) {
    for (int test = 0; test < parameters.numOfTests; test++) {
	JobSpecs testJob = parameters.jobSpecs[test];
	free(testJob.testID);
	free(testJob.inputFile);
	free(testJob.outputFile);
	free(testJob.errorFile);
	free(testJob.exitStatusFile);

	for (int i = 0; testJob.args[i] != NULL; i++) {
	    free(testJob.args[i]);
	}
	free(testJob.args);
    }
    free(parameters.jobSpecs);
}
