#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

// There's 2 kinds of commands for sqlite. 1st is meta command which is not a sql query keyword
// 2nd type is a prepare command which is a keywrod like insert or select
// statement type is to distinguish between the two.
typedef enum {META_COMMAND_SUCCESS, META_COMMAND_UNRECOGNIZED_COMMAND} MetaCommandResult;

typedef enum {PREPARE_SUCCESS, PREPARE_UNRECOGNIZED_COMMAND} PrepareResult;

// So these 2 are macros.
// they basically replace the string "COLUMN_USERNAME_SIZE" with the number 32
// they are similar to .env files that contain non secrets where you can easily tune them as its 1 place of truth
// its good standard to have fixed size username and email -> fixed size row for easy calculate offset and next row for a db

//Use this instead of a global variable for a FIXED size array. Because the macro exists during compilation, before
//running and changing C to machine code.

// in C, the size of an array inside a struct must be known at compile time.
// since macros get deleted they dont take up any space, unlike a global int variable which takes up 4 bytes in RAM
#define COLUMN_USERNAME_SIZE 32
#define COLUMN_EMAIL_SIZE 255

typedef struct{
	uint32_t id;
	char username [COLUMN_USERNAME_SIZE];
	char email [COLUMN_EMAIL_SIZE];
}Row;

typedef enum {STATEMENT_INSERT, STATEMENT_SELECT} StatementType;
typedef struct {
	StatementType type;
	Row row_to_insert; 
	
}Statement;
typedef struct{
	char* buffer;
	size_t buffer_length;
	ssize_t input_length;
}InputBuffer;

//defining a table struct
// 4096 to match with virtual memory systems of most os for efficient data transfer
const uint32_t PAGE_SIZE = 4096;
#define TABLE_MAX_PAGES 100

const uint32_t ROWS_PER_PAGE = PAGE_SIZE/ROWSIZE;
const uint32_t TABLE_MAX_ROWS = ROWS_PER_PAGE * TABLE_MAX_PAGES;

typedef struct{
	uint32_t num_rows;
	void* pages[TABLE_MAX_PAGES];
}Table;

// defining a row schema


#define size_of_attribute(Struct, Attribute) sizeof (((Struct*)0) -> Attribute)

const uint32_t ID_SIZE = size_of_attribute(Row, id);
const uint32_t USERNAME_SIZE = size_of_attribute(Row, username);
const uint32_t EMAIL_SIZE = size_of_attribute(Row, email);
const uint32_t ID_OFFSET = 0;
const uint32_t USERNAME_OFFSET = ID_OFFSET + ID_SIZE;
const uint32_t EMAIL_OFFSET = USERNAME_OFFSET + USERNAME_SIZE;
const uint32_t ROW_SIZE = ID_SIZE + USERNAME_SIZE + EMAIL_SIZE;


// read/write in memory for a particular row:

void* row_slot(Table*table, uint32_t row_num){
	uint32_t page_num = row_num/ROWS_PER_PAGE;
	void* page = table-> pages[page_num];
	if (page == NULL){
		//allocate memory only when we try to access the page
	}
	uint32_t row_offset = row_num % ROWS_PER_PAGE;
	uint32_t byte_offset = row_offset * ROW_SIZE;
	return page + byte_offset;
}





InputBuffer* new_input_buffer(){
	InputBuffer* input_buffer = malloc(sizeof(InputBuffer));
	input_buffer -> buffer = NULL;
	input_buffer -> buffer_length = 0;
	input_buffer -> input_length = 0;
	return input_buffer;
}
//serialize: taking object and put it into raw stream of bytes
// how it works here: take wahtever's at the memory of source of size size, and put it at the right destination
// use void pointer to be adaptable on what is there at the destination.
void serialize_row(Row* source, void* destination) {
	memcpy(destination + ID_OFFSET, &(source->id), ID_SIZE);
	memcpy(destination + USERNAME_OFFSET, &(source->username), USERNAME_SIZE);
	memcpy(destination + EMAIL_OFFSET, &(source->email), EMAIL_SIZE);
}

void deserialize_row(void* source, Row* destination) {
	memcpy(&(destination->id), source + ID_OFFSET, ID_SIZE);
	memcpy(&(destination->username), source + USERNAME_OFFSET, USERNAME_SIZE);
	memcpy(&(destination->email), source + EMAIL_OFFSET, EMAIL_SIZE);
}

//free memory
void close_input_buffer(InputBuffer* input_buffer){
	free(input_buffer->buffer);
	free(input_buffer);
}

// wrapping .exit which is our only meta command but allowing ability to add more later on
MetaCommandResult do_meta_command(InputBuffer* input_buffer){
	if (strcmp(input_buffer->buffer, ".exit") == 0){
		close_input_buffer(input_buffer);
		exit(EXIT_SUCCESS);
	}
	else {
		return META_COMMAND_UNRECOGNIZED_COMMAND;
	}
}
//statement passing in the address because we need to modify on heap, and have it persist out of scope of fxn
// we used strncmp with an "N" to compare number of characters, which is 6.
PrepareResult prepare_statement(InputBuffer* input_buffer, Statement* statement){
	if (strncmp(input_buffer -> buffer, "insert", 6) == 0){
		statement->type = STATEMENT_INSERT;
		int args_assigned = sscanf(
			input_buffer -> buffer, "insert %d %s %s", &(statement->row_to_insert.id),
			statement->row_to_insert.username, statement-> row_to_insert.email);
		if (args_assigned < 3){
			return PREPARE_SYNTAX_ERROR;
		}
		return PREPARE_SUCCESS;
	}
	if (strcmp(input_buffer -> buffer, "select") == 0){
		statement->type = STATEMENT_SELECT;
		return PREPARE_SUCCESS;
	}
	return PREPARE_UNRECOGNIZED_COMMAND;
}

void execute_statement(Statement*statement){
	switch (statement->type){
		case(STATEMENT_INSERT):
			printf("inserting.\n");
			break;
		case(STATEMENT_SELECT):
			printf("well be selecting.\n");
			break;
		
	}
}

void print_prompt(){
	printf("db > ");
}

void read_input(InputBuffer * input_buffer){
	ssize_t bytes_read = 
		getline(&(input_buffer->buffer), &(input_buffer -> buffer_length), stdin);
	if (bytes_read <=0){
		printf("Error reading input\n");
		exit(EXIT_FAILURE);
	}
	//subtracting one because of the new line
	input_buffer -> input_length = bytes_read -1;
	input_buffer -> buffer[bytes_read-1] = 0;
}



int main(int argc, char* argv[]){
	InputBuffer* input_buffer = new_input_buffer();
	while(true){
		print_prompt();
		read_input(input_buffer);

		if (input_buffer -> buffer[0] == '.'){
			switch (do_meta_command(input_buffer)){
				case(META_COMMAND_SUCCESS):
					continue;
				case (META_COMMAND_UNRECOGNIZED_COMMAND):
					printf("unrecognized command '%s'\n", input_buffer->buffer);
					continue;
			}
		}
		Statement statement;
		switch (prepare_statement(input_buffer, &statement)){
			case (PREPARE_SUCCESS):
				break;
			case (PREPARE_UNRECOGNIZED_COMMAND):
				printf("unrecognized keyword at start of '%s'.\n", input_buffer->buffer);
				continue;
		}
		execute_statement(&statement);
		printf("Executed.\n");
	}
}
