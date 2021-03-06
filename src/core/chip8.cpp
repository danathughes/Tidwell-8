
#include "core/chip8.h"
#include "core/chip_listener.h"

#include "core/memory.h"
#include "core/display.h"
#include "core/keyboard.h"

#include <iostream>
#include <fstream>
#include <stdlib.h>
#include <time.h>

#define CALL_STACK_SIZE		16

/************
*
* Create a Chip-8 with default setup
************/
Chip8::Chip8()
{
	memory = new Memory();

	keyboard = new Keyboard();
	display = new Display();

	call_stack = new unsigned short[CALL_STACK_SIZE];

	refresh=false;

	// Seed a random number generator
	srand(time(NULL));

	// Populate the opcode-to-operation map
	create_operation_map();
}


/************
*
* Create a Chip-8 with provided components
************/
Chip8::Chip8(Memory* _memory, Display* _display, Keyboard* _keyboard)
{
	memory = _memory;
	keyboard = _keyboard;
	display = _display;

	call_stack = new unsigned short[CALL_STACK_SIZE];

	refresh=false;

	// Seed a random number generator
	srand(time(NULL));

	// Populate the opcode-to-operation map
	create_operation_map();
}


/************
*
* Create a Chip-8 with provided components, allowing user to override default call stack size
************/
Chip8::Chip8(Memory* _memory, Display* _display, Keyboard* _keyboard, unsigned char call_stack_size)
{
	memory = _memory;
	keyboard = _keyboard;
	display = _display;

	call_stack = new unsigned short[call_stack_size];

	refresh=false;

	// Seed a random number generator
	srand(time(NULL));

	// Populate the opcode-to-operation map
	create_operation_map();
}


void Chip8::create_operation_map()
{
	// On the off chance the map is full, clear it out
	operation_map.clear();

	// Now map all the opcodes to the corresponding functions
	operation_map.insert(std::make_pair(0x00E0, &Chip8::_clear_screen));
	operation_map.insert(std::make_pair(0x00EE, &Chip8::_return));
	operation_map.insert(std::make_pair(0x0000, &Chip8::_system_call));

	operation_map.insert(std::make_pair(0x1000, &Chip8::_jump));
	operation_map.insert(std::make_pair(0x2000, &Chip8::_call));
	operation_map.insert(std::make_pair(0x3000, &Chip8::_skip_equal_register_value));
	operation_map.insert(std::make_pair(0x4000, &Chip8::_skip_not_equal_register_value));
	operation_map.insert(std::make_pair(0x5000, &Chip8::_skip_equal_register_register));
	operation_map.insert(std::make_pair(0x6000, &Chip8::_assign_register_value));
	operation_map.insert(std::make_pair(0x7000, &Chip8::_add_register_value));

	operation_map.insert(std::make_pair(0x8000, &Chip8::_assign_register_register));
	operation_map.insert(std::make_pair(0x8001, &Chip8::_or));
	operation_map.insert(std::make_pair(0x8002, &Chip8::_and));
	operation_map.insert(std::make_pair(0x8003, &Chip8::_xor));
	operation_map.insert(std::make_pair(0x8004, &Chip8::_add_register_register));
	operation_map.insert(std::make_pair(0x8005, &Chip8::_subtract_register_register));
	operation_map.insert(std::make_pair(0x8006, &Chip8::_shift_right));
	operation_map.insert(std::make_pair(0x8007, &Chip8::_subtract_negative_register_register));
	operation_map.insert(std::make_pair(0x800E, &Chip8::_shift_left));

	operation_map.insert(std::make_pair(0x9000, &Chip8::_skip_not_equal_register_register));
	operation_map.insert(std::make_pair(0xA000, &Chip8::_set_address_register));
	operation_map.insert(std::make_pair(0xB000, &Chip8::_jump_offset));
	operation_map.insert(std::make_pair(0xC000, &Chip8::_random));
	operation_map.insert(std::make_pair(0xD000, &Chip8::_draw));

	operation_map.insert(std::make_pair(0xE09E, &Chip8::_skip_key_pressed));
	operation_map.insert(std::make_pair(0xE0A1, &Chip8::_skip_key_not_pressed));

	operation_map.insert(std::make_pair(0xF007, &Chip8::_get_delay_timer));
	operation_map.insert(std::make_pair(0xF00A, &Chip8::_get_key));
	operation_map.insert(std::make_pair(0xF015, &Chip8::_set_delay_timer));
	operation_map.insert(std::make_pair(0xF018, &Chip8::_set_sound_timer));
	operation_map.insert(std::make_pair(0xF01E, &Chip8::_add_address_register));
	operation_map.insert(std::make_pair(0xF029, &Chip8::_set_address_sprite));
	operation_map.insert(std::make_pair(0xF033, &Chip8::_store_bcd));
	operation_map.insert(std::make_pair(0xF055, &Chip8::_dump_register));
	operation_map.insert(std::make_pair(0xF065, &Chip8::_load_register));
}



/*************
* reset()
*
* Set the registers, stack pointer, timers and program counter to
* initial values
************/
void Chip8::reset()
{
	// Set all the registers to 0x00
	for(int i=0; i<0x10; i++)
	{
		registers[i] = 0x00;
	}

	// Set the timers to 0
	delay_timer = 0x00;
	sound_timer = 0x00;


	// Set the stack pointer to 0x00
	// NOTE: Should the call stack be flushed / set to zero?
	stack_pointer = 0x00;


	// Set the address register to 0x00
	address_register = 0x00;


	// Set the program counter to the start of the program memory
	program_counter = 0x200;


	// Update listeners to reflect the changes
	if(gui)
	{
		for(int i=0; i<0x10; i++)
		{
			gui->update_register(i, 0x00);
		}

		gui->update_delay_timer(0x00);
		gui->update_sound_timer(0x00);
		gui->update_stack_pointer(0x00);
		gui->update_address_register(0x00);
		gui->update_program_counter(0x200);
		gui->update_stack(call_stack, 0x00, CALL_STACK_SIZE);
	}
}

	
void Chip8::add_listener(ChipListener* _gui)
{
	gui = _gui;

	gui->update_stack(call_stack, stack_pointer, CALL_STACK_SIZE);
	gui->update_memory();
}


void Chip8::cycle_delay()
{
	if(delay_timer > 0)
	{
		delay_timer--;
		gui->update_delay_timer(delay_timer);
	}
}


void Chip8::cycle_sound()
{
	if(sound_timer > 0)
	{
		sound_timer--;
		gui->update_sound_timer(sound_timer);
	}
}

/*************
* cycle()
*
* Emulate a single clock cycle.
************/
void Chip8::cycle()
{
	// Get the opcode from memory, and increment the program counter
	// The opcode takes two bytes in memory, stored big-endian
	unsigned short opcode = (memory->fetch(program_counter) << 8) | (memory->fetch(program_counter + 1));
	program_counter += 2;

	gui->update_program_counter(program_counter);
	
	// Pull out all possible variables from the opcode
	unsigned short address = 	(unsigned short) (opcode & 0x0FFF);			// 12-bit value
	unsigned char register_x = 	(unsigned char) ((opcode & 0x0F00) >> 8);	// 2nd nybble
	unsigned char register_y = 	(unsigned char) ((opcode & 0x00F0) >> 4);	// 3rd nybble
	unsigned char value =		(unsigned char) (opcode & 0x00FF);			// Last byte used as value

	// Method to be called
	void (Chip8::* operation) (unsigned short, unsigned char, unsigned char, unsigned char) = NULL;

	// Decode the opcode and act accordingly
	// opcodes are organized roughly by first nybble
	unsigned short opnum = opcode & 0xF000;

	// Opcode prefix 0x0000 may also refer to 0x00E0 or 0x00EE, check if this
	// is the case
	if(opnum == 0x0000)
	{
		if((opcode & 0x00FF) == 0x00E0 || (opcode & 0x00FF) == 0x00EE)
		{
			opnum = opcode & 0xF0FF;
		}
	}

	// Opcode prefix 0x8000 also relies on the last nybble
	// NOTE:  Check if this is critical for 0x5000 and 0x9000 as well.
	//        Do these operations necessarily need to end in 0?
	if((opnum == 0x5000) || (opnum == 0x8000) || (opnum==0x9000))
	{
		opnum = opcode & 0xF00F;
	}

	// Opcode prefix 0xE000 and 0xF000 rely on the last byte as well
	if((opnum == 0xE000) || (opnum == 0xF000))
	{
		opnum = opcode & 0xF0FF;
	}

	// Get the operation from the operation map, and execute
	// If the operation isn't in the map, inform that this is an invalid opcode
	if(operation_map.find(opnum) != operation_map.end())
	{
		operation = operation_map[opnum];
		(this->*operation) (address, register_x, register_y, value);
	}
	else		// Operation wasn't found, need to throw invalid opcode
	{
		_invalid_opcode(opcode);
	}

}


/**********************
* OPCODES
**********************/

/*********************
* _clear_screen
*
* Clear the display, set all bytes in the display memory to 0x00
*********************/
void Chip8::_clear_screen(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	display->clear();
}


/*********************
* _return
*
* Set the program counter to the address on the top of the stack, then
* subtract 1 from the stack pointer.
*********************/
void Chip8::_return(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	// Is there somthing on the stack to pop?
	if(stack_pointer == 0)
	{
		std::cout << "ERROR:  No address on stack in RETURN\tProgram Counter: 0x" << std::hex << program_counter << std::endl;
		return;
	}

	// Set the program counter to the address on the top of the stack
	stack_pointer--;
	program_counter = call_stack[stack_pointer];

	// Update the stack as seen by any listeners
	gui->update_stack(call_stack, stack_pointer, CALL_STACK_SIZE);
	gui->update_stack_pointer(stack_pointer);
	gui->update_program_counter(program_counter);
}


/*********************
* _system_call
*
* Jump to the system routine at the address provided.
*
* Parameters
*   unsigned short address - address of the routine
*********************/
void Chip8::_system_call(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	// NOTE:  This instruction is ignored.
	std::cout << "SYSTEM CALL:  Program Counter: 0x" << std::hex << program_counter-2 << std::endl;
}


/*********************
* _jump
*
* Set the program counter to the provided address.
*
* Parameters
*   unsigned short address - address to jump to
*********************/
void Chip8::_jump(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	program_counter = address;

	gui->update_program_counter(program_counter);
}


/*********************
* _call
*
* Call the subroutine at the provided address.
* Push the stack pointer on the call stack, and increment the stack pointer.
*
* Parameters
*   unsigned short address - address of the subroutine to call
*********************/
void Chip8::_call(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	// Is the stack full?
	if(stack_pointer == CALL_STACK_SIZE)
	{
		std::cout << "ERROR:  Stack full in CALL" << std::endl;
		return;
	}

	call_stack[stack_pointer] = program_counter;
	stack_pointer++;

	program_counter = address;

	// Update the stack as seen by any listeners
	gui->update_stack(call_stack, stack_pointer, CALL_STACK_SIZE);
	gui->update_stack_pointer(stack_pointer);
	gui->update_program_counter(program_counter);
}


/*********************
* _skip_equal_register_value
*
* Skip the next instruction if the value in the register is equal to
* the given value
*
* Parameters
*   unsigned char register_x - which register to compare to
*   unsigned char value - the value to compre to
*********************/
void Chip8::_skip_equal_register_value(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	if(registers[register_x] == value)
	{
		program_counter += 2;

		gui->update_program_counter(program_counter);
	}
}


/*********************
* _skip_not_equal_register_value
*
* Skip the next instruction if the value in the register is not equal to
* the given value
*
* Parameters
*   unsigned char register_x - which register to compare to
*   unsigned char value - the value to compre to
*********************/
void Chip8::_skip_not_equal_register_value(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	if(registers[register_x] != value)
	{
		program_counter += 2;

		gui->update_program_counter(program_counter);
	}
}


/*********************
* _skip_equal_register_register
*
* Skip the next instruction if the value in the two registers are equal
*
* Parameters
*   unsigned char register_x - the first register to compare
*   unsigned char register_y - the second register to compare
*********************/
void Chip8::_skip_equal_register_register(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	if(registers[register_x] == registers[register_y])
	{
		program_counter += 2;

		gui->update_program_counter(program_counter);
	}
}


/*********************
* _skip_not_equal_register_register
*
* Skip the next instruction if the value in the two registers are not equal
*
* Parameters
*   unsigned char register_x - the first register to compare
*   unsigned char register_y - the second register to compare
*********************/
void Chip8::_skip_not_equal_register_register(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	if(registers[register_x] != registers[register_y])
	{
		program_counter += 2;

		gui->update_program_counter(program_counter);
	}

}


/*********************
* _assign_register_value
*
* Assign a value to a register
*
* Parameters
*   unsigned char register_x - the register to assign to
*   unsigned char value - the value to store in the register
*********************/
void Chip8::_assign_register_value(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	registers[register_x] = value;

	gui->update_register(register_x, registers[register_x]);
}


/*********************
* _add_register_value
*
* Add a value to a register
*
* Parameters
*   unsigned char register_x - the register to add to
*   unsigned char value - the value to add to the register
*********************/
void Chip8::_add_register_value(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	registers[register_x] += value;

	gui->update_register(register_x, registers[register_x]);	
}


/*********************
* _assign_register_register
*
* Assign a value in a register to another register
*
* Parameters
*   unsigned char register_x - the register to assign to
*   unsigned char register_y - the register to assign from
*********************/
void Chip8::_assign_register_register(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	registers[register_x] = registers[register_y];

	gui->update_register(register_x, registers[register_x]);	
}


/*********************
* _or
*
* Perform a bitwise OR on the values in each register, and assign to Vx
*
* Parameters
*   unsigned char register_x - the register to assign to
*   unsigned char register_y - the other register
*********************/
void Chip8::_or(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	registers[register_x] = registers[register_x] | registers[register_y];

	gui->update_register(register_x, registers[register_x]);	
}


/*********************
* _and
*
* Perform a bitwise AND on the values in each register, and assign to Vx
*
* Parameters
*   unsigned char register_x - the register to assign to
*   unsigned char register_y - the other register
*********************/
void Chip8::_and(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	registers[register_x] = registers[register_x] & registers[register_y];

	gui->update_register(register_x, registers[register_x]);
}


/*********************
* _xor
*
* Perform a bitwise XOR on the values in each register, and assign to Vx
*
* Parameters
*   unsigned char register_x - the register to assign to
*   unsigned char register_y - the other register
*********************/
void Chip8::_xor(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	registers[register_x] = registers[register_x] ^ registers[register_y];

	gui->update_register(register_x, registers[register_x]);
}


/*********************
* _add_register_register
*
* Add the two register values, and assign to Vx.  Set Vf to the carry bit
*
* Parameters
*   unsigned char register_x - the register to assign to
*   unsigned char register_y - the other register
*********************/
void Chip8::_add_register_register(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	unsigned short total = (unsigned short) registers[register_x] + (unsigned short) registers[register_y];

	// Is there a carry bit?
	if(total > 0xFF)
	{
		registers[0x0F] = 0x01;
	}
	else
	{
		registers[0x0F] = 0x00;
	}

	// Assign the target register
	registers[register_x] = (unsigned char) (total & 0x00FF);

	gui->update_register(register_x, registers[register_x]);
	gui->update_register(0x0F, registers[0x0F]);	
}


/*********************
* _subtract_register_register
*
* Subract Vy from Vx, and assign to Vx.  Set Vf to the borrow bit
*
* Parameters
*   unsigned char register_x - the register to assign to
*   unsigned char register_y - the other register
*********************/
void Chip8::_subtract_register_register(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	unsigned short difference = (unsigned short) registers[register_x];

	// Is a borrow necessary?
	if(registers[register_x] >= registers[register_y])
	{
		registers[0x0F] = 0x01;		// Set the NOT BORROW bit
	}
	else
	{
		registers[0x0F] = 0x00;
		difference += 0x0100;		// Fill in the borrow bit
	}
	difference -= registers[register_y];

	registers[register_x] = (unsigned char) difference;

	gui->update_register(register_x, registers[register_x]);
	gui->update_register(0x0F, registers[0x0F]);		
}


/*********************
* _subtract_negative_register_register
*
* Subtract Vx from Vy, and assign to Vx.  Set Vf to the borrow bit
*
* Parameters
*   unsigned char register_x - the register to assign to
*   unsigned char register_y - the other register
*********************/
void Chip8::_subtract_negative_register_register(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	unsigned short difference = (unsigned short) registers[register_y];

	// Is a borrow necessary?
	if(registers[register_y] >= registers[register_x])
	{
		registers[0x0F] = 0x01;		// Set the NOT BORROW bit
	}
	else
	{
		registers[0x0F] = 0x00;
		difference += 0x0100;		// Fill in the borrow bit
	}
	difference -= registers[register_x];

	registers[register_x] = (unsigned char) difference;	

	gui->update_register(register_x, registers[register_x]);
	gui->update_register(0x0F, registers[0x0F]);		
}



/*********************
* _shift_right
*
* Shift the value in the register to the right, assign the value of
* the least significant bit to Vf
*
* Parameters
*   unsigned char register_x - the register to shift
*********************/
void Chip8::_shift_right(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	// Assign the LSB to register F
	if((registers[register_x] & 0x01) != 0x00)
	{
		registers[0x0F] = 0x01;
	}
	else
	{
		registers[0x0F] = 0x00;
	}

	// Right shift - make sure that the shifted in bit is 0
	registers[register_x] = (registers[register_x] >> 1) & 0x7F;

	gui->update_register(register_x, registers[register_x]);
	gui->update_register(0x0F, registers[0x0F]);		
}


/*********************
* _shift_left
*
* Shift the value in the register to the left, assign the value of
* the most significant bit to Vf
*
* Parameters
*   unsigned char register_x - the register to shift
*********************/
void Chip8::_shift_left(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	// Assign the MSB to register F
	if((registers[register_x] & 0x80) != 0x00)
	{
		registers[0x0F] = 0x01;
	}
	else
	{
		registers[0x0F] = 0x00;
	}

	// Right shift - make sure that the shifted in bit is 0
	registers[register_x] = (registers[register_x] << 1) & 0xFE;

	gui->update_register(register_x, registers[register_x]);
	gui->update_register(0x0F, registers[0x0F]);		
}


/*********************
* _set_address_register
*
* Set the address register to the provided value
*
* Parameters
*   unsigned short address - address to set the register to
*********************/
void Chip8::_set_address_register(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	address_register = address;

	gui->update_address_register(address_register);
}


/*********************
* _jump_offset
*
* Set the program counter to the provided address, offset by
* the value in V0.
*
* Parameters
*   unsigned short address - address to jump to
*********************/
void Chip8::_jump_offset(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	// NOTE:  What if the program counter exceeds an allowable value?

	program_counter = address + registers[0x00];

	gui->update_program_counter(program_counter);
}


/*********************
* _random
*
* Generate a random byte value, and AND it with the provided byte value
* and assign the results to register Vx
*
* Parameters
*   unsigned char register_x - the register to assign the value to
*	unsigned char value - the provided byte value
*********************/
void Chip8::_random(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	registers[register_x] = (rand() % 0x100) & value;

	gui->update_register(register_x, registers[register_x]);
}


/*********************
* _draw
*
* Draw a sprite at the location (Vx, Vy).  The sprite is 8 pixels wide and
* N pixels high, where N is the value passed in.  The sprite is stored in the 
* current address register.
*
* Parameters
*   unsigned char register_x - the register containing the x coordinate of the sprite
*   unsigned char reigster_y - the register containing the y coordinate of the sprite
*	unsigned char value - the number of pixels in height of the sprite.
*********************/
void Chip8::_draw(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{

	bool collision = false;

	unsigned char x = registers[register_x];
	unsigned char y = registers[register_y];

	// Only use the last 4 bits of the value as the number of lines to draw
	unsigned char n_lines = value & 0x0F;

	// Read in N bytes, starting at the current address register
	for(int i=0; i<n_lines; i++)
	{
		unsigned char line = memory->fetch(address_register + i);

		// NOTE:   Perform write_line first, otherwise short circuit could prevent
		//         call to write_line
		collision = display->write_line(x, y+i, line) || collision;
	}
	if(collision)
	{
		registers[0x0F] = 0x01;
	}
	else
	{
		registers[0x0F] = 0x00;
	}

	gui->refresh_display();
	gui->update_register(0x0F, registers[0x0F]);		
}


void Chip8::_skip_key_pressed(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	unsigned char key = registers[register_x];

	if(keyboard->is_key_pressed(key))
	{
		program_counter += 2;
		gui->update_program_counter(program_counter);
	}

}



void Chip8::_skip_key_not_pressed(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	unsigned char key = registers[register_x];

	if(!keyboard->is_key_pressed(key))
	{
		program_counter += 2;
		gui->update_program_counter(program_counter);
	}
}



void Chip8::_get_key(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	bool got_key_press = false;

	// Check for a key press
	for(int i=0; i<0x10; i++)
	{
		if(keyboard->is_key_pressed(i))
		{
			registers[register_x] = i;
			got_key_press = true;
			// Unset the key?
			keyboard->release_key(i);
			break;
		}
	}

	// if the key wasn't pressed, decrement the program counter by 2 to repeat this operations
	if(!got_key_press)
	{
		program_counter -= 2;
	}
	
	gui->update_program_counter(program_counter);
	gui->update_register(register_x, registers[register_x]);
}


/*********************
* _get_delay_timer
*
* Assign the current value of the delay timer to register x
*
* Parameters
*   unsigned char register_x - the register to assign the timer to
*********************/
void Chip8::_get_delay_timer(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	registers[register_x] = delay_timer;

	gui->update_register(register_x, registers[register_x]);
}


/*********************
* _set_delay_timer
*
* Assign the delay timer to the value in register x
*
* Parameters
*   unsigned char register_x - the register containing the delay timer value
*********************/
void Chip8::_set_delay_timer(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	delay_timer = registers[register_x];

	gui->update_delay_timer(delay_timer);
}


/*********************
* _set_sound_timer
*
* Assign the sound timer to the value in register x
*
* Parameters
*   unsigned char register_x - the register containing the sound timer value
*********************/
void Chip8::_set_sound_timer(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	sound_timer = registers[register_x];

	gui->update_sound_timer(sound_timer);
}


/*********************
* _add_address_register
*
* Add the value of register x to the address register
*
* Parameters
*   unsigned char register_x - the register to add to the address register
*********************/
void Chip8::_add_address_register(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	address_register += registers[register_x];

	gui->update_address_register(address_register);

	if(address_register > 0x0FFF)
	{
		registers[0x0F] = 0x01;
	}
}



void Chip8::_set_address_sprite(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	address_register = memory->get_sprite_address(registers[register_x]);

	gui->update_address_register(address_register);
}



void Chip8::_store_bcd(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	unsigned char bcd_value = registers[register_x];

	// What is the 100's, 10's and 1's?
	unsigned char value_100s = bcd_value / 100;
	bcd_value -= 100*(value_100s);

	unsigned char value_10s = bcd_value / 10;
	bcd_value -= 10*(value_10s);

	unsigned char value_1s = bcd_value;

	// Store the three bytes
	memory->dump(address_register, value_100s);
	memory->dump(address_register+1, value_10s);
	memory->dump(address_register+2, value_1s);

	gui->update_memory();
}


/*********************
* _dump_register
*
* Store registers V0 to Vx (including Vx) into memory starting at the address
* register.
*
* Parameters
*   unsigned char register_x - the upper register value to write to memory
*********************/
void Chip8::_dump_register(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	// Loop through the registers and write to memory
	for(int reg_num=0; reg_num <= register_x; reg_num++)
	{
		memory->dump(address_register, registers[reg_num]);
		address_register++;
	}

	gui->update_memory();
	gui->update_address_register(address_register);
}


/*********************
* _load_register
*
* Load registers V0 to Vx (including Vx) from memory starting at the address
* register.
*
* Parameters
*   unsigned char register_x - the upper register to load
*********************/
void Chip8::_load_register(unsigned short address, unsigned char register_x, unsigned char register_y, unsigned char value)
{
	// Loop through the registers and write to memory
	for(int reg_num=0; reg_num <= register_x; reg_num++)
	{
		registers[reg_num] = memory->fetch(address_register);
		address_register++;

		gui->update_register(reg_num, registers[reg_num]);
	}
	gui->update_address_register(address_register);
}

void Chip8::_invalid_opcode(unsigned short opcode)
{
	std::cout << "INVALID OPCODE: 0x" << std::hex << opcode << std::endl;
}

unsigned char Chip8::get_register(unsigned char register_number)
{
	return registers[register_number];
}

unsigned short Chip8::get_address()
{
	return address_register;
}

unsigned short Chip8::get_program_counter()
{
	return program_counter;
}

unsigned char Chip8::get_stack_pointer()
{
	return stack_pointer;
}

