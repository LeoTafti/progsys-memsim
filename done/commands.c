/**
 * @file commands.c
 * @brief Processor commands(/instructions) simulation
 *
 * @author Juillard Paul, Tafti Leo
 * @date 2019
 */

#include <stdio.h>
#include <inttypes.h>

#include "commands.h"
#include "addr_mng.h"
#include "mem_access.h"
#include "addr.h"
#include "error.h"

#define BYTE_MAX_VALUE 0xFF
#define WORD_MAX_VALUE 0xFFFFFFFF // TODO is unsigned?

int program_init(program_t* program) {
	M_REQUIRE_NON_NULL(program);

	for (int i = 0; i < MAX_COMMANDS; i++) {
		// TODO how to modify pointer value? Léo : I think this does the job (from Lecture 2 on structures)
		(void)memset(&program->listing[i], 0, sizeof(program->listing[i]));
	}
	
	//TODO : But doesn't this do the same in a more compact way ?
	//(void)memset(program->listing, 0, sizeof(program->listing));
	
	program->nb_lines = 0;
	
	program->allocated = sizeof(program->listing);
	
	return ERR_NONE;
}


int program_add_command(program_t* program, const command_t* command) {
	M_REQUIRE_NON_NULL(program);   
	M_REQUIRE_NON_NULL(command);
	
	// memory restrictions
	M_REQUIRE(program->nb_lines >= MAX_COMMANDS, ERR_MEM, "there is too many commands! max is %d", MAX_COMMANDS);
	
	// check validity of command
	// TODO brainstorming!! did I forget anything?
	
	// no command entry is null
	M_REQUIRE_NON_NULL(command->order);
	M_REQUIRE_NON_NULL(command->type);
	M_REQUIRE_NON_NULL(command->data_size);
	M_REQUIRE_NON_NULL(command->write_data);
	M_REQUIRE_NON_NULL(command->vaddr);	
	
	// not W and I
	M_REQUIRE( !(command->order == WRITE && command->type == INSTRUCTION), ERR_BAD_PARAMETER, "illegal command: cannot write into instructions", void );

	// R and empty write data?
	// here we consider that the write_data is irrelevant and should not raise an error is set
	// M_REQUIRE( !(command->order == READ, ERR_BAD_PARAMETER, "illegal command: read should not have write data", void);
	
	// data size is either word or byte
	M_REQUIRE( command->data_size == sizeof(word_t) || command->data_size == sizeof(byte_t), ERR_SIZE, "illegal command: data size is neither word or byte", void);
	
	// data_size and write_data value are coherent
	int write_word_coherent = (command->data_size == sizeof(word_t)) && (command->write_data <= WORD_MAX_VALUE);
	int write_byte_coherent = (command->data_size == sizeof(byte_t)) && (command->write_data <= BYTE_MAX_VALUE);
	M_REQUIRE( write_word_coherent || write_byte_coherent, ERR_SIZE, "illegal command: write data is too big compared to declared datasize", void);
		
	// addr offset is a multiple of size
	int word_address_case = (command->type == INSTRUCTION || command->data_size == sizeof(word_t) ) && ((command->vaddr.page_offset & 0x11) == 0);
	M_REQUIRE( word_address_case, ERR_ADDR, "illegal command: address of word is not word-aligned", void);


	// finally add command
	program->listing[program->nb_lines] = *command;
	
	return ERR_NONE;
}

int program_shrink(program_t* program) {
	M_REQUIRE_NON_NULL(program);   

	// TODO : complete (Week 6)
	
	return ERR_NONE;
}
// TODO DOC
void print_order( FILE* const o , command_t const * c ) {
	 if(c->order == READ) fprintf(o, "R ");
	 else fprintf(o, "W ");
}
void print_type_size( FILE* const o , command_t const * c ){ 
	if(c->type == INSTRUCTION) fprintf(o, "I  "); // I and two white spaces
	else { 
		if (c->data_size == sizeof(word_t)) fprintf(o, "DW ");
		else fprintf(o, "DB "); 
	}
}
void print_data( FILE* const o , command_t const * c ) {
	if(c->order == WRITE) fprintf(o, "0x%08" PRIX32 " ", c->write_data);
	else fprintf(o, "           "); // 11 whites spaces
}
void print_addr( FILE* const o , command_t const * c ) {
	fprintf(o, "@0x%016" PRIX64, virt_addr_t_to_uint64_t( &(c->vaddr)) );
}

int program_print(FILE* output, const program_t* program) {
	M_REQUIRE_NON_NULL(output);
	M_REQUIRE_NON_NULL(program);
	
	command_t c;
	
	for(int i = 0; i < program->nb_lines; i++) {
		c = program->listing[i];
		
		/*uint64_t addr = virt_addr_t_to_uint64_t(&c.vaddr);
		
		if(c.order == READ) {
			if (c.type == INSTRUCTION) { fprintf(output, "R I @0x%016" PRIX64, addr);}
			else { if (c.data_size == sizeof(word_t)) { fprintf(output, "R DW @0x%016" PRIX64, addr);}
				   else { fprintf(output, "R DB @0x%016" PRIX64, addr);}
			}
		}
		else { 
			if (c.data_size == sizeof(word_t)) { fprintf(output, "R DW 0x%08" PRIX32 " @0x%016" PRIX64, c.write_data, addr);}
		    else { fprintf(output, "R DB 0x%02" PRIX32 " @0x%016" PRIX64, c.write_data, addr);}	
		}*/
		
		print_order(output, &c);
		print_type_size(output, &c);
		print_data(output, &c);
		print_addr(output, &c);
		
		fprintf(output, "\n");	
	}
	
	return ERR_NONE;
}

/**
 * @brief Read a program (list of commands) from a file.
 * @param filename the name of the file to read from.
 * @param program the program to be filled from file.
 * @return ERR_NONE if ok, appropriate error code otherwise.
 */
int program_read(const char* filename, program_t* program){
	M_REQUIRE_NON_NULL(filename);
	M_REQUIRE_NON_NULL(program);
	
	return ERR_NONE;
}
