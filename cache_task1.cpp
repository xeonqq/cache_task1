/*
// File: task_1.cpp
//              
// Framework to implement Task 1 of the Advances in Computer Architecture lab 
// session. This uses the ACA 2009 library to interface with tracefiles which
// will drive the read/write requests
//
// Author(s): Michiel W. van Tol, Mike Lankamp, Jony Zhang, 
//            Konstantinos Bousias
// Copyright (C) 2005-2009 by Computer Systems Architecture group, 
//                            University of Amsterdam
//
// Permission is hereby granted, free of charge, to any person obtaining a copy
// of this software and associated documentation files (the "Software"), to deal
// in the Software without restriction, including without limitation the rights
// to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
// copies of the Software, and to permit persons to whom the Software is
// furnished to do so, subject to the following conditions:
//
// The above copyright notice and this permission notice shall be included in
// all copies or substantial portions of the Software.
//
// THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
// IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
// FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
// AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
// LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
// OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
// THE SOFTWARE.
//
 */

#include "aca2009.h"
#include <systemc.h>
#include <iostream>

using namespace std;

static const int MEM_SIZE = 512;

#define CACHE_SETS 8
#define CACHE_LINES 128


typedef	struct 
{
	bool valid;
	sc_uint<7> tag;
	//sc_int<8> data[32]; //32 byte line size 
	int data[8]; //8 words = 32 byte line size 
} aca_cache_line;

typedef	struct 
{
	aca_cache_line cache_line[CACHE_LINES];
} aca_cache_set; 

typedef	struct
{
	aca_cache_set cache_set[CACHE_SETS];
} aca_cache;

SC_MODULE(Cache) 
{

	public:

		enum Function 
		{
			FUNC_READ,
			FUNC_WRITE
		};

		enum RetCode 
		{
			RET_READ_DONE,
			RET_WRITE_DONE,
		};

		sc_in<bool>     Port_CLK;
		sc_in<Function> Port_Func;
		sc_in<int>      Port_Addr;
		sc_out<RetCode> Port_Done;
		sc_inout_rv<32> Port_Data;
		sc_out<bool> 	Port_Hit;

		SC_CTOR(Cache) 
		{
			SC_THREAD(execute);
			sensitive << Port_CLK.pos();
			dont_initialize();

			//m_data = new int[MEM_SIZE];
			cache = new aca_cache;
		}

		~Cache() 
		{
			//delete[] m_data;
			delete cache;

		}
	private:
		aca_cache *cache;

		void execute() 
		{
			while (true)
			{

						cout << "here"<<endl;
				wait(Port_Func.value_changed_event());

						cout << "here2"<<endl;
				Function f = Port_Func.read();
				int addr   = Port_Addr.read();
				//int *data;
				unsigned int line_index,tag  = 0;
				unsigned int word_index = 0;
				bool hit   = false;
				bool valid_lines[8] = {false};

				if (f == FUNC_WRITE) 
				{
					cout << sc_time_stamp() << ": MEM received write" << endl;

					//determine whether a write hit
					line_index = (addr & 0x0FE0) >> 5;
					tag = addr >> 12;
					cout << "line_index: " << line_index <<  "tag: " <<tag << endl;
					aca_cache_line *c_line;
					word_index = ( addr & 0x001C ) >> 2;
					for ( int i=0; i <CACHE_SETS; i++ ){
						c_line = &(cache->cache_set[i].cache_line[line_index]);
						if (c_line -> valid == true){
							valid_lines[i] = true;	
							if ( c_line -> tag == tag)
								hit = true; 
						}
						else{
							hit = false;
							valid_lines[i] = false;
						}
					}
					if (hit){ //write hit
						Port_Hit.write(true);
						stats_writehit(0);
						c_line -> data[word_index] = Port_Data.read().to_int();
						cout << sc_time_stamp() << ": Cache write hit!" << endl;
						//sth need to do with lru
					}
					else //write miss
					{		
						Port_Hit.write(false);
						stats_writemiss(0);
						cout << sc_time_stamp() << ": Cache write miss!" << endl;

						// write allocate
						for (int i = 0; i< 8; i++){
							wait(100); //fetch 8 * words data from memory to cache
							c_line -> data[i] = rand()%10000; //loading mem data to cache 					
						}
						for ( int i=0; i <CACHE_SETS; i++ ){
							if (valid_lines[i] == false){ //use an invalid line
								c_line = &(cache->cache_set[i].cache_line[line_index]);
								c_line -> data[word_index] =  Port_Data.read().to_int();
								c_line -> valid = true;
								c_line -> tag = tag;
								break;

							}
							else if(i == CACHE_SETS-1){// all lines are valid
								//lru
								//write back the previous line to mem and replace the lru line 

							}
						}
					}
					Port_Done.write( RET_WRITE_DONE );
					cout <<"write done" <<endl;

					//cout << sc_time_stamp() << ": MEM received write" << endl;
					//data = Port_Data.read().to_int();
				}
				else//a read comes to cache
				{
					cout << sc_time_stamp() << ": MEM received read" << endl;
					line_index = (addr & 0x0FE0) >> 5;
					tag = addr >> 12;
					cout << "line_index: " << line_index <<  "tag: " <<tag << endl;
					aca_cache_line *c_line;
					word_index = ( addr & 0x001C ) >> 2;
					for ( int i=0; i <CACHE_SETS; i++ ){
						c_line = &(cache->cache_set[i].cache_line[line_index]);
						if (c_line -> valid == true){
							valid_lines[i] = true;	
							if ( c_line -> tag == tag)
								hit = true; 

							else{
								hit = false;
								valid_lines[i] = false;
							}
						}
					}
					if (hit){ //read hit
						Port_Hit.write(true);
						stats_readhit(0);
						Port_Data.write( c_line -> data[word_index] );
						cout << sc_time_stamp() << ": Cache read hit!" << endl;
						//some lru stuff needed to be done
					}
					else //read miss
					{
						Port_Hit.write(false);
						stats_readmiss(0);
						cout << sc_time_stamp() << ": Cache read miss!" << endl;

	
					}
					Port_Done.write( RET_READ_DONE );
					wait();
					Port_Data.write("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");

				}
				/*
				// This simulates memory read/write delayc
				wait(99);

				if (f == FUNC_READ) 
				{
					//Port_Data.write( (addr < MEM_SIZE) ? m_data[addr] : 0 );
					Port_Done.write( RET_READ_DONE );
					wait();
					Port_Data.write("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");
				}
				else
				{
					if (addr < MEM_SIZE) 
					{
						//	m_data[addr] = data;
					}
					Port_Done.write( RET_WRITE_DONE );
				}
				 */
			}
		}
}; 

SC_MODULE(CPU) 
{

	public:
		sc_in<bool>                Port_CLK;
		sc_in<Cache::RetCode>   Port_MemDone;
		sc_out<Cache::Function> Port_MemFunc;
		sc_out<int>                Port_MemAddr;
		sc_inout_rv<32>            Port_MemData;

		SC_CTOR(CPU) 
		{
			SC_THREAD(execute);
			sensitive << Port_CLK.pos();
			dont_initialize();
		}

	private:
		void execute() 
		{
			TraceFile::Entry    tr_data;
			Cache::Function  f;

			// Loop until end of tracefile
			while(!tracefile_ptr->eof())
			{
				// Get the next action for the processor in the trace
				if(!tracefile_ptr->next(0, tr_data))
				{
					cerr << "Error reading trace for CPU" << endl;
					break;
				}

				// To demonstrate the statistic functions, we generate a 50%
				// probability of a 'hit' or 'miss', and call the statistic
				// functions below
				//int j = rand()%2;

				switch(tr_data.type)
				{
					case TraceFile::ENTRY_TYPE_READ:
						f = Cache::FUNC_READ;
						/*
						if(j)
							stats_readhit(0);
						else
							stats_readmiss(0);
						*/
						break;

					case TraceFile::ENTRY_TYPE_WRITE:
						f = Cache::FUNC_WRITE;
						/*
						   if(j)
						   stats_writehit(0);
						   else
						   stats_writemiss(0);
						 */						
						break;

					case TraceFile::ENTRY_TYPE_NOP:
						break;

					default:
						cerr << "Error, got invalid data from Trace" << endl;
						exit(0);
				}

				if(tr_data.type != TraceFile::ENTRY_TYPE_NOP)
				{
					Port_MemAddr.write(tr_data.addr);
					Port_MemFunc.write(f);

					if (f == Cache::FUNC_WRITE) 
					{
						cout << sc_time_stamp() << ": CPU sends write" << endl;

						uint32_t data = rand();
						Port_MemData.write(data);
						wait();
						Port_MemData.write("ZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZZ");
					}
					else
					{
						cout << sc_time_stamp() << ": CPU sends read" << endl;
					}

					wait(Port_MemDone.value_changed_event());

					if (f == Cache::FUNC_READ)
					{
						cout << sc_time_stamp() << ": CPU reads: " << Port_MemData.read() << endl;
					}
				}
				else
				{
					cout << sc_time_stamp() << ": CPU executes NOP" << endl;
				}
				// Advance one cycle in simulated time            
				wait();
			}

			// Finished the Tracefile, now stop the simulation
			sc_stop();
		}
};


int sc_main(int argc, char* argv[])
{
	try
	{
		// Get the tracefile argument and create Tracefile object
		// This function sets tracefile_ptr and num_cpus
		init_tracefile(&argc, &argv);

		// Initialize statistics counters
		stats_init();

		// Instantiate Modules
		Cache mem("main_memory");
		CPU    cpu("cpu");

		// Signals
		sc_buffer<Cache::Function> sigMemFunc;
		sc_buffer<Cache::RetCode>  sigMemDone;
		sc_signal<int>              sigMemAddr;
		sc_signal_rv<32>            sigMemData;
		sc_signal<bool> 	sigMemHit;
		// The clock that will drive the CPU and Cache
		sc_clock clk;

		// Connecting module ports with signals
		mem.Port_Func(sigMemFunc);
		mem.Port_Addr(sigMemAddr);
		mem.Port_Data(sigMemData);
		mem.Port_Done(sigMemDone);
		mem.Port_Hit(sigMemHit);

		cpu.Port_MemFunc(sigMemFunc);
		cpu.Port_MemAddr(sigMemAddr);
		cpu.Port_MemData(sigMemData);
		cpu.Port_MemDone(sigMemDone);

		mem.Port_CLK(clk);
		cpu.Port_CLK(clk);

		cout << "Running (press CTRL+C to interrupt)... " << endl;


		// Start Simulation
		sc_start();

		// Print statistics after simulation finished
		stats_print();
	}

	catch (exception& e)
	{
		cerr << e.what() << endl;
	}

	return 0;
}
